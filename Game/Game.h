#pragma once
#include <chrono>
#include <thread>

#include "../Models/Project_path.h"
#include "Board.h"
#include "Config.h"
#include "Hand.h"
#include "Logic.h"

class Game
{
  public:
    Game() : board(config("WindowSize", "Width"), config("WindowSize", "Hight")), hand(&board), logic(&board, &config)
    {
        ofstream fout(project_path + "log.txt", ios_base::trunc);
        fout.close();
    }

    // to start checkers
    int play()
    {
        auto start = chrono::steady_clock::now(); // начало таймера
        // Если повтор игры, то перезагружается состояние
        if (is_replay)
        {
            logic = Logic(&board, &config);
            config.reload();
            board.redraw();
        }
        else
        {
            board.start_draw();
        }
        is_replay = false;

        int turn_num = -1; // номер текущего хода
        bool is_quit = false; // флаг выхода из игры
        const int Max_turns = config("Game", "MaxNumTurns"); // получения макс. кол-ва ходов
        // Основной игровой цикл
        while (++turn_num < Max_turns)
        {
            beat_series = 0;
            // Поиск возможных ходов для текущего игрока
            logic.find_turns(turn_num % 2);
            // Если ходов нет - конец игры
            if (logic.turns.empty())
                break;
            logic.Max_depth = config("Bot", string((turn_num % 2) ? "Black" : "White") + string("BotLevel"));
            // Выбор того, кто ходит (бот или не бот)
            if (!config("Bot", string("Is") + string((turn_num % 2) ? "Black" : "White") + string("Bot")))
            {
                // Если ходит игрок, то вызывается функция хода игрока
                auto resp = player_turn(turn_num % 2); // Получение ответа от действия игрока
                // Если игрок нажал выход - игра заканчивается
                if (resp == Response::QUIT)
                {
                    is_quit = true;
                    break;
                }
                // Переигровка
                else if (resp == Response::REPLAY)
                {
                    is_replay = true;
                    break;
                }
                // Ход назад
                else if (resp == Response::BACK)
                {
                    if (config("Bot", string("Is") + string((1 - turn_num % 2) ? "Black" : "White") + string("Bot")) &&
                        !beat_series && board.history_mtx.size() > 2)
                    {
                        board.rollback();
                        --turn_num; // отмена хода бота
                    }
                    if (!beat_series) // отмена хода игрока
                        --turn_num;

                    board.rollback(); // откат состояния доски
                    --turn_num;
                    beat_series = 0;
                }
            }
            // Ход бота
            else
                bot_turn(turn_num % 2);
        }
        auto end = chrono::steady_clock::now(); // конец таймера
        ofstream fout(project_path + "log.txt", ios_base::app);
        fout << "Game time: " << (int)chrono::duration<double, milli>(end - start).count() << " millisec\n";
        fout.close();

        // Перезапуск
        if (is_replay)
            return play();
        // Выход
        if (is_quit)
            return 0;
        // Определение результата игры
        int res = 2;
        if (turn_num == Max_turns)
        {
            res = 0;
        }
        else if (turn_num % 2)
        {
            res = 1;
        }
        board.show_final(res);
        auto resp = hand.wait();
        if (resp == Response::REPLAY)
        {
            is_replay = true;
            return play();
        }
        return res;
    }

  private:
    void bot_turn(const bool color)
    {
        auto start = chrono::steady_clock::now(); // начало таймера

        // Получение задержки между ходами из конфигурации
        auto delay_ms = config("Bot", "BotDelayMS");
        // Создание отдельного потока для равной задержки на каждый ход
        // Параллельное выполнение задержки и вычислений
        // new thread for equal delay for each turn
        thread th(SDL_Delay, delay_ms);
        // Поиск лучших ходов с использованием алгоритма бота
        auto turns = logic.find_best_turns(color);
        // Ожидание завершения потока задержки
        th.join();
        bool is_first = true;
        // Выполнение найденной последовательности ходов
        // making moves
        for (auto turn : turns)
        {
            // Задержка между подходами в серии ходов
            if (!is_first)
            {
                SDL_Delay(delay_ms);
            }
            is_first = false;
            // Обновление счетчика серии взятий
            beat_series += (turn.xb != -1);
            // Выполнение одного хода на доске
            board.move_piece(turn, beat_series);
        }

        auto end = chrono::steady_clock::now(); // конец таймера
        ofstream fout(project_path + "log.txt", ios_base::app);
        fout << "Bot turn time: " << (int)chrono::duration<double, milli>(end - start).count() << " millisec\n";
        fout.close();
    }

    Response player_turn(const bool color)
    {
        // Списк клеток, с которых можно начать ход
        // return 1 if quit
        vector<pair<POS_T, POS_T>> cells;
        for (auto turn : logic.turns)
        {
            cells.emplace_back(turn.x, turn.y);
        }
        // Подсветка клеток, с которых можно начать ход
        board.highlight_cells(cells);
        move_pos pos = {-1, -1, -1, -1};
        POS_T x = -1, y = -1;
        // Выбор шашки для хода, пока не сделается ход
        // trying to make first move
        while (true)
        {
            auto resp = hand.get_cell(); // получение действия от пользователя
            if (get<0>(resp) != Response::CELL)
                return get<0>(resp);
            pair<POS_T, POS_T> cell{get<1>(resp), get<2>(resp)};

            bool is_correct = false;
            // Проверка, является ли выбранная клетка допустимой для хода
            for (auto turn : logic.turns)
            {
                // Если игрок выбрал начальную клетку шашки
                if (turn.x == cell.first && turn.y == cell.second)
                {
                    is_correct = true;
                    break;
                }
                // Если игрок уже выбрал начальную клетку и теперь выбирает конечную
                if (turn == move_pos{x, y, cell.first, cell.second})
                {
                    pos = turn;
                    break;
                }
            }
            // Если найден полный ход
            if (pos.x != -1)
                break;
            // Если выбрана некорректная клетка
            if (!is_correct)
            {
                // Если ранее уже была выбрана начальная клетка
                if (x != -1)
                {
                    // Сброс выделения и отображение всех возможных начальных клеток
                    board.clear_active();
                    board.clear_highlight();
                    board.highlight_cells(cells);
                }
                x = -1;
                y = -1;
                continue;
            }
            x = cell.first;
            y = cell.second;
            board.clear_highlight();
            board.set_active(x, y);
            // Отображение всех клеток, куда можно походить с выбранной позиции
            vector<pair<POS_T, POS_T>> cells2;
            for (auto turn : logic.turns)
            {
                if (turn.x == x && turn.y == y)
                {
                    cells2.emplace_back(turn.x2, turn.y2);
                }
            }
            board.highlight_cells(cells2);
        }
        // Очитска всех выделений после выбора хода
        board.clear_highlight();
        board.clear_active();
        // Выполнение выбранного хода
        board.move_piece(pos, pos.xb != -1);
        if (pos.xb == -1)
            return Response::OK;
        // Продолжение битья, если была взята шашка
        // continue beating while can
        beat_series = 1; // взята одна шашка
        while (true)
        {
            // Поиск возможных продолжений битья
            logic.find_turns(pos.x2, pos.y2);
            // Если нет возможности бить - конец хода
            if (!logic.have_beats)
                break;

            // Сбор всех клеток, куда можно бить
            vector<pair<POS_T, POS_T>> cells;
            for (auto turn : logic.turns)
            {
                cells.emplace_back(turn.x2, turn.y2);
            }
            // Подсветка возможных продолжений и выделение текущей шашки
            board.highlight_cells(cells);
            board.set_active(pos.x2, pos.y2);
            // Цикл выбора продолжения битья
            // trying to make move
            while (true)
            {
                auto resp = hand.get_cell();
                if (get<0>(resp) != Response::CELL)
                    return get<0>(resp);
                pair<POS_T, POS_T> cell{get<1>(resp), get<2>(resp)};

                bool is_correct = false;
                // Проверка, является ли выбранная клетка допустимым продолжением битья
                for (auto turn : logic.turns)
                {
                    if (turn.x2 == cell.first && turn.y2 == cell.second)
                    {
                        is_correct = true;
                        pos = turn;
                        break;
                    }
                }
                // Некорректный выбор
                if (!is_correct)
                    continue;
                // Очистка выделения и выполнение продолжений битья
                board.clear_highlight();
                board.clear_active();
                beat_series += 1; // счётчик серии битых шашек
                board.move_piece(pos, beat_series);
                break;
            }
        }

        return Response::OK; // Ход игрока успешно завершен
    }

  private:
    Config config;
    Board board;
    Hand hand;
    Logic logic;
    int beat_series;
    bool is_replay = false;
};

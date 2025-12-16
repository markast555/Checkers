#pragma once
#include <random>
#include <vector>

#include "../Models/Move.h"
#include "Board.h"
#include "Config.h"

const int INF = 1e9;

class Logic
{
  public:
      // Инициализация объектов доски и конфигурации
    Logic(Board *board, Config *config) : board(board), config(config)
    {
        // Инициализация генератора случайных чисел
        rand_eng = std::default_random_engine (
            !((*config)("Bot", "NoRandom")) ? unsigned(time(0)) : 0);
        scoring_mode = (*config)("Bot", "BotScoringType");
        optimization = (*config)("Bot", "Optimization");
    }

    // Основной метод поиска лучших ходов для бота
    vector<move_pos> find_best_turns(const bool color)
    {
        // Очистка предыдущих результатов поиска
        next_move.clear();
        next_best_state.clear();
        // Построение дерева решений: запуск рекурсивного поиска
        find_first_best_turn(board->get_board(), color, -1, -1, 0);
        // Восстановление последовательности ходов из дерева решений
        vector<move_pos> res;
        int state = 0;
        // Восстановление путьи лучших ходов по сохранённым состояниям
        while (state != -1 && next_move[state].x != -1) {
            res.push_back(next_move[state]); // добавление хода в результат
            state = next_best_state[state]; // переход к следующему состоянию
        }
        // Возврат последовательности лучших ходов
        return res;
    }

private:
    // Симуляция хода на копии матрицы состояния
    vector<vector<POS_T>> make_turn(vector<vector<POS_T>> mtx, move_pos turn) const
    {
        // Удаление битой шашки
        if (turn.xb != -1)
            mtx[turn.xb][turn.yb] = 0;
        // Превращение в дамку при достижении края
        if ((mtx[turn.x][turn.y] == 1 && turn.x2 == 0) || (mtx[turn.x][turn.y] == 2 && turn.x2 == 7))
            mtx[turn.x][turn.y] += 2;
        // Перемещение шашки
        mtx[turn.x2][turn.y2] = mtx[turn.x][turn.y];
        mtx[turn.x][turn.y] = 0;
        return mtx;
    }

    // Оценка позиции на доске
    double calc_score(const vector<vector<POS_T>> &mtx, const bool first_bot_color) const
    {
        // Подсчет фигур и оценка позиции
        // color - who is max player
        double w = 0, wq = 0, b = 0, bq = 0;
        for (POS_T i = 0; i < 8; ++i)
        {
            for (POS_T j = 0; j < 8; ++j)
            {
                w += (mtx[i][j] == 1);
                wq += (mtx[i][j] == 3);
                b += (mtx[i][j] == 2);
                bq += (mtx[i][j] == 4);
                // Дополнительные очки за продвижение вперед
                if (scoring_mode == "NumberAndPotential")
                {
                    w += 0.05 * (mtx[i][j] == 1) * (7 - i); // белым выгоднее вверху
                    b += 0.05 * (mtx[i][j] == 2) * (i); // чёрным выгоднее внизу
                }
            }
        }
        if (!first_bot_color)
        {
            swap(b, w);
            swap(bq, wq);
        }
        // Победа бота
        if (w + wq == 0)
            return INF;
        // Поражение бота
        if (b + bq == 0)
            return 0;
        // Коэффициент ценности дамки
        int q_coef = 4;
        if (scoring_mode == "NumberAndPotential")
        {
            q_coef = 5;
        }
        // Сила противника / сила бота
        return (b + bq * q_coef) / (w + wq * q_coef);
    }

    // Поиск лучшего хода для первого уровня рекурсии
    double find_first_best_turn(vector<vector<POS_T>> mtx, const bool color, const POS_T x, const POS_T y, size_t state,
                                double alpha = -1)
    {
        // Добавление новой записи в дерево решений для текущего состояния
        next_move.emplace_back(-1, -1, -1, -1); // инициализация лучшего хода
        next_best_state.push_back(-1); // инициализация ссылки на следующее состояние

        // Если это не корень, то поиск ходов для конкретной шашки
        if (state != 0) {
            find_turns(x, y, mtx);
        }
        // Сохранение найденных ходов
        auto now_turns = turns;
        // Сохранение флага наличия взятий
        auto now_have_beats = have_beats;
        // Если серия взятий закончилась — переход хода сопернику и переход к минимакс
        if (!now_have_beats && state != 0) {
            return find_best_turns_rec(mtx, 1 - color, 0, alpha);
        }
        double best_score = -1; // лучшая оценка для текущего состояния

        // Перебор всех возможных ходов из текущей позиции
        for (auto turn : now_turns) {
            size_t new_state = next_move.size(); // индекс для следующего состояния
            double score;
            // Если есть взятия - продолжение серии (тот же игрок ходит снова)
            if (now_have_beats) {
                // Рекурсивный вызов для продолжения серии взятий
                score = find_first_best_turn(make_turn(mtx, turn), color, turn.x2, turn.y2, new_state, best_score);
            }
            else {
                // Обычный ход - переход хода к противнику
                score = find_best_turns_rec(make_turn(mtx, turn), 1 - color, 0, best_score);
            }
            // Если найден ход с лучшей оценкой - обновление дерева решений
            if (score > best_score) {
                best_score = score;
                next_move[state] = turn;
                next_best_state[state] = (now_have_beats ? new_state : -1);
            }

        }

        // Возврат лучшей найденной оценки
        return best_score;
    }

    // Рекурсивный поиск лучшего хода с минимаксом и альфа-бета отсечением
    double find_best_turns_rec(vector<vector<POS_T>> mtx, const bool color, const size_t depth, double alpha = -1,
                               double beta = INF + 1, const POS_T x = -1, const POS_T y = -1)
    {
        // Достигнута максимальная глубина поиска
        if (depth == Max_depth) {
            // Оценка позиции с учетом чётности глубины
            return calc_score(mtx, (depth % 2 == color));
        }

        // Если продолжается серия взятий, то поиск ходов только для одной шашки
        if (x != -1) {
            find_turns(x, y, mtx);
        }
        else {
            // Иначе новый ход
            find_turns(color, mtx);
        }
        // Сохранение найденных ходов
        auto now_turns = turns;
        // Сохранение флага наличия взятий
        auto now_have_beats = have_beats;

        // Если закончилась серия взятий, то переход хода
        if (!now_have_beats && x != -1) {
            return find_best_turns_rec(mtx, 1 - color, depth + 1, alpha, beta);
        }

        // Если нет доступных ходов - конец игры
        if (turns.empty()) {
            return (depth % 2 ? 0 : INF);
        }
        double min_score = INF + 1;
        double max_score = -1;

        // Перебор всех возможных ходов
        for (auto turn : now_turns) {
            double score;
            // Если есть взятия, то серия продолжается
            if (now_have_beats) {
                score = find_best_turns_rec(make_turn(mtx, turn), color, depth, alpha, beta, turn.x2, turn.y2);
            }
            else {
                // Обычный ход: смена игрока, увеличение глубины
                score = find_best_turns_rec(make_turn(mtx, turn), 1 - color, depth + 1, alpha, beta);
            }

            min_score = min(min_score, score);
            max_score = max(max_score, score);

            // Альфа-бета отсечение
            if (depth % 2) {
                alpha = max(alpha, max_score);
            }
            else {
                beta = min(beta, min_score);
            }

            // Применение оптимизаций
            if (optimization != "00" && alpha > beta) {
                break;
            }
            if (optimization == "02" && alpha == beta) {
                return (depth % 2 ? max_score + 1 : min_score - 1);
            }
        }

        // макс возвращает максимум, мин — минимум
        return (depth % 2 ? max_score : min_score);
    }

public:
    // Поиск ходов для игрока указанного цвета
    void find_turns(const bool color)
    {
        find_turns(color, board->get_board());
    }

    // Поиск ходов для конкретной шашки
    void find_turns(const POS_T x, const POS_T y)
    {
        find_turns(x, y, board->get_board());
    }

private:
    // Поиск всех ходов для всех шашек игрока
    void find_turns(const bool color, const vector<vector<POS_T>> &mtx)
    {
        vector<move_pos> res_turns;
        bool have_beats_before = false;
        // Проверка каждой клетки
        for (POS_T i = 0; i < 8; ++i)
        {
            for (POS_T j = 0; j < 8; ++j)
            {
                if (mtx[i][j] && mtx[i][j] % 2 != color)
                {
                    find_turns(i, j, mtx);
                    // Обязательность взятия: если найдено взятие - удаление обычных ходов
                    if (have_beats && !have_beats_before)
                    {
                        have_beats_before = true;
                        res_turns.clear();
                    }
                    if ((have_beats_before && have_beats) || !have_beats_before)
                    {
                        res_turns.insert(res_turns.end(), turns.begin(), turns.end());
                    }
                }
            }
        }
        turns = res_turns;
        shuffle(turns.begin(), turns.end(), rand_eng);
        have_beats = have_beats_before;
    }

    // Поиск ходов для одной шашки
    void find_turns(const POS_T x, const POS_T y, const vector<vector<POS_T>> &mtx)
    {
        turns.clear();
        have_beats = false;
        POS_T type = mtx[x][y];
        // Проверка взятий
        // check beats
        switch (type)
        {
        case 1: // белая шашка
        case 2: // чёрная шашка
            // Проверка 4 диагональных направлений на 2 клетки
            // check pieces
            for (POS_T i = x - 2; i <= x + 2; i += 4)
            {
                for (POS_T j = y - 2; j <= y + 2; j += 4)
                {
                    if (i < 0 || i > 7 || j < 0 || j > 7)
                        continue;
                    POS_T xb = (x + i) / 2, yb = (y + j) / 2;
                    if (mtx[i][j] || !mtx[xb][yb] || mtx[xb][yb] % 2 == type % 2)
                        continue;
                    turns.emplace_back(x, y, i, j, xb, yb);
                }
            }
            break;
        default:
            // Дамка ходит по диагонали на любое расстояние
            // check queens
            for (POS_T i = -1; i <= 1; i += 2)
            {
                for (POS_T j = -1; j <= 1; j += 2)
                {
                    POS_T xb = -1, yb = -1;
                    // Движение по диагонали
                    for (POS_T i2 = x + i, j2 = y + j; i2 != 8 && j2 != 8 && i2 != -1 && j2 != -1; i2 += i, j2 += j)
                    {
                        if (mtx[i2][j2])
                        {
                            if (mtx[i2][j2] % 2 == type % 2 || (mtx[i2][j2] % 2 != type % 2 && xb != -1))
                            {
                                break;
                            }
                            xb = i2;
                            yb = j2;
                        }
                        if (xb != -1 && xb != i2)
                        {
                            turns.emplace_back(x, y, i2, j2, xb, yb);
                        }
                    }
                }
            }
            break;
        }
        // Если есть взятия - только они разрешены
        // check other turns
        if (!turns.empty())
        {
            have_beats = true;
            return;
        }
        // Проверка обычных ходов (без взятия)
        switch (type)
        {
        case 1: // белая шашка
        case 2: // чёрная шашка
            // check pieces
            {
                POS_T i = ((type % 2) ? x - 1 : x + 1);
                for (POS_T j = y - 1; j <= y + 1; j += 2)
                {
                    if (i < 0 || i > 7 || j < 0 || j > 7 || mtx[i][j])
                        continue;
                    turns.emplace_back(x, y, i, j);
                }
                break;
            }
        default:
            // Дамки
            // check queens
            for (POS_T i = -1; i <= 1; i += 2)
            {
                for (POS_T j = -1; j <= 1; j += 2)
                {
                    // Движение по диагонали пока не упремся в шашку или край
                    for (POS_T i2 = x + i, j2 = y + j; i2 != 8 && j2 != 8 && i2 != -1 && j2 != -1; i2 += i, j2 += j)
                    {
                        if (mtx[i2][j2])
                            break;
                        turns.emplace_back(x, y, i2, j2);
                    }
                }
            }
            break;
        }
    }

  public:
    vector<move_pos> turns;              // Найденные ходы
    bool have_beats;                     // Есть ли ходы со взятием
    int Max_depth;                       // Глубина поиска (сложность бота)

  private:
    default_random_engine rand_eng;      // Генератор случайных чисел
    string scoring_mode;                 // Тип оценки
    string optimization;                 // Уровень оптимизации
    vector<move_pos> next_move;          // Дерево решений
    vector<int> next_best_state;         // Следующее состояние после хода
    Board *board;                        // Ссылка на доску
    Config *config;                      // Ссылка на конфигурацию
};

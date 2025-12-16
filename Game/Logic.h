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
        // Очистка предыдущих результатов
        next_best_state.clear();
        next_move.clear();

        // Рекурсивный поиск лучшего хода
        find_first_best_turn(board->get_board(), color, -1, -1, 0);

        // Восстановление последовательности ходов из дерева решений
        int cur_state = 0;
        vector<move_pos> res;
        do
        {
            res.push_back(next_move[cur_state]);
            cur_state = next_best_state[cur_state];
        } while (cur_state != -1 && next_move[cur_state].x != -1);
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

    // Поиск лучшего хода
    double find_first_best_turn(vector<vector<POS_T>> mtx, const bool color, const POS_T x, const POS_T y, size_t state,
                                double alpha = -1)
    {
        next_best_state.push_back(-1);
        next_move.emplace_back(-1, -1, -1, -1);
        double best_score = -1;
        // Поиск ходов из текущей позиции
        if (state != 0)
            find_turns(x, y, mtx);
        auto turns_now = turns;
        bool have_beats_now = have_beats;

        // Переход хода при отсутствии обязательного боя
        if (!have_beats_now && state != 0)
        {
            return find_best_turns_rec(mtx, 1 - color, 0, alpha);
        }

        // Оценка всех возможных ходов
        vector<move_pos> best_moves;
        vector<int> best_states;

        for (auto turn : turns_now)
        {
            size_t next_state = next_move.size();
            double score;
            // Если есть взятие - продолжаем серию, иначе - ход противника
            if (have_beats_now)
            {
                score = find_first_best_turn(make_turn(mtx, turn), color, turn.x2, turn.y2, next_state, best_score);
            }
            else
            {
                score = find_best_turns_rec(make_turn(mtx, turn), 1 - color, 0, best_score);
            }
            // Сохранение лучшего варианта
            if (score > best_score)
            {
                best_score = score;
                next_best_state[state] = (have_beats_now ? int(next_state) : -1);
                next_move[state] = turn;
            }
        }
        return best_score;
    }

    // Рекурсивный поиск лучшего хода с альфа-бета отсечением
    double find_best_turns_rec(vector<vector<POS_T>> mtx, const bool color, const size_t depth, double alpha = -1,
                               double beta = INF + 1, const POS_T x = -1, const POS_T y = -1)
    {
        // Достигли максимальной глубины - оценка позиции
        if (depth == Max_depth)
        {
            return calc_score(mtx, (depth % 2 == color));
        }
        // Поиск доступных ходов
        if (x != -1)
        {
            find_turns(x, y, mtx); // для продолжения серии взятий
        }
        else
            find_turns(color, mtx); // для нового хода
        auto turns_now = turns;
        bool have_beats_now = have_beats;

        // Конец серии взятий - переход хода
        if (!have_beats_now && x != -1)
        {
            return find_best_turns_rec(mtx, 1 - color, depth + 1, alpha, beta);
        }

        // Нет ходов - конец игры
        if (turns.empty())
            return (depth % 2 ? 0 : INF);

        // Минимакс алгоритм
        double min_score = INF + 1;
        double max_score = -1;
        for (auto turn : turns_now)
        {
            double score = 0.0;
            // Обычный ход (смена игрока), иначе - продолжение серии взятий
            if (!have_beats_now && x == -1)
            {
                score = find_best_turns_rec(make_turn(mtx, turn), 1 - color, depth + 1, alpha, beta);
            }
            else
            {
                score = find_best_turns_rec(make_turn(mtx, turn), color, depth, alpha, beta, turn.x2, turn.y2);
            }
            min_score = min(min_score, score);
            max_score = max(max_score, score);
            // Альфа-бета отсечение
            // alpha-beta pruning
            if (depth % 2)
                alpha = max(alpha, max_score);
            else
                beta = min(beta, min_score);
            if (optimization != "O0" && alpha >= beta)
                return (depth % 2 ? max_score + 1 : min_score - 1);
        }
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

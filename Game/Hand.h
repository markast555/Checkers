#pragma once
#include <tuple>

#include "../Models/Move.h"
#include "../Models/Response.h"
#include "Board.h"

// Класс Hand - обработчик пользовательского ввода (мышь, события окна)
// methods for hands
class Hand
{
  public:
    Hand(Board *board) : board(board)
    {
    }
    // Ожидание и обработка действие пользователя
    tuple<Response, POS_T, POS_T> get_cell() const
    {
        SDL_Event windowEvent; // Структура для хранения события SDL
        Response resp = Response::OK; // ответ
        int x = -1, y = -1; // координаты мыши на экране
        int xc = -1, yc = -1; // координаты клетки на доске
        // Бесконечный цикл ожидания события
        while (true)
        {
            // Проверка наличия события в очереди SDL
            if (SDL_PollEvent(&windowEvent))
            {
                switch (windowEvent.type)
                {
                // Событие закрытия окна
                case SDL_QUIT:
                    resp = Response::QUIT;
                    break;
                // Нажатие кнопки мыши
                case SDL_MOUSEBUTTONDOWN:
                    // Получение координат мыши
                    x = windowEvent.motion.x;
                    y = windowEvent.motion.y;
                    // Преобразование координат экрана в координаты клеток доски
                    xc = int(y / (board->H / 10) - 1);
                    yc = int(x / (board->W / 10) - 1);
                    // Проверка нажатия на кнопку "Назад"
                    if (xc == -1 && yc == -1 && board->history_mtx.size() > 1)
                    {
                        resp = Response::BACK;
                    }
                    // Проверка нажатия на кнопку "Повторить"
                    else if (xc == -1 && yc == 8)
                    {
                        resp = Response::REPLAY;
                    }
                    // Проверка нажатия на игровое поле
                    else if (xc >= 0 && xc < 8 && yc >= 0 && yc < 8)
                    {
                        resp = Response::CELL;
                    }
                    // Нажатие вне игрового поля и кнопок - игнорирование
                    else
                    {
                        xc = -1;
                        yc = -1;
                    }
                    break;
                // События окна
                case SDL_WINDOWEVENT:
                    // Обработка изменения размера окна
                    if (windowEvent.window.event == SDL_WINDOWEVENT_SIZE_CHANGED)
                    {
                        board->reset_window_size();
                        break;
                    }
                }
                // Если получен значимый ответ - выход из цикла
                if (resp != Response::OK)
                    break;
            }
        }
        // Возврат результат: тип действия + координаты клетки
        return {resp, xc, yc};
    }
    // Ожидание действия пользователя на финальном экране
    Response wait() const
    {
        SDL_Event windowEvent;
        Response resp = Response::OK;
        while (true)
        {
            if (SDL_PollEvent(&windowEvent))
            {
                switch (windowEvent.type)
                {
                case SDL_QUIT:
                    resp = Response::QUIT;
                    break;
                case SDL_WINDOWEVENT_SIZE_CHANGED:
                    board->reset_window_size();
                    break;
                case SDL_MOUSEBUTTONDOWN: {
                    // Определение координат нажатия
                    int x = windowEvent.motion.x;
                    int y = windowEvent.motion.y;
                    int xc = int(y / (board->H / 10) - 1);
                    int yc = int(x / (board->W / 10) - 1);
                    // Проверка нажатия на кнопку "Повторить"
                    if (xc == -1 && yc == 8)
                        resp = Response::REPLAY;
                }
                break;
                }
                if (resp != Response::OK)
                    break;
            }
        }
        return resp;
    }

  private:
    Board *board;
};

// проверка кодировки

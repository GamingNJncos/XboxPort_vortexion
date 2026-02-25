#ifndef INPUT_H
#define INPUT_H

#include <SDL.h>

typedef enum {
    BTN_UP = 0,
    BTN_DOWN = 1,
    BTN_LEFT = 2,
    BTN_RIGHT = 3,
    BTN_SHOOT = 4,
    BTN_PAUSE = 5,
    BTN_X = 6,
    BTN_Y = 7,
    BTN_B = 8,
    BTN_COUNT = 9
} PlayerButton;

typedef enum {
    MENU_LEFT = 0,
    MENU_RIGHT = 1,
    MENU_SELECT = 2,
    MENU_BACK = 3,
    MENU_COUNT = 4
} MenuButton;

void input_init(void);
void input_update(void);
void input_shutdown(void);

int input_pressed(PlayerButton btn);
int input_just_pressed(PlayerButton btn);
int menu_just_pressed(MenuButton btn);
int any_button_just_pressed(void);

#endif

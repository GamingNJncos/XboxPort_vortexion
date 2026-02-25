#include "input.h"
#include <string.h>

static SDL_GameController *controller = NULL;

static int input_state[BTN_COUNT];
static int input_prev[BTN_COUNT];

static int sdl_btn_state[SDL_CONTROLLER_BUTTON_MAX];
static int sdl_btn_prev[SDL_CONTROLLER_BUTTON_MAX];

void input_init(void) {
    if (SDL_IsGameController(0)) {
        controller = SDL_GameControllerOpen(0);
    }
    memset(input_state, 0, sizeof(input_state));
    memset(input_prev, 0, sizeof(input_prev));
    memset(sdl_btn_state, 0, sizeof(sdl_btn_state));
    memset(sdl_btn_prev, 0, sizeof(sdl_btn_prev));
}

void input_update(void) {
    SDL_GameControllerUpdate();

    memcpy(input_prev, input_state, sizeof(input_prev));
    memcpy(sdl_btn_prev, sdl_btn_state, sizeof(sdl_btn_prev));

    if (controller) {
        // D-pad and Axis
        int dpad_left = SDL_GameControllerGetButton(controller, SDL_CONTROLLER_BUTTON_DPAD_LEFT);
        int dpad_right = SDL_GameControllerGetButton(controller, SDL_CONTROLLER_BUTTON_DPAD_RIGHT);
        int dpad_up = SDL_GameControllerGetButton(controller, SDL_CONTROLLER_BUTTON_DPAD_UP);
        int dpad_down = SDL_GameControllerGetButton(controller, SDL_CONTROLLER_BUTTON_DPAD_DOWN);

        int axis_leftx = SDL_GameControllerGetAxis(controller, SDL_CONTROLLER_AXIS_LEFTX);
        int axis_lefty = SDL_GameControllerGetAxis(controller, SDL_CONTROLLER_AXIS_LEFTY);

        input_state[BTN_LEFT] = dpad_left || (axis_leftx < -8000);
        input_state[BTN_RIGHT] = dpad_right || (axis_leftx > 8000);
        input_state[BTN_UP] = dpad_up || (axis_lefty < -8000);
        input_state[BTN_DOWN] = dpad_down || (axis_lefty > 8000);
        input_state[BTN_SHOOT] = SDL_GameControllerGetButton(controller, SDL_CONTROLLER_BUTTON_A);
        input_state[BTN_PAUSE] = SDL_GameControllerGetButton(controller, SDL_CONTROLLER_BUTTON_START);
        input_state[BTN_X] = SDL_GameControllerGetButton(controller, SDL_CONTROLLER_BUTTON_X);
        input_state[BTN_Y] = SDL_GameControllerGetButton(controller, SDL_CONTROLLER_BUTTON_Y);
        input_state[BTN_B] = SDL_GameControllerGetButton(controller, SDL_CONTROLLER_BUTTON_B);

        // Update sdl_btn_state
        for (int i = 0; i < SDL_CONTROLLER_BUTTON_MAX; ++i) {
            sdl_btn_state[i] = SDL_GameControllerGetButton(controller, (SDL_GameControllerButton)i);
        }
    }
}

int input_pressed(PlayerButton btn) {
    if (btn < 0 || btn >= BTN_COUNT) return 0;
    return input_state[btn];
}

int input_just_pressed(PlayerButton btn) {
    if (btn < 0 || btn >= BTN_COUNT) return 0;
    return input_state[btn] && !input_prev[btn];
}

void input_shutdown(void) {
    if (controller) {
        SDL_GameControllerClose(controller);
        controller = NULL;
    }
}

int menu_just_pressed(MenuButton btn) {
    switch (btn) {
        case MENU_LEFT:
            return input_just_pressed(BTN_LEFT);
        case MENU_RIGHT:
            return input_just_pressed(BTN_RIGHT);
        case MENU_SELECT:
            return input_just_pressed(BTN_SHOOT) || (sdl_btn_state[SDL_CONTROLLER_BUTTON_START] && !sdl_btn_prev[SDL_CONTROLLER_BUTTON_START]);
        case MENU_BACK:
            return (sdl_btn_state[SDL_CONTROLLER_BUTTON_B] && !sdl_btn_prev[SDL_CONTROLLER_BUTTON_B]);
        default:
            return 0;
    }
}

int any_button_just_pressed(void) {
    for (int i = 0; i < BTN_COUNT; ++i) {
        if (input_state[i] && !input_prev[i]) return 1;
    }
    for (int i = 0; i < SDL_CONTROLLER_BUTTON_MAX; ++i) {
        if (sdl_btn_state[i] && !sdl_btn_prev[i]) return 1;
    }
    return 0;
}

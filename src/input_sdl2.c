/*Arculator 2.0 by Sarah Walker
  SDL2 input handling*/
#include <SDL.h>
#include <string.h>
#include "arc.h"
#include "plat_input.h"
#include "video_sdl2.h"

static int mouse_buttons;
static int mouse_x = 0, mouse_y = 0;

static int mouse_capture = 0;

int mouse[3];

static void mouse_init()
{
}

static void mouse_close()
{
}

void mouse_capture_enable()
{
        rpclog("Mouse captured\n");
        SDL_SetRelativeMouseMode(SDL_TRUE);
        SDL_SetWindowGrab(sdl_main_window, SDL_TRUE);
        mouse_capture = 1;
}

void mouse_capture_disable()
{
        rpclog("Mouse released\n");
        mouse_capture = 0;
        SDL_SetWindowGrab(sdl_main_window, SDL_FALSE);
        SDL_SetRelativeMouseMode(SDL_FALSE);
}

void mouse_poll_host()
{
        if (mouse_capture)
        {
                SDL_Rect rect;
                uint32_t mb = SDL_GetRelativeMouseState(&mouse[0], &mouse[1]);

                mouse_buttons = 0;
                if (mb & SDL_BUTTON(SDL_BUTTON_LEFT))
                {
                        mouse_buttons |= 1;
                }
                if (mb & SDL_BUTTON(SDL_BUTTON_RIGHT))
                {
                        mouse_buttons |= 2;
                }
                if (mb & SDL_BUTTON(SDL_BUTTON_MIDDLE))
                {
                        mouse_buttons |= 4;
                }

                mouse_x += mouse[0];
                mouse_y += mouse[1];

                SDL_GetWindowSize(sdl_main_window, &rect.w, &rect.h);
                SDL_WarpMouseInWindow(sdl_main_window, rect.w/2, rect.h/2);
        }
        else
        {
                mouse_x = mouse_y = mouse_buttons = 0;
        }
        // printf("mouse %d, %d\n", mouse_x, mouse_y);
}

void mouse_get_mickeys(int *x, int *y)
{
        *x = mouse_x;
        *y = mouse_y;
        mouse_x = mouse_y = 0;
}

int mouse_get_buttons()
{
        return mouse_buttons;
}


int key[512];

static void keyboard_init()
{
}

static void keyboard_close()
{
}

void keyboard_poll_host()
{
        int c;
        const uint8_t *state = SDL_GetKeyboardState(NULL);
        
        for (c = 0; c < 512; c++)
                key[c] = state[c];
}


void input_init()
{
        mouse_init();
        keyboard_init();
}
void input_close()
{
        keyboard_close();
        mouse_close();
}

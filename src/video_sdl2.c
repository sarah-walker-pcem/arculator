#include <SDL2/SDL.h>

#if WIN32
#define BITMAP __win_bitmap
#include <windows.h>
#undef BITMAP
#endif

#include <stdio.h>
#include "vidc.h"
#include "plat_video.h"
#include "video_sdl2.h"

static SDL_Texture *texture = NULL;
static SDL_Renderer *renderer = NULL;
SDL_Window *window = NULL;
static SDL_Rect texture_rect;

int video_renderer_init(void *main_window)
{
        int flags = SDL_RENDERER_ACCELERATED;
        SDL_Rect screen_rect;
        
        SDL_SetHint(SDL_HINT_WINDOWS_DISABLE_THREAD_NAMING, "1");
        SDL_Init(SDL_INIT_EVERYTHING);

        window = SDL_CreateWindowFrom(main_window);
        screen_rect.w = screen_rect.h = 2048;

        if (!window)
        {
                char message[200];
                sprintf(message,
                                "SDL_CreateWindowFrom could not be created! Error: %s\n",
                                SDL_GetError());
#if WIN32
                MessageBox(main_window, message, "SDL Error", MB_OK);
#else
                printf(message);
#endif                
                return SDL_FALSE;
        }
                renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);

        if (!renderer)
        {
                char message[200];
                sprintf(message,
                                "SDL window could not be created! Error: %s\n",
                                SDL_GetError());
#if WIN32
                MessageBox(main_window, message, "SDL Error", MB_OK);
#else
                printf(message);
#endif                
                return SDL_FALSE;
        }

        texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888,
                        SDL_TEXTUREACCESS_STREAMING,
                        screen_rect.w, screen_rect.h);

        return SDL_TRUE;
}

void video_renderer_close()
{
        if (texture)
        {
                SDL_DestroyTexture(texture);
                texture = NULL;
        }
        if (renderer)
        {
                SDL_DestroyRenderer(renderer);
                renderer = NULL;
        }
}

void video_renderer_update(BITMAP *src, int src_x, int src_y, int dest_x, int dest_y, int w, int h)
{
//        rpclog("update: src=%i,%i dest=%i,%i size=%i,%i\n", src_x,src_y, dest_x,dest_y, w,h);
        texture_rect.x = dest_x;
        texture_rect.y = dest_y;
        texture_rect.w = w;
        texture_rect.h = h;
        
        if (src_x < 0)
        {
                texture_rect.w += src_x;
                src_x = 0;
        }
        if (src_x > 2047)
                return;
        if ((src_x + texture_rect.w) > 2047)
                texture_rect.w = 2048 - src_x;

        if (src_y < 0)
        {
                texture_rect.h += src_y;
                src_y = 0;
        }
        if (src_y > 2047)
                return;
        if ((src_y + texture_rect.h) > 2047)
                texture_rect.h = 2048 - src_y;
        
        if (texture_rect.x < 0)
        {
                texture_rect.w += texture_rect.x;
                texture_rect.x = 0;
        }
        if (texture_rect.x > 2047)
                return;
        if ((texture_rect.x + texture_rect.w) > 2047)
                texture_rect.w = 2048 - texture_rect.x;

        if (texture_rect.y < 0)
        {
                texture_rect.h += texture_rect.y;
                texture_rect.y = 0;
        }
        if (texture_rect.y > 2047)
                return;
        if ((texture_rect.y + texture_rect.h) > 2047)
                texture_rect.h = 2048 - texture_rect.y;

        SDL_UpdateTexture(texture, &texture_rect, &((uint32_t *)src->dat)[src_y * src->w + src_x], src->w * 4);
}

void video_renderer_present(int src_x, int src_y, int src_w, int src_h)
{
        SDL_Rect window_rect;

        SDL_GetWindowSize(window, &window_rect.w, &window_rect.h);               
        window_rect.x = 0;
        window_rect.y = 0;

        texture_rect.x = src_x;
        texture_rect.y = src_y;
        texture_rect.w = src_w;
        texture_rect.h = src_h;

        SDL_RenderClear(renderer);
/*rpclog("Present %i,%i %i,%i -> %i,%i %i,%i\n", texture_rect.x, texture_rect.y, texture_rect.w, texture_rect.h,
                                                window_rect.x, window_rect.y, window_rect.w, window_rect.h);*/
        SDL_RenderCopy(renderer, texture, &texture_rect, &window_rect);
        SDL_RenderPresent(renderer);
}

/*Arculator 2.0 by Sarah Walker
  SDL2 video handling*/
#include <SDL.h>

#if WIN32
#define BITMAP __win_bitmap
#include <windows.h>
#undef BITMAP
#endif

#include <stdio.h>
#include "arc.h"
#include "plat_video.h"
#include "vidc.h"
#include "video.h"
#include "video_sdl2.h"

static SDL_Texture *texture = NULL;
static SDL_Renderer *renderer = NULL;
SDL_Window *sdl_main_window = NULL;
static SDL_Rect texture_rect;

int selected_video_renderer;

typedef struct sdl_render_driver_t
{
        int id;
        char *sdl_id;
        int available;
} sdl_render_driver_t;

static sdl_render_driver_t sdl_render_drivers[] =
{
        {RENDERER_AUTO, "auto", 1},
        {RENDERER_DIRECT3D, "direct3d", 0},
        {RENDERER_OPENGL, "opengl", 0},
        {RENDERER_SOFTWARE, "software", 0}
};

int video_renderer_available(int id)
{
        return sdl_render_drivers[id].available;
}

char *video_renderer_get_name(int id)
{
        return sdl_render_drivers[id].sdl_id;
}
int video_renderer_get_id(char *name)
{
        int c;
        
        for (c = 0; c < RENDERER_COUNT; c++)
        {
                if (!strcmp(sdl_render_drivers[c].sdl_id, name))
                        return c;
        }
        
        return 0;
}

static int video_renderer_create(void *main_window)
{
        SDL_Rect screen_rect;

        screen_rect.w = screen_rect.h = 2048;

        SDL_SetHint(SDL_HINT_RENDER_DRIVER, sdl_render_drivers[selected_video_renderer].sdl_id);
        SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, video_linear_filtering ? "1" : "0");

        rpclog("create SDL renderer\n");
        renderer = SDL_CreateRenderer(sdl_main_window, -1, SDL_RENDERER_ACCELERATED);

        if (!renderer)
        {
                char message[200];
                sprintf(message,
                                "SDL window could not be created! Error: %s\n",
                                SDL_GetError());
#if WIN32
                MessageBox(main_window, message, "SDL Error", MB_OK);
#else
                printf("%s", message);
#endif                
                return SDL_FALSE;
        }

        rpclog("create texture\n");
        texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888,
                        SDL_TEXTUREACCESS_STREAMING,
                        screen_rect.w, screen_rect.h);

        rpclog("video initialized\n");
        return SDL_TRUE;
}

int video_renderer_init(void *main_window)
{
        int c, d;
        
        rpclog("video_renderer_init()\n");

        for (c = 0; c < SDL_GetNumRenderDrivers(); c++)
        {
                SDL_RendererInfo renderInfo;
                SDL_GetRenderDriverInfo(c, &renderInfo);

                for (d = 0; d < RENDERER_COUNT; d++)
                {
                        if (!strcmp(sdl_render_drivers[d].sdl_id, renderInfo.name))
                                sdl_render_drivers[d].available = 1;
                }
        }
        
        rpclog("create SDL window\n");
        if (main_window == NULL)
        {
                sdl_main_window = SDL_CreateWindow(
                        "Arculator",
                        SDL_WINDOWPOS_CENTERED,
                        SDL_WINDOWPOS_CENTERED,
                        768,
                        576,
                        0
                );
        }
        else
        {
                sdl_main_window = SDL_CreateWindowFrom(main_window);
        }

        if (!sdl_main_window)
        {
                char message[200];
                sprintf(message,
                                "SDL_CreateWindowFrom could not be created! Error: %s\n",
                                SDL_GetError());
#if WIN32
                MessageBox(main_window, message, "SDL Error", MB_OK);
#else
                printf("%s", message);
#endif
                return SDL_FALSE;
        }

        return video_renderer_create(main_window);
}

int video_renderer_reinit(void *main_window)
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
        return video_renderer_create(main_window);
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

/*Update display texture from memory bitmap src.*/
void video_renderer_update(BITMAP *src, int src_x, int src_y, int dest_x, int dest_y, int w, int h)
{
        LOG_VIDEO_FRAMES("video_renderer_update: src=%i,%i dest=%i,%i size=%i,%i\n", src_x,src_y, dest_x,dest_y, w,h);
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

        if (texture_rect.w <= 0 || texture_rect.h <= 0)
                return;
                
        LOG_VIDEO_FRAMES("SDL_UpdateTexture (%d, %d)+(%d, %d) from src (%d, %d) w %d\n",
                texture_rect.x, texture_rect.y,
                texture_rect.w, texture_rect.h,
                src_x, src_y, src->w);
        SDL_UpdateTexture(texture, &texture_rect, &((uint32_t *)src->dat)[src_y * src->w + src_x], src->w * 4);
}

static void sdl_scale(int scale, SDL_Rect src, SDL_Rect *dst, int w, int h)
{
        double t, b, l, r;
        int ratio_w, ratio_h;
        switch (scale)
        {
                case FULLSCR_SCALE_43:
                t = 0;
                b = src.h;
                l = (src.w / 2) - ((src.h * 4) / (3 * 2));
                r = (src.w / 2) + ((src.h * 4) / (3 * 2));
                if (l < 0)
                {
                        l = 0;
                        r = src.w;
                        t = (src.h / 2) - ((src.w * 3) / (4 * 2));
                        b = (src.h / 2) + ((src.w * 3) / (4 * 2));
                }
                break;
                case FULLSCR_SCALE_SQ:
                t = 0;
                b = src.h;
                l = (src.w / 2) - ((src.h * w) / (h * 2));
                r = (src.w / 2) + ((src.h * w) / (h * 2));
                if (l < 0)
                {
                        l = 0;
                        r = src.w;
                        t = (src.h / 2) - ((src.w * h) / (w * 2));
                        b = (src.h / 2) + ((src.w * h) / (w * 2));
                }
                break;
                case FULLSCR_SCALE_INT:
                ratio_w = src.w / w;
                ratio_h = src.h / h;
                if (ratio_h < ratio_w)
                        ratio_w = ratio_h;
                l = (src.w / 2) - ((w * ratio_w) / 2);
                r = (src.w / 2) + ((w * ratio_w) / 2);
                t = (src.h / 2) - ((h * ratio_w) / 2);
                b = (src.h / 2) + ((h * ratio_w) / 2);
                break;
                case FULLSCR_SCALE_FULL:
                default:
                l = 0;
                t = 0;
                r = src.w;
                b = src.h;
                break;
        }

        dst->x = l;
        dst->y = t;
        dst->w = r - l;
        dst->h = b - t;
}

/*Render display texture to video window.*/
void video_renderer_present(int src_x, int src_y, int src_w, int src_h, int dblscan)
{
        LOG_VIDEO_FRAMES("video_renderer_present: %d,%d + %d,%d\n", src_x, src_y, src_w, src_h);

        SDL_Rect window_rect;

        SDL_GetWindowSize(sdl_main_window, &window_rect.w, &window_rect.h);
        window_rect.x = 0;
        window_rect.y = 0;

        texture_rect.x = src_x;
        texture_rect.y = src_y;
        texture_rect.w = src_w;
        texture_rect.h = src_h;

        if (fullscreen)
        {
                if (dblscan)
                        sdl_scale(video_fullscreen_scale, window_rect, &window_rect, texture_rect.w, texture_rect.h*2);
                else
                        sdl_scale(video_fullscreen_scale, window_rect, &window_rect, texture_rect.w, texture_rect.h);
        }
        
        /*Clear the renderer backbuffer*/
        SDL_RenderClear(renderer);
/*rpclog("Present %i,%i %i,%i -> %i,%i %i,%i\n", texture_rect.x, texture_rect.y, texture_rect.w, texture_rect.h,
                                                window_rect.x, window_rect.y, window_rect.w, window_rect.h);*/
        /*Copy texture to backbuffer*/
        SDL_RenderCopy(renderer, texture, &texture_rect, &window_rect);
        /*Present backbuffer to window*/
        SDL_RenderPresent(renderer);
}

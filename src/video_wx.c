/*Arculator 2.2 by Sarah Walker
  SDL2 video handling*/
#include <stdio.h>
#include <string.h>
#include "arc.h"
#include "plat_video.h"
#include "vidc.h"
#include "video.h"
#include "video_sdl2.h"

int selected_video_renderer;

static char *vrname = "wxrender";

BITMAP *wxbuffer;

int video_renderer_available(int id)
{
	return 1;
}

char *video_renderer_get_name(int id)
{
	return vrname;
}
int video_renderer_get_id(char *name)
{
	return 0;
}

int video_renderer_init(void *main_window)
{
	wxbuffer = create_bitmap(4096, 2048);

	return 1;
}

int video_renderer_reinit(void *main_window)
{
	return 0;
}

void video_renderer_close()
{
}

/*Update display texture from memory bitmap src.*/
void video_renderer_update(BITMAP *src, int src_x, int src_y, int dest_x, int dest_y, int w, int h)
{
	//LOG_VIDEO_FRAMES
	//printf("video_renderer_update: src=%i,%i dest=%i,%i size=%i,%i  %08x\n", src_x,src_y, dest_x,dest_y, w,h, ((uint32_t *)src->line[src_y])[src_x]);

	for (int y = 0; y < h; y++)
	{
		memcpy(&((uint32_t *)wxbuffer->line[dest_y+y])[dest_x], &((uint32_t *)src->line[src_y+y])[src_x], w * 4);
	}
}

/*Render display texture to video window.*/
/*void video_renderer_present(int src_x, int src_y, int src_w, int src_h, int dblscan)
{
	LOG_VIDEO_FRAMES("video_renderer_present: %d,%d + %d,%d\n", src_x, src_y, src_w, src_h);
}*/

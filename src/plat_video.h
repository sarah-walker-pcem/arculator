#include "vidc.h"

int video_renderer_init(void *main_window);
int video_renderer_reinit(void *main_window);
void video_renderer_close();

void video_renderer_update(BITMAP *src, int x1, int y1, int x2, int y2, int dest_x, int dest_y);
void video_renderer_present(int src_x, int src_y, int src_w, int src_h, int dblscan);

#define RENDERER_AUTO 0
#define RENDERER_DIRECT3D 1
#define RENDERER_OPENGL 2
#define RENDERER_SOFTWARE 3

#define RENDERER_COUNT 4

int video_renderer_available(int id);
char *video_renderer_get_name(int id);
int video_renderer_get_id(char *name);
extern int selected_video_renderer;

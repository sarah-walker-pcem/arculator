#include "vidc.h"

int video_renderer_init(void *main_window);
void video_renderer_close();

void video_renderer_update(BITMAP *src, int x1, int y1, int x2, int y2, int dest_x, int dest_y);
void video_renderer_present(int src_x, int src_y, int src_w, int src_h);

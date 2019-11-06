extern int display_mode;

/*Never display borders*/
#define DISPLAY_MODE_NO_BORDERS     0
/*Display borders drawn by VIDC. VIDC often draws no borders on VGA/multisync modes*/
#define DISPLAY_MODE_NATIVE_BORDERS 1
/*Display area drawn by TV-res monitor*/
#define DISPLAY_MODE_TV             2

extern int video_scale;
extern int video_fullscreen_scale;

enum
{
        FULLSCR_SCALE_FULL = 0,
        FULLSCR_SCALE_43,
        FULLSCR_SCALE_SQ,
        FULLSCR_SCALE_INT
};

extern int video_linear_filtering;

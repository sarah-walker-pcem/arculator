extern int display_mode;

/*Never display borders*/
#define DISPLAY_MODE_NO_BORDERS     0
/*Display borders drawn by VIDC. VIDC often draws no borders on VGA/multisync modes*/
#define DISPLAY_MODE_NATIVE_BORDERS 1
/*Display area drawn by TV-res monitor*/
#define DISPLAY_MODE_TV             2

typedef struct
{
        const char *name;
        const char *config_name;
        void *(*init)();
        void (*close)(void *p);
        const int axis_count, button_count, pov_count;
        const int max_joysticks;
        const char *axis_names[8];
        const char *button_names[32];
        const char *pov_names[4];
} joystick_if_t;

extern int joystick_type;

void joystick_if_init();
const char *joystick_get_name(int joystick);
const char *joystick_get_config_name(int joystick);
const int joystick_get_max_joysticks(int joystick);
const int joystick_get_axis_count(int joystick);
const int joystick_get_button_count(int joystick);
const int joystick_get_pov_count(int joystick);
const char *joystick_get_axis_name(int joystick, int id);
const char *joystick_get_button_name(int joystick, int id);
const char *joystick_get_pov_name(int joystick, int id);
int joystick_get_type(char *config_name);

#define AXIS_NOT_PRESENT -99999

extern int joystick_a3010_present;
extern int joystick_gamespad_present;
extern int joystick_rtfm_present;
extern int joystick_serial_port_present;

uint8_t joystick_rtfm_read(uint32_t a);

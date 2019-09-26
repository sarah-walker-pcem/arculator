#include <stdlib.h>
#include <string.h>
#include "arc.h"
#include "config.h"
#include "joystick.h"
#include "plat_joystick.h"

int joystick_type;
int joystick_a3010_present;

static const joystick_if_t joystick_a3010;

static const joystick_if_t *joystick_list[] =
{
        &joystick_a3010,
        NULL
};

const char *joystick_get_name(int joystick)
{
        if (!joystick_list[joystick])
                return NULL;
        return joystick_list[joystick]->name;
}

const char *joystick_get_config_name(int joystick)
{
        if (!joystick_list[joystick])
                return NULL;
        return joystick_list[joystick]->config_name;
}

const int joystick_get_max_joysticks(int joystick)
{
        return joystick_list[joystick]->max_joysticks;
}
        
const int joystick_get_axis_count(int joystick)
{
        return joystick_list[joystick]->axis_count;
}

const int joystick_get_button_count(int joystick)
{
        return joystick_list[joystick]->button_count;
}

const int joystick_get_pov_count(int joystick)
{
        return joystick_list[joystick]->pov_count;
}

const char *joystick_get_axis_name(int joystick, int id)
{
        return joystick_list[joystick]->axis_names[id];
}

const char *joystick_get_button_name(int joystick, int id)
{
        return joystick_list[joystick]->button_names[id];
}

const char *joystick_get_pov_name(int joystick, int id)
{
        return joystick_list[joystick]->pov_names[id];
}


void joystick_if_init()
{
        joystick_a3010_present = !strcmp(joystick_if, "a3010");
}

static const joystick_if_t joystick_a3010 =
{
        .name = "A3010 joysticks",
        .config_name = "a3010",
        .max_joysticks = 2,
        .axis_count = 2,
        .button_count = 1,
        .pov_count = 0,
        .axis_names = {"X axis", "Y axis"},
        .button_names = {"Fire button"}
};

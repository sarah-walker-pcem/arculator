/*Arculator 2.0 by Sarah Walker
  Joystick subsystem*/
#include <stdlib.h>
#include <string.h>
#include "arc.h"
#include "config.h"
#include "joystick.h"
#include "plat_joystick.h"

int joystick_type;
int joystick_a3010_present;
int joystick_gamespad_present;
int joystick_rtfm_present;
int joystick_serial_port_present;

static const joystick_if_t joystick_a3010;
static const joystick_if_t joystick_gamespad;
static const joystick_if_t joystick_rtfm;
static const joystick_if_t joystick_serial_port;

static const joystick_if_t *joystick_list[] =
{
        &joystick_a3010,
        &joystick_gamespad,
        &joystick_rtfm,
        &joystick_serial_port,
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

int joystick_get_type(char *config_name)
{
        int c = 0;
        
        while (joystick_list[c])
        {
                if (!strcmp(config_name, joystick_list[c]->config_name))
                        return c;
                c++;
        }
        return 0;
}

void joystick_if_init()
{
        joystick_a3010_present = !strcmp(joystick_if, "a3010");
        joystick_gamespad_present = !strcmp(joystick_if, "gamespad");
        joystick_rtfm_present = !strcmp(joystick_if, "rtfm");
        joystick_serial_port_present = !strcmp(joystick_if, "serial_port");
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

static const joystick_if_t joystick_gamespad =
{
        .name = "GamesPad / GamesPad Pro",
        .config_name = "gamespad",
        .max_joysticks = 2,
        .axis_count = 2,
        .button_count = 8,
        .pov_count = 0,
        .axis_names = {"X axis", "Y axis"},
        .button_names = {"A Button", "B Button", "X Button", "Y Button", "L button", "R button", "Start", "Select"}
};

static const joystick_if_t joystick_rtfm =
{
        .name = "RTFM Joystick Interface",
        .config_name = "rtfm",
        .max_joysticks = 2,
        .axis_count = 2,
        .button_count = 1,
        .pov_count = 0,
        .axis_names = {"X axis", "Y axis"},
        .button_names = {"Fire button"}
};

static const joystick_if_t joystick_serial_port =
{
        .name = "The Serial Port / Vertical Twist Interface",
        .config_name = "serial_port",
        .max_joysticks = 2,
        .axis_count = 2,
        .button_count = 2,
        .pov_count = 0,
        .axis_names = {"X axis", "Y axis"},
        .button_names = {"Button 1", "Button 2"}
};

uint8_t joystick_rtfm_read(uint32_t a)
{
        uint8_t temp = 0xff;
        
        switch (a & 0xc)
        {
                case 4:
                temp = 0;
                if (joystick_state[0].axis[0] > 16383)
                        temp |= 0x01;
                if (joystick_state[0].axis[0] < -16383)
                        temp |= 0x02;
                if (joystick_state[0].axis[1] > 16383)
                        temp |= 0x04;
                if (joystick_state[0].axis[1] < -16383)
                        temp |= 0x08;
                if (joystick_state[0].button[0])
                        temp |= 0x10;
                break;
                case 8:
                temp = 0;
                if (joystick_state[1].axis[0] > 16383)
                        temp |= 0x08;
                if (joystick_state[1].axis[0] < -16383)
                        temp |= 0x20;
                if (joystick_state[1].axis[1] > 16383)
                        temp |= 0x40;
                if (joystick_state[1].axis[1] < -16383)
                        temp |= 0x80;
                if (joystick_state[1].button[0])
                        temp |= 0x10;
                break;
        }
        return temp;
}

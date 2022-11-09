#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <SDL2/SDL.h>
#include "joystick_api.h"
#include "podule_api.h"

#define MIN(a, b) (((a) < (b)) ? (a) : (b))

int joysticks_present;
joystick_t joystick_state[MAX_JOYSTICKS];

plat_joystick_t plat_joystick_state[MAX_PLAT_JOYSTICKS];
static SDL_Joystick *sdl_joy[MAX_PLAT_JOYSTICKS];

void joystick_init(podule_t *podule, const podule_callbacks_t *podule_callbacks)
{
	int c;

	SDL_Init(SDL_INIT_JOYSTICK);

	joysticks_present = SDL_NumJoysticks();

	memset(sdl_joy, 0, sizeof(sdl_joy));
	for (c = 0; c < joysticks_present; c++)
	{
		sdl_joy[c] = SDL_JoystickOpen(c);

		if (sdl_joy[c])
		{
			int d;

/*                        rpclog("Opened Joystick %i\n", c);
			rpclog(" Name: %s\n", SDL_JoystickName(sdl_joy[c]));
			rpclog(" Number of Axes: %d\n", SDL_JoystickNumAxes(sdl_joy[c]));
			rpclog(" Number of Buttons: %d\n", SDL_JoystickNumButtons(sdl_joy[c]));
			rpclog(" Number of Hats: %d\n", SDL_JoystickNumHats(sdl_joy[c]));*/

			strncpy(plat_joystick_state[c].name, SDL_JoystickNameForIndex(c), 64);
			plat_joystick_state[c].nr_axes = SDL_JoystickNumAxes(sdl_joy[c]);
			plat_joystick_state[c].nr_buttons = SDL_JoystickNumButtons(sdl_joy[c]);
			plat_joystick_state[c].nr_povs = SDL_JoystickNumHats(sdl_joy[c]);

			for (d = 0; d < MIN(plat_joystick_state[c].nr_axes, 8); d++)
			{
				sprintf(plat_joystick_state[c].axis[d].name, "Axis %i", d);
				plat_joystick_state[c].axis[d].id = d;
			}
			for (d = 0; d < MIN(plat_joystick_state[c].nr_buttons, 8); d++)
			{
				sprintf(plat_joystick_state[c].button[d].name, "Button %i", d);
				plat_joystick_state[c].button[d].id = d;
			}
			for (d = 0; d < MIN(plat_joystick_state[c].nr_povs, 4); d++)
			{
				sprintf(plat_joystick_state[c].pov[d].name, "POV %i", d);
				plat_joystick_state[c].pov[d].id = d;
			}
		}
	}

	if (podule)
	{
		for (c = 0; c < joystick_get_max_joysticks(); c++)
		{
			char s[80];

			sprintf(s, "joystick_%i_nr", c);
			joystick_state[c].plat_joystick_nr = podule_callbacks->config_get_int(podule, s, 0);

			if (joystick_state[c].plat_joystick_nr)
			{
				int d;

				for (d = 0; d < joystick_get_axis_count(); d++)
				{
					sprintf(s, "joystick_%i_axis_%i", c, d);
					joystick_state[c].axis_mapping[d] = podule_callbacks->config_get_int(podule, s, d);
				}
				for (d = 0; d < joystick_get_button_count(); d++)
				{
					sprintf(s, "joystick_%i_button_%i", c, d);
					joystick_state[c].button_mapping[d] = podule_callbacks->config_get_int(podule, s, d);
				}
				for (d = 0; d < joystick_get_pov_count(); d++)
				{
					sprintf(s, "joystick_%i_pov_%i_x", c, d);
					joystick_state[c].pov_mapping[d][0] = podule_callbacks->config_get_int(podule, s, d);
					sprintf(s, "joystick_%i_pov_%i_y", c, d);
					joystick_state[c].pov_mapping[d][1] = podule_callbacks->config_get_int(podule, s, d);
				}
			}
		}
	}
}
void joystick_close(void)
{
	int c;

	for (c = 0; c < joysticks_present; c++)
	{
		if (sdl_joy[c])
			SDL_JoystickClose(sdl_joy[c]);
	}
}

static int joystick_get_axis(int joystick_nr, int mapping)
{
	if (mapping & POV_X)
	{
		switch (plat_joystick_state[joystick_nr].p[mapping & 3])
		{
			case SDL_HAT_LEFTUP: case SDL_HAT_LEFT: case SDL_HAT_LEFTDOWN:
			return -32767;

			case SDL_HAT_RIGHTUP: case SDL_HAT_RIGHT: case SDL_HAT_RIGHTDOWN:
			return 32767;

			default:
			return 0;
		}
	}
	else if (mapping & POV_Y)
	{
		switch (plat_joystick_state[joystick_nr].p[mapping & 3])
		{
			case SDL_HAT_LEFTUP: case SDL_HAT_UP: case SDL_HAT_RIGHTUP:
			return -32767;

			case SDL_HAT_LEFTDOWN: case SDL_HAT_DOWN: case SDL_HAT_RIGHTDOWN:
			return 32767;

			default:
			return 0;
		}
	}
	else
		return plat_joystick_state[joystick_nr].a[plat_joystick_state[joystick_nr].axis[mapping].id];
}
void joystick_poll_host(void)
{
	int c, d;

	SDL_JoystickUpdate();
	for (c = 0; c < joysticks_present; c++)
	{
		int b;

		plat_joystick_state[c].a[0] = SDL_JoystickGetAxis(sdl_joy[c], 0);
		plat_joystick_state[c].a[1] = SDL_JoystickGetAxis(sdl_joy[c], 1);
		plat_joystick_state[c].a[2] = SDL_JoystickGetAxis(sdl_joy[c], 2);
		plat_joystick_state[c].a[3] = SDL_JoystickGetAxis(sdl_joy[c], 3);
		plat_joystick_state[c].a[4] = SDL_JoystickGetAxis(sdl_joy[c], 4);
		plat_joystick_state[c].a[5] = SDL_JoystickGetAxis(sdl_joy[c], 5);

		for (b = 0; b < 16; b++)
			plat_joystick_state[c].b[b] = SDL_JoystickGetButton(sdl_joy[c], b);

		for (b = 0; b < 4; b++)
			plat_joystick_state[c].p[b] = SDL_JoystickGetHat(sdl_joy[c], b);
//                rpclog("joystick %i - x=%i y=%i b[0]=%i b[1]=%i  %i\n", c, joystick_state[c].x, joystick_state[c].y, joystick_state[c].b[0], joystick_state[c].b[1], joysticks_present);
	}

	for (c = 0; c < joystick_get_max_joysticks(); c++)
	{
		if (joystick_state[c].plat_joystick_nr)
		{
			int joystick_nr = joystick_state[c].plat_joystick_nr - 1;

			for (d = 0; d < joystick_get_axis_count(); d++)
				joystick_state[c].axis[d] = joystick_get_axis(joystick_nr, joystick_state[c].axis_mapping[d]);
			for (d = 0; d < joystick_get_button_count(); d++)
				joystick_state[c].button[d] = plat_joystick_state[joystick_nr].b[joystick_state[c].button_mapping[d]];
			for (d = 0; d < joystick_get_pov_count(); d++)
			{
				int x, y;
				double angle, magnitude;

				x = joystick_get_axis(joystick_nr, joystick_state[c].pov_mapping[d][0]);
				y = joystick_get_axis(joystick_nr, joystick_state[c].pov_mapping[d][1]);

				angle = (atan2((double)y, (double)x) * 360.0) / (2*M_PI);
				magnitude = sqrt((double)x*(double)x + (double)y*(double)y);

				if (magnitude < 16384)
					joystick_state[c].pov[d] = -1;
				else
					joystick_state[c].pov[d] = ((int)angle + 90 + 360) % 360;
			}
		}
		else
		{
			for (d = 0; d < joystick_get_axis_count(); d++)
				joystick_state[c].axis[d] = 0;
			for (d = 0; d < joystick_get_button_count(); d++)
				joystick_state[c].button[d] = 0;
			for (d = 0; d < joystick_get_pov_count(); d++)
				joystick_state[c].pov[d] = -1;
		}
	}
}

podule_config_selection_t *joystick_devices_config(const podule_callbacks_t *podule_callbacks)
{
	podule_config_selection_t *sel;
	podule_config_selection_t *sel_p;
	char *joystick_dev_text = malloc(65536);
	int c;

	joystick_init(NULL, podule_callbacks);

	sel = malloc(sizeof(podule_config_selection_t) * (joysticks_present+2));
	sel_p = sel;

	strcpy(joystick_dev_text, "None");
	sel_p->description = joystick_dev_text;
	sel_p->value = 0;
	sel_p++;
	joystick_dev_text += strlen(joystick_dev_text)+1;

	for (c = 0; c < joysticks_present; c++)
	{
		strcpy(joystick_dev_text, plat_joystick_state[c].name);
		sel_p->description = joystick_dev_text;
		sel_p->value = c+1;
		sel_p++;

		joystick_dev_text += strlen(joystick_dev_text)+1;
	}

	strcpy(joystick_dev_text, "");
	sel_p->description = joystick_dev_text;

	joystick_close();

	return sel;
}

static char joystick_button_text[65536];
podule_config_selection_t joystick_button_config_selection[33];

void joystick_update_buttons_config(int joy_device)
{
	podule_config_selection_t *sel_p = joystick_button_config_selection;
	char *text_p = joystick_button_text;

	if (joy_device)
	{
		int c;

		for (c = 0; c < MIN(plat_joystick_state[joy_device-1].nr_buttons, 8); c++)
		{
			strcpy(text_p, plat_joystick_state[joy_device-1].button[c].name);
			sel_p->description = text_p;
			sel_p->value = c;
			sel_p++;
			text_p += strlen(text_p)+1;
		}
	}
	else
	{
		strcpy(text_p, "None");
		sel_p->description = text_p;
		sel_p->value = 0;
		sel_p++;
		text_p += strlen(text_p)+1;
	}

	strcpy(text_p, "");
	sel_p->description = text_p;
}

static char joystick_axis_text[65536];
podule_config_selection_t joystick_axis_config_selection[13];

void joystick_update_axes_config(int joy_device)
{
	podule_config_selection_t *sel_p = joystick_axis_config_selection;
	char *text_p = joystick_axis_text;

	if (joy_device)
	{
		int c;

		for (c = 0; c < MIN(plat_joystick_state[joy_device-1].nr_axes, 8); c++)
		{
			strcpy(text_p, plat_joystick_state[joy_device-1].axis[c].name);
			sel_p->description = text_p;
			sel_p->value = c;
			sel_p++;
			text_p += strlen(text_p)+1;
		}

		for (c = 0; c < MIN(plat_joystick_state[joy_device-1].nr_povs, 4); c++)
		{
			sprintf(text_p, "%s (X axis)", plat_joystick_state[joy_device-1].pov[c].name);
			sel_p->description = text_p;
			sel_p->value = c | POV_X;
			sel_p++;
			text_p += strlen(text_p)+1;

			sprintf(text_p, "%s (Y axis)", plat_joystick_state[joy_device-1].pov[c].name);
			sel_p->description = text_p;
			sel_p->value = c | POV_Y;
			sel_p++;
			text_p += strlen(text_p)+1;
		}
	}
	else
	{
		strcpy(text_p, "None");
		sel_p->description = text_p;
		sel_p->value = 0;
		sel_p++;
		text_p += strlen(text_p)+1;
	}

	strcpy(text_p, "");
	sel_p->description = text_p;
}

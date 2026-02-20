/*Arculator 2.2 by Sarah Walker
  CLI frontend — no wxWidgets dependency.

  Provides main(), argument parsing, the SDL event loop, and stub
  functions that the emulation core calls into (arc_stop_emulation,
  arc_popup_menu, arc_update_menu, arc_print_error).

  Build as part of the arculator-cli target (see CMakeLists.txt).*/

#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/time.h>

#ifndef _WIN32
#include <X11/Xlib.h>
#endif

#include <SDL2/SDL.h>

#include "arc.h"
#include "config.h"
#include "debugger.h"
#include "disc.h"
#include "ioc.h"
#include "plat_input.h"
#include "plat_joystick.h"
#include "plat_video.h"
#include "podule_api.h"
#include "podules.h"
#include "romload.h"
#include "vidc.h"
#include "video.h"
#include "video_sdl2.h"

/* ------------------------------------------------------------------ */
/* Globals expected by the emulation core (defined elsewhere)         */
/* ------------------------------------------------------------------ */
extern int mousecapture;

/* ------------------------------------------------------------------ */
/* Window management                                                  */
/* ------------------------------------------------------------------ */
static int winsizex, winsizey;
static int win_doresize;
static int win_dofullscreen;
static int win_renderer_reset;

void updatewindowsize(int x, int y)
{
	winsizex = (x * (video_scale + 1)) / 2;
	winsizey = (y * (video_scale + 1)) / 2;
	win_doresize = 1;
}

/* ------------------------------------------------------------------ */
/* Mouse capture helpers                                              */
/* ------------------------------------------------------------------ */
static void sdl_enable_mouse_capture(void)
{
	mouse_capture_enable();
	SDL_SetWindowGrab(sdl_main_window, SDL_TRUE);
	mousecapture = 1;
	updatemips = 1;
}

static void sdl_disable_mouse_capture(void)
{
	SDL_SetWindowGrab(sdl_main_window, SDL_FALSE);
	mouse_capture_disable();
	mousecapture = 0;
	updatemips = 1;
}

/* ------------------------------------------------------------------ */
/* Stubs for functions the core calls into (wx-app.cc in the GUI)     */
/* ------------------------------------------------------------------ */
static volatile int quited;

void arc_stop_emulation(void)
{
	quited = 1;
}

void arc_popup_menu(void)
{
	/* No popup menu in CLI mode. */
}

void arc_update_menu(void)
{
	/* No menu to update in CLI mode. */
}

void arc_print_error(const char *format, ...)
{
	va_list ap;
	va_start(ap, format);
	vfprintf(stderr, format, ap);
	va_end(ap);
}

void arc_enter_fullscreen(void)
{
	win_dofullscreen = 1;
}

void arc_renderer_reset(void)
{
	win_renderer_reset = 1;
}

void arc_set_display_mode(int new_display_mode)
{
	display_mode = new_display_mode;
	clearbitmap();
	setredrawall();
}

void arc_set_dblscan(int new_dblscan)
{
	dblscan = new_dblscan;
	clearbitmap();
}

void arc_set_resizeable(void)
{
	/* Nothing to do in CLI mode. */
}

/* ------------------------------------------------------------------ */
/* Debugger console — wired to terminal stdin/stdout.                  */
/*                                                                    */
/* debugger_do() enters a blocking while(1) loop that calls           */
/* console_input_get() on every iteration.  In the wx build this      */
/* polls a shared buffer; here we simply fgets() from stdin which     */
/* blocks the main thread — that is fine because the emulation is     */
/* paused while the debugger is active.                               */
/* ------------------------------------------------------------------ */
void console_output(char *s)
{
	fputs(s, stdout);
	fflush(stdout);
}

int console_input_get(char *s)
{
	if (debugger_in_reset)
		return CONSOLE_INPUT_GET_ERROR_IN_RESET;

	if (!fgets(s, 256, stdin))
		return CONSOLE_INPUT_GET_ERROR_WINDOW_CLOSED; /* EOF */

	/* Strip trailing newline so the command parser sees clean input */
	size_t len = strlen(s);
	if (len && s[len - 1] == '\n')
		s[len - 1] = '\0';

	return 1;
}

void console_input_disable(void)
{
}

void console_input_enable(void)
{
	fputs("debug> ", stdout);
	fflush(stdout);
}

/* Podule config stubs (wx-podule-config.cc in the GUI build).
   Runtime podule config dialogs are not available in CLI mode. */
void *podule_config_get_current(void *window_p, int id)
{
	return NULL;
}

void podule_config_set_current(void *window_p, int id, void *val)
{
}

int podule_config_file_selector(void *window_p, const char *title,
	const char *default_path, const char *default_fn,
	const char *default_ext, const char *wildcard,
	char *dest, int dest_len, int flags)
{
	return 0; /* cancelled */
}

int podule_config_open(void *window_p, podule_config_t *config,
	const char *prefix)
{
	return -1; /* cancelled */
}

/* These are only called from the wx thread management code which
   the CLI binary does not use; provide them to satisfy the linker. */
void arc_do_reset(void)
{
	arc_reset();
}

void arc_disc_change(int drive, char *fn)
{
	disc_close(drive);
	strcpy(discname[drive], fn);
	disc_load(drive, discname[drive]);
	ioc_discchange(drive);
}

void arc_disc_eject(int drive)
{
	ioc_discchange(drive);
	disc_close(drive);
	discname[drive][0] = 0;
}

/* ------------------------------------------------------------------ */
/* Usage                                                              */
/* ------------------------------------------------------------------ */
static void usage(const char *progname)
{
	fprintf(stderr,
		"Usage: %s [OPTIONS] [config-name]\n"
		"\n"
		"Options:\n"
		"  -c, --config NAME   Machine configuration name\n"
		"  -d, --debug         Start with debugger enabled (break at first instruction)\n"
		"  -f, --fullscreen    Start in fullscreen mode\n"
		"  -h, --help          Show this help message\n"
		"\n"
		"While running, press Ctrl+F12 to break into the debugger.\n",
		progname);
}

/* ------------------------------------------------------------------ */
/* Main                                                               */
/* ------------------------------------------------------------------ */
int main(int argc, char **argv)
{
	int start_fullscreen = 0;
	int start_debug = 0;
	const char *config_name = NULL;

	static struct option long_opts[] = {
		{"config",     required_argument, NULL, 'c'},
		{"debug",      no_argument,       NULL, 'd'},
		{"fullscreen", no_argument,       NULL, 'f'},
		{"help",       no_argument,       NULL, 'h'},
		{NULL, 0, NULL, 0}
	};

	int opt;
	while ((opt = getopt_long(argc, argv, "c:dfh", long_opts, NULL)) != -1)
	{
		switch (opt)
		{
			case 'c':
				config_name = optarg;
				break;
			case 'd':
				start_debug = 1;
				break;
			case 'f':
				start_fullscreen = 1;
				break;
			case 'h':
				usage(argv[0]);
				return 0;
			default:
				usage(argv[0]);
				return 1;
		}
	}

	/* Positional argument: config name */
	if (!config_name && optind < argc)
		config_name = argv[optind];

	if (!config_name)
	{
		fprintf(stderr, "Error: no configuration name specified.\n\n");
		usage(argv[0]);
		return 1;
	}

#ifndef _WIN32
	XInitThreads();
#endif

	/* Derive exe directory (same logic as wx-main.cc) */
	strncpy(exname, argv[0], 511);
	exname[511] = '\0';
	char *p = (char *)get_filename(exname);
	*p = '\0';

	/* Build config path: <exedir>/configs/<name>.cfg */
	snprintf(machine_config_file, sizeof(machine_config_file),
		 "%sconfigs/%s.cfg", exname, config_name);

	{
		struct stat st;
		if (stat(machine_config_file, &st) != 0)
		{
			fprintf(stderr, "Error: configuration '%s' not found (%s)\n",
				config_name, machine_config_file);
			return 1;
		}
	}

	strncpy(machine_config_name, config_name, sizeof(machine_config_name) - 1);
	machine_config_name[sizeof(machine_config_name) - 1] = '\0';

	/* Init sequence — same order as the GUI path */
	podule_build_list();
	opendlls();

	if (rom_establish_availability())
	{
		fprintf(stderr, "Error: no ROMs available.\n"
			"Arculator needs at least one ROM set present to run.\n");
		return 1;
	}

	if (SDL_Init(SDL_INIT_EVERYTHING) != 0)
	{
		fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
		return 1;
	}

	joystick_init();
	arc_init();

	if (start_debug)
	{
		debugon = 1;
		debug = 1;
		debug_start();
		fprintf(stderr, "Debugger enabled. Use Ctrl+F12 to break while running.\n");
	}

	if (!video_renderer_init(NULL))
	{
		fatal("Video renderer init failed\n");
	}

	if (start_fullscreen)
	{
		SDL_RaiseWindow(sdl_main_window);
		SDL_SetWindowFullscreen(sdl_main_window, SDL_WINDOW_FULLSCREEN_DESKTOP);
		fullscreen = 1;
		sdl_enable_mouse_capture();
	}

	input_init();

	/* ---- Main loop (adapted from wx-sdl2.c:arc_main_thread) ---- */
	struct timeval tp;
	time_t last_seconds = 0;
	Uint32 last_timer_ticks = 0;
	int timer_offset = 0;

	while (!quited)
	{
		if (gettimeofday(&tp, NULL) == -1)
		{
			perror("gettimeofday");
			fatal("gettimeofday failed\n");
		}
		else if (!last_seconds)
		{
			last_seconds = tp.tv_sec;
		}
		else if (last_seconds != tp.tv_sec)
		{
			updateins();
			last_seconds = tp.tv_sec;
		}

		SDL_Event e;
		while (SDL_PollEvent(&e) != 0)
		{
			if (e.type == SDL_QUIT)
			{
				arc_stop_emulation();
			}
			if (e.type == SDL_MOUSEBUTTONUP)
			{
				if (e.button.button == SDL_BUTTON_LEFT && !mousecapture)
				{
					sdl_enable_mouse_capture();
				}
			}
			if (e.type == SDL_WINDOWEVENT)
			{
				if (e.window.event == SDL_WINDOWEVENT_FOCUS_LOST && mousecapture)
				{
					sdl_disable_mouse_capture();
				}
			}
			if ((key[KEY_LCONTROL] || key[KEY_RCONTROL])
			    && key[KEY_END]
			    && !fullscreen && mousecapture)
			{
				sdl_disable_mouse_capture();
			}
			/* Ctrl+F12: toggle debugger / break into debugger */
			if ((key[KEY_LCONTROL] || key[KEY_RCONTROL])
			    && key[KEY_F12])
			{
				if (!debugon)
				{
					debugon = 1;
					debug = 1;
					debug_start();
					fprintf(stderr, "Debugger enabled.\n");
				}
				else
				{
					debug = 1; /* request break */
				}
				/* Consume the key so it doesn't repeat */
				key[KEY_F12] = 0;
			}
		}

		/* Resize window to match screen mode */
		if (!fullscreen && win_doresize)
		{
			SDL_Rect rect;

			win_doresize = 0;

			SDL_GetWindowSize(sdl_main_window, &rect.w, &rect.h);
			if (rect.w != winsizex || rect.h != winsizey)
			{
				SDL_GetWindowPosition(sdl_main_window, &rect.x, &rect.y);
				SDL_SetWindowSize(sdl_main_window, winsizex, winsizey);
				SDL_SetWindowPosition(sdl_main_window, rect.x, rect.y);
			}
		}

		/* Toggle fullscreen with RWIN-Enter */
		if (win_dofullscreen ||
			(key[KEY_RWIN] && key[KEY_ENTER] && !fullscreen))
		{
			win_dofullscreen = 0;

			SDL_RaiseWindow(sdl_main_window);
			SDL_SetWindowFullscreen(sdl_main_window, SDL_WINDOW_FULLSCREEN_DESKTOP);
			sdl_enable_mouse_capture();
			fullscreen = 1;
		}
		else if (fullscreen && (
			((key[KEY_LCONTROL] || key[KEY_RCONTROL]) && key[KEY_END])
			|| (key[KEY_RWIN] && key[KEY_ENTER])))
		{
			SDL_SetWindowFullscreen(sdl_main_window, 0);
			sdl_disable_mouse_capture();

			fullscreen = 0;
			if (fullborders) updatewindowsize(800, 600);
			else             updatewindowsize(672, 544);
		}

		if (win_renderer_reset)
		{
			win_renderer_reset = 0;
			if (!video_renderer_reinit(NULL))
				fatal("Video renderer init failed\n");
		}

		/* Run for ~10 ms of processor time */
		arc_run();

		/* Sleep to make it up to 10 ms of real time */
		Uint32 current_timer_ticks = SDL_GetTicks();
		Uint32 ticks_since_last = current_timer_ticks - last_timer_ticks;
		last_timer_ticks = current_timer_ticks;
		timer_offset += 10 - (int)ticks_since_last;
		if (timer_offset > 100 || timer_offset < -100)
			timer_offset = 0;
		else if (timer_offset > 0)
			SDL_Delay(timer_offset);

		if (updatemips)
		{
			char s[80];
			sprintf(s, "Arculator %s - %i%% - %s",
				VERSION_STRING, inssec,
				mousecapture ?
					"Press CTRL-END to release mouse" :
					"Click to capture mouse");
			vidc_framecount = 0;
			if (!fullscreen)
				SDL_SetWindowTitle(sdl_main_window, s);
			updatemips = 0;
		}
	}

	/* Shutdown */
	arc_close();
	input_close();
	video_renderer_close();
	SDL_DestroyWindow(sdl_main_window);
	SDL_Quit();

	return 0;
}

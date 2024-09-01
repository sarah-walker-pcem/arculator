/*Arculator 2.1 by Sarah Walker
  wxApp implementation
  Menus are also handled here*/
#include <sstream>
#include <SDL.h>

#ifdef _WIN32
#define BITMAP WINDOWS_BITMAP
#include "wx-app.h"
#include <wx/xrc/xmlres.h>
#include <wx/event.h>

#include <windows.h>
#include <windowsx.h>
#undef BITMAP
#else
#include "wx-app.h"
#include <wx/xrc/xmlres.h>
#include <wx/event.h>
#endif

#include "wx-config.h"
#include "wx-config_sel.h"
#include "wx-console.h"

extern "C"
{
	#include "arc.h"
	#include "config.h"
	#include "debugger.h"
	#include "disc.h"
	#include "plat_joystick.h"
	#include "plat_video.h"
	#include "romload.h"
	#include "sound.h"
	#include "video.h"
}

extern void InitXmlResource();

wxDEFINE_EVENT(WX_STOP_EMULATION_EVENT, wxCommandEvent);
wxDEFINE_EVENT(WX_POPUP_MENU_EVENT, PopupMenuEvent);
wxDEFINE_EVENT(WX_UPDATE_MENU_EVENT, UpdateMenuEvent);
wxDEFINE_EVENT(WX_PRINT_ERROR_EVENT, wxCommandEvent);
#ifdef _WIN32
wxDEFINE_EVENT(WX_WIN_SEND_MESSAGE_EVENT, WinSendMessageEvent);
#endif

wxBEGIN_EVENT_TABLE(Frame, wxFrame)
wxEND_EVENT_TABLE()

wxIMPLEMENT_APP_NO_MAIN(App);

App::App()
{
	this->frame = NULL;
}

bool App::OnInit()
{
	wxImage::AddHandler( new wxPNGHandler );
	wxXmlResource::Get()->InitAllHandlers();
	InitXmlResource();

	if (rom_establish_availability())
	{
		wxMessageBox("No ROMs available\nArculator needs at least one ROM set present to run", "Arculator", wxOK | wxCENTRE | wxSTAY_ON_TOP);
		exit(-1);
	}

	SDL_Init(SDL_INIT_EVERYTHING);
	joystick_init();

	frame = new Frame(this, "null frame", wxPoint(500, 500),
			wxSize(100, 100));
#ifdef _WIN32
	frame->SetIcon(wxICON(ICON_ACORN));
#endif
	frame->Start();
	return true;
}

static void *main_frame = NULL;
static void *main_menu = NULL;

Frame::Frame(App* app, const wxString& title, const wxPoint& pos,
		const wxSize& size) :
		wxFrame(NULL, wxID_ANY, title, pos, size, 0)
{
	main_frame = this;

	this->menu = wxXmlResource::Get()->LoadMenu(wxT("main_menu"));
	main_menu = this->menu;

	Bind(wxEVT_MENU, &Frame::OnMenuCommand, this);
	Bind(WX_POPUP_MENU_EVENT, &Frame::OnPopupMenuEvent, this);
	Bind(WX_UPDATE_MENU_EVENT, &Frame::OnUpdateMenuEvent, this);
	Bind(WX_STOP_EMULATION_EVENT, &Frame::OnStopEmulationEvent, this);
	Bind(WX_PRINT_ERROR_EVENT, &Frame::OnPrintErrorEvent, this);
#ifdef _WIN32
	Bind(WX_WIN_SEND_MESSAGE_EVENT, &Frame::OnWinSendMessageEvent, this);
#endif

	CenterOnScreen();
}

Frame::~Frame()
{
	main_frame = NULL;
}

void Frame::Start()
{
	if (strlen(machine_config_name) != 0 || !ShowConfigSelection())
		arc_start_main_thread(this, this->menu);
	else
		Quit(0);
}

wxMenu* Frame::GetMenu()
{
	return menu;
}

void Frame::Quit(bool stop_emulator)
{
	Destroy();
}

void Frame::OnStopEmulationEvent(wxCommandEvent &event)
{
	CloseConsoleWindow();
	arc_stop_main_thread();

	debug_end();

	if (!ShowConfigSelection())
		arc_start_main_thread(this, this->menu);
	else
		Quit(0);
}

void Frame::OnPrintErrorEvent(wxCommandEvent &event)
{
	wxMessageBox(event.GetString(), "Arculator", wxOK | wxCENTRE | wxSTAY_ON_TOP | wxICON_ERROR, this);
}

void Frame::UpdateMenu(wxMenu *menu)
{
	char menuitem[80];
	wxMenuItem *item = ((wxMenu*)menu)->FindItem(XRCID("IDM_SOUND_ENABLE"));
	item->Check(soundena);
	item = ((wxMenu*)menu)->FindItem(XRCID("IDM_SOUND_STEREO"));
	item->Check(stereo);
	if (sound_filter == 0)
		item = ((wxMenu*)menu)->FindItem(XRCID("IDM_FILTER_ORIGINAL"));
	else if (sound_filter == 1)
		item = ((wxMenu*)menu)->FindItem(XRCID("IDM_FILTER_REDUCED"));
	else if (sound_filter == 2)
		item = ((wxMenu*)menu)->FindItem(XRCID("IDM_FILTER_MORE_REDUCED"));
	item->Check(true);

	if (disc_noise_gain == 0)
		item = ((wxMenu*)menu)->FindItem(XRCID("IDM_DISC_NOISE[1]"));
	else if (disc_noise_gain == -2)
		item = ((wxMenu*)menu)->FindItem(XRCID("IDM_DISC_NOISE[2]"));
	else if (disc_noise_gain == -4)
		item = ((wxMenu*)menu)->FindItem(XRCID("IDM_DISC_NOISE[3]"));
	else if (disc_noise_gain == -6)
		item = ((wxMenu*)menu)->FindItem(XRCID("IDM_DISC_NOISE[4]"));
	else
		item = ((wxMenu*)menu)->FindItem(XRCID("IDM_DISC_NOISE[0]"));
	item->Check(true);

	if (dblscan)
		item = ((wxMenu*)menu)->FindItem(XRCID("IDM_BLIT_SCALE"));
	else
		item = ((wxMenu*)menu)->FindItem(XRCID("IDM_BLIT_SCAN"));
	item->Check(true);
	if (display_mode == DISPLAY_MODE_NO_BORDERS)
		item = ((wxMenu*)menu)->FindItem(XRCID("IDM_VIDEO_NO_BORDERS"));
	else if (display_mode == DISPLAY_MODE_NATIVE_BORDERS)
		item = ((wxMenu*)menu)->FindItem(XRCID("IDM_VIDEO_NATIVE_BORDERS"));
	else
		item = ((wxMenu*)menu)->FindItem(XRCID("IDM_VIDEO_TV"));
	item->Check(true);
	if (video_fullscreen_scale == FULLSCR_SCALE_FULL)
		item = ((wxMenu*)menu)->FindItem(XRCID("IDM_VIDEO_FS_FULL"));
	else if (video_fullscreen_scale == FULLSCR_SCALE_43)
		item = ((wxMenu*)menu)->FindItem(XRCID("IDM_VIDEO_FS_43"));
	else if (video_fullscreen_scale == FULLSCR_SCALE_SQ)
		item = ((wxMenu*)menu)->FindItem(XRCID("IDM_VIDEO_FS_SQ"));
	else if (video_fullscreen_scale == FULLSCR_SCALE_INT)
		item = ((wxMenu*)menu)->FindItem(XRCID("IDM_VIDEO_FS_INT"));
	item->Check(true);
	if (video_linear_filtering)
		item = ((wxMenu*)menu)->FindItem(XRCID("IDM_VIDEO_SCALE_LINEAR"));
	else
		item = ((wxMenu*)menu)->FindItem(XRCID("IDM_VIDEO_SCALE_NEAREST"));
	item->Check(true);
	if (video_black_level == BLACK_LEVEL_ACORN)
		item = ((wxMenu*)menu)->FindItem(XRCID("IDM_BLACK_ACORN"));
	else
		item = ((wxMenu*)menu)->FindItem(XRCID("IDM_BLACK_NORMAL"));
	item->Check(true);
	sprintf(menuitem, "IDM_VIDEO_SCALE[%d]", video_scale);
	item = ((wxMenu*)menu)->FindItem(XRCID(menuitem));
	item->Check(true);

	item = ((wxMenu*)menu)->FindItem(XRCID("IDM_DRIVER_AUTO"));
	item->Enable(video_renderer_available(RENDERER_AUTO) ? true : false);
	item->Check((selected_video_renderer == RENDERER_AUTO) ? true : false);
	item = ((wxMenu*)menu)->FindItem(XRCID("IDM_DRIVER_DIRECT3D"));
	item->Enable(video_renderer_available(RENDERER_DIRECT3D) ? true : false);
	item->Check((selected_video_renderer == RENDERER_DIRECT3D) ? true : false);
	item = ((wxMenu*)menu)->FindItem(XRCID("IDM_DRIVER_OPENGL"));
	item->Enable(video_renderer_available(RENDERER_OPENGL) ? true : false);
	item->Check((selected_video_renderer == RENDERER_OPENGL) ? true : false);
	item = ((wxMenu*)menu)->FindItem(XRCID("IDM_DRIVER_SOFTWARE"));
	item->Enable(video_renderer_available(RENDERER_SOFTWARE) ? true : false);
	item->Check((selected_video_renderer == RENDERER_SOFTWARE) ? true : false);

	item = ((wxMenu*)menu)->FindItem(XRCID("IDM_DEBUGGER_ENABLE"));
	item->Check(debug);
}

void Frame::OnPopupMenuEvent(PopupMenuEvent &event)
{
	wxWindow *window = event.GetWindow();
	wxMenu *menu = event.GetMenu();

	UpdateMenu(menu);

	window->PopupMenu(menu);
}

void Frame::ChangeDisc(int drive)
{
	wxString old_fn(discname[drive]);

	wxFileDialog dlg(NULL, "Select a disc image", "", old_fn,
			"All disc images|*.adf;*.img;*.fdi;*.apd;*.hfe;*.scp;*.ssd;*.dsd|FDI Disc Image|*.fdi|APD Disc Image|*.apd|ADFS Disc Image|*.adf|DOS Disc Image|*.img|HFE Disc Image|*.hfe|SCP Disc Image|*.scp|Single sided DFS Disc Image|*.ssd|Double sided DFS Disc Image|*.dsd|All Files|*.*",
			wxFD_OPEN | wxFD_FILE_MUST_EXIST);

	if (dlg.ShowModal() == wxID_OK)
	{
		char new_fn[512];
		wxString new_fn_str = dlg.GetPath();

		strcpy(new_fn, new_fn_str.mb_str());
		arc_disc_change(drive, new_fn);
	}
}

#ifdef _WIN32
extern "C" void arc_send_close();
#endif

void Frame::OnMenuCommand(wxCommandEvent &event)
{
	if (event.GetId() == XRCID("IDM_FILE_EXIT"))
	{
#ifdef _WIN32
		arc_send_close();
#else
		arc_stop_emulation();
#endif
	}
	else if (event.GetId() == XRCID("IDM_FILE_RESET"))
	{
		arc_do_reset();
	}
	else if (event.GetId() == XRCID("IDM_DISC_CHANGE_0"))
	{
		ChangeDisc(0);
	}
	else if (event.GetId() == XRCID("IDM_DISC_CHANGE_1"))
	{
		ChangeDisc(1);
	}
	else if (event.GetId() == XRCID("IDM_DISC_CHANGE_2"))
	{
		ChangeDisc(2);
	}
	else if (event.GetId() == XRCID("IDM_DISC_CHANGE_3"))
	{
		ChangeDisc(3);
	}
	else if (event.GetId() == XRCID("IDM_DISC_EJECT_0"))
	{
		arc_disc_eject(0);
	}
	else if (event.GetId() == XRCID("IDM_DISC_EJECT_1"))
	{
		arc_disc_eject(1);
	}
	else if (event.GetId() == XRCID("IDM_DISC_EJECT_2"))
	{
		arc_disc_eject(2);
	}
	else if (event.GetId() == XRCID("IDM_DISC_EJECT_3"))
	{
		arc_disc_eject(3);
	}
	else if (event.GetId() == XRCID("IDM_DISC_NOISE[0]") || event.GetId() == XRCID("IDM_DISC_NOISE[1]") ||
		 event.GetId() == XRCID("IDM_DISC_NOISE[2]") || event.GetId() == XRCID("IDM_DISC_NOISE[3]") ||
		 event.GetId() == XRCID("IDM_DISC_NOISE[4]"))
	{
		wxMenuItem *item = ((wxMenu*)menu)->FindItem(event.GetId());
		item->Check(true);

		if (event.GetId() == XRCID("IDM_DISC_NOISE[0]"))
			disc_noise_gain = DISC_NOISE_DISABLED;
		else if (event.GetId() == XRCID("IDM_DISC_NOISE[1]"))
			disc_noise_gain = -2 * 0;
		else if (event.GetId() == XRCID("IDM_DISC_NOISE[2]"))
			disc_noise_gain = -2 * 1;
		else if (event.GetId() == XRCID("IDM_DISC_NOISE[3]"))
			disc_noise_gain = -2 * 2;
		else
			disc_noise_gain = -2 * 3;
	}
	else if (event.GetId() == XRCID("IDM_SOUND_ENABLE"))
	{
		soundena ^= 1;

		wxMenuItem *item = ((wxMenu*)menu)->FindItem(event.GetId());
		item->Check(soundena);
	}
	else if (event.GetId() == XRCID("IDM_SOUND_STEREO"))
	{
		stereo ^= 1;

		wxMenuItem *item = ((wxMenu*)menu)->FindItem(event.GetId());
		item->Check(stereo);
	}
	else if (event.GetId() == XRCID("IDM_SOUND_GAIN[0]") || event.GetId() == XRCID("IDM_SOUND_GAIN[1]") ||
		 event.GetId() == XRCID("IDM_SOUND_GAIN[2]") || event.GetId() == XRCID("IDM_SOUND_GAIN[3]") ||
		 event.GetId() == XRCID("IDM_SOUND_GAIN[4]") || event.GetId() == XRCID("IDM_SOUND_GAIN[5]") ||
		 event.GetId() == XRCID("IDM_SOUND_GAIN[6]") || event.GetId() == XRCID("IDM_SOUND_GAIN[7]") ||
		 event.GetId() == XRCID("IDM_SOUND_GAIN[8]") || event.GetId() == XRCID("IDM_SOUND_GAIN[9]"))
	{
		wxMenuItem *item = ((wxMenu*)menu)->FindItem(event.GetId());
		item->Check(true);

		if (event.GetId() == XRCID("IDM_SOUND_GAIN[0]"))
			sound_gain = 2 * 0;
		else if (event.GetId() == XRCID("IDM_SOUND_GAIN[1]"))
			sound_gain = 2 * 1;
		else if (event.GetId() == XRCID("IDM_SOUND_GAIN[2]"))
			sound_gain = 2 * 2;
		else if (event.GetId() == XRCID("IDM_SOUND_GAIN[3]"))
			sound_gain = 2 * 3;
		else if (event.GetId() == XRCID("IDM_SOUND_GAIN[4]"))
			sound_gain = 2 * 4;
		else if (event.GetId() == XRCID("IDM_SOUND_GAIN[5]"))
			sound_gain = 2 * 5;
		else if (event.GetId() == XRCID("IDM_SOUND_GAIN[6]"))
			sound_gain = 2 * 6;
		else if (event.GetId() == XRCID("IDM_SOUND_GAIN[7]"))
			sound_gain = 2 * 7;
		else if (event.GetId() == XRCID("IDM_SOUND_GAIN[8]"))
			sound_gain = 2 * 8;
		else if (event.GetId() == XRCID("IDM_SOUND_GAIN[9]"))
			sound_gain = 2 * 9;
	}
	else if (event.GetId() == XRCID("IDM_FILTER_ORIGINAL"))
	{
		wxMenuItem *item = ((wxMenu*)menu)->FindItem(event.GetId());
		item->Check(true);

		sound_filter = 0;
		sound_update_filter();
	}
	else if (event.GetId() == XRCID("IDM_FILTER_REDUCED"))
	{
		wxMenuItem *item = ((wxMenu*)menu)->FindItem(event.GetId());
		item->Check(true);

		sound_filter = 1;
		sound_update_filter();
	}
	else if (event.GetId() == XRCID("IDM_FILTER_MORE_REDUCED"))
	{
		wxMenuItem *item = ((wxMenu*)menu)->FindItem(event.GetId());
		item->Check(true);

		sound_filter = 2;
		sound_update_filter();
	}
	else if (event.GetId() == XRCID("IDM_SETTINGS_CONFIGURE") && !indebug)
	{
		arc_pause_main_thread();
		ShowConfig(true);
		arc_resume_main_thread();
	}
	else if (event.GetId() == XRCID("IDM_VIDEO_FULLSCR") && !indebug)
	{
		if (firstfull)
		{
			firstfull = 0;

			arc_pause_main_thread();
			wxMessageBox("Use CTRL + END to return to windowed mode", "Arculator", wxOK | wxCENTRE | wxSTAY_ON_TOP);
			arc_resume_main_thread();
		}
		arc_enter_fullscreen();
	}
	else if (event.GetId() == XRCID("IDM_VIDEO_NO_BORDERS"))
	{
		wxMenuItem *item = ((wxMenu*)menu)->FindItem(event.GetId());
		item->Check(true);

		arc_set_display_mode(DISPLAY_MODE_NO_BORDERS);
	}
	else if (event.GetId() == XRCID("IDM_VIDEO_NATIVE_BORDERS"))
	{
		wxMenuItem *item = ((wxMenu*)menu)->FindItem(event.GetId());
		item->Check(true);

		arc_set_display_mode(DISPLAY_MODE_NATIVE_BORDERS);
	}
	else if (event.GetId() == XRCID("IDM_VIDEO_TV"))
	{
		wxMenuItem *item = ((wxMenu*)menu)->FindItem(event.GetId());
		item->Check(true);

		arc_set_display_mode(DISPLAY_MODE_TV);
	}
	else if (event.GetId() == XRCID("IDM_DRIVER_AUTO"))
	{
		wxMenuItem *item = ((wxMenu*)menu)->FindItem(event.GetId());
		item->Check(true);

		selected_video_renderer = RENDERER_AUTO;
		arc_renderer_reset();
	}
	else if (event.GetId() == XRCID("IDM_DRIVER_DIRECT3D"))
	{
		wxMenuItem *item = ((wxMenu*)menu)->FindItem(event.GetId());
		item->Check(true);

		selected_video_renderer = RENDERER_DIRECT3D;
		arc_renderer_reset();
	}
	else if (event.GetId() == XRCID("IDM_DRIVER_OPENGL"))
	{
		wxMenuItem *item = ((wxMenu*)menu)->FindItem(event.GetId());
		item->Check(true);

		selected_video_renderer = RENDERER_OPENGL;
		arc_renderer_reset();
	}
	else if (event.GetId() == XRCID("IDM_DRIVER_SOFTWARE"))
	{
		wxMenuItem *item = ((wxMenu*)menu)->FindItem(event.GetId());
		item->Check(true);

		selected_video_renderer = RENDERER_SOFTWARE;
		arc_renderer_reset();
	}
	else if (event.GetId() == XRCID("IDM_VIDEO_SCALE_NEAREST"))
	{
		wxMenuItem *item = ((wxMenu*)menu)->FindItem(event.GetId());
		item->Check(true);

		video_linear_filtering = 0;
		arc_renderer_reset();
	}
	else if (event.GetId() == XRCID("IDM_VIDEO_SCALE_LINEAR"))
	{
		wxMenuItem *item = ((wxMenu*)menu)->FindItem(event.GetId());
		item->Check(true);

		video_linear_filtering = 1;
		arc_renderer_reset();
	}
	else if (event.GetId() == XRCID("IDM_VIDEO_SCALE[0]") || event.GetId() == XRCID("IDM_VIDEO_SCALE[1]") ||
		 event.GetId() == XRCID("IDM_VIDEO_SCALE[2]") || event.GetId() == XRCID("IDM_VIDEO_SCALE[3]") ||
		 event.GetId() == XRCID("IDM_VIDEO_SCALE[4]") || event.GetId() == XRCID("IDM_VIDEO_SCALE[5]") ||
		 event.GetId() == XRCID("IDM_VIDEO_SCALE[6]") || event.GetId() == XRCID("IDM_VIDEO_SCALE[7]"))
	{
		wxMenuItem *item = ((wxMenu*)menu)->FindItem(event.GetId());
		item->Check(true);

		if (event.GetId() == XRCID("IDM_VIDEO_SCALE[0]"))
			video_scale = 0;
		else if (event.GetId() == XRCID("IDM_VIDEO_SCALE[1]"))
			video_scale = 1;
		else if (event.GetId() == XRCID("IDM_VIDEO_SCALE[2]"))
			video_scale = 2;
		else if (event.GetId() == XRCID("IDM_VIDEO_SCALE[3]"))
			video_scale = 3;
		else if (event.GetId() == XRCID("IDM_VIDEO_SCALE[4]"))
			video_scale = 4;
		else if (event.GetId() == XRCID("IDM_VIDEO_SCALE[5]"))
			video_scale = 5;
		else if (event.GetId() == XRCID("IDM_VIDEO_SCALE[6]"))
			video_scale = 6;
		else if (event.GetId() == XRCID("IDM_VIDEO_SCALE[7]"))
			video_scale = 7;
	}
	else if (event.GetId() == XRCID("IDM_VIDEO_FS_FULL"))
	{
		wxMenuItem *item = ((wxMenu*)menu)->FindItem(event.GetId());
		item->Check(true);

		video_fullscreen_scale = FULLSCR_SCALE_FULL;
	}
	else if (event.GetId() == XRCID("IDM_VIDEO_FS_43"))
	{
		wxMenuItem *item = ((wxMenu*)menu)->FindItem(event.GetId());
		item->Check(true);

		video_fullscreen_scale = FULLSCR_SCALE_43;
	}
	else if (event.GetId() == XRCID("IDM_VIDEO_FS_SQ"))
	{
		wxMenuItem *item = ((wxMenu*)menu)->FindItem(event.GetId());
		item->Check(true);

		video_fullscreen_scale = FULLSCR_SCALE_SQ;
	}
	else if (event.GetId() == XRCID("IDM_VIDEO_FS_INT"))
	{
		wxMenuItem *item = ((wxMenu*)menu)->FindItem(event.GetId());
		item->Check(true);

		video_fullscreen_scale = FULLSCR_SCALE_INT;
	}
	else if (event.GetId() == XRCID("IDM_BLIT_SCAN"))
	{
		wxMenuItem *item = ((wxMenu*)menu)->FindItem(event.GetId());
		item->Check(true);

		arc_set_dblscan(0);
	}
	else if (event.GetId() == XRCID("IDM_BLIT_SCALE"))
	{
		wxMenuItem *item = ((wxMenu*)menu)->FindItem(event.GetId());
		item->Check(true);

		arc_set_dblscan(1);
	}
	else if (event.GetId() == XRCID("IDM_BLACK_ACORN"))
	{
		wxMenuItem *item = ((wxMenu*)menu)->FindItem(event.GetId());
		item->Check(true);

		video_black_level = BLACK_LEVEL_ACORN;
		vidc_redopalette();
	}
	else if (event.GetId() == XRCID("IDM_BLACK_NORMAL"))
	{
		wxMenuItem *item = ((wxMenu*)menu)->FindItem(event.GetId());
		item->Check(true);

		video_black_level = BLACK_LEVEL_NORMAL;
		vidc_redopalette();
	}
	else if (event.GetId() == XRCID("IDM_DEBUGGER_ENABLE"))
	{
		wxMenuItem *item = ((wxMenu*)menu)->FindItem(event.GetId());

		if (!debugon)
		{
			arc_pause_main_thread();
			debugon = 1;
			debug = 1;
			debug_start();
			ShowConsoleWindow(this);
			arc_resume_main_thread();
		}
		else
		{
			debug = 0;
			CloseConsoleWindow();
			debug_end();
		}

		item->Check(debugon);
	}
	else if (event.GetId() == XRCID("IDM_DEBUGGER_BREAK"))
	{
		debug = 1;
	}
}

extern "C" void arc_stop_emulation()
{
	wxCommandEvent* event = new wxCommandEvent(WX_STOP_EMULATION_EVENT, wxID_ANY);
	event->SetEventObject((wxWindow*)main_frame);
	wxQueueEvent((wxWindow*)main_frame, event);
}

extern "C" void arc_popup_menu()
{
	PopupMenuEvent *event = new PopupMenuEvent((wxWindow *)main_frame, (wxMenu *)main_menu);
	wxQueueEvent((wxWindow *)main_frame, event);
}

extern "C" void *wx_getnativemenu(void *menu)
{
#ifdef _WIN32
	return ((wxMenu*)menu)->GetHMenu();
#endif
	return 0;
}

extern "C" void arc_update_menu()
{
	UpdateMenuEvent *event = new UpdateMenuEvent((wxMenu *)main_menu);
	wxQueueEvent((wxWindow *)main_frame, event);
}

void Frame::OnUpdateMenuEvent(UpdateMenuEvent &event)
{
	wxMenu *menu = event.GetMenu();

	UpdateMenu(menu);
}

extern "C" void arc_print_error(const char *format, ...)
{
	wxCommandEvent *event = new wxCommandEvent(WX_PRINT_ERROR_EVENT, wxID_ANY);
	char buf[1024];
	va_list ap;

	va_start(ap, format);
	vsprintf(buf, format, ap);
	va_end(ap);

	event->SetString(wxString(buf));
	wxQueueEvent((wxWindow *)main_frame, event);
}

#ifdef _WIN32
static void *wx_getnativewindow(void *window)
{
	return ((wxWindow *)window)->GetHWND();
}

extern "C" void wx_winsendmessage(void *window, int msg, INT_PARAM wParam, LONG_PARAM lParam)
{
	WinSendMessageEvent *event = new WinSendMessageEvent(wx_getnativewindow(window), msg, wParam, lParam);
	wxQueueEvent((wxWindow *)window, event);
}

void Frame::OnWinSendMessageEvent(WinSendMessageEvent& event)
{
	SendMessage((HWND)event.GetHWND(), event.GetMessage(), event.GetWParam(), event.GetLParam());
}
#endif

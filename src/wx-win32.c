/*Arculator 2.0 by Sarah Walker
  Win32-specific main window handling*/
#define BITMAP __win_bitmap
#include <windows.h>
#undef BITMAP
#include <process.h>
#include <stdio.h>
#include <stdlib.h>
#include <wx/defs.h>
#include "arc.h"
#include "disc.h"
#include "ioc.h"
#include "plat_input.h"
#include "plat_video.h"
#include "vidc.h"
#include "video.h"
#include "video_sdl2.h"

#ifndef LONG_PARAM
#define LONG_PARAM wxIntPtr
#endif

#ifndef INT_PARAM
#define INT_PARAM wxInt32
#endif

void wx_winsendmessage(void *window, int msg, INT_PARAM wParam, LONG_PARAM lParam);

static void *wx_window_ptr;
static char szClassName[ ] = "WindowsApp";
static HWND ghwnd = NULL;
static HMENU menu = 0;
static HINSTANCE hinstance;
static HANDLE main_thread_h;
static HANDLE ui_thread_h;
static RECT arcclip, oldclip;

static SDL_mutex *main_thread_mutex = NULL;

LRESULT CALLBACK WindowProcedure (HWND, UINT, WPARAM, LPARAM);

static int winsizex = 0, winsizey = 0;
static int win_doresize = 0;
static int win_dofullscreen = 0;
static int win_dosetresize = 0;
static int win_renderer_reset = 0;

static int pause_main_thread = 0;

static int infocus = 0;

void updatewindowsize(int x, int y)
{
        winsizex = (x*(video_scale + 1)) / 2;
        winsizey = (y*(video_scale + 1)) / 2;
        win_doresize = 1;
}

static void window_create(void *wx_menu)
{
        WNDCLASSEX wincl;
        HMENU native_menu;
        int count, c;

        hinstance = (HINSTANCE)GetModuleHandle(NULL);
        
        /* The Window structure */
        wincl.hInstance = hinstance;
        wincl.lpszClassName = szClassName;
        wincl.lpfnWndProc = WindowProcedure;      /* This function is called by windows */
        wincl.style = CS_DBLCLKS;                 /* Catch double-clicks */
        wincl.cbSize = sizeof(WNDCLASSEX);

        /* Use default icon and mouse-pointer */
        wincl.hIcon = LoadIcon(hinstance, "ArculatorIconName");
        wincl.hIconSm = LoadIcon(hinstance, "ArculatorIconName");
        wincl.hCursor = NULL;//LoadCursor (NULL, IDC_ARROW);
        wincl.lpszMenuName = NULL;                 /* No menu */
        wincl.cbClsExtra = 0;                      /* No extra bytes after the window class */
        wincl.cbWndExtra = 0;                      /* structure or the window instance */
        /* Use Windows's default color as the background of the window */
        wincl.hbrBackground = (HBRUSH) COLOR_BACKGROUND;

        menu = CreateMenu();//wx_getnativemenu(wx_menu);//NULL;//LoadMenu(hThisInstance,TEXT("MainMenu"));
        native_menu = (HMENU)wx_getnativemenu(wx_menu);
        count = GetMenuItemCount(native_menu);

        for (c = 0; c < count; c++)
        {
                char label[256];
                MENUITEMINFO info;
                
                memset(&info, 0, sizeof(MENUITEMINFO));
                info.cbSize = sizeof(MENUITEMINFO);
                info.fMask = MIIM_TYPE | MIIM_ID;
                info.fType = MFT_STRING;
                info.cch = 256;
                info.dwTypeData = label;
                if (GetMenuItemInfo(native_menu, c, 1, &info))
                        AppendMenu(menu, MF_STRING | MF_POPUP, (UINT)GetSubMenu(native_menu, c), info.dwTypeData);
        }

        /* Register the window class, and if it fails quit the program */
        if (!RegisterClassEx(&wincl))
                fatal("Can not register window class\n");

        /* The class is registered, let's create the program*/
        ghwnd = CreateWindowEx (
           0,                   /* Extended possibilites for variation */
           szClassName,         /* Classname */
           "Arculator " VERSION_STRING,    /* Title Text */
           WS_OVERLAPPEDWINDOW&~(WS_MAXIMIZEBOX|WS_SIZEBOX), /* default window */
           CW_USEDEFAULT,       /* Windows decides the position */
           CW_USEDEFAULT,       /* where the window ends up on the screen */
           800+(GetSystemMetrics(SM_CXFIXEDFRAME)*2),/* The programs width */
           600+(GetSystemMetrics(SM_CYFIXEDFRAME)*2)+GetSystemMetrics(SM_CYMENUSIZE)+GetSystemMetrics(SM_CYCAPTION)+2, /* and height in pixels */
           HWND_DESKTOP,        /* The window is a child-window to desktop */
           menu,                /* Menu */
           hinstance,           /* Program Instance handler */
           NULL                 /* No Window Creation data */
           );

        /* Make the window visible on the screen */
        ShowWindow(ghwnd, 1);
}

static volatile int quited = 0;

void mainthread(LPVOID param)
{
        int frames = 0;
        int draw_count = 0;
        DWORD old_time, new_time;
        DWORD current_thread = GetCurrentThreadId();
        DWORD window_thread = GetWindowThreadProcessId(ghwnd, NULL);

        AttachThreadInput(window_thread, current_thread, TRUE);
        
        if (!video_renderer_init(ghwnd))
        {
                MessageBox(ghwnd, "Video renderer init failed", "Arculator error", MB_OK);
                exit(-1);
        }
        input_init();
        
        arc_update_menu();

        old_time = GetTickCount();
        while (!quited)
        {
                new_time = GetTickCount();
                draw_count += new_time - old_time;
                old_time = new_time;
                
                SDL_LockMutex(main_thread_mutex);
                if (draw_count > 0 && !pause_main_thread)
                {
                        draw_count -= 10;
                        if (draw_count > 80)
                                draw_count = 0;

                        arc_run();

                        frames++;
                        SDL_UnlockMutex(main_thread_mutex);
                }
                else
                {
                        SDL_UnlockMutex(main_thread_mutex);
                        Sleep(1);
                }

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

                if (win_dofullscreen)
                {
                        win_dofullscreen = 0;

                        SetMenu(ghwnd, 0);
                        SDL_RaiseWindow(sdl_main_window);
                        SDL_SetWindowFullscreen(sdl_main_window, SDL_WINDOW_FULLSCREEN_DESKTOP);
                        mouse_capture_enable();
                        fullscreen = 1;

                }
                
                if (win_renderer_reset)
                {
                        win_renderer_reset = 0;

                        if (!video_renderer_reinit(ghwnd))
                        {
                                MessageBox(ghwnd, "Video renderer init failed", "Arculator error", MB_OK);
                                exit(-1);
                        }
                }
                
                if ((key[KEY_LCONTROL] || key[KEY_RCONTROL]) && key[KEY_END] && fullscreen)
                {
                        mouse_capture_disable();
                        SDL_SetWindowFullscreen(sdl_main_window, 0);
                        SetMenu(ghwnd, menu);

                        fullscreen=0;
                        if (fullborders) updatewindowsize(800,600);
                        else             updatewindowsize(672,544);
                }

                if (updatemips)
                {
                        char s[80];
                        sprintf(s, "Arculator %s - %i%% - %s", VERSION_STRING, inssec, mousecapture ? "Press CTRL-END to release mouse" : "Click to capture mouse");
                        vidc_framecount = 0;
                        if (!fullscreen) SetWindowText(ghwnd, s);
                        updatemips=0;
                }
        }

        video_renderer_close();
        AttachThreadInput(current_thread, window_thread, FALSE);
}

static void arc_main_thread(LPVOID wx_menu)
{
        MSG messages;
        
        rpclog("Arculator startup\n");

        window_create(wx_menu);
        
        if (arc_init())
        {
                MessageBox(NULL, "Configured ROM set is not available.\nConfiguration could not be run.", "Arculator", MB_OK);

                arc_close();
                UnregisterClass(szClassName, hinstance);
                arc_stop_emulation();
                
                return;
        }

        arc_update_menu();
        
        main_thread_h = (HANDLE)_beginthread(mainthread, 0, NULL);

        while (GetMessage(&messages, NULL, 0, 0))
        {
                /* Translate virtual-key messages into character messages */
                TranslateMessage(&messages);
                /* Send message to WindowProcedure */
                DispatchMessage(&messages);

                if ((key[KEY_LCONTROL] || key[KEY_RCONTROL]) && key[KEY_END] && !fullscreen && mousecapture)
                {
                        ClipCursor(&oldclip);
                        mouse_capture_disable();
                        mousecapture=0;
                        updatemips=1;
                }
        }

        arc_close();
        UnregisterClass(szClassName, hinstance);
}

void arc_start_main_thread(void *wx_window, void *wx_menu)
{
        quited = 0;
        pause_main_thread = 0;
        main_thread_mutex = SDL_CreateMutex();
        wx_window_ptr = wx_window;
        ui_thread_h = (HANDLE)_beginthread(arc_main_thread, 0, wx_menu);
}

void arc_stop_main_thread()
{
        quited = 1;
        WaitForSingleObject(main_thread_h, INFINITE);
        SDL_DestroyMutex(main_thread_mutex);
        main_thread_mutex = NULL;
}

void arc_pause_main_thread()
{
        SDL_LockMutex(main_thread_mutex);
        pause_main_thread = 1;
        SDL_UnlockMutex(main_thread_mutex);
}

void arc_resume_main_thread()
{
        SDL_LockMutex(main_thread_mutex);
        pause_main_thread = 0;
        SDL_UnlockMutex(main_thread_mutex);
}

void arc_do_reset()
{
        SDL_LockMutex(main_thread_mutex);
        arc_reset();
        SDL_UnlockMutex(main_thread_mutex);
}

void arc_disc_change(int drive, char *fn)
{
        rpclog("arc_disc_change: drive=%i fn=%s\n", drive, fn);

        SDL_LockMutex(main_thread_mutex);

        disc_close(drive);
        strcpy(discname[drive], fn);
        disc_load(drive, discname[drive]);
        ioc_discchange(drive);

        SDL_UnlockMutex(main_thread_mutex);
}

void arc_disc_eject(int drive)
{
        rpclog("arc_disc_eject: drive=%i\n", drive);

        SDL_LockMutex(main_thread_mutex);

        ioc_discchange(drive);
        disc_close(drive);
        discname[drive][0] = 0;

        SDL_UnlockMutex(main_thread_mutex);
}

void arc_enter_fullscreen()
{
        win_dofullscreen = 1;
}

void arc_renderer_reset()
{
        win_renderer_reset = 1;
}

void arc_set_display_mode(int new_display_mode)
{
        SDL_LockMutex(main_thread_mutex);

        display_mode = new_display_mode;
        clearbitmap();
        setredrawall();

        SDL_UnlockMutex(main_thread_mutex);
}

void arc_set_dblscan(int new_dblscan)
{
        SDL_LockMutex(main_thread_mutex);

        dblscan = new_dblscan;
        clearbitmap();

        SDL_UnlockMutex(main_thread_mutex);
}

#define TIMER_1SEC 1
LRESULT CALLBACK WindowProcedure(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
        switch (message)                  /* handle the messages */
        {
                case WM_CREATE:
                SetTimer(hwnd, TIMER_1SEC, 1000, NULL);
                break;

                case WM_COMMAND:
                /* pass through commands to wx window */
                wx_winsendmessage(wx_window_ptr, message, wParam, lParam);
                return 0;

                case WM_SETFOCUS:
                infocus=1;
                break;
                case WM_KILLFOCUS:
                infocus=0;
                if (mousecapture)
                {
                        ClipCursor(&oldclip);
                        mouse_capture_disable();
                        mousecapture=0;
                }
                break;
                case WM_LBUTTONUP:
                if (!mousecapture && !fullscreen && !pause_main_thread)
                {
                        GetClipCursor(&oldclip);
                        GetWindowRect(hwnd,&arcclip);
                        arcclip.left+=GetSystemMetrics(SM_CXFIXEDFRAME)+10;
                        arcclip.right-=GetSystemMetrics(SM_CXFIXEDFRAME)+10;
                        arcclip.top+=GetSystemMetrics(SM_CXFIXEDFRAME)+GetSystemMetrics(SM_CYMENUSIZE)+GetSystemMetrics(SM_CYCAPTION)+10;
                        arcclip.bottom-=GetSystemMetrics(SM_CXFIXEDFRAME)+10;
                        ClipCursor(&arcclip);
                        mousecapture=1;
                        updatemips=1;
                        mouse_capture_enable();
                }
                break;

                case WM_TIMER:
                if (wParam == TIMER_1SEC)
                        updateins();
                break;

                case WM_DESTROY:
                arc_stop_emulation();
                SetMenu(hwnd, 0);
                PostMessage(hwnd, WM_QUIT, 0, 0);
                break;
        }
        return DefWindowProc (hwnd, message, wParam, lParam);
}

#ifdef WIN32

/*Arculator 0.8 by Tom Walker
  Windows interfacing*/
#define BITMAP __win_bitmap
#include <windows.h>
#undef BITMAP
#include <process.h>
#include <stdio.h>
#include <stdlib.h>
#include "arc.h"
#include "arm.h"
#include "config.h"
#include "disc.h"
#include "ioc.h"
#include "keyboard.h"
#include "memc.h"
#include "plat_input.h"
#include "resources.h"
#include "vidc.h"
#include "plat_video.h"
#include "video.h"
#include "video_sdl2.h"
#include "win.h"

int infocus;
HANDLE mainthreadh;

/*  Make the class name into a global variable  */
char szClassName[ ] = "WindowsApp";
HWND ghwnd;
HINSTANCE hinstance;
static HMENU menu = 0;

CRITICAL_SECTION cs;

int limitspeed=0;

RECT oldclip,arcclip;

static int quited=0;

int pause = 0;

extern int firstfull;

extern uint8_t hw_to_mycode[256];

extern int romsavailable[6];

/*  Declare Windows procedure  */
LRESULT CALLBACK WindowProcedure (HWND, UINT, WPARAM, LPARAM);

void error(const char *format, ...)
{
   char buf[256];

   va_list ap;
   va_start(ap, format);
   vsprintf(buf, format, ap);
   va_end(ap);
   MessageBox(NULL,buf,"Arculator error",MB_OK);
}

static int winsizex = 0, winsizey = 0;
static int win_doresize = 0;
static int win_dofullscreen;

void updatewindowsize(int x, int y)
{
        winsizex = x; winsizey = y;
        win_doresize = 1;
}

void clearmemmenu()
{
        HMENU menu=GetMenu(ghwnd);
        CheckMenuItem(menu,IDM_MEMSIZE_512K,MF_UNCHECKED);
        CheckMenuItem(menu,IDM_MEMSIZE_1M,  MF_UNCHECKED);
        CheckMenuItem(menu,IDM_MEMSIZE_2M,  MF_UNCHECKED);
        CheckMenuItem(menu,IDM_MEMSIZE_4M,  MF_UNCHECKED);
        CheckMenuItem(menu,IDM_MEMSIZE_8M,  MF_UNCHECKED);
        CheckMenuItem(menu,IDM_MEMSIZE_16M, MF_UNCHECKED);
}

void initmenu()
{
        HMENU menu=GetMenu(ghwnd);
        if (limitspeed) CheckMenuItem(menu,IDM_OPTIONS_LIMIT,MF_CHECKED);
        if (soundena)   CheckMenuItem(menu,IDM_OPTIONS_SOUND,MF_CHECKED);
        if (stereo)     CheckMenuItem(menu,IDM_OPTIONS_STEREO,MF_CHECKED);
        rpclog("initmenu - fullborders=%i\n", fullborders);
        CheckMenuItem(menu, IDM_VIDEO_NO_BORDERS,     (display_mode == DISPLAY_MODE_NO_BORDERS) ? MF_CHECKED : MF_UNCHECKED);
        CheckMenuItem(menu, IDM_VIDEO_NATIVE_BORDERS, (display_mode == DISPLAY_MODE_NATIVE_BORDERS) ? MF_CHECKED : MF_UNCHECKED);
        CheckMenuItem(menu, IDM_VIDEO_TV,             (display_mode == DISPLAY_MODE_TV) ? MF_CHECKED : MF_UNCHECKED);
        if (fastdisc)   CheckMenuItem(menu,IDM_DISC_FAST,    MF_CHECKED);
        if (!dblscan)   CheckMenuItem(menu,IDM_VIDEO_DBLSCAN,MF_CHECKED);
        switch (memsize)
        {
                case 512:  CheckMenuItem(menu,IDM_MEMSIZE_512K, MF_CHECKED); break;
                case 1024:  CheckMenuItem(menu,IDM_MEMSIZE_1M, MF_CHECKED); break;
                case 2048:  CheckMenuItem(menu,IDM_MEMSIZE_2M, MF_CHECKED); break;
                case 4096:  CheckMenuItem(menu,IDM_MEMSIZE_4M, MF_CHECKED); break;
                case 8192:  CheckMenuItem(menu,IDM_MEMSIZE_8M, MF_CHECKED); break;
                case 16384: CheckMenuItem(menu,IDM_MEMSIZE_16M,MF_CHECKED); break;
        }
        if (fdctype) CheckMenuItem(menu,IDM_FDC_82C711,MF_CHECKED);
        else         CheckMenuItem(menu,IDM_FDC_WD1772,MF_CHECKED);

        CheckMenuItem(menu, IDM_CPU_ARM2_MEMC1 + arm_cpu_type, MF_CHECKED);

        if (dblscan) CheckMenuItem(menu,IDM_BLIT_SCALE,MF_CHECKED);
        else         CheckMenuItem(menu,IDM_BLIT_SCAN,MF_CHECKED);
        if (hires) CheckMenuItem(menu,IDM_MONITOR_HIRES,MF_CHECKED);
        else       CheckMenuItem(menu,IDM_MONITOR_NORMAL,MF_CHECKED);

        CheckMenuItem(menu,IDM_ROM_ARTHUR+romset, MF_CHECKED);
}

void startblit()
{
        EnterCriticalSection(&cs);
}

void endblit()
{
        LeaveCriticalSection(&cs);
}

void mainthread(LPVOID param)
{
        int frames = 0;
        int draw_count = 0;
        DWORD old_time, new_time;
//        mainthreadon=1;

        if (!video_renderer_init(ghwnd))
        {
                MessageBox(ghwnd, "Video renderer init failed", "Arculator error", MB_OK);
                exit(-1);
        }
        input_init();

        old_time = GetTickCount();
        while (!quited)
        {
                        new_time = GetTickCount();
                        draw_count += new_time - old_time;
                        old_time = new_time;
                        if (draw_count > 0 && !pause)
                        {
//                                printf("Drawits %i\n",drawits);
                                draw_count -= 10;
                                if (draw_count > 80) 
                                        draw_count = 0;
                                //wokeups++;
                                startblit();

                                arc_run();

                                endblit();
                                frames++;
                        }
                        else
                        {
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
                        sprintf(s, "Arculator %s - %i%% - %s", __DATE__, inssec, mousecapture ? "Press CTRL-END to release mouse" : "Click to capture mouse");
                        vidc_framecount = 0;                        
                        if (!fullscreen) SetWindowText(ghwnd, s);
                        updatemips=0;
                }
        }
        rpclog("mainthread exit\n");
//        mainthreadon=0;

        video_renderer_close();
}

#define TIMER_1SEC 1

void get_executable_name(char *s, int size)
{
        GetModuleFileName(hinstance, s, size);
}

int WINAPI WinMain (HINSTANCE hThisInstance,
                    HINSTANCE hPrevInstance,
                    LPSTR lpszArgument,
                    int nFunsterStil)

{
        /* This is the handle for our window */
        MSG messages;            /* Here messages to the application are saved */
        WNDCLASSEX wincl;        /* Data structure for the windowclass */

        hinstance = hThisInstance;
        
        /* The Window structure */
        wincl.hInstance = hThisInstance;
        wincl.lpszClassName = szClassName;
        wincl.lpfnWndProc = WindowProcedure;      /* This function is called by windows */
        wincl.style = CS_DBLCLKS;                 /* Catch double-clicks */
        wincl.cbSize = sizeof (WNDCLASSEX);

        /* Use default icon and mouse-pointer */
        wincl.hIcon = LoadIcon(hThisInstance, "allegro_icon");
        wincl.hIconSm = LoadIcon(hThisInstance, "allegro_icon");
        wincl.hCursor = NULL;//LoadCursor (NULL, IDC_ARROW);
        wincl.lpszMenuName = NULL;                 /* No menu */
        wincl.cbClsExtra = 0;                      /* No extra bytes after the window class */
        wincl.cbWndExtra = 0;                      /* structure or the window instance */
        /* Use Windows's default color as the background of the window */
        wincl.hbrBackground = (HBRUSH) COLOR_BACKGROUND;

        menu = LoadMenu(hThisInstance,TEXT("MainMenu"));
        
        /* Register the window class, and if it fails quit the program */
        if (!RegisterClassEx (&wincl))
           return 0;
           
        /* The class is registered, let's create the program*/
        ghwnd = CreateWindowEx (
           0,                   /* Extended possibilites for variation */
           szClassName,         /* Classname */
           "Arculator v1.0",    /* Title Text */
           WS_OVERLAPPEDWINDOW&~(WS_MAXIMIZEBOX|WS_SIZEBOX), /* default window */
           CW_USEDEFAULT,       /* Windows decides the position */
           CW_USEDEFAULT,       /* where the window ends up on the screen */
           800+(GetSystemMetrics(SM_CXFIXEDFRAME)*2),/* The programs width */
           600+(GetSystemMetrics(SM_CYFIXEDFRAME)*2)+GetSystemMetrics(SM_CYMENUSIZE)+GetSystemMetrics(SM_CYCAPTION)+2, /* and height in pixels */
           HWND_DESKTOP,        /* The window is a child-window to desktop */
           menu,                /* Menu */
           hThisInstance,       /* Program Instance handler */
           NULL                 /* No Window Creation data */
           );

        /* Make the window visible on the screen */
        ShowWindow (ghwnd, nFunsterStil);

        arc_init();
        
        SendMessage(ghwnd,WM_USER,0,0);

        initmenu();

        InitializeCriticalSection(&cs);
        mainthreadh = (HANDLE)_beginthread(mainthread, 0, NULL);
        SetThreadPriority(mainthreadh, THREAD_PRIORITY_HIGHEST);
        /* Run the message loop. It will run until GetMessage() returns 0 */
        while (!quited)
        {
                while (GetMessage(&messages, NULL, 0, 0) && !quited)
                {
                        if (messages.message == WM_QUIT)
                        {
                                rpclog("GetMessage returned WM_QUIT\n");                        
                                quited = 1;
                        }
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
/*                        if ((key[KEY_LCONTROL] || key[KEY_RCONTROL]) && key[KEY_END] && fullscreen)
                        {
                                fullscreen=0;
                                reinitvideo();
                                if (fullborders) updatewindowsize(800,600);
                                else             updatewindowsize(672,544);
                        }*/
                }
                quited = 1;
        }
        rpclog("WM_QUIT received\n");
        startblit();

        Sleep(200);

        TerminateThread(mainthreadh,0);
        rpclog("arc_close\n");
        arc_close();
        if (mousecapture) ClipCursor(&oldclip);
        rpclog("all done\n");
        return 0;
}

void changedisc(HWND hwnd, int drive)
{
        char fn[512];
        char fn2[512];
        OPENFILENAME ofn;
        memcpy(fn2,discname[drive],512);
//        p=get_filename(fn2);
//        *p=0;
        fn[0]=0;
        memset(&ofn,0,sizeof(OPENFILENAME));
        ofn.lStructSize = sizeof(OPENFILENAME);
	ofn.hwndOwner = hwnd;
	ofn.hInstance = NULL;
	ofn.lpstrFilter = "All disc images\0*.adf;*.img;*.fdi;*.apd;*.jfd\0FDI Disc Image\0*.fdi\0APD Disc Image\0*.apd\0ADFS Disc Image\0*.adf\0DOS Disc Image\0*.img\0All Files\0*.*\0";
	ofn.lpstrCustomFilter = NULL;
	ofn.nMaxCustFilter = 0;
	ofn.nFilterIndex = 1;
	ofn.lpstrFile = fn;
	ofn.nMaxFile = sizeof(fn);
	ofn.lpstrFileTitle = NULL;
	ofn.nMaxFileTitle = 0;
	ofn.lpstrInitialDir = fn2;
	ofn.lpstrTitle = NULL;
	ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY;
	ofn.nFileOffset = 0;
	ofn.nFileExtension = 0;
	ofn.lpstrDefExt = NULL;
	ofn.lCustData = 0;
	ofn.lpfnHook = NULL;
	ofn.lpTemplateName = NULL;
        if (GetOpenFileName(&ofn))
        {
                disc_close(drive);

                strcpy(discname[drive],fn);

                rpclog("changedisc - %i %s\n", drive, discname[drive]);                
                disc_load(drive, discname[drive]);

                ioc_discchange(drive);
                //discchangeint(1,drive);
        }
}

void win_changecpuspeed(HMENU hmenu)
{
        resetarm();
        memset(ram,0,memsize*1024);
        resetmouse();
        ioc_reset();
        keyboard_init();
        CheckMenuItem(hmenu,IDM_CPU_ARM2_MEMC1, MF_UNCHECKED);
        CheckMenuItem(hmenu,IDM_CPU_ARM2, MF_UNCHECKED);
        CheckMenuItem(hmenu,IDM_CPU_ARM250, MF_UNCHECKED);
        CheckMenuItem(hmenu,IDM_CPU_ARM3,   MF_UNCHECKED);
        CheckMenuItem(hmenu,IDM_CPU_ARM3_33,MF_UNCHECKED);
        CheckMenuItem(hmenu,IDM_CPU_ARM3_66,MF_UNCHECKED);
        CheckMenuItem(hmenu,IDM_CPU_ARM3_FPA,   MF_UNCHECKED);
        CheckMenuItem(hmenu,IDM_CPU_ARM3_33_FPA,MF_UNCHECKED);
        CheckMenuItem(hmenu,IDM_CPU_ARM3_66_FPA,MF_UNCHECKED);
}        

HHOOK hKeyboardHook;

LRESULT CALLBACK LowLevelKeyboardProc( int nCode, WPARAM wParam, LPARAM lParam )
{
        if (nCode < 0 || nCode != HC_ACTION || !mousecapture) 
                return CallNextHookEx( hKeyboardHook, nCode, wParam, lParam); 
	
	KBDLLHOOKSTRUCT *p = (KBDLLHOOKSTRUCT *)lParam;

        if(p->vkCode == VK_TAB && p->flags & LLKHF_ALTDOWN) return 1; //disable alt-tab
        if(p->vkCode == VK_SPACE && p->flags & LLKHF_ALTDOWN) return 1; //disable alt-tab    
	if((p->vkCode == VK_LWIN) || (p->vkCode == VK_RWIN)) return 1;//disable windows keys
	if (p->vkCode == VK_ESCAPE && p->flags & LLKHF_ALTDOWN) return 1;//disable alt-escape
	BOOL bControlKeyDown = GetAsyncKeyState (VK_CONTROL) >> ((sizeof(SHORT) * 8) - 1);//checks ctrl key pressed
	if (p->vkCode == VK_ESCAPE && bControlKeyDown) return 1; //disable ctrl-escape
	
	return CallNextHookEx( hKeyboardHook, nCode, wParam, lParam );
}

/*  This function is called by the Windows function DispatchMessage()  */
LRESULT CALLBACK WindowProcedure (HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
        HMENU hmenu;
        MENUITEMINFO mii;
        int c;
        switch (message)                  /* handle the messages */
        {
                case WM_CREATE:
                //hKeyboardHook = SetWindowsHookEx( WH_KEYBOARD_LL,  LowLevelKeyboardProc, GetModuleHandle(NULL), 0 );
                SetTimer(hwnd, TIMER_1SEC, 1000, NULL);
                break;

                case WM_USER:
                hmenu=GetMenu(hwnd);
                memset(&mii,0,sizeof(MENUITEMINFO));
                mii.cbSize=sizeof(MENUITEMINFO);
                mii.fMask=MIIM_STATE;
                mii.fState=MFS_GRAYED;
                for (c=0;c<6;c++)
                {
                        if (!romsavailable[c])
                        {
                                SetMenuItemInfo(hmenu,IDM_ROM_ARTHUR+c,0,&mii);
                        }
                }
                break;
                case WM_COMMAND:
                hmenu=GetMenu(hwnd);
                switch (LOWORD(wParam))
                {
                        case IDM_DISC_CHANGE0:
                        changedisc(ghwnd,0);
                        return 0;
                        case IDM_DISC_CHANGE1:
                        changedisc(ghwnd,1);
                        return 0;
                        case IDM_DISC_CHANGE2:
                        changedisc(ghwnd,2);
                        return 0;
                        case IDM_DISC_CHANGE3:
                        changedisc(ghwnd,3);
                        return 0;
                        case IDM_DISC_REMOVE0:
                        ioc_discchange(0);
                        disc_close(0);
                        discname[0][0]=0;
                        return 0;
                        case IDM_DISC_REMOVE1:
                        ioc_discchange(1);
                        disc_close(1);
                        discname[1][0]=0;
                        return 0;
                        case IDM_DISC_REMOVE2:
                        ioc_discchange(2);
                        disc_close(2);
                        discname[2][0]=0;
                        return 0;
                        case IDM_DISC_REMOVE3:
                        ioc_discchange(3);
                        disc_close(3);
                        discname[3][0]=0;
                        return 0;
                        case IDM_DISC_FAST:
                        fastdisc^=1;
                        if (fastdisc) CheckMenuItem(hmenu,IDM_DISC_FAST,MF_CHECKED);
                        else          CheckMenuItem(hmenu,IDM_DISC_FAST,MF_UNCHECKED);
                        return 0;
                        case IDM_FILE_RESET:
                        startblit();
                        Sleep(200);
                        resetarm();
                        memset(ram,0,memsize*1024);
                        resetmouse();
                        keyboard_init();
                        endblit();
                        return 0;
                        case IDM_FILE_EXIT:
                        rpclog("IDM_FILE_EXIT : PostQuitMessage\n");
                        PostQuitMessage(0);
                        return 0;
                        case IDM_OPTIONS_SOUND:
                        soundena^=1;
                        if (soundena)
                        {
                                CheckMenuItem(hmenu,IDM_OPTIONS_SOUND,MF_CHECKED);
                                if (!soundena) CheckMenuItem(hmenu,IDM_OPTIONS_SOUND,MF_UNCHECKED);
                        }
                        else
                        {
                                CheckMenuItem(hmenu,IDM_OPTIONS_SOUND,MF_UNCHECKED);
                        }
                        return 0;
                        case IDM_OPTIONS_STEREO:
                        stereo^=1;
                        if (stereo) CheckMenuItem(hmenu,IDM_OPTIONS_STEREO,MF_CHECKED);
                        else        CheckMenuItem(hmenu,IDM_OPTIONS_STEREO,MF_UNCHECKED);
                        return 0;
                        case IDM_OPTIONS_LIMIT:
                        limitspeed^=1;
                        if (limitspeed) CheckMenuItem(hmenu,IDM_OPTIONS_LIMIT,MF_CHECKED);
                        else            CheckMenuItem(hmenu,IDM_OPTIONS_LIMIT,MF_UNCHECKED);
                        return 0;

                        case IDM_VIDEO_FULLSCR:                        
//                        fullscreen=1;
                        if (firstfull)
                        {
                                firstfull=0;
                                MessageBox(hwnd,"Use CTRL + END to return to windowed mode","Arculator",MB_OK);
                        }
                        if (mousecapture)
                        {
                                ClipCursor(&oldclip);
                                mousecapture=0;
                        }
                        win_dofullscreen = 1;
//                        reinitvideo();
                        return 0;
                        case IDM_VIDEO_NO_BORDERS:
                        case IDM_VIDEO_NATIVE_BORDERS:
                        case IDM_VIDEO_TV:
                        display_mode = LOWORD(wParam) - IDM_VIDEO_NO_BORDERS;
                        CheckMenuItem(menu, IDM_VIDEO_NO_BORDERS,     (display_mode == DISPLAY_MODE_NO_BORDERS) ? MF_CHECKED : MF_UNCHECKED);
                        CheckMenuItem(menu, IDM_VIDEO_NATIVE_BORDERS, (display_mode == DISPLAY_MODE_NATIVE_BORDERS) ? MF_CHECKED : MF_UNCHECKED);
                        CheckMenuItem(menu, IDM_VIDEO_TV,             (display_mode == DISPLAY_MODE_TV) ? MF_CHECKED : MF_UNCHECKED);
                        clearbitmap();
                        setredrawall();
                        return 0;
                        case IDM_VIDEO_DBLSCAN:
                        dblscan^=1;
                        if (!dblscan) CheckMenuItem(hmenu,IDM_VIDEO_DBLSCAN,MF_CHECKED);
                        else          CheckMenuItem(hmenu,IDM_VIDEO_DBLSCAN,MF_UNCHECKED);
                        clearbitmap();
                        return 0;
                        case IDM_BLIT_SCAN:
                        dblscan=0;
                        clearbitmap();
                        CheckMenuItem(hmenu,IDM_BLIT_SCAN,  MF_CHECKED);
                        CheckMenuItem(hmenu,IDM_BLIT_SCALE, MF_UNCHECKED);
                        CheckMenuItem(hmenu,IDM_BLIT_HSCALE,MF_UNCHECKED);
                        return 0;
                        case IDM_BLIT_SCALE:
                        dblscan=1;
                        clearbitmap();
                        CheckMenuItem(hmenu,IDM_BLIT_SCAN,  MF_UNCHECKED);
                        CheckMenuItem(hmenu,IDM_BLIT_SCALE, MF_CHECKED);
                        CheckMenuItem(hmenu,IDM_BLIT_HSCALE,MF_UNCHECKED);
                        return 0;
                        case IDM_BLIT_HSCALE:
                        dblscan=1;
                        clearbitmap();
                        CheckMenuItem(hmenu,IDM_BLIT_SCAN,  MF_UNCHECKED);
                        CheckMenuItem(hmenu,IDM_BLIT_SCALE, MF_UNCHECKED);
                        CheckMenuItem(hmenu,IDM_BLIT_HSCALE,MF_CHECKED);
                        return 0;

                        case IDM_MONITOR_NORMAL:
                        startblit();
                        Sleep(200);
                        hires=0;
                        CheckMenuItem(hmenu,IDM_MONITOR_NORMAL,MF_CHECKED);
                        CheckMenuItem(hmenu,IDM_MONITOR_HIRES, MF_UNCHECKED);
                        reinitvideo();
                        if (fullborders) updatewindowsize(800,600);
                        else             updatewindowsize(672,544);
                        endblit();
                        return 0;
                        case IDM_MONITOR_HIRES:
                        startblit();
                        Sleep(200);
                        hires=1;
                        CheckMenuItem(hmenu,IDM_MONITOR_NORMAL,MF_UNCHECKED);
                        CheckMenuItem(hmenu,IDM_MONITOR_HIRES, MF_CHECKED);
                        reinitvideo();
                        endblit();
                        return 0;
                        
                        case IDM_MACHINE_CONFIGURE:
                        startblit();
                        Sleep(200);
                        config_open(hwnd);
                        endblit();
                        return 0;
                }
                break;
                case WM_SETFOCUS:
                for (c=0;c<128;c++) key[c]=0;
                infocus=1;
                if (fullscreen) reinitvideo();
                break;
                case WM_KILLFOCUS:
                for (c=0;c<128;c++) key[c]=0;
                infocus=0;
                if (mousecapture)
                {
                        ClipCursor(&oldclip);
                        mouse_capture_disable();
                        mousecapture=0;
                }
                break;
                case WM_LBUTTONUP:
                if (!mousecapture && !fullscreen)
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
                rpclog("WM_DESTROY\n");
                //UnhookWindowsHookEx(hKeyboardHook);
                PostQuitMessage (0);       /* send a WM_QUIT to the message queue */
                break;
                default:                      /* for messages that we don't deal with */
                return DefWindowProc (hwnd, message, wParam, lParam);
        }
        return 0;
}

#endif

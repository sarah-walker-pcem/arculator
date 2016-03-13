#ifdef WIN32

/*Arculator 0.8 by Tom Walker
  Windows interfacing*/
#include <stdio.h>
#include <stdlib.h>
#include <allegro.h>
#include <winalleg.h>
#include "arc.h"
#include "arm.h"
#include "memc.h"
#include "resources.h"
#include "vidc.h"

int infocus;
HANDLE mainthreadh;

/*  Make the class name into a global variable  */
char szClassName[ ] = "WindowsApp";
HWND ghwnd;
HINSTANCE hinstance;

CRITICAL_SECTION cs;

int limitspeed=0;

int mousecapture=0;
RECT oldclip,arcclip;

int quited=0;

int pause = 0;

extern int updatemips,inssec;

extern int firstfull;

extern uint8_t hw_to_mycode[256];

extern int romsavailable[6];

/*  Declare Windows procedure  */
LRESULT CALLBACK WindowProcedure (HWND, UINT, WPARAM, LPARAM);

void error(char *format, ...)
{
   char buf[256];

   va_list ap;
   va_start(ap, format);
   vsprintf(buf, format, ap);
   va_end(ap);
   MessageBox(NULL,buf,"Arculator error",MB_OK);
}

static int winsizex, winsizey;
static int win_doresize = 0;

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
        if (fullborders)
        {
                updatewindowsize(800,600);
                CheckMenuItem(menu,IDM_VIDEO_FULLBOR, MF_CHECKED);
        }
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
                        RECT r;
                        GetWindowRect(ghwnd, &r);
                        MoveWindow(ghwnd, r.left, r.top,
                                winsizex + (GetSystemMetrics(SM_CXFIXEDFRAME) * 2),
                                winsizey + (GetSystemMetrics(SM_CYFIXEDFRAME) * 2) + GetSystemMetrics(SM_CYMENUSIZE) + GetSystemMetrics(SM_CYCAPTION) + 1,
                                TRUE);
                        win_doresize = 0;
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
        wincl.hCursor = LoadCursor (NULL, IDC_ARROW);
        wincl.lpszMenuName = NULL;                 /* No menu */
        wincl.cbClsExtra = 0;                      /* No extra bytes after the window class */
        wincl.cbWndExtra = 0;                      /* structure or the window instance */
        /* Use Windows's default color as the background of the window */
        wincl.hbrBackground = (HBRUSH) COLOR_BACKGROUND;

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
           672+(GetSystemMetrics(SM_CXFIXEDFRAME)*2),/* The programs width */
           544+(GetSystemMetrics(SM_CYFIXEDFRAME)*2)+GetSystemMetrics(SM_CYMENUSIZE)+GetSystemMetrics(SM_CYCAPTION)+2, /* and height in pixels */
           HWND_DESKTOP,        /* The window is a child-window to desktop */
           LoadMenu(hThisInstance,TEXT("MainMenu")),                /* Menu */
           hThisInstance,       /* Program Instance handler */
           NULL                 /* No Window Creation data */
           );

        /* Make the window visible on the screen */
        ShowWindow (ghwnd, nFunsterStil);
        win_set_window(ghwnd);

        arc_init();
        set_display_switch_mode(SWITCH_BACKGROUND);
        
        SendMessage(ghwnd,WM_USER,0,0);

        MoveWindow(ghwnd,100,100,640+(GetSystemMetrics(SM_CXFIXEDFRAME)*2),512+(GetSystemMetrics(SM_CYFIXEDFRAME)*2)+GetSystemMetrics(SM_CYMENUSIZE)+GetSystemMetrics(SM_CYCAPTION)+1,TRUE);

        initmenu();

        timeBeginPeriod(1);
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
                                mousecapture=0;
                                updatemips=1;
                        }
                        if ((key[KEY_LCONTROL] || key[KEY_RCONTROL]) && key[KEY_END] && fullscreen)
                        {
                                fullscreen=0;
                                reinitvideo();
                                if (fullborders) updatewindowsize(800,600);
                                else             updatewindowsize(672,544);
                        }
                }
                quited = 1;
//                while (limitspeed && !spdcount && infocus)
//                      sleep(0);
        }
        rpclog("WM_QUIT received\n");
        startblit();

        Sleep(200);

        TerminateThread(mainthreadh,0);
        timeEndPeriod(1);
        rpclog("arc_close\n");
        arc_close();
        if (mousecapture) ClipCursor(&oldclip);
        rpclog("all done\n");
        return 0;
}

void changedisc(HWND hwnd, int drive)
{
        char fn[512];
        char start[512];
        char fn2[512];
        OPENFILENAME ofn;
        memcpy(fn2,discname[drive],512);
//        p=get_filename(fn2);
//        *p=0;
        fn[0]=0;
        start[0]=0;
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
                hKeyboardHook = SetWindowsHookEx( WH_KEYBOARD_LL,  LowLevelKeyboardProc, GetModuleHandle(NULL), 0 );
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
                        fullscreen=1;
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
                        reinitvideo();
                        return 0;
                        case IDM_VIDEO_FULLBOR:
                        fullborders^=1;
                        noborders=0;
                        if (fullborders) updatewindowsize(800,600);
                        else             updatewindowsize(672,544);
                        if (fullborders) CheckMenuItem(hmenu,IDM_VIDEO_FULLBOR,MF_CHECKED);
                        else             CheckMenuItem(hmenu,IDM_VIDEO_FULLBOR,MF_UNCHECKED);
                        CheckMenuItem(hmenu,IDM_VIDEO_NOBOR,MF_UNCHECKED);
                        clearbitmap();
                        setredrawall();
                        return 0;
                        case IDM_VIDEO_NOBOR:
                        noborders^=1;
                        fullborders=0;
                        if (noborders) CheckMenuItem(hmenu,IDM_VIDEO_NOBOR,MF_CHECKED);
                        else           CheckMenuItem(hmenu,IDM_VIDEO_NOBOR,MF_UNCHECKED);
                        CheckMenuItem(hmenu,IDM_VIDEO_FULLBOR,MF_UNCHECKED);
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
                        hardwareblit=0;
                        clearbitmap();
                        CheckMenuItem(hmenu,IDM_BLIT_SCAN,  MF_CHECKED);
                        CheckMenuItem(hmenu,IDM_BLIT_SCALE, MF_UNCHECKED);
                        CheckMenuItem(hmenu,IDM_BLIT_HSCALE,MF_UNCHECKED);
                        return 0;
                        case IDM_BLIT_SCALE:
                        dblscan=1;
                        hardwareblit=0;
                        clearbitmap();
                        CheckMenuItem(hmenu,IDM_BLIT_SCAN,  MF_UNCHECKED);
                        CheckMenuItem(hmenu,IDM_BLIT_SCALE, MF_CHECKED);
                        CheckMenuItem(hmenu,IDM_BLIT_HSCALE,MF_UNCHECKED);
                        return 0;
                        case IDM_BLIT_HSCALE:
                        dblscan=1;
                        hardwareblit=1;
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
                }
                break;

        	case WM_SYSKEYDOWN:
        	case WM_KEYDOWN:
                if (LOWORD(wParam)!=255)
                {
//                        rpclog("Key %04X %04X\n",LOWORD(wParam),VK_LEFT);
                        c=MapVirtualKey(LOWORD(wParam),0);
                        c=hw_to_mycode[c];
//                        rpclog("MVK %i %i %i\n",c,hw_to_mycode[c],KEY_PGUP);
                        if (LOWORD(wParam)==VK_LEFT)   c=KEY_LEFT;
                        if (LOWORD(wParam)==VK_RIGHT)  c=KEY_RIGHT;
                        if (LOWORD(wParam)==VK_UP)     c=KEY_UP;
                        if (LOWORD(wParam)==VK_DOWN)   c=KEY_DOWN;
                        if (LOWORD(wParam)==VK_HOME)   c=KEY_HOME;
                        if (LOWORD(wParam)==VK_END)    c=KEY_END;
                        if (LOWORD(wParam)==VK_INSERT) c=KEY_INSERT;
                        if (LOWORD(wParam)==VK_DELETE) c=KEY_DEL;
                        if (LOWORD(wParam)==VK_PRIOR)  c=KEY_PGUP;
                        if (LOWORD(wParam)==VK_NEXT)   c=KEY_PGDN;
//                        rpclog("MVK2 %i %i %i\n",c,hw_to_mycode[c],KEY_PGUP);
                        key[c]=1;
                }
                break;
        	case WM_SYSKEYUP:
        	case WM_KEYUP:
                if (LOWORD(wParam)!=255)
                {
//                        rpclog("Key %04X %04X\n",LOWORD(wParam),VK_LEFT);
                        c=MapVirtualKey(LOWORD(wParam),0);
                        c=hw_to_mycode[c];
                        if (LOWORD(wParam)==VK_LEFT)   c=KEY_LEFT;
                        if (LOWORD(wParam)==VK_RIGHT)  c=KEY_RIGHT;
                        if (LOWORD(wParam)==VK_UP)     c=KEY_UP;
                        if (LOWORD(wParam)==VK_DOWN)   c=KEY_DOWN;
                        if (LOWORD(wParam)==VK_HOME)   c=KEY_HOME;
                        if (LOWORD(wParam)==VK_END)    c=KEY_END;
                        if (LOWORD(wParam)==VK_INSERT) c=KEY_INSERT;
                        if (LOWORD(wParam)==VK_DELETE) c=KEY_DEL;
                        if (LOWORD(wParam)==VK_PRIOR)  c=KEY_PGUP;
                        if (LOWORD(wParam)==VK_NEXT)   c=KEY_PGDN;
//                        rpclog("MVK %i\n",c);
                        key[c]=0;
                }
                break;

                case WM_DESTROY:
                rpclog("WM_DESTROY\n");
                UnhookWindowsHookEx(hKeyboardHook);
                PostQuitMessage (0);       /* send a WM_QUIT to the message queue */
                break;
                default:                      /* for messages that we don't deal with */
                return DefWindowProc (hwnd, message, wParam, lParam);
        }
        return 0;
}

uint8_t hw_to_mycode[256] = {
   /* 0x00 */ 0, KEY_ESC, KEY_1, KEY_2,
   /* 0x04 */ KEY_3, KEY_4, KEY_5, KEY_6,
   /* 0x08 */ KEY_7, KEY_8, KEY_9, KEY_0,
   /* 0x0C */ KEY_MINUS, KEY_EQUALS, KEY_BACKSPACE, KEY_TAB,
   /* 0x10 */ KEY_Q, KEY_W, KEY_E, KEY_R,
   /* 0x14 */ KEY_T, KEY_Y, KEY_U, KEY_I,
   /* 0x18 */ KEY_O, KEY_P, KEY_OPENBRACE, KEY_CLOSEBRACE,
   /* 0x1C */ KEY_ENTER, KEY_LCONTROL, KEY_A, KEY_S,
   /* 0x20 */ KEY_D, KEY_F, KEY_G, KEY_H,
   /* 0x24 */ KEY_J, KEY_K, KEY_L, KEY_SEMICOLON,
   /* 0x28 */ KEY_QUOTE, KEY_TILDE, KEY_LSHIFT, KEY_BACKSLASH,
   /* 0x2C */ KEY_Z, KEY_X, KEY_C, KEY_V,
   /* 0x30 */ KEY_B, KEY_N, KEY_M, KEY_COMMA,
   /* 0x34 */ KEY_STOP, KEY_SLASH, KEY_RSHIFT, KEY_ASTERISK,
   /* 0x38 */ KEY_ALT, KEY_SPACE, KEY_CAPSLOCK, KEY_F1,
   /* 0x3C */ KEY_F2, KEY_F3, KEY_F4, KEY_F5,
   /* 0x40 */ KEY_F6, KEY_F7, KEY_F8, KEY_F9,
   /* 0x44 */ KEY_F10, KEY_NUMLOCK, KEY_SCRLOCK, KEY_7_PAD,
   /* 0x48 */ KEY_8_PAD, KEY_9_PAD, KEY_MINUS_PAD, KEY_4_PAD,
   /* 0x4C */ KEY_5_PAD, KEY_6_PAD, KEY_PLUS_PAD, KEY_1_PAD,
   /* 0x50 */ KEY_2_PAD, KEY_3_PAD, KEY_0_PAD, KEY_DEL_PAD,
   /* 0x54 */ KEY_PRTSCR, 0, KEY_BACKSLASH2, KEY_F11,
   /* 0x58 */ KEY_F12, 0, 0, KEY_LWIN,
   /* 0x5C */ KEY_RWIN, KEY_MENU, 0, 0,
   /* 0x60 */ 0, 0, 0, 0,
   /* 0x64 */ 0, 0, 0, 0,
   /* 0x68 */ 0, 0, 0, 0,
   /* 0x6C */ 0, 0, 0, 0,
   /* 0x70 */ KEY_KANA, 0, 0, KEY_ABNT_C1,
   /* 0x74 */ 0, 0, 0, 0,
   /* 0x78 */ 0, KEY_CONVERT, 0, KEY_NOCONVERT,
   /* 0x7C */ 0, KEY_YEN, 0, 0,
   /* 0x80 */ 0, 0, 0, 0,
   /* 0x84 */ 0, 0, 0, 0,
   /* 0x88 */ 0, 0, 0, 0,
   /* 0x8C */ 0, 0, 0, 0,
   /* 0x90 */ 0, KEY_AT, KEY_COLON2, 0,
   /* 0x94 */ KEY_KANJI, 0, 0, 0,
   /* 0x98 */ 0, 0, 0, 0,
   /* 0x9C */ KEY_ENTER_PAD, KEY_RCONTROL, 0, 0,
   /* 0xA0 */ 0, 0, 0, 0,
   /* 0xA4 */ 0, 0, 0, 0,
   /* 0xA8 */ 0, 0, 0, 0,
   /* 0xAC */ 0, 0, 0, 0,
   /* 0xB0 */ 0, 0, 0, 0,
   /* 0xB4 */ 0, KEY_SLASH_PAD, 0, KEY_PRTSCR,
   /* 0xB8 */ KEY_ALTGR, 0, 0, 0,
   /* 0xBC */ 0, 0, 0, 0,
   /* 0xC0 */ 0, 0, 0, 0,
   /* 0xC4 */ 0, KEY_PAUSE, 0, KEY_HOME,
   /* 0xC8 */ KEY_UP, KEY_PGUP, 0, KEY_LEFT,
   /* 0xCC */ 0, KEY_RIGHT, 0, KEY_END,
   /* 0xD0 */ KEY_DOWN, KEY_PGDN, KEY_INSERT, KEY_DEL,
   /* 0xD4 */ 0, 0, 0, 0,
   /* 0xD8 */ 0, 0, 0, KEY_LWIN,
   /* 0xDC */ KEY_RWIN, KEY_MENU, 0, 0,
   /* 0xE0 */ 0, 0, 0, 0,
   /* 0xE4 */ 0, 0, 0, 0,
   /* 0xE8 */ 0, 0, 0, 0,
   /* 0xEC */ 0, 0, 0, 0,
   /* 0xF0 */ 0, 0, 0, 0,
   /* 0xF4 */ 0, 0, 0, 0,
   /* 0xF8 */ 0, 0, 0, 0,
   /* 0xFC */ 0, 0, 0, 0
};

#endif

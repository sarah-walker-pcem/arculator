/*Arculator 0.8 by Tom Walker
  Main loop + Windows interfacing*/
#include <stdio.h>
#include <stdlib.h>
#include <allegro.h>
#include <winalleg.h>
#include "arc.h"

int framecount,framecount2;

/*0=Arthur
  1=RiscOS 2
  2=RiscOS 3.1 with WD1772
  3=RiscOS 3.1 with 82c711
  4=MAME 'ertictac' set
  5=MAME 'poizone' set
  There are two RiscOS 3.1 sets as configuring for 82c711 corrupts ADFS CMOS space
  used for WD1772 - the effect is that WD1772 will hang more often if they are the
  same set.*/
int romset=2;
int romsavailable[6];

int hardwareblit;
void fdiclose();
int disccint;
BITMAP *b;
int firstfull=1;
int memsize=4096;
int wmx,wmy;
unsigned long memctrl;
char bigs[256];
FILE *olog;
float sampdiff,inssecf;
int output;
int inssec;
int inscount,updatemips;
unsigned long sdif;
int frameco=0;

void updateins()
{
        inssecf=(float)inscount/1000000;
        inscount=0;
        inssec=frameco*2;
        frameco=0;
        updatemips=1;
        framecount2=framecount;
        framecount=0;
}

END_OF_FUNCTION(updateins);

int infocus;
int spdcount=0;
static HANDLE frameevent;
void updatespd()
{
        if (infocus)
        {
                spdcount++;
                SetEvent(frameevent);
        }
}

END_OF_FUNCTION(updatespd);

void installins()
{
        LOCK_FUNCTION(updateins);
        LOCK_VARIABLE(inssec);
        LOCK_VARIABLE(inscount);
        install_int_ex(updateins,MSEC_TO_TIMER(1000));
        inscount=0;
        LOCK_FUNCTION(updatespd);
        LOCK_VARIABLE(spdcount);
        install_int_ex(updatespd,MSEC_TO_TIMER(20));
}

char bigs[256];
FILE *olog;

void loadfile(FILE *f)
{
        int c;
        unsigned char temp;
        c=0x8000;
        while (!feof(f))
        {
                temp=getc(f);
                writememb(c,temp);
                c++;
        }
}

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

FILE *rlog;
void rpclog(char *format, ...)
{
   char buf[1024];
//   return;
   if (!rlog) rlog=fopen("arclog.txt","wt");
//turn;
   va_list ap;
   va_start(ap, format);
   vsprintf(buf, format, ap);
   va_end(ap);
   fputs(buf,rlog);
   fflush(rlog);
}


/*  Make the class name into a global variable  */
char szClassName[ ] = "WindowsApp";
HWND ghwnd;
int quitting=0;
HMENU menu;
int fastvsync=0,limitspeed=0,logsound=0;
int vsyncbreak;
int soundtime;

#define IDM_FILE_RESET    40000
#define IDM_FILE_EXIT     40001
#define IDM_DISC_CHANGE0  40010
#define IDM_DISC_CHANGE1  40011
#define IDM_DISC_CHANGE2  40012
#define IDM_DISC_CHANGE3  40013
#define IDM_DISC_REMOVE0  40014
#define IDM_DISC_REMOVE1  40015
#define IDM_DISC_REMOVE2  40016
#define IDM_DISC_REMOVE3  40017
#define IDM_DISC_FAST     40018
#define IDM_OPTIONS_SOUND 40020
#define IDM_OPTIONS_FAST  40021
#define IDM_OPTIONS_LIMIT 40022
#define IDM_OPTIONS_MOUSE 40023
#define IDM_OPTIONS_STEREO 40024
#define IDM_MEMSIZE_512K  40030
#define IDM_MEMSIZE_1M    40031
#define IDM_MEMSIZE_2M    40032
#define IDM_MEMSIZE_4M    40033
#define IDM_MEMSIZE_8M    40034
#define IDM_MEMSIZE_16M   40035
#define IDM_FDC_WD1772    40040
#define IDM_FDC_82C711    40041
#define IDM_VIDEO_FULLSCR 40050
#define IDM_VIDEO_FULLBOR 40051
#define IDM_VIDEO_DBLSCAN 40052
#define IDM_VIDEO_NOBOR   40053
#define IDM_BLIT_SCAN     40060
#define IDM_BLIT_SCALE    40061
#define IDM_BLIT_HSCALE   40062
#define IDM_CPU_ARM2      40070
#define IDM_CPU_ARM250    40071
#define IDM_CPU_ARM3      40072
#define IDM_CPU_ARM3_33   40073
#define IDM_CPU_FPA       40074
#define IDM_ROM_ARTHUR    40080
#define IDM_ROM_RO2       40081
#define IDM_ROM_RO3_OLD   40082
#define IDM_ROM_RO3_NEW   40083
#define IDM_ROM_TACTIC    40084
#define IDM_ROM_POIZONE   40085
#define IDM_MONITOR_NORMAL 40090
#define IDM_MONITOR_HIRES  40091

void makemenu()
{
        HMENU hpop,hpop2;
        menu=CreateMenu();
        hpop=CreateMenu();
        AppendMenu(hpop,MF_STRING,IDM_FILE_RESET,"&Reset");
        AppendMenu(hpop,MF_SEPARATOR,0,NULL);
        AppendMenu(hpop,MF_STRING,IDM_FILE_EXIT,"E&xit");
        AppendMenu(menu,MF_POPUP,hpop,"&File");
        hpop=CreateMenu();
        hpop2=CreateMenu();
        AppendMenu(hpop2,MF_STRING,IDM_DISC_CHANGE0,"Drive &0...");
        AppendMenu(hpop2,MF_STRING,IDM_DISC_CHANGE1,"Drive &1...");
        AppendMenu(hpop2,MF_STRING,IDM_DISC_CHANGE2,"Drive &2...");
        AppendMenu(hpop2,MF_STRING,IDM_DISC_CHANGE3,"Drive &3...");
        AppendMenu(hpop,MF_POPUP,hpop2,"&Change Disc");
        hpop2=CreateMenu();
        AppendMenu(hpop2,MF_STRING,IDM_DISC_REMOVE0,"Drive &0...");
        AppendMenu(hpop2,MF_STRING,IDM_DISC_REMOVE1,"Drive &1...");
        AppendMenu(hpop2,MF_STRING,IDM_DISC_REMOVE2,"Drive &2...");
        AppendMenu(hpop2,MF_STRING,IDM_DISC_REMOVE3,"Drive &3...");
        AppendMenu(hpop,MF_POPUP,hpop2,"&Remove Disc");
        AppendMenu(hpop,MF_STRING,IDM_DISC_FAST,"F&ast Disc Access");
        AppendMenu(menu,MF_POPUP,hpop,"&Disc");
        hpop=CreateMenu();
        AppendMenu(hpop,MF_STRING,IDM_OPTIONS_SOUND,"&Sound Enable");
        AppendMenu(hpop,MF_STRING,IDM_OPTIONS_STEREO,"S&tereo Sound");
        AppendMenu(hpop,MF_STRING,IDM_OPTIONS_LIMIT,"&Limit Speed");
        AppendMenu(menu,MF_POPUP,hpop,"&Options");
        hpop=CreateMenu();
        hpop2=CreateMenu();
        AppendMenu(hpop2,MF_STRING,IDM_CPU_ARM2,  "ARM&2");
        AppendMenu(hpop2,MF_STRING,IDM_CPU_ARM250,"ARM2&50");
        AppendMenu(hpop2,MF_STRING,IDM_CPU_ARM3,  "ARM&3 25mhz");
        AppendMenu(hpop2,MF_STRING,IDM_CPU_ARM3_33,"ARM3 33mhz");
//        AppendMenu(hpop2,MF_STRING,IDM_CPU_FPA,    "&FPA");
        AppendMenu(hpop,MF_POPUP,hpop2,"&CPU type");
        hpop2=CreateMenu();
        AppendMenu(hpop2,MF_STRING,IDM_MEMSIZE_512K,"&512 kilobytes");
        AppendMenu(hpop2,MF_STRING,IDM_MEMSIZE_1M,"&1 megabyte");
        AppendMenu(hpop2,MF_STRING,IDM_MEMSIZE_2M,"&2 megabytes");
        AppendMenu(hpop2,MF_STRING,IDM_MEMSIZE_4M,"&4 megabytes");
        AppendMenu(hpop2,MF_STRING,IDM_MEMSIZE_8M,"&8 megabytes");
        AppendMenu(hpop2,MF_STRING,IDM_MEMSIZE_16M, "1&6 megabytes");
        AppendMenu(hpop,MF_POPUP,hpop2,"&RAM size");
        hpop2=CreateMenu();
        AppendMenu(hpop2,MF_STRING,IDM_ROM_ARTHUR, "&Arthur");
        AppendMenu(hpop2,MF_STRING,IDM_ROM_RO2,    "RISC OS &2");
        AppendMenu(hpop2,MF_STRING,IDM_ROM_RO3_OLD,"RISC OS 3 (&old FDC)");
        AppendMenu(hpop2,MF_STRING,IDM_ROM_RO3_NEW,"RISC OS 3 (&new FDC)");
        AppendMenu(hpop2,MF_STRING,IDM_ROM_TACTIC,"Ertictac/Tactic");
        AppendMenu(hpop2,MF_STRING,IDM_ROM_POIZONE,"Poizone");
        AppendMenu(hpop,MF_POPUP,hpop2,"&Operating System");
        hpop2=CreateMenu();
        AppendMenu(hpop2,MF_STRING,IDM_MONITOR_NORMAL, "&Normal");
        AppendMenu(hpop2,MF_STRING,IDM_MONITOR_HIRES,  "&Hi-res Mono");
        AppendMenu(hpop,MF_POPUP,hpop2,"&Monitor");
        AppendMenu(menu,MF_POPUP,hpop,"&Machine");
        hpop=CreateMenu();
        AppendMenu(hpop,MF_STRING,IDM_VIDEO_FULLSCR,"&Fullscreen");
        AppendMenu(hpop,MF_STRING,IDM_VIDEO_FULLBOR,"Full &borders");
        AppendMenu(hpop,MF_STRING,IDM_VIDEO_NOBOR,"&No borders");
        hpop2=CreateMenu();
        AppendMenu(hpop2,MF_STRING,IDM_BLIT_SCAN,"&Scanlines");
        AppendMenu(hpop2,MF_STRING,IDM_BLIT_SCALE,"&Line doubling");
//        AppendMenu(hpop2,MF_STRING,IDM_BLIT_HSCALE,"&Hardware scale");
        AppendMenu(hpop,MF_POPUP,hpop2,"&Blit method");
        AppendMenu(menu,MF_POPUP,hpop,"&Video");
}

void updatewindowsize(int x, int y)
{
        RECT r;
//        rpclog("Window size now %i %i\n",x,y);
        GetWindowRect(ghwnd,&r);
        MoveWindow(ghwnd,r.left,r.top,
                     x+(GetSystemMetrics(SM_CXFIXEDFRAME)*2),
                     y+(GetSystemMetrics(SM_CYFIXEDFRAME)*2)+GetSystemMetrics(SM_CYMENUSIZE)+GetSystemMetrics(SM_CYCAPTION)+2,
                     TRUE);
}

void clearmemmenu()
{
        CheckMenuItem(menu,IDM_MEMSIZE_512K,MF_UNCHECKED);
        CheckMenuItem(menu,IDM_MEMSIZE_1M,  MF_UNCHECKED);
        CheckMenuItem(menu,IDM_MEMSIZE_2M,  MF_UNCHECKED);
        CheckMenuItem(menu,IDM_MEMSIZE_4M,  MF_UNCHECKED);
        CheckMenuItem(menu,IDM_MEMSIZE_8M,  MF_UNCHECKED);
        CheckMenuItem(menu,IDM_MEMSIZE_16M, MF_UNCHECKED);
}

int deskdepth;

int mousecapture=0;
RECT oldclip,arcclip;

void loadconfig()
{
        char *p;
        p=get_config_string(NULL,"limit_speed",NULL);
        if (!p || strcmp(p,"0")) limitspeed=1;
        else                     limitspeed=0;
        p=get_config_string(NULL,"sound_enable",NULL);
        if (!p || strcmp(p,"0")) soundena=1;
        else                     soundena=0;
        p=get_config_string(NULL,"full_borders",NULL);
        if (!p || strcmp(p,"1")) fullborders=0;
        else                     fullborders=1;
        noborders=get_config_int(NULL,"no_borders",0);
        p=get_config_string(NULL,"cpu_type",NULL);
        if (!p || !strcmp(p,"0")) arm3=0;
        else if (!strcmp(p,"2"))  arm3=2;
        else if (!strcmp(p,"3"))  arm3=3;
        else                      arm3=1;
        p=get_config_string(NULL,"fpa",NULL);
        if (!p || strcmp(p,"1")) fpaena=0;
        else                     fpaena=1;
        p=get_config_string(NULL,"hires",NULL);
        if (!p || strcmp(p,"1")) hires=0;
        else                     hires=1;
        p=get_config_string(NULL,"first_fullscreen",NULL);
        if (!p || strcmp(p,"0")) firstfull=1;
        else                     firstfull=0;
        p=get_config_string(NULL,"double_scan",NULL);
        if (!p || strcmp(p,"0")) dblscan=1;
        else                     dblscan=0;
        p=get_config_string(NULL,"hardware_blit",NULL);
        if (!p || strcmp(p,"0")) hardwareblit=1;
        else                     hardwareblit=0;
        p=get_config_string(NULL,"fast_disc",NULL);
        if (!p || strcmp(p,"0")) fastdisc=1;
        else                     fastdisc=0;
        p=get_config_string(NULL,"fdc_type",NULL);
        if (!p || strcmp(p,"0")) fdctype=1;
        else                     fdctype=0;
        p=get_config_string(NULL,"stereo",NULL);
        if (!p || strcmp(p,"0")) stereo=1;
        else                     stereo=0;
}

void saveconfig()
{
        char s[80];
        set_config_string(NULL,"disc_name_0",discname[0]);
        set_config_string(NULL,"disc_name_1",discname[1]);
        set_config_string(NULL,"disc_name_2",discname[2]);
        set_config_string(NULL,"disc_name_3",discname[3]);
        sprintf(s,"%i",limitspeed);
        set_config_string(NULL,"limit_speed",s);
        sprintf(s,"%i",soundena);
        set_config_string(NULL,"sound_enable",s);
        sprintf(s,"%i",memsize);
        set_config_string(NULL,"mem_size",s);
        sprintf(s,"%i",arm3);
        set_config_string(NULL,"cpu_type",s);
        sprintf(s,"%i",fpaena);
        set_config_string(NULL,"fpa",s);
        sprintf(s,"%i",hires);
        set_config_string(NULL,"hires",s);
        sprintf(s,"%i",fullborders);
        set_config_string(NULL,"full_borders",s);
        sprintf(s,"%i",firstfull);
        set_config_string(NULL,"first_fullscreen",s);
        sprintf(s,"%i",dblscan);
        set_config_string(NULL,"double_scan",s);
        sprintf(s,"%i",hardwareblit);
        set_config_string(NULL,"hardware_blit",s);
        sprintf(s,"%i",fastdisc);
        set_config_string(NULL,"fast_disc",s);
        sprintf(s,"%i",fdctype);
        set_config_string(NULL,"fdc_type",s);
        sprintf(s,"%i",romset);
        set_config_string(NULL,"rom_set",s);
        sprintf(s,"%i",stereo);
        set_config_string(NULL,"stereo",s);
        set_config_int(NULL,"no_borders",noborders);
}

/*Preference order : ROS3, ROS2, Arthur, Poizone, Erotactic/Tictac
  ROS3 with WD1772 is not considered, if ROS3 is available run with 82c711 as
  this is of more use to most users*/
int rompreffered[5]={3,1,0,5,4};

/*Establish which ROMs are available*/
void establishromavailability()
{
        int d=0;
        int c=romset;
        for (romset=0;romset<6;romset++)
        {
                romsavailable[romset]=!loadrom();
//                rpclog("romset %i %s\n",romset,(romsavailable[romset])?"available":"not available");
        }
        romset=c;
        for (c=0;c<6;c++)
            d|=romsavailable[c];
        if (!d)
        {
                error("No ROMs are present!");
                exit(-1);
        }
        if (romsavailable[romset])
        {
                loadrom();
                return;
        }
        for (c=0;c<5;c++)
        {
                romset=rompreffered[c];
                if (!loadrom())
                   return;
        }
        error("No ROM sets available!");
        exit(-1);
}

int framenum=0;

int quited=0;
int soundrunning=0;
static HANDLE soundobject;
void soundthread(PVOID pvoid)
{
//        rpclog("Started soundthread\n");
        soundrunning=1;
        while (!quited)
        {
                WaitForSingleObject(soundobject,INFINITE);
                if (quited) break;
                mixsound();
//                sleep(1);
        }
        soundrunning=0;
}

void wakeupsoundthread()
{
//        rpclog("Wakeupsoundthread\n");
        SetEvent(soundobject);
}


void closesoundthread()
{
        quited=1;
        wakeupsoundthread();
        while (soundrunning)
        {
                Sleep(1);
        }
}


int WINAPI WinMain (HINSTANCE hThisInstance,
                    HINSTANCE hPrevInstance,
                    LPSTR lpszArgument,
                    int nFunsterStil)

{
        int c;
        char *p,s[80];
        char fn[512];
        /* This is the handle for our window */
        MSG messages;            /* Here messages to the application are saved */
        WNDCLASSEX wincl;        /* Data structure for the windowclass */

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

        makemenu();
        /* The class is registered, let's create the program*/
        ghwnd = CreateWindowEx (
           0,                   /* Extended possibilites for variation */
           szClassName,         /* Classname */
           "Arculator v0.99",    /* Title Text */
           WS_OVERLAPPEDWINDOW&~(WS_MAXIMIZEBOX|WS_SIZEBOX), /* default window */
           CW_USEDEFAULT,       /* Windows decides the position */
           CW_USEDEFAULT,       /* where the window ends up on the screen */
           672+(GetSystemMetrics(SM_CXFIXEDFRAME)*2),/* The programs width */
           544+(GetSystemMetrics(SM_CYFIXEDFRAME)*2)+GetSystemMetrics(SM_CYMENUSIZE)+GetSystemMetrics(SM_CYCAPTION)+2, /* and height in pixels */
           HWND_DESKTOP,        /* The window is a child-window to desktop */
           menu,                /* No menu */
           hThisInstance,       /* Program Instance handler */
           NULL                 /* No Window Creation data */
           );

        /* Make the window visible on the screen */
        ShowWindow (ghwnd, nFunsterStil);
        win_set_window(ghwnd);
        
        initalmain(NULL,0);

        initvid();
        get_executable_name(exname,511);
        p=get_filename(exname);
        *p=0;
        append_filename(fn,exname,"arc.cfg",511);
        set_config_file(fn);
        atexit(fdiclose);
        initarculfs();
        resetide();
        for (c=0;c<4;c++)
        {
                sprintf(s,"disc_name_%i",c);
                p=get_config_string(NULL,s,NULL);
                if (!p) discchangeint(0,c);
                else
                {
                        strcpy(discname[c],p);
                        p=get_extension(discname[c]);
                        if (p[0]=='i' || p[0]=='I')
                           loaddisc(discname[c],1,c);
                        else if (p[0]=='f' || p[0]=='F')
                           fdiload(discname[c],c,0);
                        else if ((p[0]=='a' || p[0]=='A') && (p[1]=='p' || p[1]=='P') && (p[2]=='d' || p[2]=='D'))
                           fdiload(discname[c],c,1);
                        else
                           loaddisc(discname[c],0,c);
                }
        }
        p=get_config_string(NULL,"mem_size",NULL);
        if (!p || !strcmp(p,"4096")) memsize=4096;
        else if (!strcmp(p,"8192"))  memsize=8192;
        else if (!strcmp(p,"2048"))  memsize=2048;
        else if (!strcmp(p,"1024"))  memsize=1024;
        else if (!strcmp(p,"512"))   memsize=512;
        else                         memsize=16384;
        initmem(memsize);

        p=get_config_string(NULL,"rom_set",NULL);
        if (!p || !strcmp(p,"3")) romset=3;
        else if (!strcmp(p,"1"))  romset=1;
        else if (!strcmp(p,"2"))  romset=2;
        else if (!strcmp(p,"4"))  romset=4;
        else if (!strcmp(p,"5"))  romset=5;
        else                      romset=0;
        establishromavailability();
        SendMessage(ghwnd,WM_USER,0,0);
//        error("Firstinit3");
/*        if (loadrom())
        {
                MessageBox(NULL,"RiscOS ROMs missing!","Arculator",MB_OK|MB_ICONSTOP);
                return 0;
        }*/
//        error("Firstinit");
        initmemc();
        resetarm();
//        error("Firstinit2");
        loadcmos();
        resetioc();
        install_keyboard();
        install_mouse();
        initkeyboard();
        install_timer();
        installins();
        reset82c711();
        resetmouse();
        MoveWindow(ghwnd,100,100,640+(GetSystemMetrics(SM_CXFIXEDFRAME)*2),512+(GetSystemMetrics(SM_CYFIXEDFRAME)*2)+GetSystemMetrics(SM_CYMENUSIZE)+GetSystemMetrics(SM_CYCAPTION)+1,TRUE);
        loadconfig();
        fullscreen=0;
        mousehack=0;
limitspeed=1;
        reinitvideo();
        initsound();
        initjoy();

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
if (romset==3) fdctype=1;
else		fdctype=0;
        if (fdctype) CheckMenuItem(menu,IDM_FDC_82C711,MF_CHECKED);
        else         CheckMenuItem(menu,IDM_FDC_WD1772,MF_CHECKED);
                        if (fdctype) init82c711();
                        else         init1772();
        if (arm3==0)      CheckMenuItem(menu,IDM_CPU_ARM2,MF_CHECKED);
        else if (arm3==1) CheckMenuItem(menu,IDM_CPU_ARM3,MF_CHECKED);
        else if (arm3==3) CheckMenuItem(menu,IDM_CPU_ARM3_33,MF_CHECKED);
        else              CheckMenuItem(menu,IDM_CPU_ARM250,MF_CHECKED);
        if (fpaena) CheckMenuItem(menu,IDM_CPU_FPA,MF_CHECKED);
//        if (hardwareblit) CheckMenuItem(menu,IDM_BLIT_HSCALE,MF_CHECKED);
        if (dblscan) CheckMenuItem(menu,IDM_BLIT_SCALE,MF_CHECKED);
        else         CheckMenuItem(menu,IDM_BLIT_SCAN,MF_CHECKED);
        if (hires) CheckMenuItem(menu,IDM_MONITOR_HIRES,MF_CHECKED);
        else       CheckMenuItem(menu,IDM_MONITOR_NORMAL,MF_CHECKED);

        CheckMenuItem(menu,IDM_ROM_ARTHUR+romset, MF_CHECKED);

        if (arm3) speed=1;
//        timeBeginPeriod(1);
        resetst506();
        resetics();
        spdcount=0;
        SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_ABOVE_NORMAL);
        frameevent=CreateEvent(NULL, FALSE, FALSE, NULL);
//        _beginthread(soundthread,0,NULL);
        soundobject=CreateEvent(NULL, FALSE, FALSE, NULL);
//        atexit(closesoundthread);
//        SetThreadPriority(soundthread, THREAD_PRIORITY_HIGHEST);
//        error("Inited\n");
        /* Run the message loop. It will run until GetMessage() returns 0 */
        while (!quited)
        {
                /*
                if (framenum==5)
                {
//                        spdcount=0;
                        framenum=0;
                        mixsound();
                }
                */
                if (!infocus) Sleep(100);
                if (infocus && (!limitspeed || spdcount))
                {
//                        if (spdcount>1) spdcount=1;
                        if (!speed)        execarm(160000);
                        else if (speed==1) execarm(240000);
                        else if (speed==2) execarm(480000);
                        else               execarm(640000);
                        cmostick();
                        spdcount--;
                        if (limitspeed && !spdcount)
                        {
                                WaitForSingleObject(frameevent,100);
                        }
                                if (updatemips)
                                {
                                        sprintf(s,"Arculator v0.99 - %i%% - %s",inssec,(mousecapture)?"Press CTRL-END to release mouse":"Click to capture mouse");
                                        if (!fullscreen) SetWindowText(ghwnd, s);
                                        updatemips=0;
                                }
                        framenum++;
                        frameco++;
                        if (framenum==5)
                        {
//                        spdcount=0;
                                framenum=0;
//                                mixsound();
                        }
                        polljoy();
                }
                if (updatemips && !limitspeed)
                {
                        sprintf(s,"Arculator v0.99 - %i%% - %s",inssec,(mousecapture)?"Press CTRL-END to release mouse":"Click to capture mouse");
                        if (!fullscreen) SetWindowText(ghwnd, s);
                        spdcount=0;
                        updatemips=0;
                }
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
                if (PeekMessage(&messages,NULL,0,0,PM_REMOVE))
                {
                        if (messages.message==WM_QUIT)
                           quited=1;
                        /* Translate virtual-key messages into character messages */
                        TranslateMessage(&messages);
                        /* Send message to WindowProcedure */
                        DispatchMessage(&messages);
                }
//                while (limitspeed && !spdcount && infocus)
//                      sleep(0);
        }
        dumpregs();
        closesoundthread();
//        timeEndPeriod(1);
        savecmos();
        saveconfig();
        updatedisc(discname[0],0);
        updatedisc(discname[1],1);
        updatedisc(discname[2],2);
        updatedisc(discname[3],3);
        closevideo();
        if (mousecapture) ClipCursor(&oldclip);
        return 0;
}

void changedisc(HWND hwnd, int drive)
{
        char *p;
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
	ofn.lpstrFilter = "All disc images\0*.adf;*.img;*.fdi;*.apd\0FDI Disc Image\0*.fdi\0APD Disc Image\0*.apd\0ADFS Disc Image\0*.adf\0DOS Disc Image\0*.img\0All Files\0*.*\0";
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
                updatedisc(discname[drive],drive);
                strcpy(discname[drive],fn);
                p=get_extension(fn);
//                rpclog("Extension %s",p);
                if (p[0]=='i' || p[0]=='I')
                   loaddisc(fn,1,drive);
                else if (p[0]=='f' || p[0]=='F')
                   fdiload(fn,drive,0);
                else if ((p[0]=='a' || p[0]=='A') && (p[1]=='p' || p[1]=='P') && (p[2]=='d' || p[2]=='D'))
                   fdiload(fn,drive,1);
                else
                   loaddisc(fn,0,drive);
                discchangeint(1,drive);
        }
//                resetsound();
//                framenum=0;
}

/*  This function is called by the Windows function DispatchMessage()  */
LRESULT CALLBACK WindowProcedure (HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
        HMENU hmenu;
        MENUITEMINFO mii;
        int c;
        switch (message)                  /* handle the messages */
        {
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
                        spdcount=0;
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
                        discchangeint(0,0);
                        updatedisc(discname[0],0);
                        discname[0][0]=0;
                        return 0;
                        case IDM_DISC_REMOVE1:
                        discchangeint(0,1);
                        updatedisc(discname[1],1);
                        discname[1][0]=0;
                        return 0;
                        case IDM_DISC_REMOVE2:
                        discchangeint(0,2);
                        updatedisc(discname[2],2);
                        discname[2][0]=0;
                        return 0;
                        case IDM_DISC_REMOVE3:
                        discchangeint(0,3);
                        updatedisc(discname[3],3);
                        discname[3][0]=0;
                        return 0;
                        case IDM_DISC_FAST:
                        fastdisc^=1;
                        if (fastdisc) CheckMenuItem(hmenu,IDM_DISC_FAST,MF_CHECKED);
                        else          CheckMenuItem(hmenu,IDM_DISC_FAST,MF_UNCHECKED);
                        return 0;
                        case IDM_FILE_RESET:
                        resetarm();
                        memset(ram,0,memsize*1024);
                        resetmouse();
                        initkeyboard();
                        return 0;
                        case IDM_FILE_EXIT:
                        PostQuitMessage(0);
                        return 0;
                        case IDM_OPTIONS_SOUND:
                        soundena^=1;
                        if (soundena)
                        {
                                CheckMenuItem(hmenu,IDM_OPTIONS_SOUND,MF_CHECKED);
                                initsound();
                                if (!soundena) CheckMenuItem(hmenu,IDM_OPTIONS_SOUND,MF_UNCHECKED);
//                resetsound();
//                framenum=0;
                        }
                        else
                        {
                                CheckMenuItem(hmenu,IDM_OPTIONS_SOUND,MF_UNCHECKED);
                                deinitsound();
                        }
                        return 0;
                        case IDM_OPTIONS_STEREO:
                        stereo^=1;
                        if (stereo) CheckMenuItem(hmenu,IDM_OPTIONS_STEREO,MF_CHECKED);
                        else        CheckMenuItem(hmenu,IDM_OPTIONS_STEREO,MF_UNCHECKED);
//                resetsound();
//                framenum=0;
                        return 0;
                        case IDM_OPTIONS_LIMIT:
                        limitspeed^=1;
                        if (limitspeed) CheckMenuItem(hmenu,IDM_OPTIONS_LIMIT,MF_CHECKED);
                        else            CheckMenuItem(hmenu,IDM_OPTIONS_LIMIT,MF_UNCHECKED);
                        return 0;
                        case IDM_OPTIONS_MOUSE:
                        mousehack^=1;
                        if (mousehack) CheckMenuItem(hmenu,IDM_OPTIONS_MOUSE,MF_CHECKED);
                        else           CheckMenuItem(hmenu,IDM_OPTIONS_MOUSE,MF_UNCHECKED);
                        return 0;
                        case IDM_MEMSIZE_512K:
                        resizemem(512);
                        resetarm();
                        resetmouse();
                        clearmemmenu();
                        CheckMenuItem(hmenu,IDM_MEMSIZE_512K,MF_CHECKED);
                        memsize=512;
//                resetsound();
//                framenum=0;
                        return 0;
                        case IDM_MEMSIZE_1M:
                        resizemem(1024);
                        resetarm();
                        resetmouse();
                        clearmemmenu();
                        CheckMenuItem(hmenu,IDM_MEMSIZE_1M,MF_CHECKED);
                        memsize=1024;
//                resetsound();
//                framenum=0;
                        return 0;
                        case IDM_MEMSIZE_2M:
                        resizemem(2048);
                        resetarm();
                        resetmouse();
                        clearmemmenu();
                        CheckMenuItem(hmenu,IDM_MEMSIZE_2M,MF_CHECKED);
                        memsize=2048;
//                resetsound();
//                framenum=0;
                        return 0;
                        case IDM_MEMSIZE_4M:
                        resizemem(4096);
                        resetarm();
                        resetmouse();
                        clearmemmenu();
                        CheckMenuItem(hmenu,IDM_MEMSIZE_4M,MF_CHECKED);
                        memsize=4096;
//                resetsound();
//                framenum=0;
                        return 0;
                        case IDM_MEMSIZE_8M:
                        resizemem(8192);
                        resetarm();
                        resetmouse();
                        clearmemmenu();
                        CheckMenuItem(hmenu,IDM_MEMSIZE_8M,MF_CHECKED);
                        memsize=8192;
//                resetsound();
//                framenum=0;
                        return 0;
                        case IDM_MEMSIZE_16M:
                        resizemem(16384);
                        resetarm();
                        resetmouse();
                        clearmemmenu();
                        CheckMenuItem(hmenu,IDM_MEMSIZE_16M,MF_CHECKED);
                        memsize=16384;
//                resetsound();
//                framenum=0;
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
//                resetsound();
//                framenum=0;
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
//                resetsound();
//                framenum=0;
                        return 0;
                        case IDM_BLIT_SCAN:
                        dblscan=0;
                        hardwareblit=0;
                        clearbitmap();
                        CheckMenuItem(hmenu,IDM_BLIT_SCAN,  MF_CHECKED);
                        CheckMenuItem(hmenu,IDM_BLIT_SCALE, MF_UNCHECKED);
                        CheckMenuItem(hmenu,IDM_BLIT_HSCALE,MF_UNCHECKED);
//                resetsound();
//                framenum=0;
                        return 0;
                        case IDM_BLIT_SCALE:
                        dblscan=1;
                        hardwareblit=0;
                        clearbitmap();
                        CheckMenuItem(hmenu,IDM_BLIT_SCAN,  MF_UNCHECKED);
                        CheckMenuItem(hmenu,IDM_BLIT_SCALE, MF_CHECKED);
                        CheckMenuItem(hmenu,IDM_BLIT_HSCALE,MF_UNCHECKED);
//                resetsound();
//                framenum=0;
                        return 0;
                        case IDM_BLIT_HSCALE:
                        dblscan=1;
                        hardwareblit=1;
                        clearbitmap();
                        CheckMenuItem(hmenu,IDM_BLIT_SCAN,  MF_UNCHECKED);
                        CheckMenuItem(hmenu,IDM_BLIT_SCALE, MF_UNCHECKED);
                        CheckMenuItem(hmenu,IDM_BLIT_HSCALE,MF_CHECKED);
//                resetsound();
//                framenum=0;
                        return 0;
                        case IDM_CPU_ARM2:
                        speed=arm3=0;
                        resetarm();
                        memset(ram,0,memsize*1024);
                        resetmouse();
                        CheckMenuItem(hmenu,IDM_CPU_ARM2,   MF_CHECKED);
                        CheckMenuItem(hmenu,IDM_CPU_ARM250, MF_UNCHECKED);
                        CheckMenuItem(hmenu,IDM_CPU_ARM3,   MF_UNCHECKED);
                        CheckMenuItem(hmenu,IDM_CPU_ARM3_33,MF_UNCHECKED);
                        resetioc();
                        initkeyboard();
                        spdcount=0;
//                resetsound();
//                framenum=0;
                        return 0;
                        case IDM_CPU_ARM250:
                        speed=1;
                        arm3=2;
                        resetarm();
                        memset(ram,0,memsize*1024);
                        resetmouse();
                        CheckMenuItem(hmenu,IDM_CPU_ARM2,   MF_UNCHECKED);
                        CheckMenuItem(hmenu,IDM_CPU_ARM250, MF_CHECKED);
                        CheckMenuItem(hmenu,IDM_CPU_ARM3,   MF_UNCHECKED);
                        CheckMenuItem(hmenu,IDM_CPU_ARM3_33,MF_UNCHECKED);
                        resetioc();
                        initkeyboard();
                        spdcount=0;
//                resetsound();
//                framenum=0;
                        return 0;
                        case IDM_CPU_ARM3:
                        speed=arm3=1;
                        resetarm();
                        memset(ram,0,memsize*1024);
                        resetmouse();
                        CheckMenuItem(hmenu,IDM_CPU_ARM2,   MF_UNCHECKED);
                        CheckMenuItem(hmenu,IDM_CPU_ARM250, MF_UNCHECKED);
                        CheckMenuItem(hmenu,IDM_CPU_ARM3,   MF_CHECKED);
                        CheckMenuItem(hmenu,IDM_CPU_ARM3_33,MF_UNCHECKED);
                        resetioc();
                        initkeyboard();
                        spdcount=0;
//                resetsound();
//                framenum=0;
                        return 0;
                        case IDM_CPU_ARM3_33:
                        speed=1;
                        arm3=3;
                        resetarm();
                        memset(ram,0,memsize*1024);
                        resetmouse();
                        CheckMenuItem(hmenu,IDM_CPU_ARM2,   MF_UNCHECKED);
                        CheckMenuItem(hmenu,IDM_CPU_ARM250, MF_UNCHECKED);
                        CheckMenuItem(hmenu,IDM_CPU_ARM3,   MF_UNCHECKED);
                        CheckMenuItem(hmenu,IDM_CPU_ARM3_33,MF_CHECKED);
                        resetioc();
                        initkeyboard();
                        spdcount=0;
//                resetsound();
//                framenum=0;
                        return 0;
                        case IDM_CPU_FPA:
                        fpaena^=1;
                        resetarm();
                        memset(ram,0,memsize*1024);
                        resetmouse();
                        CheckMenuItem(hmenu,IDM_CPU_FPA,(fpaena)?MF_CHECKED:MF_UNCHECKED);
                        resetioc();
                        initkeyboard();
                        spdcount=0;
                        return 0;
                        case IDM_ROM_ARTHUR: case IDM_ROM_RO2: case IDM_ROM_RO3_OLD:
                        case IDM_ROM_RO3_NEW: case IDM_ROM_TACTIC: case IDM_ROM_POIZONE:
                        savecmos();
                        romset=LOWORD(wParam)-IDM_ROM_ARTHUR;
                        if (!romsavailable[romset])
                        {
//                                rpclog("romset %i not available!\n");
                                return 0;
                        }
                        fdctype=(LOWORD(wParam)==IDM_ROM_RO3_NEW)?1:0;
                        if (LOWORD(wParam)<IDM_ROM_RO3_OLD && memsize>4096)
                        {
                                memsize=4096; /*Arthur and RO2 only support max of 4mb RAM*/
                                resizemem(4096);
                        }
                        loadrom();
                        loadcmos();
                        resetarm();
                        memset(ram,0,memsize*1024);
                        resetmouse();
                        resetioc();
                        initkeyboard();
                        for (c=0;c<6;c++)
                            CheckMenuItem(hmenu,IDM_ROM_ARTHUR+c, MF_UNCHECKED);
                        CheckMenuItem(hmenu,LOWORD(wParam),MF_CHECKED);
                        if (fdctype) init82c711();
                        else         init1772();
//                resetsound();
//                framenum=0;
                        return 0;
                        case IDM_MONITOR_NORMAL:
                        hires=0;
                        CheckMenuItem(hmenu,IDM_MONITOR_NORMAL,MF_CHECKED);
                        CheckMenuItem(hmenu,IDM_MONITOR_HIRES, MF_UNCHECKED);
                        reinitvideo();
                        if (fullborders) updatewindowsize(800,600);
                        else             updatewindowsize(672,544);
                        return 0;
                        case IDM_MONITOR_HIRES:
                        hires=1;
                        CheckMenuItem(hmenu,IDM_MONITOR_NORMAL,MF_UNCHECKED);
                        CheckMenuItem(hmenu,IDM_MONITOR_HIRES, MF_CHECKED);
                        reinitvideo();
                        return 0;
                }
                break;
                case WM_SETFOCUS:
                infocus=1;
                spdcount=0;
                if (fullscreen) reinitvideo();
//                resetsound();
//                framenum=0;
                break;
                case WM_KILLFOCUS:
                infocus=0;
                spdcount=0;
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
                case WM_DESTROY:
                PostQuitMessage (0);       /* send a WM_QUIT to the message queue */
                break;
                default:                      /* for messages that we don't deal with */
                return DefWindowProc (hwnd, message, wParam, lParam);
        }
        return 0;
}

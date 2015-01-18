#include <stdio.h>
#include <stdlib.h>
#include <allegro.h>
#include "arc.h"

#include "82c711_fdc.h"
#include "arm.h"
#include "disc.h"
#include "disc_adf.h"
#include "disc_apd.h"
#include "disc_fdi.h"
#include "disc_jfd.h"
#include "disc_ssd.h"
#include "keyboard.h"
#include "memc.h"
#include "sound.h"
#include "wd1770.h"

#include "hostfs.h"

#ifdef LINUX
#include "podules-linux.h"
#endif

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
int firstfull=1;
int memsize=4096;
float inssecf;
int inssec;
int inscount,updatemips;
int frameco=0;

int jint,jtotal;

void updateins()
{
        inssecf=(float)inscount/1000000;
        inscount=0;
        inssec=frameco;
        frameco=0;
        jtotal=jint;
        jint=0;
        updatemips=1;
}

END_OF_FUNCTION(updateins);

void installins()
{
        LOCK_FUNCTION(updateins);
        LOCK_VARIABLE(inssec);
        LOCK_VARIABLE(inscount);
        install_int_ex(updateins,MSEC_TO_TIMER(1000));
        inscount=0;
}


FILE *rlog;
void rpclog(char *format, ...)
{
   char buf[1024];
   return;
   if (!rlog) rlog=fopen("arclog.txt","wt");

   va_list ap;
   va_start(ap, format);
   vsprintf(buf, format, ap);
   va_end(ap);
   fputs(buf,rlog);
   fflush(rlog);
}

void fatal(char *format, ...)
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
        dumpregs();
        exit(-1);
}

int limitspeed;

/*Preference order : ROS3, ROS2, Arthur, Poizone, Erotactic/Tictac
  ROS3 with WD1772 is not considered, if ROS3 is available run with 82c711 as
  this is of more use to most users*/
int rompreffered[5]={3,1,0,5,4};

/*Establish which ROMs are available*/
void establishromavailability()
{
        int d=0;
        int c=romset;
        for (romset=0;romset<7;romset++)
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

void arc_init()
{
        char *p;
        char fn[512],s[512];
        int c;
        al_init_main(NULL,0);

        initvid();
        get_executable_name(exname,511);
        p=get_filename(exname);
        *p=0;
        append_filename(fn,exname,"arc.cfg",511);
        set_config_file(fn);
#if 0
        initarculfs();
#endif
        hostfs_init();
        resetide();
        for (c=0;c<4;c++)
        {
                sprintf(s,"disc_name_%i",c);
                p = (char *)get_config_string(NULL,s,NULL);
                if (!p) 
                   ioc_discchange(c);
                else
                   disc_load(c, discname[c]);
        }
        p = (char *)get_config_string(NULL,"mem_size",NULL);
        if (!p || !strcmp(p,"4096")) memsize=4096;
        else if (!strcmp(p,"8192"))  memsize=8192;
        else if (!strcmp(p,"2048"))  memsize=2048;
        else if (!strcmp(p,"1024"))  memsize=1024;
        else if (!strcmp(p,"512"))   memsize=512;
        else                         memsize=16384;
rpclog("mem_size = %i %s cfg %s\n", memsize, p, fn);
        initmem(memsize);
        
        p = (char *)get_config_string(NULL,"rom_set",NULL);
        if (!p || !strcmp(p,"3")) romset=3;
        else if (!strcmp(p,"1"))  romset=1;
        else if (!strcmp(p,"2"))  romset=2;
        else if (!strcmp(p,"4"))  romset=4;
        else if (!strcmp(p,"5"))  romset=5;
        else if (!strcmp(p,"6"))  romset=6;
        else                      romset=0;
        establishromavailability();

        resizemem(memsize);
        
        initmemc();
        resetarm();
        loadcmos();
        ioc_reset();
        install_keyboard();
        install_mouse();
        keyboard_init();
        install_timer();
        installins();
        resetmouse();

        loadconfig();
        fullscreen=0;
        //mousehack=0;
        limitspeed=1;
        reinitvideo();
        if (soundena) 
           al_init();
        initjoy();


        disc_init();
        adf_init();
        apd_init();
        fdi_init();
        jfd_init();
        ssd_init();
        ddnoise_init();

        wd1770_reset();
        c82c711_fdc_reset();

        if (romset==3) fdctype=1;
        else	       fdctype=0;

        arc_set_cpu(arm_cpu_type);

        resetst506();
        resetics();
        
        podules_reset();
        opendlls();
}

int speed_mhz;

void arc_setspeed(int mhz)
{
        rpclog("arc_setspeed : %i MHz\n", mhz);
//        ioc_recalctimers(mhz);
        speed_mhz = mhz;
        disc_poll_time = 2 * mhz;
        sound_poll_time = 4 * mhz;
        keyboard_poll_time = 10000 * mhz;
}

static struct
{
        char name[50];
        int cpu_speed;
        int mem_speed;
        int has_memc1;
        int has_swp;
        int has_cp15;
        int has_fpa;
} arc_cpus[] =
{
        {"ARM2 w/MEMC1",         8,  8, 1, 0, 0, 0},
        {"ARM2",                 8,  8, 0, 0, 0, 0},
        {"ARM250",              12, 12, 0, 1, 0, 0},
        {"ARM3 (25 MHz)",       25, 12, 0, 1, 1, 0},
        {"ARM3 (33 MHz)",       33, 12, 0, 1, 1, 0},
        {"ARM3 (66 MHz)",       66, 12, 0, 1, 1, 0},
        {"ARM3 w/FPA (25 MHz)", 25, 12, 0, 1, 1, 1},
        {"ARM3 w/FPA (33 MHz)", 33, 12, 0, 1, 1, 1},
        {"ARM3 w/FPA (66 MHz)", 66, 12, 0, 1, 1, 1}
};

void arc_set_cpu(int cpu)
{
        rpclog("arc_setcpu : setting CPU to %s\n", arc_cpus[cpu].name);
        arm_cpu_speed = arc_cpus[cpu].cpu_speed;
        arm_mem_speed = arc_cpus[cpu].mem_speed;
        memc_is_memc1 = arc_cpus[cpu].has_memc1;
        arm_has_swp   = arc_cpus[cpu].has_swp;
        arm_has_cp15  = arc_cpus[cpu].has_cp15;
        fpaena        = arc_cpus[cpu].has_fpa;
        arc_setspeed(arm_mem_speed);
}

static int ddnoise_frames = 0;
void arc_run()
{
        execarm((speed_mhz * 1000000) / 100);
        cmostick();
        polljoy();
        poll_mouse();
        if (mousehack) doosmouse();
        frameco++;
        ddnoise_frames++;
        if (ddnoise_frames == 10)
        {
                ddnoise_frames = 0;
                ddnoise_mix();
        }
}

void arc_close()
{
//        output=1;
//        execarm(16000);
//        vidc_dumppal();
        dumpregs();
        savecmos();
        saveconfig();
        disc_close(0);
        disc_close(1);
        disc_close(2);
        disc_close(3);
        rpclog("ddnoise_close\n");
        ddnoise_close();
        rpclog("closevideo\n");
        closevideo();
        rpclog("arc_close done\n");
}

int main(int argc, char *argv[])
{
    arc_init();
    arc_set_cpu(4);
    do {
        arc_run();
    } while (1);
    arc_close();
}


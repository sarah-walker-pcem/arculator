/*Arculator 2.0 by Sarah Walker
  Main init/close/run functions*/
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <sys/time.h>

#include "82c711.h"
#include "82c711_fdc.h"
#include "arc.h"
#include "arm.h"
#include "cmos.h"
#include "config.h"
#include "ddnoise.h"
#include "disc.h"
#include "disc_adf.h"
#include "disc_apd.h"
#include "disc_fdi.h"
#include "disc_jfd.h"
#include "disc_ssd.h"
#include "ds2401.h"
#include "ide.h"
#include "ioc.h"
#include "ioeb.h"
#include "joystick.h"
#include "keyboard.h"
#include "mem.h"
#include "memc.h"
#include "plat_input.h"
#include "plat_joystick.h"
#include "plat_video.h"
#include "podules.h"
#include "sound.h"
#include "soundopenal.h"
#include "st506.h"
#include "timer.h"
#include "vidc.h"
#include "video.h"
#include "video_sdl2.h"
#include "wd1770.h"

#include "hostfs.h"

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

void fdiclose();
int firstfull=1;
int memsize=4096;
static float inssecf;  /*Millions of instructions executed in the last second*/
int inssec;            /*Speed ratio percentage (100% = realtime emulation), updated by updateins()*/
int updatemips;        /*1 if MIPS counter has not been updated since last updateins() call*/
static int frameco=0;  /*Number of 1/100 second executions (arm_run() calls) since last updateins()*/
char exname[512];

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

FILE *rlog = NULL;
void rpclog(const char *format, ...)
{
#ifdef DEBUG_LOG
   char buf[1024];

   if (!rlog)
   {
           rlog=fopen("arclog.txt","wt");
           if (!rlog)
           {
                   perror("fopen");
                   exit(-1);
           }
   }

   va_list ap;
   va_start(ap, format);
   vsprintf(buf, format, ap);
   va_end(ap);

   fprintf(stderr, "[%08i]: %s", (uint32_t)(tsc >> 32), buf);
   fprintf(rlog, "[%08i]: %s", (uint32_t)(tsc >> 32), buf);

   fflush(rlog);
#endif
}

void fatal(const char *format, ...)
{
   char buf[1024];

   if (!rlog) rlog=fopen("arclog.txt","wt");

   va_list ap;
   va_start(ap, format);
   vsprintf(buf, format, ap);
   va_end(ap);
   fputs(buf,rlog);
   fflush(rlog);

   fprintf(stderr, "%s", buf);

   dumpregs();
   exit(-1);
}
void error(const char *format, ...)
{
   char buf[1024];

   if (!rlog) rlog=fopen("arclog.txt","wt");

   va_list ap;
   va_start(ap, format);
   vsprintf(buf, format, ap);
   va_end(ap);
   fputs(buf,rlog);
   fflush(rlog);

   fprintf(stderr, "%s", buf);

   dumpregs();
   exit(-1);
}

void arc_set_cpu(int cpu, int memc);

int arc_init()
{
        char *p;
        char s[512];
        int c;

        loadconfig();
        
        initvid();

        arc_set_cpu(arm_cpu_type, memc_type);
        timer_reset();
#if 0
        initarculfs();
#endif
        hostfs_init();
        initmem(memsize);
        
        if (loadrom())
                return -1;

        resizemem(memsize);
        
        initmemc();
        resetarm();
        cmos_load();
        ioc_reset();
        vidc_reset();
        keyboard_init();
        resetmouse();
        sound_init();

        fullscreen=0;
        //mousehack=0;
        reinitvideo();
        if (soundena) 
           al_init();
//        joystick_init();

        c82c711_init();
        disc_init();
        disc_reset();
        adf_init();
        apd_init();
        fdi_init();
        jfd_init();
        ssd_init();
        ddnoise_init();

        wd1770_reset();
        c82c711_fdc_init();

        for (c=0;c<4;c++)
        {
                sprintf(s,"disc_name_%i",c);
                p = (char *)config_get_string(CFG_MACHINE, NULL,s,NULL);
                if (p) {
                   disc_close(c);
                   strcpy(discname[c], p);
                   disc_load(c, discname[c]);
                }
                ioc_discchange(c);
        }

        if (!fdctype && st506_present)
                st506_internal_init();

        cmos_init();
        ds2401_init();
        podules_init();
        podules_reset();
        joystick_if_init();
        ioeb_init();
        
        return 0;
}

int speed_mhz;

void arc_reset()
{
        arc_set_cpu(arm_cpu_type, memc_type);
        timer_reset();
        st506_internal_close();
        if (cmos_changed)
        {
                cmos_changed = 0;
                cmos_save();
        }
        loadrom();
        cmos_load();
        resizemem(memsize);
        resetarm();
        memset(ram,0,memsize*1024);
        resetmouse();
        ioc_reset();
        vidc_reset();
        keyboard_init();
        disc_reset();
        wd1770_reset();
        c82c711_fdc_init();
        if (!fdctype && st506_present)
                st506_internal_init();
        sound_init();
        cmos_init();
        ds2401_init();
        podules_close();
        podules_init();
        podules_reset();
        joystick_if_init();
        ioeb_init();
}

static struct
{
        char name[50];
        int mem_speed;
        int is_memc1;
} arc_memcs[] =
{
        {"MEMC1",             8, 1},
        {"MEMC1A at 8 MHz",   8, 0},
        {"MEMC1A at 12 MHz", 12, 0},
        {"MEMC1A at 16 MHz", 16, 0}
};

static struct
{
        char name[50];
        int cpu_speed;
        int has_swp;
        int has_cp15;
} arc_cpus[] =
{
        {"ARM2",          0,  0, 0},
        {"ARM250",        0,  1, 0},
        {"ARM3 (20 MHz)", 20, 1, 1},
        {"ARM3 (25 MHz)", 25, 1, 1},
        {"ARM3 (26 MHz)", 26, 1, 1},
        {"ARM3 (30 MHz)", 30, 1, 1},
        {"ARM3 (33 MHz)", 33, 1, 1},
        {"ARM3 (35 MHz)", 35, 1, 1},
};

void arc_set_cpu(int cpu, int memc)
{
        rpclog("arc_setcpu : setting CPU to %s\n", arc_cpus[cpu].name);
        arm_mem_speed = arc_memcs[memc].mem_speed;
        memc_is_memc1 = arc_memcs[memc].is_memc1;
        rpclog("setting memc to %i %i %i\n", memc, memc_is_memc1, arm_mem_speed);
        if (arc_cpus[cpu].cpu_speed)
                arm_cpu_speed = arc_cpus[cpu].cpu_speed;
        else
                arm_cpu_speed = arm_mem_speed;
        arm_has_swp   = arc_cpus[cpu].has_swp;
        arm_has_cp15  = arc_cpus[cpu].has_cp15;
        ref8m_period = (arm_cpu_speed * 1024) / 8;
        speed_mhz = arm_cpu_speed;
        mem_updatetimings();
}

static int ddnoise_frames = 0;
void arc_run()
{
        LOG_EVENT_LOOP("arc_run()\n");
        execarm((speed_mhz * 1000000) / 100);
        joystick_poll_host();
        mouse_poll_host();
        keyboard_poll_host();
        if (mousehack) doosmouse();
        frameco++;
        ddnoise_frames++;
        if (ddnoise_frames == 10)
        {
                ddnoise_frames = 0;
                ddnoise_mix();
        }
        if (cmos_changed)
        {
                cmos_changed--;
                if (!cmos_changed)
                        cmos_save();
        }
        LOG_EVENT_LOOP("END arc_run()\n");
}

void arc_close()
{
//        output=1;
//        execarm(16000);
//        vidc_dumppal();
        dumpregs();
        cmos_save();
        saveconfig();
        podules_close();
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

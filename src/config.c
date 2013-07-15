#include <allegro.h>
#include <stdio.h>
#include "arc.h"
#include "arm.h"

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
        arm_cpu_type = get_config_int(NULL, "cpu_type", 0);
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
        set_config_int(NULL, "cpu_type", arm_cpu_type);
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

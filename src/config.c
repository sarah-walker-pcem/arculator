/*Arculator 2.0 by Sarah Walker
  Configuration system*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "arc.h"
#include "arm.h"
#include "config.h"
#include "disc.h"
#include "fpa.h"
#include "joystick.h"
#include "memc.h"
#include "plat_joystick.h"
#include "plat_video.h"
#include "podules.h"
#include "sound.h"
#include "st506.h"
#include "video.h"

char hd_fn[2][512];
int hd_spt[2], hd_hpc[2], hd_cyl[2];

char machine[7];

int monitor_type;

uint32_t unique_id;

char joystick_if[16];

char *get_filename(char *s)
{
        int c = strlen(s) - 1;
        while (c > 0)
        {
                if (s[c] == '/' || s[c] == '\\')
                   return &s[c+1];
                c--;
        }
        return s;
}

void append_filename(char *dest, const char *s1, const char *s2, int size)
{
        sprintf(dest, "%s%s", s1, s2);
}

void append_slash(char *s, int size)
{
        int c = strlen(s)-1;
        if (s[c] != '/' && s[c] != '\\')
        {
                if (c < size-2)
                        strcat(s, "/");
                else
                        s[c] = '/';
        }
}

void put_backslash(char *s)
{
        int c = strlen(s) - 1;
        if (s[c] != '/' && s[c] != '\\')
        {
                s[c+1] = '/';
                s[c+2] = 0;
        }
}

char *get_extension(char *s)
{
        int c = strlen(s) - 1;

        if (c <= 0)
                return s;
        
        while (c && s[c] != '.')
                c--;
                
        if (!c)
                return &s[strlen(s)];

        return &s[c+1];
} 

char config_file_default[256];
char config_name[256];
char machine_config_name[256];
char machine_config_file[256];

static char config_file[256];

typedef struct list_t
{
        struct list_t *next;
} list_t;

static list_t global_config_head;
static list_t machine_config_head;

typedef struct section_t
{
        struct list_t list;
        
        char name[256];
        
        struct list_t entry_head;
} section_t;

typedef struct entry_t
{
        struct list_t list;
        
        char name[256];
        char data[256];
} entry_t;

#define list_add(new, head)                             \
        {                                               \
                struct list_t *next = head;             \
                                                        \
                while (next->next)                      \
                        next = next->next;              \
                                                        \
                (next)->next = new;                     \
                (new)->next = NULL;                     \
        }

void config_dump(int is_global)
{
        section_t *current_section;
        list_t *head = is_global ? &global_config_head : &machine_config_head;
                
        rpclog("Config data :\n");
        
        current_section = (section_t *)head->next;
        
        while (current_section)
        {
                entry_t *current_entry;
                
                rpclog("[%s]\n", current_section->name);
                
                current_entry = (entry_t *)current_section->entry_head.next;
                
                while (current_entry)
                {
                        rpclog("%s = %s\n", current_entry->name, current_entry->data);

                        current_entry = (entry_t *)current_entry->list.next;
                }

                current_section = (section_t *)current_section->list.next;
        }
}

void config_free(int is_global)
{
        section_t *current_section;
        list_t *head = is_global ? &global_config_head : &machine_config_head;
        current_section = (section_t *)head->next;
        
        while (current_section)
        {
                section_t *next_section = (section_t *)current_section->list.next;
                entry_t *current_entry;
                
                current_entry = (entry_t *)current_section->entry_head.next;
                
                while (current_entry)
                {
                        entry_t *next_entry = (entry_t *)current_entry->list.next;
                        
                        free(current_entry);
                        current_entry = next_entry;
                }

                free(current_section);                
                current_section = next_section;
        }
}

int config_free_section(int is_global, const char *name)
{
        section_t *current_section, *prev_section;
        list_t *head = is_global ? &global_config_head : &machine_config_head;
        current_section = (section_t *)head->next;
        prev_section = 0;

        while (current_section)
        {
                section_t *next_section = (section_t *)current_section->list.next;
                if (!strcmp(current_section->name, name))
                {
                        entry_t *current_entry;

                        current_entry = (entry_t *)current_section->entry_head.next;

                        while (current_entry)
                        {
                                entry_t *next_entry = (entry_t *)current_entry->list.next;

                                free(current_entry);
                                current_entry = next_entry;
                        }

                        free(current_section);
                        if (!prev_section)
                                head->next = (list_t*)next_section;
                        else
                                prev_section->list.next = (list_t*)next_section;
                        return 1;
                }
                prev_section = current_section;
                current_section = next_section;
        }
        return 0;
}

void config_load(int is_global, char *fn)
{
        FILE *f = fopen(fn, "rt");
        section_t *current_section;
        list_t *head = is_global ? &global_config_head : &machine_config_head;
        
        memset(head, 0, sizeof(list_t));

        current_section = (section_t *)malloc(sizeof(section_t));
        memset(current_section, 0, sizeof(section_t));
        list_add(&current_section->list, head);

        if (!f)
        {
                rpclog("failed to open %s\n", fn);
                return;
        }

        while (1)
        {
                int c;
                char buffer[256];

                fgets(buffer, 255, f);
                if (feof(f)) break;
                
                c = 0;
                
                while (buffer[c] == ' ' && buffer[c])
                      c++;

                if (!buffer[c]) continue;
                
                if (buffer[c] == '#') /*Comment*/
                        continue;

                if (buffer[c] == '[') /*Section*/
                {
                        section_t *new_section;
                        char name[256];
                        int d = 0;
                        
                        c++;
                        while (buffer[c] != ']' && buffer[c])
                                name[d++] = buffer[c++];

                        if (buffer[c] != ']')
                                continue;
                        name[d] = 0;
                        
                        new_section = (section_t *)malloc(sizeof(section_t));
                        memset(new_section, 0, sizeof(section_t));
                        strncpy(new_section->name, name, 256);
                        list_add(&new_section->list, head);
                        
                        current_section = new_section;
                        
//                        rpclog("New section : %s %p\n", name, (void *)current_section);
                }
                else
                {
                        entry_t *new_entry;
                        char name[256];
                        int d = 0, data_pos;

                        while (buffer[c] != '=' && buffer[c] != ' ' && buffer[c])
                                name[d++] = buffer[c++];
                
                        if (!buffer[c]) continue;
                        name[d] = 0;

                        while ((buffer[c] == '=' || buffer[c] == ' ') && buffer[c])
                                c++;
                        
                        if (!buffer[c]) continue;
                        
                        data_pos = c;
                        while (buffer[c])
                        {
                                if (buffer[c] == '\n')
                                        buffer[c] = 0;
                                c++;
                        }

                        new_entry = (entry_t *)malloc(sizeof(entry_t));
                        memset(new_entry, 0, sizeof(entry_t));
                        strncpy(new_entry->name, name, 256);
                        strncpy(new_entry->data, &buffer[data_pos], 256);
                        list_add(&new_entry->list, &current_section->entry_head);

//                        rpclog("New data under section [%s] : %s = %s\n", current_section->name, new_entry->name, new_entry->data);
                }
        }
        
        fclose(f);
}



void config_new()
{
        FILE *f = fopen(config_file, "wt");
        fclose(f);
}

static section_t *find_section(const char *name, int is_global)
{
        section_t *current_section;
        char blank[] = "";
        list_t *head = is_global ? &global_config_head : &machine_config_head;

        current_section = (section_t *)head->next;
        if (!name)
                name = blank;

        while (current_section)
        {
                if (!strncmp(current_section->name, name, 256))
                        return current_section;
                
                current_section = (section_t *)current_section->list.next;
        }
        return NULL;
}

static entry_t *find_entry(section_t *section, const char *name)
{
        entry_t *current_entry;
        
        current_entry = (entry_t *)section->entry_head.next;

        while (current_entry)
        {
                if (!strncmp(current_entry->name, name, 256))
                        return current_entry;

                current_entry = (entry_t *)current_entry->list.next;
        }
        return NULL;
}

static section_t *create_section(const char *name, int is_global)
{
        section_t *new_section = (section_t *)malloc(sizeof(section_t));
        list_t *head = is_global ? &global_config_head : &machine_config_head;
        
        memset(new_section, 0, sizeof(section_t));
        strncpy(new_section->name, name, 256);
        list_add(&new_section->list, head);
        
        return new_section;
}

static entry_t *create_entry(section_t *section, const char *name)
{
        entry_t *new_entry = (entry_t *)malloc(sizeof(entry_t));
        memset(new_entry, 0, sizeof(entry_t));
        strncpy(new_entry->name, name, 256);
        list_add(&new_entry->list, &section->entry_head);
        
        return new_entry;
}
        
int config_get_int(int is_global, const char *head, const char *name, int def)
{
        section_t *section;
        entry_t *entry;
        int value;

        section = find_section(head, is_global);
        
        if (!section)
                return def;
                
        entry = find_entry(section, name);

        if (!entry)
                return def;
        
        sscanf(entry->data, "%i", &value);
        
        return value;
}

float config_get_float(int is_global, const char *head, const char *name, float def)
{
        section_t *section;
        entry_t *entry;
        float value;

        section = find_section(head, is_global);

        if (!section)
                return def;

        entry = find_entry(section, name);

        if (!entry)
                return def;

        sscanf(entry->data, "%f", &value);

        return value;
}

const char *config_get_string(int is_global, const char *head, const char *name, const char *def)
{
        section_t *section;
        entry_t *entry;

        section = find_section(head, is_global);
        
        if (!section)
                return def;
                
        entry = find_entry(section, name);

        if (!entry)
                return def;
       
        return entry->data; 
}

void config_set_int(int is_global, const char *head, const char *name, int val)
{
        section_t *section;
        entry_t *entry;

        section = find_section(head, is_global);
        
        if (!section)
                section = create_section(head, is_global);
                
        entry = find_entry(section, name);

        if (!entry)
                entry = create_entry(section, name);

        sprintf(entry->data, "%i", val);
}

void config_set_float(int is_global, const char *head, const char *name, float val)
{
        section_t *section;
        entry_t *entry;

        section = find_section(head, is_global);

        if (!section)
                section = create_section(head, is_global);

        entry = find_entry(section, name);

        if (!entry)
                entry = create_entry(section, name);

        sprintf(entry->data, "%f", val);
}

void config_set_string(int is_global, const char *head, const char *name, char *val)
{
        section_t *section;
        entry_t *entry;

        section = find_section(head, is_global);
        
        if (!section)
                section = create_section(head, is_global);
                
        entry = find_entry(section, name);

        if (!entry)
                entry = create_entry(section, name);

        strncpy(entry->data, val, 256);
}

void config_save(int is_global, char *fn)
{
        FILE *f = fopen(fn, "wt");
        section_t *current_section;
        list_t *head = is_global ? &global_config_head : &machine_config_head;
                
        current_section = (section_t *)head->next;
        
        while (current_section)
        {
                entry_t *current_entry;
                
                if (current_section->name[0])
                        fprintf(f, "\n[%s]\n", current_section->name);
                
                current_entry = (entry_t *)current_section->entry_head.next;
                
                while (current_entry)
                {
                        fprintf(f, "%s = %s\n", current_entry->name, current_entry->data);

                        current_entry = (entry_t *)current_entry->list.next;
                }

                current_section = (section_t *)current_section->list.next;
        }
        
        fclose(f);
}

static struct
{
        int romset;
        char *config_name;
        char *cmos_name;
} romset_lookup[] =
{
        {ROM_ARTHUR_030, "arthur030", "arthur"},
        {ROM_ARTHUR_120, "arthur120", "arthur"},
        {ROM_RISCOS_200, "riscos200", "riscos2"},
        {ROM_RISCOS_201, "riscos201", "riscos2"},
        {ROM_RISCOS_300, "riscos300", "riscos3"},
        {ROM_RISCOS_310, "riscos310", "riscos3"},
        {ROM_RISCOS_311, "riscos311", "riscos3"},
        {ROM_RISCOS_319, "riscos319", "riscos3"}
};

static int get_romset(char *name)
{
        int c;
        
        for (c = 0; c < nr_elems(romset_lookup); c++)
        {
                if (!strcmp(name, romset_lookup[c].config_name))
                        return romset_lookup[c].romset;
        }
        
        return 0;
}

char *config_get_romset_name(int romset)
{
        int c;

        for (c = 0; c < nr_elems(romset_lookup); c++)
        {
                if (romset == romset_lookup[c].romset)
                        return romset_lookup[c].config_name;
        }

        return "riscos311";
}

static char cmos_name[80];
char *config_get_cmos_name(int romset, int fdctype)
{
        int c;

        for (c = 0; c < nr_elems(romset_lookup); c++)
        {
                if (romset == romset_lookup[c].romset)
                {
                        if (romset >= ROM_RISCOS_300 && romset <= ROM_RISCOS_319)
                        {
                                snprintf(cmos_name, sizeof(cmos_name), "%s_%s",
                                                romset_lookup[c].cmos_name, (fdctype == FDC_WD1770) ? "old" : "new");
                                                
                                return cmos_name;
                        }
                        else
                                return romset_lookup[c].cmos_name;
                }
        }
        
        return "riscos3";
}

static struct
{
        int monitor_type;
        char *config_name;
} monitor_type_lookup[] =
{
        {MONITOR_STANDARD,  "standard"},
        {MONITOR_MULTISYNC, "multisync"},
        {MONITOR_VGA,       "vga"},
        {MONITOR_MONO,      "mono"}
};

static int get_monitor_type(char *name)
{
        int c;

        for (c = 0; c < nr_elems(monitor_type_lookup); c++)
        {
                if (!strcmp(name, monitor_type_lookup[c].config_name))
                        return monitor_type_lookup[c].monitor_type;
        }

        return 0;
}

static char *get_monitor_type_name(int monitor_type)
{
        int c;

        for (c = 0; c < nr_elems(monitor_type_lookup); c++)
        {
                if (monitor_type == monitor_type_lookup[c].monitor_type)
                        return monitor_type_lookup[c].config_name;
        }

        return "multisync";
}

void loadconfig()
{
        char config_file[512];
        char *p;
        int c;

        append_filename(config_file, exname, "arc.cfg", 511);
        rpclog("config_file=%s\n", config_file);
        config_load(CFG_GLOBAL, config_file);
        config_dump(CFG_GLOBAL);
        config_load(CFG_MACHINE, machine_config_file);
        config_dump(CFG_MACHINE);

        p = (char *)config_get_string(CFG_MACHINE, NULL, "machine", "a3000");
        if (p)
                strcpy(machine, p);
        else
                machine[0] = 0;
        soundena = config_get_int(CFG_GLOBAL, NULL, "sound_enable", 1);
        display_mode = config_get_int(CFG_MACHINE, NULL, "display_mode", DISPLAY_MODE_NO_BORDERS);
        arm_cpu_type = config_get_int(CFG_MACHINE, NULL, "cpu_type", 0);
        memc_type = config_get_int(CFG_MACHINE, NULL, "memc_type", 0);
        fpaena = config_get_int(CFG_MACHINE, NULL, "fpa", 0);
        fpu_type = config_get_int(CFG_MACHINE, NULL, "fpu_type", 0);
        firstfull = config_get_int(CFG_GLOBAL, NULL, "first_fullscreen", 1);
        dblscan = config_get_int(CFG_MACHINE, NULL, "double_scan", 1);
        video_scale = config_get_int(CFG_MACHINE, NULL, "video_scale", 1);
        video_fullscreen_scale = config_get_int(CFG_MACHINE, NULL, "video_fullscreen_scale", FULLSCR_SCALE_FULL);
        video_linear_filtering = config_get_int(CFG_MACHINE, NULL, "video_linear_filtering", 0);
        fdctype = config_get_int(CFG_MACHINE, NULL, "fdc_type", 1);
        st506_present = config_get_int(CFG_MACHINE, NULL, "st506_present", 0);
        stereo = config_get_int(CFG_GLOBAL, NULL, "stereo", 1);
        sound_gain = config_get_int(CFG_GLOBAL, NULL, "sound_gain", 0);
        sound_filter = config_get_int(CFG_GLOBAL, NULL, "sound_filter", 0);
        disc_noise_gain = config_get_int(CFG_GLOBAL, NULL, "disc_noise_gain", 0);
        unique_id = config_get_int(CFG_MACHINE, NULL, "unique_id", 0);
        memsize = config_get_int(CFG_MACHINE, NULL, "mem_size", 4096);
        p = (char *)config_get_string(CFG_MACHINE, NULL, "rom_set", "riscos311");
        romset = get_romset(p);
        p = (char *)config_get_string(CFG_MACHINE, NULL, "monitor_type", "multisync");
        monitor_type = get_monitor_type(p);

        p = (char *)config_get_string(CFG_MACHINE, NULL, "joystick_if", "");
        strcpy(joystick_if, p);
        joystick_type = joystick_get_type(joystick_if);
        for (c = 0; c < joystick_get_max_joysticks(joystick_type); c++)
        {
                char s[80];
                
                sprintf(s, "joystick_%i_nr", c);
                joystick_state[c].plat_joystick_nr = config_get_int(CFG_MACHINE, "Joysticks", s, 0);

                if (joystick_state[c].plat_joystick_nr)
                {
                        int d;
                        
                        for (d = 0; d < joystick_get_axis_count(joystick_type); d++)
                        {
                                sprintf(s, "joystick_%i_axis_%i", c, d);
                                joystick_state[c].axis_mapping[d] = config_get_int(CFG_MACHINE, "Joysticks", s, d);
                        }
                        for (d = 0; d < joystick_get_button_count(joystick_type); d++)
                        {
                                sprintf(s, "joystick_%i_button_%i", c, d);
                                joystick_state[c].button_mapping[d] = config_get_int(CFG_MACHINE, "Joysticks", s, d);
                        }
                        for (d = 0; d < joystick_get_pov_count(joystick_type); d++)
                        {
                                sprintf(s, "joystick_%i_pov_%i_x", c, d);
                                joystick_state[c].pov_mapping[d][0] = config_get_int(CFG_MACHINE, "Joysticks", s, d);
                                sprintf(s, "joystick_%i_pov_%i_y", c, d);
                                joystick_state[c].pov_mapping[d][1] = config_get_int(CFG_MACHINE, "Joysticks", s, d);
                        }
                }
        }

        p = (char *)config_get_string(CFG_MACHINE, NULL,"hd4_fn",NULL);
        if (p)
                strcpy(hd_fn[0], p);
        else
                hd_fn[0][0] = 0;
        hd_spt[0] = config_get_int(CFG_MACHINE, NULL, "hd4_sectors", 63);
        hd_hpc[0] = config_get_int(CFG_MACHINE, NULL, "hd4_heads", 16);
        hd_cyl[0] = config_get_int(CFG_MACHINE, NULL, "hd4_cylinders", 100);
        p = (char *)config_get_string(CFG_MACHINE, NULL,"hd5_fn",NULL);
        if (p)
                strcpy(hd_fn[1], p);
        else
                hd_fn[1][0] = 0;
        hd_spt[1] = config_get_int(CFG_MACHINE, NULL, "hd5_sectors", 63);
        hd_hpc[1] = config_get_int(CFG_MACHINE, NULL, "hd5_heads", 16);
        hd_cyl[1] = config_get_int(CFG_MACHINE, NULL, "hd5_cylinders", 100);
        p = (char *)config_get_string(CFG_MACHINE, NULL, "renderer_driver", "auto");
        selected_video_renderer = video_renderer_get_id(p);
        p = (char *)config_get_string(CFG_MACHINE, NULL, "podule_0", NULL);
        if (p)
                strncpy(podule_names[0], p, 15);
        else
                strcpy(podule_names[0], "");
        p = (char *)config_get_string(CFG_MACHINE, NULL, "podule_1", NULL);
        if (p)
                strncpy(podule_names[1], p, 15);
        else
                strcpy(podule_names[1], "");
        p = (char *)config_get_string(CFG_MACHINE, NULL, "podule_2", NULL);
        if (p)
                strncpy(podule_names[2], p, 15);
        else
                strcpy(podule_names[2], "");
        p = (char *)config_get_string(CFG_MACHINE, NULL, "podule_3", NULL);
        if (p)
                strncpy(podule_names[3], p, 15);
        else
                strcpy(podule_names[3], "");
}

void saveconfig()
{
        char config_file[512];
        int c;

        append_filename(config_file, exname, "arc.cfg", 511);

        config_set_string(CFG_MACHINE, NULL, "machine", machine);
        config_set_string(CFG_MACHINE, NULL,"disc_name_0",discname[0]);
        config_set_string(CFG_MACHINE, NULL,"disc_name_1",discname[1]);
        config_set_string(CFG_MACHINE, NULL,"disc_name_2",discname[2]);
        config_set_string(CFG_MACHINE, NULL,"disc_name_3",discname[3]);
        config_set_int(CFG_GLOBAL, NULL, "sound_enable", soundena);
        config_set_int(CFG_MACHINE, NULL, "mem_size", memsize);
        config_set_int(CFG_MACHINE, NULL, "cpu_type", arm_cpu_type);
        config_set_int(CFG_MACHINE, NULL, "memc_type", memc_type);
        config_set_int(CFG_MACHINE, NULL, "fpa", fpaena);
        config_set_int(CFG_MACHINE, NULL, "fpu_type", fpu_type);
        config_set_int(CFG_MACHINE, NULL, "display_mode", display_mode);
        config_set_int(CFG_GLOBAL, NULL, "first_fullscreen", firstfull);
        config_set_int(CFG_MACHINE, NULL, "double_scan", dblscan);
        config_set_int(CFG_MACHINE, NULL, "video_scale", video_scale);
        config_set_int(CFG_MACHINE, NULL, "video_fullscreen_scale", video_fullscreen_scale);
        config_set_int(CFG_MACHINE, NULL, "video_linear_filtering", video_linear_filtering);
        config_set_int(CFG_MACHINE, NULL, "fdc_type", fdctype);
        config_set_int(CFG_MACHINE, NULL, "st506_present", st506_present);
        config_set_string(CFG_MACHINE, NULL, "rom_set", config_get_romset_name(romset));
        config_set_string(CFG_MACHINE, NULL, "monitor_type", get_monitor_type_name(monitor_type));

        config_set_string(CFG_MACHINE, NULL, "joystick_if", joystick_if);
        for (c = 0; c < joystick_get_max_joysticks(joystick_type); c++)
        {
                char s[80];

                sprintf(s, "joystick_%i_nr", c);
                config_set_int(CFG_MACHINE, "Joysticks", s, joystick_state[c].plat_joystick_nr);

                if (joystick_state[c].plat_joystick_nr)
                {
                        int d;
                        
                        for (d = 0; d < joystick_get_axis_count(joystick_type); d++)
                        {
                                sprintf(s, "joystick_%i_axis_%i", c, d);
                                config_set_int(CFG_MACHINE, "Joysticks", s, joystick_state[c].axis_mapping[d]);
                        }
                        for (d = 0; d < joystick_get_button_count(joystick_type); d++)
                        {
                                sprintf(s, "joystick_%i_button_%i", c, d);
                                config_set_int(CFG_MACHINE, "Joysticks", s, joystick_state[c].button_mapping[d]);
                        }
                        for (d = 0; d < joystick_get_pov_count(joystick_type); d++)
                        {
                                sprintf(s, "joystick_%i_pov_%i_x", c, d);
                                config_set_int(CFG_MACHINE, "Joysticks", s, joystick_state[c].pov_mapping[d][0]);
                                sprintf(s, "joystick_%i_pov_%i_y", c, d);
                                config_set_int(CFG_MACHINE, "Joysticks", s, joystick_state[c].pov_mapping[d][1]);
                        }
                }
        }

        config_set_int(CFG_GLOBAL, NULL, "stereo", stereo);
        config_set_int(CFG_GLOBAL, NULL, "sound_gain", sound_gain);
        config_set_int(CFG_GLOBAL, NULL, "sound_filter", sound_filter);
        config_set_int(CFG_GLOBAL, NULL, "disc_noise_gain", disc_noise_gain);
        config_set_int(CFG_MACHINE, NULL, "unique_id", unique_id);
        config_set_string(CFG_MACHINE, NULL, "hd4_fn", hd_fn[0]);
        config_set_int(CFG_MACHINE, NULL, "hd4_sectors", hd_spt[0]);
        config_set_int(CFG_MACHINE, NULL, "hd4_heads", hd_hpc[0]);
        config_set_int(CFG_MACHINE, NULL, "hd4_cylinders", hd_cyl[0]);
        config_set_string(CFG_MACHINE, NULL, "hd5_fn", hd_fn[1]);
        config_set_int(CFG_MACHINE, NULL, "hd5_sectors", hd_spt[1]);
        config_set_int(CFG_MACHINE, NULL, "hd5_heads", hd_hpc[1]);
        config_set_int(CFG_MACHINE, NULL, "hd5_cylinders", hd_cyl[1]);
        config_set_string(CFG_MACHINE, NULL, "renderer_driver", video_renderer_get_name(selected_video_renderer));
        config_set_string(CFG_MACHINE, NULL, "podule_0", podule_names[0]);
        config_set_string(CFG_MACHINE, NULL, "podule_1", podule_names[1]);
        config_set_string(CFG_MACHINE, NULL, "podule_2", podule_names[2]);
        config_set_string(CFG_MACHINE, NULL, "podule_3", podule_names[3]);

        config_save(CFG_GLOBAL, config_file);
        config_save(CFG_MACHINE, machine_config_file);
}

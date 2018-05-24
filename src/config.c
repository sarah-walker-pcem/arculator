#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "arc.h"
#include "arm.h"
#include "config.h"
#include "memc.h"
#include "video.h"

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

static char config_file[256];

typedef struct list_t
{
        struct list_t *next;
} list_t;

static list_t config_head;

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

void config_dump()
{
        section_t *current_section;
        list_t *head = &config_head;
                
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

void config_free()
{
        section_t *current_section;
        list_t *head = &config_head;
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

int config_free_section(const char *name)
{
        section_t *current_section, *prev_section;
        list_t *head = &config_head;
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

void config_load(char *fn)
{
        FILE *f = fopen(fn, "rt");
        section_t *current_section;
        list_t *head = &config_head;
        
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

static section_t *find_section(const char *name)
{
        section_t *current_section;
        char blank[] = "";
        list_t *head = &config_head;
                
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

static section_t *create_section(const char *name)
{
        section_t *new_section = (section_t *)malloc(sizeof(section_t));
        list_t *head = &config_head;
        
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
        
int config_get_int(const char *head, const char *name, int def)
{
        section_t *section;
        entry_t *entry;
        int value;

        section = find_section(head);
        
        if (!section)
                return def;
                
        entry = find_entry(section, name);

        if (!entry)
                return def;
        
        sscanf(entry->data, "%i", &value);
        
        return value;
}

float config_get_float(const char *head, const char *name, float def)
{
        section_t *section;
        entry_t *entry;
        float value;

        section = find_section(head);

        if (!section)
                return def;

        entry = find_entry(section, name);

        if (!entry)
                return def;

        sscanf(entry->data, "%f", &value);

        return value;
}

const char *config_get_string(const char *head, const char *name, const char *def)
{
        section_t *section;
        entry_t *entry;

        section = find_section(head);
        
        if (!section)
                return def;
                
        entry = find_entry(section, name);

        if (!entry)
                return def;
       
        return entry->data; 
}

void config_set_int(const char *head, const char *name, int val)
{
        section_t *section;
        entry_t *entry;

        section = find_section(head);
        
        if (!section)
                section = create_section(head);
                
        entry = find_entry(section, name);

        if (!entry)
                entry = create_entry(section, name);

        sprintf(entry->data, "%i", val);
}

void config_set_float(const char *head, const char *name, float val)
{
        section_t *section;
        entry_t *entry;

        section = find_section(head);

        if (!section)
                section = create_section(head);

        entry = find_entry(section, name);

        if (!entry)
                entry = create_entry(section, name);

        sprintf(entry->data, "%f", val);
}

void config_set_string(const char *head, const char *name, char *val)
{
        section_t *section;
        entry_t *entry;

        section = find_section(head);
        
        if (!section)
                section = create_section(head);
                
        entry = find_entry(section, name);

        if (!entry)
                entry = create_entry(section, name);

        strncpy(entry->data, val, 256);
}

void config_save(char *fn)
{
        FILE *f = fopen(fn, "wt");
        section_t *current_section;
        list_t *head = &config_head;
                
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


void loadconfig()
{
        char config_file[512];
        const char *p;

        append_filename(config_file, exname, "arc.cfg", 511);
        rpclog("config_file=%s\n", config_file);
        config_load(config_file);
        config_dump();

        p=config_get_string(NULL,"limit_speed",NULL);
        if (!p || strcmp(p,"0")) limitspeed=1;
        else                     limitspeed=0;
        p=config_get_string(NULL,"sound_enable",NULL);
        if (!p || strcmp(p,"0")) soundena=1;
        else                     soundena=0;
        display_mode = config_get_int(NULL, "display_mode", DISPLAY_MODE_NO_BORDERS);
        arm_cpu_type = config_get_int(NULL, "cpu_type", 0);
        memc_type = config_get_int(NULL, "memc_type", 0);
        fpaena = config_get_int(NULL, "fpa", 0);
        p=config_get_string(NULL,"hires",NULL);
        if (!p || strcmp(p,"1")) hires=0;
        else                     hires=1;
        p=config_get_string(NULL,"first_fullscreen",NULL);
        if (!p || strcmp(p,"0")) firstfull=1;
        else                     firstfull=0;
        p=config_get_string(NULL,"double_scan",NULL);
        if (!p || strcmp(p,"0")) dblscan=1;
        else                     dblscan=0;
        p=config_get_string(NULL,"fast_disc",NULL);
        if (!p || strcmp(p,"0")) fastdisc=1;
        else                     fastdisc=0;
        p=config_get_string(NULL,"fdc_type",NULL);
        if (!p || strcmp(p,"0")) fdctype=1;
        else                     fdctype=0;
        p=config_get_string(NULL,"stereo",NULL);
        if (!p || strcmp(p,"0")) stereo=1;
        else                     stereo=0;
}

void saveconfig()
{
        char s[80];
        char config_file[512];

        append_filename(config_file, exname, "arc.cfg", 511);

        config_set_string(NULL,"disc_name_0",discname[0]);
        config_set_string(NULL,"disc_name_1",discname[1]);
        config_set_string(NULL,"disc_name_2",discname[2]);
        config_set_string(NULL,"disc_name_3",discname[3]);
        sprintf(s,"%i",limitspeed);
        config_set_string(NULL,"limit_speed",s);
        sprintf(s,"%i",soundena);
        config_set_string(NULL,"sound_enable",s);
        sprintf(s,"%i",memsize);
        config_set_string(NULL,"mem_size",s);
        config_set_int(NULL, "cpu_type", arm_cpu_type);
        config_set_int(NULL, "memc_type", memc_type);
        config_set_int(NULL, "fpa", fpaena);
        sprintf(s,"%i",hires);
        config_set_string(NULL,"hires",s);
        sprintf(s,"%i",fullborders);
        config_set_int(NULL, "display_mode", display_mode);
        config_set_string(NULL,"first_fullscreen",s);
        sprintf(s,"%i",dblscan);
        config_set_string(NULL,"double_scan",s);
        sprintf(s,"%i",fastdisc);
        config_set_string(NULL,"fast_disc",s);
        sprintf(s,"%i",fdctype);
        config_set_string(NULL,"fdc_type",s);
        sprintf(s,"%i",romset);
        config_set_string(NULL,"rom_set",s);
        sprintf(s,"%i",stereo);
        config_set_string(NULL,"stereo",s);
        
        config_save(config_file);
}

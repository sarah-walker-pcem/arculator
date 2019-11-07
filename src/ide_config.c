/*Arculator 2.0 by Sarah Walker
  IDE/ST-506 configuration dialogues*/
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include "podule_api.h"

enum
{
        ID_PATH,
        ID_CYLINDERS,
        ID_HEADS,
        ID_SECTORS,
        ID_SIZE,
        ID_DRIVE_4,
        ID_DRIVE_5
};

static int MAX_CYLINDERS = 1024;
static int MAX_HEADS = 16;
static int MIN_SECTORS = 1;
static int MAX_SECTORS = 63;
static int SECTOR_SIZE = 512;
#define MAX_SIZE ((MAX_CYLINDERS * MAX_HEADS * MAX_SECTORS) / (1024 * 1024 / SECTOR_SIZE))

static const podule_callbacks_t *podule_callbacks;

void ide_config_init(const podule_callbacks_t *callbacks)
{
        podule_callbacks = callbacks;
}

void st506_config_init(const podule_callbacks_t *callbacks)
{
        podule_callbacks = callbacks;
}

static int changed_chs(void *window_p, const struct podule_config_item_t *item, void *new_data)
{
        char *temp_s;
        char size_s[80];
        int cylinders, heads, sectors;
        int size;

        temp_s = podule_callbacks->config_get_current(window_p, ID_CYLINDERS);
        cylinders = atoi(temp_s);
        if (cylinders > MAX_CYLINDERS)
        {
                cylinders = MAX_CYLINDERS;
                snprintf(size_s, sizeof(size_s), "%i", cylinders);
                podule_callbacks->config_set_current(window_p, ID_CYLINDERS, size_s);
        }

        temp_s = podule_callbacks->config_get_current(window_p, ID_HEADS);
        heads = atoi(temp_s);
        if (heads > MAX_HEADS)
        {
                heads = MAX_HEADS;
                snprintf(size_s, sizeof(size_s), "%i", heads);
                podule_callbacks->config_set_current(window_p, ID_HEADS, size_s);
        }

        temp_s = podule_callbacks->config_get_current(window_p, ID_SECTORS);
        sectors = atoi(temp_s);
        if (sectors > MAX_SECTORS)
        {
                sectors = MAX_SECTORS;
                snprintf(size_s, sizeof(size_s), "%i", sectors);
                podule_callbacks->config_set_current(window_p, ID_SECTORS, size_s);
        }
        else if (sectors < MIN_SECTORS)
        {
                sectors = MIN_SECTORS;
                snprintf(size_s, sizeof(size_s), "%i", sectors);
                podule_callbacks->config_set_current(window_p, ID_SECTORS, size_s);
        }

        size = (cylinders * heads * sectors) / (1024 * 1024 / SECTOR_SIZE);
        snprintf(size_s, sizeof(size_s), "%i", size);
        podule_callbacks->config_set_current(window_p, ID_SIZE, size_s);

        return 0;
}

static int changed_size(void *window_p, const struct podule_config_item_t *item, void *new_data)
{
        char *temp_s;
        char size_s[80];
        int cylinders, heads, sectors;
        int size;

        temp_s = podule_callbacks->config_get_current(window_p, ID_SIZE);
        size = atoi(temp_s);
        if (size > MAX_SIZE)
        {
                size = MAX_SIZE;
                snprintf(size_s, sizeof(size_s), "%i", size);
                podule_callbacks->config_set_current(window_p, ID_SIZE, size_s);
        }

        heads = MAX_HEADS;
        sectors = MAX_SECTORS;
        cylinders = (size * (1024 * 1024 / SECTOR_SIZE)) / (MAX_HEADS * MAX_SECTORS);

        snprintf(size_s, sizeof(size_s), "%i", cylinders);
        podule_callbacks->config_set_current(window_p, ID_CYLINDERS, size_s);
        snprintf(size_s, sizeof(size_s), "%i", heads);
        podule_callbacks->config_set_current(window_p, ID_HEADS, size_s);
        snprintf(size_s, sizeof(size_s), "%i", sectors);
        podule_callbacks->config_set_current(window_p, ID_SECTORS, size_s);

        return 0;
}

int change_path(void *window_p, const struct podule_config_item_t *item, void *new_data)
{
        char fn[256];

        if (!podule_callbacks->config_file_selector(window_p, "Please enter a file name",
                        NULL, NULL, NULL, "HDF files (*.hdf)|*.hdf", fn, sizeof(fn), CONFIG_FILESEL_SAVE))
        {
//                rpclog("Filename: %s\n", fn);

                podule_callbacks->config_set_current(window_p, ID_PATH, fn);
        }
        return 0;
}

static int new_cylinders, new_heads, new_sectors;
static char new_fn[256];
static int new_drive_valid;

static void new_drive_init(void *window_p)
{
        char size_s[80];
        
        snprintf(size_s, sizeof(size_s), "%i", 100);
        podule_callbacks->config_set_current(window_p, ID_CYLINDERS, size_s);
        snprintf(size_s, sizeof(size_s), "%i", MAX_HEADS);
        podule_callbacks->config_set_current(window_p, ID_HEADS, size_s);
        snprintf(size_s, sizeof(size_s), "%i", MAX_SECTORS);
        podule_callbacks->config_set_current(window_p, ID_SECTORS, size_s);
}

static int new_drive_close(void *window_p)
{
        FILE *f;
        char *temp_s;
        int cylinders, heads, sectors;
//        int size;

        temp_s = podule_callbacks->config_get_current(window_p, ID_CYLINDERS);
        cylinders = atoi(temp_s);
        temp_s = podule_callbacks->config_get_current(window_p, ID_HEADS);
        heads = atoi(temp_s);
        temp_s = podule_callbacks->config_get_current(window_p, ID_SECTORS);
        sectors = atoi(temp_s);

//        size = cylinders*heads*sectors*512;

        temp_s = podule_callbacks->config_get_current(window_p, ID_PATH);
//        rpclog("new_drive_close: path=%s cylinders=%i heads=%i sectors=%i size=%i\n", temp_s, cylinders, heads, sectors, size);
        f = fopen(temp_s, "wb");
        if (f)
        {
                /*Write out disc image*/
                uint8_t data[512];
                int c;
                int nr_sectors = cylinders*heads*sectors;

                memset(data, 0, SECTOR_SIZE);
                for (c = 0; c < nr_sectors; c++)
                        fwrite(data, SECTOR_SIZE, 1, f);
                fclose(f);

                new_cylinders = cylinders;
                new_heads = heads;
                new_sectors = sectors;
                strncpy(new_fn, temp_s, sizeof(new_fn));
                new_drive_valid = 1;
        }

        return 0;
}

static podule_config_t ide_new_drive_config =
{
        .title = "New drive",
        .init = new_drive_init,
        .close = new_drive_close,
        .items =
        {
                {
                        .description = "Path :",
                        .type = CONFIG_STRING,
                        .flags = CONFIG_FLAGS_DISABLED,
                        .default_string = "",
                        .id = ID_PATH
                },
                {
                        .description = "Change path...",
                        .type = CONFIG_BUTTON,
                        .function = change_path
                },
                {
                        .description = "Cylinders :",
                        .type = CONFIG_STRING,
                        .default_string = "100",
                        .id = ID_CYLINDERS,
                        .function = changed_chs
                },
                {
                        .description = "Heads :",
                        .type = CONFIG_STRING,
                        .default_string = "16",
                        .id = ID_HEADS,
                        .function = changed_chs
                },
                {
                        .description = "Sectors :",
                        .type = CONFIG_STRING,
                        .default_string = "63",
                        .id = ID_SECTORS,
                        .function = changed_chs
                },
                {
                        .description = "Size (MB) :",
                        .type = CONFIG_STRING,
                        .default_string = "50",
                        .id = ID_SIZE,
                        .function = changed_size
                },
                {
                        .type = -1
                }
        }
};

static int config_new_drive(void *window_p, const struct podule_config_item_t *item, void *new_data)
{
        new_drive_valid = 0;

        podule_callbacks->config_open(window_p, &ide_new_drive_config, NULL);

        if (new_drive_valid)
        {
                char temp_s[80];

                snprintf(temp_s, sizeof(temp_s), "%i", new_cylinders);
                podule_callbacks->config_set_current(window_p, ID_CYLINDERS, temp_s);
                snprintf(temp_s, sizeof(temp_s), "%i", new_heads);
                podule_callbacks->config_set_current(window_p, ID_HEADS, temp_s);
                snprintf(temp_s, sizeof(temp_s), "%i", new_sectors);
                podule_callbacks->config_set_current(window_p, ID_SECTORS, temp_s);
                snprintf(temp_s, sizeof(temp_s), "%i", (new_cylinders * new_heads * new_sectors) / (1024 * 1024 / SECTOR_SIZE));
                podule_callbacks->config_set_current(window_p, ID_SIZE, temp_s);
                podule_callbacks->config_set_current(window_p, ID_PATH, new_fn);
                
                return 1;

        }

        return 0;
}

static void drive_load_open(void *window_p)
{
        char temp_s[80];

        snprintf(temp_s, sizeof(temp_s), "%i", new_cylinders);
        podule_callbacks->config_set_current(window_p, ID_CYLINDERS, temp_s);
        snprintf(temp_s, sizeof(temp_s), "%i", new_heads);
        podule_callbacks->config_set_current(window_p, ID_HEADS, temp_s);
        snprintf(temp_s, sizeof(temp_s), "%i", new_sectors);
        podule_callbacks->config_set_current(window_p, ID_SECTORS, temp_s);
        snprintf(temp_s, sizeof(temp_s), "%i", (new_cylinders * new_heads * new_sectors) / (1024 * 1024 / SECTOR_SIZE));
        podule_callbacks->config_set_current(window_p, ID_SIZE, temp_s);
}

static int drive_load_close(void *window_p)
{
        char *temp_s;
        
        temp_s = podule_callbacks->config_get_current(window_p, ID_CYLINDERS);
        new_cylinders = atoi(temp_s);
        temp_s = podule_callbacks->config_get_current(window_p, ID_HEADS);
        new_heads = atoi(temp_s);
        temp_s = podule_callbacks->config_get_current(window_p, ID_SECTORS);
        new_sectors = atoi(temp_s);

        return 1;
}

static podule_config_t ide_load_drive_config =
{
        .title = "Drive geometry",
        .init = drive_load_open,
        .close = drive_load_close,
        .items =
        {
                {
                        .description = "Cylinders :",
                        .type = CONFIG_STRING,
                        .default_string = "100",
                        .id = ID_CYLINDERS,
                        .function = changed_chs
                },
                {
                        .description = "Heads :",
                        .type = CONFIG_STRING,
                        .default_string = "16",
                        .id = ID_HEADS,
                        .function = changed_chs
                },
                {
                        .description = "Sectors :",
                        .type = CONFIG_STRING,
                        .default_string = "63",
                        .id = ID_SECTORS,
                        .function = changed_chs
                },
                {
                        .description = "Size (MB) :",
                        .type = CONFIG_STRING,
                        .default_string = "50",
                        .id = ID_SIZE,
                        .function = changed_size
                },
                {
                        .type = -1
                }
        }
};

static int config_load_drive(void *window_p, const struct podule_config_item_t *item, void *new_data)
{
        char fn[256];

        if (!podule_callbacks->config_file_selector(window_p, "Please select a hard drive image",
                        NULL, NULL, NULL, "HDF files (*.hdf)|*.hdf", fn, sizeof(fn), CONFIG_FILESEL_LOAD))
        {
                FILE *f;
                int filesize;
                int log2secsize, density;
                
//                rpclog("Filename: %s\n", fn);

                f = fopen(fn, "rb");
                if (!f)
                        return 0;
                fseek(f, -1, SEEK_END);
                filesize = ftell(f) + 1;
                fseek(f, 0, SEEK_SET);
                
//                rpclog("filesize=%i\n", filesize);
                
                /*Try to detect drive size geometry disc record. Valid disc record
                  will have log2secsize of 8 (256 bytes per sector) or 9 (512
                  bytes per sector), density of 0, and sector and head counts
                  within valid range*/
                /*Initially assume stupid 512 byte header created by RPCemu and
                  older Arculator*/
                fseek(f, 0xFC0, SEEK_SET);
                log2secsize = getc(f);
                new_sectors = getc(f);
                new_heads = getc(f);
                density = getc(f);

                if ((log2secsize != 8 && log2secsize != 9) || !new_sectors || !new_heads || new_sectors > 63 || new_heads > 16 || density != 0)
                {
                        /*Invalid geometry, try without header*/
                        fseek(f, 0xDC0, SEEK_SET);
                        log2secsize = getc(f);
                        new_sectors = getc(f);
                        new_heads = getc(f);
                        density = getc(f);
                        
                        if ((log2secsize != 8 && log2secsize != 9) || !new_sectors || !new_heads || new_sectors > 63 || new_heads > 16 || density != 0)
                        {
                                /*Invalid geometry, assume max*/
                                new_sectors = 63;
                                new_heads = 16;
                        }
                }
                else
                        filesize -= 512; /*Account for header*/

                fclose(f);

//                rpclog("sectors=%i, heads=%i\n", new_sectors, new_heads);
                new_cylinders = filesize / (512 * new_sectors * new_heads);
//                rpclog("cylinder guess = %i\n", new_cylinders);
//                podule_callbacks->config_set_current(window_p, ID_PATH, fn);

                if (podule_callbacks->config_open(window_p, &ide_load_drive_config, NULL))
                {
                        char temp_s[80];

//                        rpclog("OK, setting new params\n");
                        snprintf(temp_s, sizeof(temp_s), "%i", new_cylinders);
                        podule_callbacks->config_set_current(window_p, ID_CYLINDERS, temp_s);
                        snprintf(temp_s, sizeof(temp_s), "%i", new_heads);
                        podule_callbacks->config_set_current(window_p, ID_HEADS, temp_s);
                        snprintf(temp_s, sizeof(temp_s), "%i", new_sectors);
                        podule_callbacks->config_set_current(window_p, ID_SECTORS, temp_s);
                        snprintf(temp_s, sizeof(temp_s), "%i", (new_cylinders * new_heads * new_sectors) / (1024 * 1024 / 512));
                        podule_callbacks->config_set_current(window_p, ID_SIZE, temp_s);
                        podule_callbacks->config_set_current(window_p, ID_PATH, fn);
                        
                        return 1;
                }
        }
        
        return 0;
}

static int config_eject_drive(void *window_p, const struct podule_config_item_t *item, void *new_data)
{
        podule_callbacks->config_set_current(window_p, ID_CYLINDERS, "");
        podule_callbacks->config_set_current(window_p, ID_HEADS, "");
        podule_callbacks->config_set_current(window_p, ID_SECTORS, "");
        podule_callbacks->config_set_current(window_p, ID_SIZE, "");
        podule_callbacks->config_set_current(window_p, ID_PATH, "");

        return 1;
}

static void drive_edit_open(void *window_p)
{
        changed_chs(window_p, NULL, NULL);
}

static podule_config_t ide_drive_edit_config =
{
        .title = "Drive configuration",
        .init = drive_edit_open,
        .items =
        {
                {
                        .name = "fn",
                        .description = "Path :",
                        .type = CONFIG_STRING,
                        .flags = CONFIG_FLAGS_DISABLED | CONFIG_FLAGS_NAME_PREFIXED,
                        .id = ID_PATH
                },
                {
                        .name = "cylinders",
                        .description = "Cylinders :",
                        .type = CONFIG_STRING,
                        .flags = CONFIG_FLAGS_DISABLED | CONFIG_FLAGS_NAME_PREFIXED,
                        .id = ID_CYLINDERS
                },
                {
                        .name = "heads",
                        .description = "Heads :",
                        .type = CONFIG_STRING,
                        .flags = CONFIG_FLAGS_DISABLED | CONFIG_FLAGS_NAME_PREFIXED,
                        .id = ID_HEADS
                },
                {
                        .name = "sectors",
                        .description = "Sectors :",
                        .type = CONFIG_STRING,
                        .flags = CONFIG_FLAGS_DISABLED | CONFIG_FLAGS_NAME_PREFIXED,
                        .id = ID_SECTORS
                },
                {
                        .description = "Size (MB) :",
                        .type = CONFIG_STRING,
                        .flags = CONFIG_FLAGS_DISABLED,
                        .default_string = "50",
                        .id = ID_SIZE,
                        .function = changed_size
                },
                {
                        .description = "New drive...",
                        .type = CONFIG_BUTTON,
                        .function = config_new_drive
                },
                {
                        .description = "Load drive...",
                        .type = CONFIG_BUTTON,
                        .function = config_load_drive
                },
                {
                        .description = "Eject drive",
                        .type = CONFIG_BUTTON,
                        .function = config_eject_drive
                },
                {
                        .type = -1
                }
        }
};

static int config_drive(void *window_p, const struct podule_config_item_t *item, void *new_data)
{
        if (item->id == ID_DRIVE_4)
                podule_callbacks->config_open(window_p, &ide_drive_edit_config, "hd4_");
        else
                podule_callbacks->config_open(window_p, &ide_drive_edit_config, "hd5_");

        return 0;
}

static void ide_podule_config_init(void *window_p)
{
        MAX_CYLINDERS = 1024;
        MAX_HEADS = 16;
        MIN_SECTORS = 1;
        MAX_SECTORS = 63;
        SECTOR_SIZE = 512;
}

podule_config_t ide_podule_config =
{
        .title = "IDE podule configuration",
        .init = ide_podule_config_init,
        .items =
        {
                {
                        .description = "Configure drive :4...",
                        .type = CONFIG_BUTTON,
                        .function = config_drive,
                        .id = ID_DRIVE_4
                },
                {
                        .description = "Configure drive :5...",
                        .type = CONFIG_BUTTON,
                        .function = config_drive,
                        .id = ID_DRIVE_5
                },
                {
                        .type = -1
                }
        }
};

static void st506_podule_config_init(void *window_p)
{
        MAX_CYLINDERS = 1024;
        MAX_HEADS = 8;
        MIN_SECTORS = 32;
        MAX_SECTORS = 32;
        SECTOR_SIZE = 256;
}

podule_config_t st506_podule_config =
{
        .title = "ST-506 podule configuration",
        .init = st506_podule_config_init,
        .items =
        {
                {
                        .description = "Configure drive :4...",
                        .type = CONFIG_BUTTON,
                        .function = config_drive,
                        .id = ID_DRIVE_4
                },
                {
                        .description = "Configure drive :5...",
                        .type = CONFIG_BUTTON,
                        .function = config_drive,
                        .id = ID_DRIVE_5
                },
                {
                        .type = -1
                }
        }
};

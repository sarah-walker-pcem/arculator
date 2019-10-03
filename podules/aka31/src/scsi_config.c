#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include "podule_api.h"
#include "aka31.h"

enum
{
        ID_PATH,
        ID_BLOCKS,
        ID_SIZE,
        ID_ID_0,
        ID_ID_1,
        ID_ID_2,
        ID_ID_3,
        ID_ID_4,
        ID_ID_5,
        ID_ID_6
};

static int MAX_BLOCKS = (1 << 24) - 1;
static int SECTOR_SIZE = 512;
#define MAX_SIZE (MAX_BLOCKS / (1024 * 1024 / SECTOR_SIZE))

//static const podule_callbacks_t *podule_callbacks;

static int changed_blocks(void *window_p, const struct podule_config_item_t *item, void *new_data)
{
        char *new_s = new_data;
        char *temp_s;
        char size_s[80];
        int blocks;
        int size;

        temp_s = podule_callbacks->config_get_current(window_p, ID_BLOCKS);
        blocks = atoi(temp_s);
        if (blocks > MAX_BLOCKS)
        {
                blocks = MAX_BLOCKS;
                snprintf(size_s, sizeof(size_s), "%i", blocks);
                podule_callbacks->config_set_current(window_p, ID_BLOCKS, size_s);
        }

        size = blocks / (1024 * 1024 / SECTOR_SIZE);
        snprintf(size_s, sizeof(size_s), "%i", size);
        podule_callbacks->config_set_current(window_p, ID_SIZE, size_s);

        return 0;
}

static int changed_size(void *window_p, const struct podule_config_item_t *item, void *new_data)
{
        char *new_s = new_data;
        char *temp_s;
        char size_s[80];
        int blocks;
        int size;

        temp_s = podule_callbacks->config_get_current(window_p, ID_SIZE);
        size = atoi(temp_s);
        if (size > MAX_SIZE)
        {
                size = MAX_SIZE;
                snprintf(size_s, sizeof(size_s), "%i", size);
                podule_callbacks->config_set_current(window_p, ID_SIZE, size_s);
        }

        blocks = size * (1024 * 1024 / 512);

        snprintf(size_s, sizeof(size_s), "%i", blocks);
        podule_callbacks->config_set_current(window_p, ID_BLOCKS, size_s);

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

static int new_blocks;
static char new_fn[256];
static int new_drive_valid;

static void new_drive_init(void *window_p)
{
        char size_s[80];

        snprintf(size_s, sizeof(size_s), "%i", 102400);
        podule_callbacks->config_set_current(window_p, ID_BLOCKS, size_s);
}

static int new_drive_close(void *window_p)
{
        FILE *f;
        char *temp_s;
        int blocks;
//        int size;

        temp_s = podule_callbacks->config_get_current(window_p, ID_BLOCKS);
        blocks = atoi(temp_s);

        temp_s = podule_callbacks->config_get_current(window_p, ID_PATH);

        f = fopen(temp_s, "wb");
        if (f)
        {
                /*Write out disc image*/
                uint8_t data[512];
                int c;

                memset(data, 0, SECTOR_SIZE);
                for (c = 0; c < blocks; c++)
                        fwrite(data, SECTOR_SIZE, 1, f);
                fclose(f);

                new_blocks = blocks;
                strncpy(new_fn, temp_s, sizeof(new_fn));
                new_drive_valid = 1;
        }

        return 0;
}

static podule_config_t scsi_new_drive_config =
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
                        .description = "Blocks :",
                        .type = CONFIG_STRING,
                        .default_string = "102400",
                        .id = ID_BLOCKS,
                        .function = changed_blocks
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

        podule_callbacks->config_open(window_p, &scsi_new_drive_config, NULL);

        if (new_drive_valid)
        {
                char temp_s[80];

                snprintf(temp_s, sizeof(temp_s), "%i", new_blocks);
                podule_callbacks->config_set_current(window_p, ID_BLOCKS, temp_s);
                snprintf(temp_s, sizeof(temp_s), "%i", new_blocks / (1024 * 1024 / 512));
                podule_callbacks->config_set_current(window_p, ID_SIZE, temp_s);
                podule_callbacks->config_set_current(window_p, ID_PATH, new_fn);

                return 1;

        }

        return 0;
}

static void drive_load_open(void *window_p)
{
        char temp_s[80];

        snprintf(temp_s, sizeof(temp_s), "%i", new_blocks);
        podule_callbacks->config_set_current(window_p, ID_BLOCKS, temp_s);
        snprintf(temp_s, sizeof(temp_s), "%i", new_blocks / (1024 * 1024 / SECTOR_SIZE));
        podule_callbacks->config_set_current(window_p, ID_SIZE, temp_s);
}

static int drive_load_close(void *window_p)
{
        char *temp_s;

        temp_s = podule_callbacks->config_get_current(window_p, ID_BLOCKS);
        new_blocks = atoi(temp_s);

        return 1;
}

static podule_config_t scsi_load_drive_config =
{
        .title = "Drive geometry",
        .init = drive_load_open,
        .close = drive_load_close,
        .items =
        {
                {
                        .description = "Blocks :",
                        .type = CONFIG_STRING,
                        .default_string = "102400",
                        .id = ID_BLOCKS,
                        .function = changed_blocks
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
                char temp_s[256];

//                rpclog("Filename: %s\n", fn);

                f = fopen(fn, "rb");
                if (!f)
                        return 0;
                fseek(f, -1, SEEK_END);
                filesize = ftell(f) + 1;
                fclose(f);

                snprintf(temp_s, sizeof(temp_s), "%i", filesize / 512);
                podule_callbacks->config_set_current(window_p, ID_BLOCKS, temp_s);
                snprintf(temp_s, sizeof(temp_s), "%i", filesize / (1024 * 1024));
                podule_callbacks->config_set_current(window_p, ID_SIZE, temp_s);
                podule_callbacks->config_set_current(window_p, ID_PATH, fn);

                return 1;
        }

        return 0;
}

static int config_eject_drive(void *window_p, const struct podule_config_item_t *item, void *new_data)
{
        podule_callbacks->config_set_current(window_p, ID_BLOCKS, "");
        podule_callbacks->config_set_current(window_p, ID_SIZE, "");
        podule_callbacks->config_set_current(window_p, ID_PATH, "");

        return 1;
}

static void drive_edit_open(void *window_p)
{
        changed_blocks(window_p, NULL, NULL);
}

static podule_config_t scsi_drive_edit_config =
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
                        .name = "size",
                        .description = "Blocks :",
                        .type = CONFIG_STRING,
                        .flags = CONFIG_FLAGS_DISABLED | CONFIG_FLAGS_NAME_PREFIXED,
                        .id = ID_BLOCKS
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
        switch (item->id)
        {
                case ID_ID_0:
                podule_callbacks->config_open(window_p, &scsi_drive_edit_config, "device0_");
                break;
                case ID_ID_1:
                podule_callbacks->config_open(window_p, &scsi_drive_edit_config, "device1_");
                break;
                case ID_ID_2:
                podule_callbacks->config_open(window_p, &scsi_drive_edit_config, "device2_");
                break;
                case ID_ID_3:
                podule_callbacks->config_open(window_p, &scsi_drive_edit_config, "device3_");
                break;
                case ID_ID_4:
                podule_callbacks->config_open(window_p, &scsi_drive_edit_config, "device4_");
                break;
                case ID_ID_5:
                podule_callbacks->config_open(window_p, &scsi_drive_edit_config, "device5_");
                break;
                case ID_ID_6:
                podule_callbacks->config_open(window_p, &scsi_drive_edit_config, "device6_");
                break;
        }

        return 0;
}

static void aka31_podule_config_init(void *window_p)
{
}

podule_config_t aka31_podule_config =
{
        .title = "SCSI device configuration",
        .init = aka31_podule_config_init,
        .items =
        {
                {
                        .description = "Configure ID 0...",
                        .type = CONFIG_BUTTON,
                        .function = config_drive,
                        .id = ID_ID_0
                },
                {
                        .description = "Configure ID 1...",
                        .type = CONFIG_BUTTON,
                        .function = config_drive,
                        .id = ID_ID_1
                },
                {
                        .description = "Configure ID 2...",
                        .type = CONFIG_BUTTON,
                        .function = config_drive,
                        .id = ID_ID_2
                },
                {
                        .description = "Configure ID 3...",
                        .type = CONFIG_BUTTON,
                        .function = config_drive,
                        .id = ID_ID_3
                },
                {
                        .description = "Configure ID 4...",
                        .type = CONFIG_BUTTON,
                        .function = config_drive,
                        .id = ID_ID_4
                },
                {
                        .description = "Configure ID 5...",
                        .type = CONFIG_BUTTON,
                        .function = config_drive,
                        .id = ID_ID_5
                },
                {
                        .description = "Configure ID 6...",
                        .type = CONFIG_BUTTON,
                        .function = config_drive,
                        .id = ID_ID_6
                },
                {
                        .type = -1
                }
        }
};

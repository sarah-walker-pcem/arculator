#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "aka31.h"
#include "scsi.h"
#include "scsi_hd.h"

typedef struct scsi_hd_data
{
        FILE *f;
        int blocks;
        
        int cmd_pos;
        
        int addr, len;
        int sector_pos;
        
        uint8_t buf[512];
} scsi_hd_data;

static void *scsi_hd_init()
{
        scsi_hd_data *data = malloc(sizeof(scsi_hd_data));
        memset(data, 0, sizeof(scsi_hd_data));
        
        data->f = fopen("scsihd4.hdf", "rb+");
        if (!data->f)
        {
                data->f = fopen("scsihd4.hdf", "wb");
                if (!data->f)
                        aka31_log("Failed to open hdf\n");

                putc(0, data->f);
                fclose(data->f);
                data->f = fopen("scsihd4.hdf", "rb+");
        }
        aka31_log("Opened %p\n", (void *)data->f);
        
        return data;
}

static void scsi_hd_close(void *p)
{
        scsi_hd_data *data = p;
        
        fclose(data->f);
        free(data);
}

#define add_data_len(v)                 \
        if (i < len)                    \
                scsi_add_data(v);       \
        i++;

static int scsi_hd_command(uint8_t *cdb, void *p)
{
        scsi_hd_data *data = p;
        int addr, len;
        int i = 0;

        aka31_log("SCSI HD command %02x len %i\n", cdb[0], len);
        
        switch (cdb[0])
        {
		case SCSI_TEST_UNIT_READY:
                scsi_send_complete();
                data->cmd_pos = 0;
                return 1;

                case SCSI_INQUIRY:
                len = cdb[4] | (cdb[3] << 8);

                add_data_len(0 | (0 << 5)); /*Hard disc*/
                add_data_len(0);            /*Not removeable*/
                add_data_len(0);            /*No version*/
                add_data_len(2);            /*Response data*/
                add_data_len(0);            /*Additional length*/
                add_data_len(0);
                add_data_len(0);
                add_data_len(2);
                
                add_data_len('A');
                add_data_len('r');
                add_data_len('c');
                add_data_len('u');
                add_data_len('l');
                add_data_len('a');
                add_data_len('t');
                add_data_len('r');
                
                add_data_len('A');
                add_data_len('r');
                add_data_len('c');
                add_data_len('u');
                add_data_len('l');
                add_data_len('a');
                add_data_len('t');
                add_data_len('o');

                add_data_len('r');
                add_data_len('S');
                add_data_len('C');
                add_data_len('S');
                add_data_len('I');
                add_data_len('H');
                add_data_len('D');
                add_data_len(' ');

                add_data_len(0); /*Product revision level*/
                add_data_len(0);                
                add_data_len(0);
                add_data_len(0);                

                add_data_len(0); /*Drive serial number*/
                add_data_len(0);                
                add_data_len(0);
                add_data_len(0);                
                add_data_len(0);
                add_data_len(0);                
                add_data_len(0);
                add_data_len(0);                

                add_data_len(0); /*Vendor unique*/
                add_data_len(0);                
                add_data_len(0);
                add_data_len(0);                
                add_data_len(0);
                add_data_len(0);                
                add_data_len(0);
                add_data_len(0);                
                add_data_len(0);
                add_data_len(0);                
                add_data_len(0);
                add_data_len(0);                

                add_data_len(0);
                add_data_len(0);                

                add_data_len(0); /*Vendor descriptor*/
                add_data_len(0);                
                add_data_len(0);
                add_data_len(0);                
                add_data_len(0);
                add_data_len(0);                
                add_data_len(0);
                add_data_len(0);                
                add_data_len(0);
                add_data_len(0);                
                add_data_len(0);
                add_data_len(0);                
                add_data_len(0);
                add_data_len(0);                
                add_data_len(0);
                add_data_len(0);                

                add_data_len(0); /*Reserved*/
                add_data_len(0);                
                add_data_len(0);
                add_data_len(0);                
                add_data_len(0);
                add_data_len(0);                
                add_data_len(0);
                add_data_len(0);                
                add_data_len(0);
                add_data_len(0);                
                add_data_len(0);
                add_data_len(0);                
                add_data_len(0);
                add_data_len(0);                
                add_data_len(0);
                add_data_len(0);                
                add_data_len(0);
                add_data_len(0);                
                add_data_len(0);
                add_data_len(0);                
                add_data_len(0);
                add_data_len(0);                

                data->cmd_pos = 0;
                return 1;
                
                case SCSI_PREVENT_ALLOW_MEDIUM_REMOVAL:
                scsi_send_complete();
                data->cmd_pos = 0;
                return 1;
                
                case SCSI_MODE_SENSE_6:
                len = cdb[4];

                add_data_len(len-4);
                add_data_len(0);
                add_data_len(0);
                add_data_len(0);
                
                if ((cdb[2] & 0x3f) == 0x03 || (cdb[2] & 0x3f) == 0x3f)
                {
                        add_data_len(3);
                        add_data_len(0x16);
                        add_data_len(0); add_data_len(1); /*Tracks per zone*/
                        add_data_len(0); add_data_len(1); /*Alternate sectors per zone*/
                        add_data_len(0); add_data_len(1); /*Alternate tracks per zone*/
                        add_data_len(0); add_data_len(1); /*Alternate tracks per volume*/
                        add_data_len(1); add_data_len(0); /*Sectors per track*/
                        add_data_len(2); add_data_len(0); /*Data bytes per physical sector*/
                        add_data_len(0); add_data_len(0); /*Interleave*/
                        add_data_len(0); add_data_len(0); /*Track skew*/
                        add_data_len(0); add_data_len(0); /*Cylinder skew*/
                        add_data_len(0);                  /*Drive type*/
                        add_data_len(0); add_data_len(0); add_data_len(0);
                }

                if ((cdb[2] & 0x3f) == 0x04 || (cdb[2] & 0x3f) == 0x3f)
                {
                        add_data_len(4);
                        add_data_len(0x16);
                        add_data_len(0); add_data_len(0x10); add_data_len(0); /*Cylinder count*/
                        add_data_len(64); /*Heads*/
                        add_data_len(0); add_data_len(0); add_data_len(0); /*Write recomp*/
                        add_data_len(0); add_data_len(0); add_data_len(0); /*Reduced write current*/
                        add_data_len(0); add_data_len(0); /*Drive step rate*/
                        add_data_len(0); add_data_len(0); add_data_len(0); /*Landing zone cylinder*/
                        add_data_len(0); /*RPL*/
                        add_data_len(0); /*Rotational offset*/
                        add_data_len(0);
                        add_data_len(0x10); add_data_len(0); /*Rotain rate*/
                        add_data_len(0); add_data_len(0);
                }                
                
                for (; len >= 0; len--)
                        add_data_len(0);
                
//                scsi_illegal_field();
                data->cmd_pos = 0;
                return 1;
                
                case SCSI_READ_CAPACITY_10:
                scsi_add_data((528807 >> 24) & 0xff);
                scsi_add_data((528807 >> 16) & 0xff);
                scsi_add_data((528807 >> 8)  & 0xff);
                scsi_add_data( 528807        & 0xff);
                scsi_add_data((512    >> 24) & 0xff);
                scsi_add_data((512    >> 16) & 0xff);
                scsi_add_data((512    >> 8)  & 0xff);
                scsi_add_data( 512           & 0xff);
                data->cmd_pos = 0;
                return 1;

                case SCSI_READ_6:
                if (!data->cmd_pos)
                {
                        data->addr = cdb[3] | (cdb[2] << 8) | ((cdb[1] & 0x1f) << 16);
                        data->len = cdb[4];
                        if (!data->len)
                                data->len = 256;
                        aka31_log("SCSI_READ_6: addr=%08x len=%04x %p\n", data->addr, data->len, (void *)data->f);
                        fseek(data->f, data->addr * 512, SEEK_SET);
                        
                        data->cmd_pos = 1;
                        data->sector_pos = 0;
                }
                else
                        aka31_log("SCSI_READ_6 continue addr=%08x len=%04x sector_pos=%02x\n", data->addr, data->len, data->sector_pos);
                while (data->len)
                {
                        if (data->cmd_pos == 1)
                                fread(data->buf, 512, 1, data->f);
                        data->cmd_pos = 2;
                        for (; data->sector_pos < 512; data->sector_pos++)
                        {
                                int ret = scsi_add_data(data->buf[data->sector_pos]);
                                
                                if (ret == -1)
                                        return 0;
                                if (ret & 0x100)
                                {
                                        data->len = 1;
                                        break;
                                }
                        }
                        data->cmd_pos = 1;

                        data->sector_pos = 0;
                        data->len--;
                }
                data->cmd_pos = 0;
                return 1;
                
                case SCSI_READ_10:
                if (!data->cmd_pos)
                {
                        data->addr = cdb[5] | (cdb[4] << 8) | (cdb[3] << 16) | (cdb[2] << 24);
                        data->len = cdb[8] | (cdb[7] << 8);
                        aka31_log("SCSI_READ_10: addr=%08x len=%04x %p\n", data->addr, data->len, (void *)data->f);
                        fseek(data->f, data->addr * 512, SEEK_SET);
                        
                        data->cmd_pos = 1;
                        data->sector_pos = 0;
                }
                else
                        aka31_log("SCSI_READ_10 continue addr=%08x len=%04x sector_pos=%02x\n", data->addr, data->len, data->sector_pos);
                while (data->len)
                {
                        if (data->cmd_pos == 1)
                                fread(data->buf, 512, 1, data->f);
                        data->cmd_pos = 2;
                        for (; data->sector_pos < 512; data->sector_pos++)
                        {
                                int ret = scsi_add_data(data->buf[data->sector_pos]);
                                aka31_log("SCSI_READ_10 sector_pos=%i %x\n", data->sector_pos, ftell(data->f));                                
                                if (ret == -1)
                                        return 0;
                                if (ret & 0x100)
                                {
                                        data->len = 1;
                                        break;
                                }
                        }
                        data->cmd_pos = 1;

                        data->sector_pos = 0;
                        data->len--;
                }
                data->cmd_pos = 0;
                return 1;

                case SCSI_WRITE_6:
                if (!data->cmd_pos)
                {
                        data->addr = cdb[3] | (cdb[2] << 8) | ((cdb[1] & 0x1f) << 16);
                        data->len = cdb[4];
                        if (!data->len)
                                data->len = 256;
                        aka31_log("SCSI_WRITE_6: addr=%08x len=%04x %p\n", data->addr, data->len, (void *)data->f);
                        fseek(data->f, data->addr * 512, SEEK_SET);
                        
                        data->cmd_pos = 1;
                        data->sector_pos = 0;
                }
                else
                        aka31_log("SCSI_WRITE_6 continue addr=%08x len=%04x sector_pos=%02x\n", data->addr, data->len, data->sector_pos);
                while (data->len)
                {
                        for (; data->sector_pos < 512; data->sector_pos++)
                        {
                                int ret = scsi_get_data();
                                if (ret == -1)
                                        return 0;
                                data->buf[data->sector_pos] = ret & 0xff;
                                if (ret & 0x100)
                                {
                                        data->len = 1;
                                        break;
                                }
                        }

                        fwrite(data->buf, 512, 1, data->f);
                        data->sector_pos = 0;
                        data->len--;
                }
                data->cmd_pos = 0;
                return 1;

                case SCSI_WRITE_10:
                if (!data->cmd_pos)
                {
                        data->addr = cdb[5] | (cdb[4] << 8) | (cdb[3] << 16) | (cdb[2] << 24);
                        data->len = cdb[8] | (cdb[7] << 8);
                        aka31_log("SCSI_WRITE_10: addr=%08x len=%04x %p\n", data->addr, data->len, (void *)data->f);
                        fseek(data->f, data->addr * 512, SEEK_SET);
                        
                        data->cmd_pos = 1;
                        data->sector_pos = 0;
                }
                else
                        aka31_log("SCSI_WRITE_10 continue addr=%08x len=%04x sector_pos=%02x\n", data->addr, data->len, data->sector_pos);
                while (data->len)
                {
                        for (; data->sector_pos < 512; data->sector_pos++)
                        {
                                int ret = scsi_get_data();
                                aka31_log("SCSI_WRITE_10 sector_pos=%i %x\n", data->sector_pos, ftell(data->f));
                                if (ret == -1)
                                        return 0;
                                data->buf[data->sector_pos] = ret & 0xff;
                                if (ret & 0x100)
                                {
                                        data->len = 1;
                                        break;
                                }
                        }

                        fwrite(data->buf, 512, 1, data->f);
                        data->sector_pos = 0;
                        data->len--;
                }
                data->cmd_pos = 0;
                return 1;
        }
        
        return 0;
}

scsi_device_t scsi_hd =
{
        scsi_hd_init,
        scsi_hd_close,
        
        scsi_hd_command
};

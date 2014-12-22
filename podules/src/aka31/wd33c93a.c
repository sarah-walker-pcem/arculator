#include <string.h>
#include <stdint.h>
#include "podules-win.h"
#include "aka31.h"
#include "wd33c93a.h"

#include "scsi.h"
#include "scsi_hd.h"

#define AUX_STATUS_DBR 0x01 /*Data Buffer Ready*/
#define AUX_STATUS_PE  0x02 /*Parity Error*/
#define AUX_STATUS_CIP 0x10 /*Command In Progress*/
#define AUX_STATUS_BSY 0x20 /*Busy*/
#define AUX_STATUS_LCI 0x40 /*Last Command Ignored*/
#define AUX_STATUS_INT 0x80 /*Interrupt Pending*/

#define CMD_RESET                   0x00
#define CMD_SEL_W_ATN_AND_TRANSFER  0x08
#define CMD_SEL_WO_ATN_AND_TRANSFER 0x09
#define CMD_TRANSFER_INFO           0x20

#define CMD_MASK 0x7f

#define CMD_SBT 0x80

#define OWNID_EAF 0x08

#define REG_OWNID     0x00
#define REG_CTRL      0x01
#define REG_CDB       0x03
#define REG_TARGETSTAT 0x0f
#define REG_CMD_PHASE 0x10
#define REG_TRANSFER  0x12
#define REG_DESTID    0x15
#define REG_STATUS    0x17
#define REG_CMD       0x18
#define REG_DATA      0x19

#define DMA_MODE(x) ((x >> 5) & 7)

#define DMA_MODE_PIO 0

struct
{
        uint8_t aux_status;
        uint8_t cdb[12];
        uint8_t command;
        uint8_t command_phase;
        uint8_t ctrl;
        uint8_t destid;
        uint8_t ownid;
        uint8_t status;
        uint8_t target_status;
        uint32_t transfer_count;
        
        uint8_t fifo[12];
        int fifo_read, fifo_write;
        
        int disconnect_pending;

        uint8_t info;
        
        int addr_reg;
        podule *p;
} wd;

static scsi_device_t *devices[8];
static void *device_data[8];

void wd33c93a_init()
{
        memset(&wd, 0, sizeof(wd));
        memset(devices, 0, sizeof(devices));
        
        devices[0] = &scsi_hd;
        device_data[0] = devices[0]->init();
}

int scsi_add_data(uint8_t val)
{
        if (wd.command & CMD_SBT)
        {
                wd.fifo[(wd.fifo_write++) % 12] = val;

                aka31_log("Command complete\n");
                switch (wd.command & CMD_MASK)
                {
                        case CMD_TRANSFER_INFO:
                        wd.status = 0x18;
                        break;
                        default:
                        wd.status = 0x16;
                        break;
                }
                wd.aux_status = AUX_STATUS_DBR;
//                wd.p->irq = 1;
        }
        else
//        if (DMA_MODE(wd.ctrl) == DMA_MODE_PIO)
        {
                aka31_log("scsi_add_data: transfer_count=%d\n", wd.transfer_count);
                if (dma_write(0, val, wd.p))
                {
                        aka31_log("No data\n");
                        return -1;
                }
//                wd.fifo[(wd.fifo_write++) % 12] = val;
                if (wd.transfer_count)
                        wd.transfer_count--;
                if (!wd.transfer_count)
                {
                        aka31_log("Command complete\n");
                        switch (wd.command & CMD_MASK)
                        {
                                case CMD_TRANSFER_INFO:
                                wd.status = 0x18;
                                break;
                                default:
                                wd.status = 0x16;
                                break;
                        }
                        wd.aux_status = AUX_STATUS_INT;
                        aka31_sbic_int(wd.p);
                        return 0x100;
                }
        }
        return 0;
}

int scsi_get_data()
{
        int val = dma_read(0, wd.p);
        
        if (val == -1)
                return -1;

//                wd.fifo[(wd.fifo_write++) % 12] = val;
        if (wd.transfer_count)
                wd.transfer_count--;
        if (!wd.transfer_count)
        {
                aka31_log("Command complete\n");
                switch (wd.command & CMD_MASK)
                {
                        case CMD_TRANSFER_INFO:
                        wd.status = 0x18;
                        break;
                        default:
                        wd.status = 0x16;
                        break;
                }
                wd.aux_status = AUX_STATUS_INT;
                aka31_sbic_int(wd.p);
                return val | 0x100;
        }
        
        return val;
}

void scsi_send_complete()
{
        wd.status = 0x16;
        wd.aux_status = AUX_STATUS_INT;
        aka31_sbic_int(wd.p);
}

void scsi_illegal_field()
{
        wd.status = 0x4b;
        wd.aux_status = AUX_STATUS_INT;
        aka31_sbic_int(wd.p);
        wd.info = 2;
}

void scsi_set_phase(uint8_t phase)
{
        wd.command_phase = phase;
}

void scsi_set_irq(uint8_t status)
{
        wd.status = status;
        wd.aux_status = AUX_STATUS_INT;
        aka31_sbic_int(wd.p);
}

void wd33c93a_reset(podule *p)
{
	aka31_log("wd33c93a_reset\n");
	wd.aux_status = AUX_STATUS_INT;
        wd.status = 0x00; /*Reset*/
        aka31_sbic_int(wd.p);
}
        
void wd33c93a_poll(podule *p)
{
        int id = wd.destid & 7;
        
        wd.p = p;
        
        if (wd.aux_status & AUX_STATUS_CIP)
        {
                switch (wd.command & CMD_MASK)
                {
                        case CMD_RESET:
                        aka31_log("Reset command processed\n");
                        wd.aux_status = AUX_STATUS_INT;
                        if (wd.ownid & OWNID_EAF)
                                wd.status = 0x01; /*Reset with advanced features enabled*/
                        else
                                wd.status = 0x00; /*Reset*/
			aka31_sbic_int(wd.p);
                        break;
                        
                        case CMD_SEL_W_ATN_AND_TRANSFER:
                        case CMD_SEL_WO_ATN_AND_TRANSFER:
                        aka31_log("Sel and transfer command %i %p\n", id, (void *)devices[id]);
                        if (!devices[id])
                        {
                                wd.aux_status = AUX_STATUS_INT;
                                wd.status = 0x42; /*Timeout during Select*/
                                wd.command_phase = 0;
                                aka31_sbic_int(wd.p);
                        }
                        else
                        {
                                if (devices[id]->command(wd.cdb, device_data[0]))
                                {
                                        aka31_log("command over\n");
                                        wd.aux_status &= ~AUX_STATUS_CIP;
                                        wd.disconnect_pending = 15;
                                }
                        }
                        break;
                        
                        case CMD_TRANSFER_INFO:
                        scsi_add_data(wd.info);
                        wd.aux_status &= ~AUX_STATUS_CIP;
                        wd.disconnect_pending = 15;
                        break;
                }
        }
        else if (wd.disconnect_pending)
        {
                wd.disconnect_pending--;
                if (!wd.disconnect_pending)
                {
                        aka31_log("Disconnect IRQ\n");
                        wd.status = 0x85;
                        wd.aux_status = AUX_STATUS_INT | 1;
                        wd.target_status = 0;
                        aka31_sbic_int(wd.p);
                }
        }
}

void wd33c93a_write(uint32_t addr, uint8_t val, podule *p)
{
        int reg;
        aka31_log("wd33c93a_write %04x %02x\n", addr, val);
        if (!(addr & 4))
        {
                wd.addr_reg = val & 0x1f;
                return;
        }

        reg = wd.addr_reg;
        if (wd.addr_reg < 0x18)
                wd.addr_reg = (wd.addr_reg + 1) & 0x1f;
        
        switch (reg)
        {
                case REG_OWNID:
                wd.ownid = val;
                break;
                case REG_CDB:
                wd.cdb[0] = val;
                break;
                case REG_CDB+1:
                wd.cdb[1] = val;
                break;
                case REG_CDB+2:
                wd.cdb[2] = val;
                break;
                case REG_CDB+3:
                wd.cdb[3] = val;
                break;
                case REG_CDB+4:
                wd.cdb[4] = val;
                break;
                case REG_CDB+5:
                wd.cdb[5] = val;
                break;
                case REG_CDB+6:
                wd.cdb[6] = val;
                break;
                case REG_CDB+7:
                wd.cdb[7] = val;
                break;
                case REG_CDB+8:
                wd.cdb[8] = val;
                break;
                case REG_CDB+9:
                wd.cdb[9] = val;
                break;
                case REG_CDB+10:
                wd.cdb[10] = val;
                break;
                case REG_CDB+11:
                wd.cdb[11] = val;
                break;

                case REG_TRANSFER:
                wd.transfer_count = (wd.transfer_count & 0x00ffff) | (val << 16);
                break;
                case REG_TRANSFER+1:
                wd.transfer_count = (wd.transfer_count & 0xff00ff) | (val << 8);
                break;
                case REG_TRANSFER+2:
                wd.transfer_count = (wd.transfer_count & 0xffff00) | val;
                break;
                
                case REG_DESTID:
                wd.destid = val;
                break;
                case REG_CMD:
                if (wd.aux_status & AUX_STATUS_CIP)
                {
                        aka31_log("Tried to start new command while old in progress\n");
                        return;
                }
                aka31_log("Start command %02x\n", val);
                wd.aux_status |= AUX_STATUS_CIP;
                wd.command = val;
                break;
                
                default:
                aka31_log("Write to bad WD reg %02x %02x\n", reg, val);
                break;
        }
}

uint8_t wd33c93a_read(uint32_t addr, podule *p)
{
        int reg;
        
        if (!(addr & 4))
        {
                uint8_t temp = wd.aux_status;
                
                if (wd.fifo_read != wd.fifo_write)
                        temp |= AUX_STATUS_DBR;
                        
                return temp;
        }
        
        reg = wd.addr_reg;
        if (wd.addr_reg < 0x18)
                wd.addr_reg = (wd.addr_reg + 1) & 0x1f;
        
        switch (reg)
        {
                case REG_CMD_PHASE:
                return wd.command_phase;

                case REG_CDB:
                return wd.cdb[0];
                case REG_CDB+1:
                return wd.cdb[1];
                case REG_CDB+2:
                return wd.cdb[2];
                case REG_CDB+3:
                return wd.cdb[3];
                case REG_CDB+4:
                return wd.cdb[4];
                case REG_CDB+5:
                return wd.cdb[5];
                case REG_CDB+6:
                return wd.cdb[6];
                case REG_CDB+7:
                return wd.cdb[7];
                case REG_CDB+8:
                return wd.cdb[8];
                case REG_CDB+9:
                return wd.cdb[9];
                case REG_CDB+10:
                return wd.cdb[10];
                case REG_CDB+11:
                return wd.cdb[11];

                case REG_TARGETSTAT:
                return wd.target_status;
                
                case REG_TRANSFER:
                return (wd.transfer_count >> 16) & 0xff;
                case REG_TRANSFER+1:
                return (wd.transfer_count >> 8) & 0xff;
                case REG_TRANSFER+2:
                return wd.transfer_count & 0xff;

                case REG_DESTID:
                return wd.destid;

                case REG_STATUS:
                wd.aux_status &= ~AUX_STATUS_INT;
                aka31_sbic_int_clear(p);
//                p->irq = 0;
                aka31_log("Read status %02x\n", wd.status);
                return wd.status;
                
                case REG_CMD:
                return wd.command;
                
                case REG_DATA:
                if (wd.fifo_read != wd.fifo_write)
                        return wd.fifo[(wd.fifo_write++) % 12];
                return wd.fifo[wd.fifo_write % 12];
                
                default:
                aka31_log("Read from bad WD reg %02x\n", reg);
                break;
        }
        
        return 0xff;
}

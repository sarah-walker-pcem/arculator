#include <string.h>
#include <stdint.h>
#include "aka31.h"
#include "d71071l.h"
#include "wd33c93a.h"

#include "scsi.h"
#include "scsi_hd.h"

#define POLL_TIME_US 100
#define MAX_BYTES_TRANSFERRED_PER_POLL 500



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
#define REG_CDB_SIZE  0x00
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



void wd33c93a_init(wd33c93a_t *wd, podule_t *podule, d71071l_t *dma, struct scsi_bus_t *bus)
{
        memset(wd, 0, sizeof(wd33c93a_t));
        wd->podule = podule;
        wd->dma = dma;
        wd->bus = bus;
        scsi_bus_init(wd->bus, podule);
}

void wd33c93a_close(wd33c93a_t *wd)
{
        scsi_bus_close(wd->bus);
}

int scsi_add_data(wd33c93a_t *wd, uint8_t val)
{
        if (wd->command & CMD_SBT)
                wd->fifo[(wd->fifo_write++) % 12] = val;
        else
        {
                if (dma_write(wd->dma, 0, val))
                {
//                        aka31_log("No data\n");
                        return -1;
                }
        }
        return 0;
}

int scsi_get_data(wd33c93a_t *wd)
{
        int val = dma_read(wd->dma, 0);
        
        if (val == -1)
                return -1;

        return val;
}

void scsi_send_complete(void *controller_p)
{
        wd33c93a_t *wd = controller_p;
        
        wd->status = 0x16;
        wd->aux_status = AUX_STATUS_INT;
        aka31_sbic_int(wd->podule);
}

void scsi_illegal_field(void *controller_p)
{
        wd33c93a_t *wd = controller_p;
        
        wd->status = 0x4b;
        wd->aux_status = AUX_STATUS_INT;
        aka31_sbic_int(wd->podule);
        wd->info = 2;
}

void scsi_select_failed(wd33c93a_t *wd)
{
        wd->aux_status = AUX_STATUS_INT;
        wd->status = 0x42; /*Timeout during Select*/
        wd->command_phase = 0;
        aka31_sbic_int(wd->podule);
}

void scsi_set_phase(void *controller_p, uint8_t phase)
{
        wd33c93a_t *wd = controller_p;
        
        wd->command_phase = phase;
}

void scsi_set_irq(void *controller_p, uint8_t status)
{
        wd33c93a_t *wd = controller_p;
//        aka31_log("scsi_set_irq: %02x\n", status);
        wd->status = status;
        wd->aux_status = AUX_STATUS_INT;
        aka31_sbic_int(wd->podule);
}

void wd33c93a_reset(wd33c93a_t *wd)
{
//	aka31_log("wd33c93a_reset\n");
	wd->aux_status = AUX_STATUS_INT;
        wd->status = 0x00; /*Reset*/
        aka31_sbic_int(wd->podule);
        scsi_bus_reset(wd->bus);
}

void wd33c93a_finish_command(wd33c93a_t *wd)
{
//        aka31_log("wd33c93a_finish_command\n");
        
        switch (wd->command & CMD_MASK)
        {
                case CMD_TRANSFER_INFO:
                wd->status = 0x18;
                break;
                default:
                wd->status = 0x16;
                break;
        }
        wd->aux_status = AUX_STATUS_INT;
        aka31_sbic_int(wd->podule);
        wd->disconnect_pending = 25;
}

void wd33c93a_poll(wd33c93a_t *wd)
{
        int id = wd->destid & 7;
        
        if (wd->aux_status & AUX_STATUS_CIP)
        {
                switch (wd->command & CMD_MASK)
                {
                        case CMD_RESET:
//                        aka31_log("Reset command processed\n");
                        wd->aux_status = AUX_STATUS_INT;
                        if (wd->ownid & OWNID_EAF)
                                wd->status = 0x01; /*Reset with advanced features enabled*/
                        else
                                wd->status = 0x00; /*Reset*/
			aka31_sbic_int(wd->podule);
                        break;
                        
                        case CMD_SEL_W_ATN_AND_TRANSFER:
                        case CMD_SEL_WO_ATN_AND_TRANSFER:
//                        aka31_log("Sel and transfer command %i %p\n", id, NULL/*(void *)devices[id]*/);
                        if (wd->scsi_state == SCSI_STATE_IDLE)
                        {
//                                aka31_log("Run command! %i  %i\n", id, wd->scsi_state);
                                wd->bytes_transferred = 0;
                                wd->scsi_state = SCSI_STATE_SELECT;
                                wd->cdb_idx = 0;
                                switch (wd->cdb[0] & 0xe0)
                                {
                                        case 0x00:
                                        wd->cdb_len = 6;
                                        break;
                                        case 0x20:
                                        wd->cdb_len = 10;
                                        break;
                                        case 0xa0:
                                        wd->cdb_len = 12;
                                        break;
                                        default:
                                        wd->cdb_len = wd->cdb_size;
                                        break;
                                }
                        }
                        break;
                        
                        case CMD_TRANSFER_INFO:
                        scsi_add_data(wd, wd->info);
                        wd->aux_status &= ~AUX_STATUS_CIP;
                        wd->disconnect_pending = 5;
                        break;
                }
        }
        else if (wd->disconnect_pending && !(wd->aux_status & AUX_STATUS_INT))
        {
                wd->disconnect_pending--;
                if (!wd->disconnect_pending)
                {
//                        aka31_log("Disconnect IRQ\n");
                        wd->status = 0x85;
                        wd->aux_status = AUX_STATUS_INT | 1;
                        wd->target_status = 0;
                        aka31_sbic_int(wd->podule);
                }
        }
}

void wd33c93a_write(wd33c93a_t *wd, uint32_t addr, uint8_t val)
{
        int reg;
//        aka31_log("wd33c93a_write %04x %02x\n", addr, val);
        if (!(addr & 4))
        {
                wd->addr_reg = val & 0x1f;
                return;
        }

        reg = wd->addr_reg;
        if (wd->addr_reg < 0x18)
                wd->addr_reg = (wd->addr_reg + 1) & 0x1f;
//        aka31_log("wd33c93a_write %02x %02x\n", reg, val);
        switch (reg)
        {
                case REG_OWNID:
                wd->ownid = val;
                wd->cdb_size = val;
                break;
                case REG_CDB:
                wd->cdb[0] = val;
                break;
                case REG_CDB+1:
                wd->cdb[1] = val;
                break;
                case REG_CDB+2:
                wd->cdb[2] = val;
                break;
                case REG_CDB+3:
                wd->cdb[3] = val;
                break;
                case REG_CDB+4:
                wd->cdb[4] = val;
                break;
                case REG_CDB+5:
                wd->cdb[5] = val;
                break;
                case REG_CDB+6:
                wd->cdb[6] = val;
                break;
                case REG_CDB+7:
                wd->cdb[7] = val;
                break;
                case REG_CDB+8:
                wd->cdb[8] = val;
                break;
                case REG_CDB+9:
                wd->cdb[9] = val;
                break;
                case REG_CDB+10:
                wd->cdb[10] = val;
                break;
                case REG_CDB+11:
                wd->cdb[11] = val;
                break;

                case REG_TRANSFER:
                wd->transfer_count = (wd->transfer_count & 0x00ffff) | (val << 16);
                break;
                case REG_TRANSFER+1:
                wd->transfer_count = (wd->transfer_count & 0xff00ff) | (val << 8);
                break;
                case REG_TRANSFER+2:
                wd->transfer_count = (wd->transfer_count & 0xffff00) | val;
                break;
                
                case REG_DESTID:
                wd->destid = val;
                break;
                case REG_CMD:
                if (wd->aux_status & AUX_STATUS_CIP)
                {
                        aka31_log("Tried to start new command while old in progress\n");
                        return;
                }
//                aka31_log("Start command %02x\n", val);
                wd->aux_status |= AUX_STATUS_CIP;
                wd->command = val;
                break;
                
                default:
//                aka31_log("Write to bad WD reg %02x %02x\n", reg, val);
                break;
        }
}

uint8_t wd33c93a_read(wd33c93a_t *wd, uint32_t addr)
{
        int reg;
        
        if (!(addr & 4))
        {
                uint8_t temp = wd->aux_status;
                
                if (wd->fifo_read != wd->fifo_write)
                        temp |= AUX_STATUS_DBR;
                        
                return temp;
        }
        
        reg = wd->addr_reg;
        if (wd->addr_reg < 0x18)
                wd->addr_reg = (wd->addr_reg + 1) & 0x1f;
        
        switch (reg)
        {
                case REG_CMD_PHASE:
                return wd->command_phase;

                case REG_CDB:
                return wd->cdb[0];
                case REG_CDB+1:
                return wd->cdb[1];
                case REG_CDB+2:
                return wd->cdb[2];
                case REG_CDB+3:
                return wd->cdb[3];
                case REG_CDB+4:
                return wd->cdb[4];
                case REG_CDB+5:
                return wd->cdb[5];
                case REG_CDB+6:
                return wd->cdb[6];
                case REG_CDB+7:
                return wd->cdb[7];
                case REG_CDB+8:
                return wd->cdb[8];
                case REG_CDB+9:
                return wd->cdb[9];
                case REG_CDB+10:
                return wd->cdb[10];
                case REG_CDB+11:
                return wd->cdb[11];

                case REG_TARGETSTAT:
                return wd->target_status;
                
                case REG_TRANSFER:
                return (wd->transfer_count >> 16) & 0xff;
                case REG_TRANSFER+1:
                return (wd->transfer_count >> 8) & 0xff;
                case REG_TRANSFER+2:
                return wd->transfer_count & 0xff;

                case REG_DESTID:
                return wd->destid;

                case REG_STATUS:
                wd->aux_status &= ~AUX_STATUS_INT;
                aka31_sbic_int_clear(wd->podule);
//                p->irq = 0;
//                aka31_log("Read status %02x\n", wd->status);
                return wd->status;
                
                case REG_CMD:
                return wd->command;
                
                case REG_DATA:
                if (wd->fifo_read != wd->fifo_write)
                        return wd->fifo[(wd->fifo_write++) % 12];
                return wd->fifo[wd->fifo_write % 12];
                
                default:
//                aka31_log("Read from bad WD reg %02x\n", reg);
                break;
        }
        
        return 0xff;
}

static int wait_for_bus(scsi_bus_t *bus, int state, int req_needed)
{
        int c;

        for (c = 0; c < 20; c++)
        {
                int bus_state = scsi_bus_read(bus);

                if ((bus_state & (BUS_IO | BUS_CD | BUS_MSG)) == state && (bus_state & BUS_BSY))
                {
                        if (!req_needed || (bus_state & BUS_REQ))
                                return 1;
                }
        }

        return 0;
}

void wd33c93a_process_scsi(wd33c93a_t *wd)
{
        int c;
        int bytes_transferred = 0;
        int target_id = wd->destid & 7;

//        if (wd->scsi_state != SCSI_STATE_IDLE)
//                aka31_log("process_scsi: scsi_state=%i\n", wd->scsi_state);
        switch (wd->scsi_state)
        {
                case SCSI_STATE_IDLE:
                break;

                case SCSI_STATE_SELECT:
                wd->target_status = 0;
//                pclog("Select target ID %i\n", scsi->ccb.target_id);
                scsi_bus_update(wd->bus, BUS_SEL | BUS_SETDATA(1 << target_id));
                if (!(scsi_bus_read(wd->bus) & BUS_BSY) || target_id > 6)
                {
//                        aka31_log("STATE_SCSI_SELECT failed to select target %i\n", target_id);
                        wd->scsi_state = SCSI_STATE_SELECT_FAILED;
                        break;
                }

                scsi_bus_update(wd->bus, 0);
                if (!(scsi_bus_read(wd->bus) & BUS_BSY))
                {
//                        aka31_log("STATE_SCSI_SELECT failed to select target %i 2\n", target_id);
                        wd->scsi_state = SCSI_STATE_SELECT_FAILED;
                        break;
                }

                /*Device should now be selected*/
                if (!wait_for_bus(wd->bus, BUS_CD, 1))
                {
//                        aka31_log("Device failed to request command\n");
                        wd->scsi_state = SCSI_STATE_SELECT_FAILED;
                        break;
                }

                wd->scsi_state = SCSI_STATE_SEND_COMMAND;
                break;

                case SCSI_STATE_SELECT_FAILED:
//                aka31_log("Select failed\n");
                scsi_select_failed(wd);
                wd->scsi_state = SCSI_STATE_IDLE;
                break;

                case SCSI_STATE_SEND_COMMAND:
//                aka31_log(" SCSI_STATE_SEND_COMMAND idx=%i len=%i transferred=%i\n", wd->cdb_idx, wd->cdb_len, bytes_transferred);
                while (wd->cdb_idx < wd->cdb_len && bytes_transferred < MAX_BYTES_TRANSFERRED_PER_POLL)
                {
                        int bus_out;

                        for (c = 0; c < 20; c++)
                        {
                                int bus_state = scsi_bus_read(wd->bus);

                                if (!(bus_state & BUS_BSY))
                                        fatal("SEND_COMMAND - dropped BSY\n");
                                if ((bus_state & (BUS_IO | BUS_CD | BUS_MSG)) != BUS_CD)
                                {
//                                        aka31_log("SEND_COMMAND - bus phase incorrect  %08x\n", bus_state);
                                        wd->scsi_state = SCSI_STATE_NEXT_PHASE;
                                        break;
                                }
                                if (bus_state & BUS_REQ)
                                        break;
                        }
                        if (c == 20)
                        {
//                                aka31_log("SEND_COMMAND timed out\n");
                                break;
                        }
                        if (wd->scsi_state == SCSI_STATE_NEXT_PHASE)
                                break;

                        bus_out = BUS_SETDATA(wd->cdb[wd->cdb_idx]);
//                        aka31_log("  Command send %02x %i\n", wd->cdb[wd->cdb_idx], wd->cdb_len);
                        wd->cdb_idx++;
                        bytes_transferred++;

                        scsi_bus_update(wd->bus, bus_out | BUS_ACK);
                        scsi_bus_update(wd->bus, bus_out & ~BUS_ACK);
                }
                if (wd->cdb_idx == wd->cdb_len)
                        wd->scsi_state = SCSI_STATE_NEXT_PHASE;
                break;

                case SCSI_STATE_NEXT_PHASE:
                /*Wait for SCSI command to move to next phase*/
                for (c = 0; c < 20; c++)
                {
                        int bus_state = scsi_bus_read(wd->bus);

                        if (!(bus_state & BUS_BSY))
                                fatal("NEXT_PHASE - dropped BSY waiting\n");

                        if (bus_state & BUS_REQ)
                        {
                                int bus_out;
//                                aka31_log("SCSI next phase - %x\n", bus_state);
                                switch (bus_state & (BUS_IO | BUS_CD | BUS_MSG))
                                {
                                        case 0:
//                                        aka31_log("Move to write data\n");
                                        if (wd->transfer_count)
                                                wd->scsi_state = SCSI_STATE_WRITE_DATA;
                                        else
                                                wd->scsi_state = SCSI_STATE_WRITE_DATA_NULL;
                                        break;

                                        case BUS_IO:
//                                        aka31_log("Move to read data\n");
                                        if (wd->transfer_count)
                                                wd->scsi_state = SCSI_STATE_READ_DATA;
                                        else
                                                wd->scsi_state = SCSI_STATE_READ_DATA_NULL;
                                        break;

                                        case (BUS_CD | BUS_IO):
//                                        aka31_log("Move to read status\n");
                                        wd->scsi_state = SCSI_STATE_READ_STATUS;
                                        break;

                                        case (BUS_CD | BUS_IO | BUS_MSG):
//                                        aka31_log("Move to read message\n");
                                        wd->scsi_state = SCSI_STATE_READ_MESSAGE;
                                        break;

                                        case BUS_CD:
//                                        aka31_log("Move to BUS_CD\n");
                                        bus_out = BUS_SETDATA(0);

                                        scsi_bus_update(wd->bus, bus_out | BUS_ACK);
                                        scsi_bus_update(wd->bus, bus_out & ~BUS_ACK);
                                        break;

                                        default:
                                        fatal(" Bad new phase %x\n", bus_state);
                                }
                                break;
                        }
                }
                break;

                case SCSI_STATE_END_PHASE:
                /*Wait for SCSI command to move to next phase*/
                for (c = 0; c < 20; c++)
                {
                        int bus_state = scsi_bus_read(wd->bus);

                        if (!(bus_state & BUS_BSY))
                        {
//                                aka31_log("END_PHASE - dropped BSY waiting\n");
                                wd->scsi_state = SCSI_STATE_IDLE;
                                wd33c93a_finish_command(wd);
                                break;
                        }

                        if (bus_state & BUS_REQ)
                                fatal("END_PHASE - unexpected REQ\n");
                }
                break;

                case SCSI_STATE_READ_DATA:
//pclog("READ_DATA %i,%i %i\n", scsi->cdb.data_idx,scsi->cdb.data_len, scsi->cdb.scatter_gather);
                while (wd->transfer_count > 0 && wd->scsi_state == SCSI_STATE_READ_DATA && bytes_transferred < MAX_BYTES_TRANSFERRED_PER_POLL)
                {
                        int d;

                        for (d = 0; d < 20; d++)
                        {
                                int bus_state = scsi_bus_read(wd->bus);

                                if (!(bus_state & BUS_BSY))
                                        fatal("READ_DATA - dropped BSY waiting\n");

                                if ((bus_state & (BUS_IO | BUS_CD | BUS_MSG)) != BUS_IO)
                                {
//                                        aka31_log("READ_DATA - changed phase\n");
                                        wd->scsi_state = SCSI_STATE_NEXT_PHASE;
                                        break;
                                }

                                if (bus_state & BUS_REQ)
                                {
                                        uint8_t data = BUS_GETDATA(bus_state);
                                        int bus_out = 0;

                                        int ret = scsi_add_data(wd, data);
//                                        aka31_log("scsi_add_data: data=%02x ret=%i\n", data, ret);
                                        if (ret == -1)
                                                break;
                                        wd->transfer_count--;

//                                        pclog("Read data %02x %i %06x\n", data, scsi->cdb.data_idx, scsi->cdb.data_pointer + scsi->cdb.data_idx);
                                        scsi_bus_update(wd->bus, bus_out | BUS_ACK);
                                        scsi_bus_update(wd->bus, bus_out & ~BUS_ACK);
                                        break;
                                }
                        }

                        bytes_transferred++;
                }
//                aka31_log("Transferred %i bytes\n", bytes_transferred);
                if (wd->transfer_count == 0)
                        wd->scsi_state = SCSI_STATE_NEXT_PHASE;
                break;

                case SCSI_STATE_WRITE_DATA:
                while (wd->transfer_count > 0 && wd->scsi_state == SCSI_STATE_WRITE_DATA && bytes_transferred < MAX_BYTES_TRANSFERRED_PER_POLL)
                {
                        int d;

                        for (d = 0; d < 20; d++)
                        {
                                int bus_state = scsi_bus_read(wd->bus);

                                if (!(bus_state & BUS_BSY))
                                        fatal("WRITE_DATA - dropped BSY waiting\n");

                                if ((bus_state & (BUS_IO | BUS_CD | BUS_MSG)) != 0)
                                {
//                                        aka31_log("WRITE_DATA - changed phase\n");
                                        wd->scsi_state = SCSI_STATE_NEXT_PHASE;
                                        break;
                                }

                                if (bus_state & BUS_REQ)
                                {
                                        int data;// = BUS_GETDATA(bus_state);
                                        int bus_out;

                                        data = scsi_get_data(wd);
                                        if (data == -1)
                                                break;
                                        
                                        wd->transfer_count--;

//                                        aka31_log("Write data %02x %i\n", data);
                                        bus_out = BUS_SETDATA(data);
                                        scsi_bus_update(wd->bus, bus_out | BUS_ACK);
                                        scsi_bus_update(wd->bus, bus_out & ~BUS_ACK);
                                        break;
                                }
                        }

                        bytes_transferred++;
                }
                if (wd->transfer_count == 0)
                        wd->scsi_state = SCSI_STATE_NEXT_PHASE;
                break;

                case SCSI_STATE_READ_STATUS:
                for (c = 0; c < 20; c++)
                {
                        int bus_state = scsi_bus_read(wd->bus);

                        if (!(bus_state & BUS_BSY))
                                fatal("READ_STATUS - dropped BSY waiting\n");

                        if ((bus_state & (BUS_IO | BUS_CD | BUS_MSG)) != (BUS_CD | BUS_IO))
                        {
//                                aka31_log("READ_STATUS - changed phase\n");
                                wd->scsi_state = SCSI_STATE_NEXT_PHASE;
                                break;
                        }

                        if (bus_state & BUS_REQ)
                        {
                                uint8_t status = BUS_GETDATA(bus_state);
                                int bus_out = 0;

//                                pclog("Read status %02x\n", status);
                                wd->target_status = status;

                                scsi_bus_update(wd->bus, bus_out | BUS_ACK);
                                scsi_bus_update(wd->bus, bus_out & ~BUS_ACK);

                                wd->scsi_state = SCSI_STATE_NEXT_PHASE;
                                break;
                        }
                }
                break;

                case SCSI_STATE_READ_MESSAGE:
                for (c = 0; c < 20; c++)
                {
                        int bus_state = scsi_bus_read(wd->bus);

                        if (!(bus_state & BUS_BSY))
                                fatal("READ_MESSAGE - dropped BSY waiting\n");

                        if ((bus_state & (BUS_IO | BUS_CD | BUS_MSG)) != (BUS_CD | BUS_IO | BUS_MSG))
                        {
//                                aka31_log("READ_MESSAGE - changed phase\n");
                                wd->scsi_state = SCSI_STATE_NEXT_PHASE;
                                break;
                        }

                        if (bus_state & BUS_REQ)
                        {
                                uint8_t msg = BUS_GETDATA(bus_state);
                                int bus_out = 0;

//                                pclog("Read message %02x\n", msg);
                                scsi_bus_update(wd->bus, bus_out | BUS_ACK);
                                scsi_bus_update(wd->bus, bus_out & ~BUS_ACK);

                                switch (msg)
                                {
                                        case MSG_COMMAND_COMPLETE:
                                        wd->scsi_state = SCSI_STATE_END_PHASE;
                                        break;

                                        default:
                                        fatal("READ_MESSAGE - unknown message %02x\n", msg);
                                }
                                break;
                        }
                }
                break;

                case SCSI_STATE_READ_DATA_NULL:
//pclog("READ_DATA %i,%i %i\n", scsi->cdb.data_idx,scsi->cdb.data_len, scsi->cdb.scatter_gather);
                while (wd->scsi_state == SCSI_STATE_READ_DATA_NULL && bytes_transferred < MAX_BYTES_TRANSFERRED_PER_POLL)
                {
                        int d;

                        for (d = 0; d < 20; d++)
                        {
                                int bus_state = scsi_bus_read(wd->bus);

                                if (!(bus_state & BUS_BSY))
                                        fatal("READ_DATA_NULL - dropped BSY waiting\n");

                                if ((bus_state & (BUS_IO | BUS_CD | BUS_MSG)) != BUS_IO)
                                {
//                                        aka31_log("READ_DATA_NULL - changed phase\n");
                                        wd->scsi_state = SCSI_STATE_NEXT_PHASE;
                                        break;
                                }

                                if (bus_state & BUS_REQ)
                                {
                                        uint8_t data = 0;
                                        int bus_out = 0;
//                                        pclog("Read data %02x %i %06x\n", data, scsi->cdb.data_idx, scsi->cdb.data_pointer + scsi->cdb.data_idx);
                                        scsi_bus_update(wd->bus, bus_out | BUS_ACK);
                                        scsi_bus_update(wd->bus, bus_out & ~BUS_ACK);
                                        break;
                                }
                        }

                        bytes_transferred++;
                }
                break;

                case SCSI_STATE_WRITE_DATA_NULL:
                while (wd->scsi_state == SCSI_STATE_WRITE_DATA_NULL && bytes_transferred < MAX_BYTES_TRANSFERRED_PER_POLL)
                {
                        int d;

                        for (d = 0; d < 20; d++)
                        {
                                int bus_state = scsi_bus_read(wd->bus);

                                if (!(bus_state & BUS_BSY))
                                        fatal("WRITE_DATA_NULL - dropped BSY waiting\n");

                                if ((bus_state & (BUS_IO | BUS_CD | BUS_MSG)) != 0)
                                {
//                                        aka31_log("WRITE_DATA_NULL - changed phase\n");
                                        wd->scsi_state = SCSI_STATE_NEXT_PHASE;
                                        break;
                                }

                                if (bus_state & BUS_REQ)
                                {
                                        int bus_out = BUS_SETDATA(0);
                                        
                                        scsi_bus_update(wd->bus, bus_out | BUS_ACK);
                                        scsi_bus_update(wd->bus, bus_out & ~BUS_ACK);
                                        break;
                                }
                        }

                        bytes_transferred++;
                }
                break;

                default:
                fatal("Unknown SCSI_state %d\n", wd->scsi_state);
        }
}

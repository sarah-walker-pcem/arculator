#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "aka31.h"
#include "scsi.h"
#include "scsi_cd.h"
#include "scsi_hd.h"

#define STATE_IDLE 0
#define STATE_COMMAND 1
#define STATE_COMMANDWAIT 2
#define STATE_DATAIN 3
#define STATE_DATAOUT 4
#define STATE_STATUS 5
#define STATE_MESSAGEIN 6
#define STATE_PHASESEL 7

#define SET_BUS_STATE(bus, state) bus->bus_out = (bus->bus_out & ~(BUS_CD | BUS_IO | BUS_MSG)) | (state & (BUS_CD | BUS_IO | BUS_MSG))

static const int cmd_len[8] = {6, 10, 10, 6, 16, 12, 6, 6};
/*Toshiba CD-ROM vendor commands are 10 bytes long*/
static const int specific_cmd_len[256] =
{
        [0xc0] = 10,
        [0xc1] = 10,
        [0xc2] = 10,
        [0xc3] = 10,
        [0xc4] = 10,
        [0xc6] = 10,
        [0xc7] = 10,
        [0xc8] = 10
};

static int get_dev_id(uint8_t data)
{
        int c;
        
        for (c = 0; c < 8; c++)
        {
                if (data & (1 << c))
                        return c;
        }
        
        return -1;
}

int scsi_bus_update(scsi_bus_t *bus, int bus_assert)
{
        scsi_device_t *dev = NULL;
        void *dev_data = NULL;

//        aka31_log("scsi_hd_bus_update: state=%d bus_assert=%04x bus_in=%02x %i %02x %p\n", bus->state, bus_assert, bus->bus_in, bus->clear_req, bus->bus_out, bus);

        if (bus_assert & BUS_ARB)
                bus->state = STATE_IDLE;

        if (bus->dev_id != -1)
        {
                dev = bus->devices[bus->dev_id];
                dev_data = bus->device_data[bus->dev_id];
        }
        
        switch (bus->state)
        {
                case STATE_IDLE:
                bus->clear_req = bus->change_state_delay = bus->new_req_delay = 0;
                if ((bus_assert & BUS_SEL) && !(bus_assert & BUS_BSY))
                {
                        uint8_t sel_data = BUS_GETDATA(bus_assert);
                        
                        //bus->state = STATE_PHASESEL;
                        bus->dev_id = get_dev_id(sel_data);
//                        aka31_log("bus->dev_id=%i device=%p\n", bus->dev_id, (bus->dev_id != -1) ? bus->devices[bus->dev_id] : 0);
//                        pclog("PHASESEL %02x %2x %i %p\n", sel_data, bus->bus_out, bus->dev_id, bus->devices[bus->dev_id]);
                        if (bus->dev_id != -1 && bus->devices[bus->dev_id])
                        {
                                bus->bus_out |= BUS_BSY;
                                bus->state = STATE_PHASESEL;
//                                pclog("Move to phase sel\n");
                        }
                        break;
                }
                break;
                case STATE_PHASESEL:
                if (!(bus_assert & BUS_SEL))
                {
                        if (!(bus_assert & BUS_ATN))
                        {
/*                                uint8_t sel_data = BUS_GETDATA(bus_assert);
                                
                                bus->dev_id = get_dev_id(sel_data);*/
//                                pclog("STATE_PHASESEL: %i %p\n", bus->dev_id, bus->devices[bus->dev_id]);
                                if (bus->dev_id != -1 && bus->devices[bus->dev_id])
                                {
                                        bus->state = STATE_COMMAND;
                                        bus->bus_out = BUS_BSY | BUS_REQ;
                                        bus->command_pos = 0;
                                        SET_BUS_STATE(bus, BUS_CD);
                                }
                                else
                                {
                                        bus->state = STATE_IDLE;
                                        bus->bus_out = 0;
                                }
                        }
                        else
                                fatal("dropped sel %x\n", bus_assert & BUS_ATN);
                }
                break;
                case STATE_COMMAND:
                if ((bus_assert & BUS_ACK) && !(bus->bus_in & BUS_ACK))
                {
                        int len;
                        
//                        aka31_log("  get data %02x\n", BUS_GETDATA(bus_assert));
                        bus->command[bus->command_pos++] = BUS_GETDATA(bus_assert);
                        bus->clear_req = 3;
                        bus->new_state = bus->bus_out & (BUS_IO | BUS_CD | BUS_MSG);
                        bus->bus_out &= ~BUS_REQ;
                        
                        if (bus->is_atapi)
                                len = 12;
                        else if (specific_cmd_len[bus->command[0]])
                                len = specific_cmd_len[bus->command[0]];
                        else
                                len = cmd_len[bus->command[0] >> 5];
                        
                        if (bus->command_pos == len)
                        {
                                int new_state;
                                
                                dev->start_command(dev_data);
                                new_state = dev->command(bus->command, dev_data);
//                                pclog("COMMAND new_state = %x\n", new_state);
                                if ((new_state & (BUS_IO | BUS_CD | BUS_MSG)) == BUS_CD)
                                {
                                        bus->state = STATE_COMMANDWAIT;
                                        bus->clear_req = 0;
                                }
                                else
                                {
//                                        pclog("Set change_state_delay COMMAND\n");
                                        bus->new_state = new_state;
                                        bus->change_state_delay = 4;
                                }
                        }
                }
                break;

                case STATE_COMMANDWAIT:
                {
                        int new_state;
                        
//                        pclog("COMMANDWAIT\n");
                        new_state = dev->command(bus->command, dev_data);
//                        pclog("new_state=%x\n", new_state);
                        if ((new_state & (BUS_IO | BUS_CD | BUS_MSG)) != BUS_CD)
                        {
//                                pclog("Set change_state_delay\n");
                                bus->new_state = new_state;
                                bus->change_state_delay = 4;
                                bus->clear_req = 4;
                        }
                }
                break;
                                        
                case STATE_DATAIN:
                if ((bus_assert & BUS_ACK) && !(bus->bus_in & BUS_ACK))
                {
                        if (dev->read_complete(dev_data))
                        {
//                                pclog("Read complete\n");
                                bus->bus_out &= ~BUS_REQ;
                                //bus->clear_req = 3;
                                bus->new_state = BUS_CD | BUS_IO;
//                                pclog("change_state_delay STATE_DATAIN\n");
                                bus->change_state_delay = 4;
                                bus->new_req_delay = 8;
//                                SET_BUS_STATE(data, BUS_CD | BUS_IO);
                        }
                        else
                        {
                                uint8_t val = dev->read(dev_data);
                                
//                                pclog("  Read data pos %04x %02x  %x %x\n", bus->data_pos_read, bus->data_in[bus->data_pos_read], bus->data_pos_read, bus->data_pos_write);
                                bus->bus_out = (bus->bus_out & ~BUS_DATAMASK) | BUS_SETDATA(val) | BUS_DBP | BUS_REQ;
//                                bus->bus_out |= BUS_REQ;
                                bus->clear_req = 3;
                                bus->bus_out &= ~BUS_REQ;
                                bus->new_state = BUS_IO;
                        }
//                        scsi_hd_command(bus->command, data);
                }
                break;

                case STATE_DATAOUT:
                if ((bus_assert & BUS_ACK) && !(bus->bus_in & BUS_ACK))
                {
                        dev->write(BUS_GETDATA(bus_assert), dev_data);
//                        pclog("Got data %02x\n", BUS_GETDATA(bus_assert));
//                        pclog("  Write data pos %04x %02x  %08x %08x\n", bus->data_pos_write-1, bus->data_out[bus->data_pos_write-1], cpu_getd(5), cpu_getd(6));

                        if (dev->write_complete(dev_data))
                        {
                                int new_state;
                                //pclog("  Write data, command complete\n");
                                bus->bus_out &= ~BUS_REQ;
                                new_state = dev->command(bus->command, dev_data);

                                if ((new_state & (BUS_IO | BUS_CD | BUS_MSG)) == BUS_CD)
                                {
                                        bus->state = STATE_COMMANDWAIT;
                                        bus->clear_req = 0;
                                }
                                else
                                {
                                        bus->new_state = new_state;
//                                        pclog("change_state_delay STATE_DATAOUT\n");
                                        bus->change_state_delay = 4;
                                        bus->new_req_delay = 8;
                                }
                        }
                        else
                        {
                                bus->bus_out |= BUS_REQ;
                        }
                }
                break;
                
                case STATE_STATUS:
                if ((bus_assert & BUS_ACK) && !(bus->bus_in & BUS_ACK))
                {
                        bus->bus_out &= ~BUS_REQ;
/*                        bus->clear_req = 3;*/
                        bus->new_state = BUS_CD | BUS_IO | BUS_MSG;
//                                pclog("change_state_delay STATE_STATUS\n");
                        bus->change_state_delay = 4;
                        bus->new_req_delay = 8;
/*                        bus->state = STATE_MESSAGEIN;*/
                }
                break;
                
                case STATE_MESSAGEIN:
                if ((bus_assert & BUS_ACK) && !(bus->bus_in & BUS_ACK))
                {
//                        pclog("Scheduling change to idle\n");
                        bus->bus_out &= ~BUS_REQ;
                        bus->new_state = BUS_IDLE;
//                                pclog("change_state_delay STATE_MESSAGEIN\n");
                        bus->change_state_delay = 4;
/*                        bus->bus_out &= ~BUS_BSY;
                        SET_BUS_STATE(bus, BUS_CD | BUS_IO);
                        bus->state = STATE_IDLE;*/
                }
                break;
        }
        bus->bus_in = bus_assert;
        
        return bus->bus_out | bus->bus_in;
}

int scsi_bus_read(scsi_bus_t *bus)
{
        scsi_device_t *dev = NULL;
        void *dev_data = NULL;

        if (bus->dev_id != -1)
        {
                dev = bus->devices[bus->dev_id];
                dev_data = bus->device_data[bus->dev_id];
        }

//        pclog("scsi_hd_bus_read: bus_out=%02x bus_in=%02x  %i %i %i\n", bus->bus_out, bus->bus_in, bus->clear_req, bus->change_state_delay, bus->new_req_delay);
        
        if (bus->clear_req)
        {
                bus->clear_req--;
                if (!bus->clear_req)
                {
                        SET_BUS_STATE(bus, bus->new_state);
//                        pclog("clear_req\n");
                        bus->bus_out |= BUS_REQ;
                }
        }
        
        if (bus->change_state_delay)
        {
                bus->change_state_delay--;
                if (!bus->change_state_delay)
                {
                        uint8_t val;
                        
//                        pclog("change_state_delay %08x\n", bus->bus_out & (BUS_IO | BUS_CD | BUS_MSG | BUS_IDLE));
                        SET_BUS_STATE(bus, bus->new_state);

                        switch (bus->bus_out & (BUS_IO | BUS_CD | BUS_MSG))
                        {
                                case BUS_IO:
                                bus->state = STATE_DATAIN;
                                val = dev->read(dev_data);
                                bus->bus_out = (bus->bus_out & ~BUS_DATAMASK) | BUS_SETDATA(val) | BUS_DBP;
                                break;
                                
                                case 0:
                                if (bus->new_state & BUS_IDLE)
                                {
                                        bus->state = STATE_IDLE;
                                        bus->bus_out &= ~BUS_BSY;
                                }
                                else
                                        bus->state = STATE_DATAOUT;
                                break;

                                case (BUS_IO | BUS_CD):
                                bus->state = STATE_STATUS;
                                bus->bus_out = (bus->bus_out & ~BUS_DATAMASK) | BUS_SETDATA(dev->get_status(dev_data)) | BUS_DBP;
                                break;
                                
                                case (BUS_CD | BUS_IO | BUS_MSG):
                                bus->state = STATE_MESSAGEIN;
                                bus->bus_out = (bus->bus_out & ~BUS_DATAMASK) | BUS_SETDATA(0) | BUS_DBP;
                                break;
                                
                                default:
                                fatal("change_state_delay bad state %x\n", bus->bus_out);
                        }

                }
        }
        if (bus->new_req_delay)
        {
                bus->new_req_delay--;
                if (!bus->new_req_delay)
                {
//                        pclog("new_req_delay\n");
                        bus->bus_out |= BUS_REQ;
                }
        }
        
//        pclog("  bus_out now %02x\n", bus->bus_out);
        return bus->bus_out;// | bus->bus_in;
}

int scsi_bus_match(scsi_bus_t *bus, int bus_assert)
{
//        pclog("bus_match  %02x %02x\n", bus_assert & (BUS_CD | BUS_IO | BUS_MSG), bus->bus_out & (BUS_CD | BUS_IO | BUS_MSG));

        return (bus_assert & (BUS_CD | BUS_IO | BUS_MSG)) == (bus->bus_out & (BUS_CD | BUS_IO | BUS_MSG));
}

void scsi_bus_kick(scsi_bus_t *bus)
{
//        pclog("scsi_bus_kick\n");
        scsi_bus_update(bus, 0);
}

void scsi_bus_atapi_init(scsi_bus_t *bus, scsi_device_t *device, int id, struct atapi_device_t *atapi_dev)
{
	memset(bus->devices, 0, sizeof(bus->devices));
	memset(bus->device_data, 0, sizeof(bus->device_data));

        bus->devices[0] = device;
        bus->device_data[0] = bus->devices[0]->atapi_init(bus, id, atapi_dev);
        if (!bus->device_data[0])
                bus->devices[0] = NULL;
        
        bus->is_atapi = 1;
}

void scsi_bus_init(scsi_bus_t *bus, podule_t *podule)
{
        int c;
        
	memset(bus->devices, 0, sizeof(bus->devices));
	memset(bus->device_data, 0, sizeof(bus->device_data));

	for (c = 0; c < 7; c++)
	{
                char config_name[20];
                const char *type;
                
                sprintf(config_name, "device%i_type", c);
                type = podule_callbacks->config_get_string(podule, config_name, "none");
                
                if (!strcmp(type, "hd"))
                        bus->devices[c] = &scsi_hd;
                else if (!strcmp(type, "cd"))
                        bus->devices[c] = &scsi_cd;
                else
                        bus->devices[c] = NULL;

                if (bus->devices[c])
                {
                        bus->device_data[c] = bus->devices[c]->init(bus, c, podule);
                        if (!bus->device_data[c])
                                bus->devices[c] = NULL;
                }
	}

        bus->is_atapi = 0;
}

void scsi_bus_close(scsi_bus_t *bus)
{
        int c;
        
	for (c = 0; c < 8; c++)
	{
                if (bus->device_data[c])
                        bus->devices[c]->close(bus->device_data[c]);
        }

	memset(bus->devices, 0, sizeof(bus->devices));
	memset(bus->device_data, 0, sizeof(bus->device_data));
}

void scsi_bus_reset(scsi_bus_t *bus)
{
        int c;
        
        bus->state = STATE_IDLE;
        bus->clear_req = 0;
        bus->change_state_delay = 0;
        bus->new_req_delay = 0;
        bus->bus_in = bus->bus_out = 0;
        bus->command_pos = 0;

	for (c = 0; c < 8; c++)
	{
                if (bus->device_data[c] && bus->devices[c]->reset)
                        bus->devices[c]->reset(bus->device_data[c]);
        }
}

void scsi_bus_timer_run(scsi_bus_t *bus, int time_us)
{
        int c;

        for (c = 0; c < 8; c++)
        {
                if (bus->device_timer_us[c])
                {
                        bus->device_timer_us[c] -= time_us;
                        if (bus->device_timer_us[c] <= 0)
                        {
                                bus->device_timer_us[c] = 0;
                                bus->devices[c]->timer_callback(bus->device_data[c]);
                        }
                }
        }
}

void scsi_bus_device_set_timer(scsi_bus_t *bus, int id, int timer_us)
{
        bus->device_timer_us[id] = timer_us;
}

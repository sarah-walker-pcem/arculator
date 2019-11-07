/*Arculator 2.0 by Sarah Walker
  DS2401 unique ID emulation*/
#include <string.h>
#include "arc.h"
#include "config.h"
#include "ds2401.h"
#include "timer.h"

#define RESET_PULSE_LENGTH 480 /*Reset pulse line low for >= 480 us = reset*/
#define PRESENCE_DELAY ((15+60)/2) /*Delay between reset pulses = 15 - 60 us*/
#define PRESENCE_PULSE_LENGTH ((60+240)/2) /*Presence pulse low for 60 - 240 us*/
#define PULSE_THRESHOLD ((15+60)/2) /*1 <= 15us, 0 >= 60us*/
#define DATA_ZERO_PULSE_LENGTH 60 /*Zero pulse = 60-120 us, RISC OS uses slot time of 90us*/

enum
{
        STATE_IDLE = 0,
        STATE_PRESENCE_DELAY,
        STATE_PRESENCE_PULSE,
        STATE_COMMAND,
        STATE_READ_ROM
};

static struct
{
        int old_val;
        uint64_t old_tsc;
        
        int rx_val;
        
        int state;
        int state_pos;
        
        uint8_t command;
        
        uint64_t id;
        
        emu_timer_t timer;
} ds2401;

/*Taken from Maxim Application Note 27*/
static uint8_t crc_table[256] =
{
        0, 94, 188, 226, 97, 63, 221, 131, 194, 156, 126, 32, 163, 253, 31, 65,
        157, 195, 33, 127, 252, 162, 64, 30, 95, 1, 227, 189, 62, 96, 130, 220,
        35, 125, 159, 193, 66, 28, 254, 160, 225, 191, 93, 3, 128, 222, 60, 98,
        190, 224, 2, 92, 223, 129, 99, 61, 124, 34, 192, 158, 29, 67, 161, 255,
        70, 24, 250, 164, 39, 121, 155, 197, 132, 218, 56, 102, 229, 187, 89,7,
        219, 133, 103, 57, 186, 228, 6, 88, 25, 71, 165, 251, 120, 38, 196, 154,
        101, 59, 217, 135, 4, 90, 184, 230, 167, 249, 27, 69, 198, 152, 122, 36,
        248, 166, 68, 26, 153, 199, 37, 123, 58, 100, 134, 216, 91, 5, 231, 185,
        140, 210, 48, 110, 237, 179, 81, 15, 78, 16, 242, 172, 47, 113, 147, 205,
        17, 79, 173, 243, 112, 46, 204, 146, 211, 141, 111, 49, 178, 236, 14,80,
        175, 241, 19, 77, 206, 144, 114, 44, 109, 51, 209, 143, 12, 82, 176, 238,
        50, 108, 142, 208, 83, 13, 239, 177, 240, 174, 76, 18, 145, 207, 45, 115,
        202, 148, 118, 40, 171, 245, 23, 73, 8, 86, 180, 234, 105, 55, 213, 139,
        87, 9, 235, 181, 54, 104, 138, 212, 149, 203, 41, 119, 244, 170, 72, 22,
        233, 183, 85, 11, 136, 214, 52, 106, 43, 117, 151, 201, 74, 20, 246, 168,
        116, 42, 200, 150, 21, 75, 169, 247, 182, 232, 10, 84, 215, 137, 107,53
};

static void ds2401_callback(void *p);

void ds2401_init(void)
{
        uint8_t crc;
        
        memset(&ds2401, 0, sizeof(ds2401));

        ds2401.rx_val = 1;
        timer_add(&ds2401.timer, ds2401_callback, NULL, 0);
        
        ds2401.id = 0x01ull;
        ds2401.id |= ((uint64_t)unique_id << 8);
       // ds2401.id |= (0x123456789abcull << 8);
        
        crc = 0;
        crc = crc_table[crc ^ (ds2401.id & 0xff)];
        crc = crc_table[crc ^ ((ds2401.id >> 8) & 0xff)];
        crc = crc_table[crc ^ ((ds2401.id >> 16) & 0xff)];
        crc = crc_table[crc ^ ((ds2401.id >> 24) & 0xff)];
        crc = crc_table[crc ^ ((ds2401.id >> 32) & 0xff)];
        crc = crc_table[crc ^ ((ds2401.id >> 40) & 0xff)];
        crc = crc_table[crc ^ ((ds2401.id >> 48) & 0xff)];
        ds2401.id |= (uint64_t)crc << 56;

}

void ds2401_write(int val)
{
        if (!val && ds2401.old_val)
        {
                /*Start of pulse. Start timer*/
                /*1ms, longer than any valid pulse in 1-wire protocol*/
                ds2401.old_tsc = tsc;
//                timer_set_delay_u64(&ds2401.timer, TIMER_USEC * 1000);
        }
        else if (val && !ds2401.old_val)
        {
                /*End of pulse. Read timer to calculate length of pulse*/
                uint64_t pulse_length_tsc = tsc - ds2401.old_tsc;
                uint64_t pulse_length_us = pulse_length_tsc / TIMER_USEC;

//                rpclog("ds2401 pulse: %llu us %lli\n", pulse_length_us, pulse_length_tsc >> 32);

                if (pulse_length_us > RESET_PULSE_LENGTH)
                {
                        ds2401.rx_val = 1;
                        ds2401.state = STATE_PRESENCE_DELAY;
                        timer_set_delay_u64(&ds2401.timer, TIMER_USEC * PRESENCE_DELAY);
                }
                else switch (ds2401.state)
                {
                        case STATE_COMMAND:
                        if (pulse_length_us < PULSE_THRESHOLD)
                                ds2401.command |= (1 << ds2401.state_pos);
                        ds2401.state_pos++;
                        if (ds2401.state_pos == 8)
                        {
                                if (ds2401.command == 0x0f)
                                {
                                        ds2401.state = STATE_READ_ROM;
                                        ds2401.state_pos = 0;
                                }
#ifndef RELEASE_BUILD
                                else
                                        fatal("DS2401 command %02x\n", ds2401.command);
#endif
                        }
                        break;
                        
                        case STATE_READ_ROM:
//                        rpclog("DS2401 shift bit %i %i  %016llx\n", ds2401.state_pos, (ds2401.id & (1ull << ds2401.state_pos)) ? 1 : 0, ds2401.id);
                        if (ds2401.id & (1ull << ds2401.state_pos))
                                ds2401.rx_val = 1; /*1 = short pulse*/
                        else
                                ds2401.rx_val = 0; /*0 = long pulse*/
                        timer_set_delay_u64(&ds2401.timer, TIMER_USEC * DATA_ZERO_PULSE_LENGTH);
                        break;

                }
//                uint32_t remaining_us = timer_get_remaining_us(&ds2401.timer);
//                uint32_t pulse_length = 1000 - remaining_us;
                
//                rpclog("ds2401 pulse: %u us\n", pulse_length);
        }
        ds2401.old_val = val;
}

int ds2401_read(void)
{
        return ds2401.rx_val;
}


static void ds2401_callback(void *p)
{
//        rpclog("ds2401_callback: state=%i\n", ds2401.state);
        
        switch (ds2401.state)
        {
                case STATE_PRESENCE_DELAY:
                ds2401.rx_val = 0;
                ds2401.state = STATE_PRESENCE_PULSE;
                timer_set_delay_u64(&ds2401.timer, TIMER_USEC * PRESENCE_PULSE_LENGTH);
                break;

                case STATE_PRESENCE_PULSE:
                ds2401.rx_val = 1;
                ds2401.state = STATE_COMMAND;
                ds2401.state_pos = 0;
                ds2401.command = 0;
                break;
                
                case STATE_READ_ROM:
                ds2401.rx_val = 1;
                ds2401.state_pos++;
                if (ds2401.state_pos == 64)
                        ds2401.state = STATE_IDLE;
                break;
        }
}

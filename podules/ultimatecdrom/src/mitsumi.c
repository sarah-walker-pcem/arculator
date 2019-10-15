/*Mitsumi CD-ROM drive emulation
  Currently emulates an FX001D (double speed drive supplied with HCCS Ultimate CD-ROM interface)
*/
#include <stdint.h>
#include "mitsumi.h"
#include "ultimatecdrom.h"
#include "cdrom.h"

ATAPI *atapi;

/*Constants mainly borrowed from Linux & FreeBSD MCD drivers*/
#define STATUS_CMD_CHECK   (1 << 0)
#define STATUS_BUSY        (1 << 1)
#define STATUS_READ_ERROR  (1 << 2)
#define STATUS_DISC_TYPE   (1 << 3)
#define STATUS_SERVO_CHECK (1 << 4)
#define STATUS_DISC_CHANGE (1 << 5)
#define STATUS_DISC_READY  (1 << 6)
#define STATUS_DOOR_OPEN   (1 << 7)

#define FLAGS_DATA_AVAIL   (1 << 1)
#define FLAGS_STATUS_AVAIL (1 << 2)

#define MODE_PLAY_AUDIO    (1 << 0)
#define MODE_TOC           (1 << 2)
#define MODE_DATA_LENGTH   (1 << 6)

#define CMD_GET_VOL_INFO   0x10
#define CMD_GET_DISC_INFO  0x11
#define CMD_GET_Q_CHANNEL  0x20
#define CMD_GET_STATUS     0x40
#define CMD_SET_MODE       0x50
#define CMD_SOFT_RESET     0x60
#define CMD_STOP_AUDIO     0x70
#define CMD_CONFIG_DRIVE   0x90
#define CMD_SET_DRIVE_MODE 0xa0
#define CMD_SET_VOLUME     0xae
#define CMD_PLAY_READ      0xc0
#define CMD_PLAY_READ_2X   0xc1
#define CMD_GET_DRIVE_MODE 0xc2
#define CMD_GET_VERSION    0xdc
#define CMD_STOP           0xf0
#define CMD_EJECT          0xf6

static void mitsumi_reset_params(mitsumi_t *mitsumi)
{
        mitsumi->params_wp = 0;
        mitsumi->nr_params = 0;
}

static void mitsumi_reset_status(mitsumi_t *mitsumi)
{
        mitsumi->status_rp = 0;
        mitsumi->status_wp = 0;
}

static void mitsumi_add_status(mitsumi_t *mitsumi, uint8_t val)
{
        mitsumi->status_dat[mitsumi->status_wp++] = val;
}

static void mitsumi_command_start(mitsumi_t *mitsumi, uint8_t cmd)
{
        mitsumi->cmd = cmd;
        
        switch (cmd)
        {
                case CMD_GET_VERSION:
                mitsumi->state = CMD_STATE_BUSY;
                mitsumi->cmd_time = 2;
                break;
                
                case CMD_GET_STATUS:
                mitsumi->state = CMD_STATE_BUSY;
                mitsumi->cmd_time = 2;
                break;

                case CMD_SOFT_RESET:
                mitsumi->state = CMD_STATE_BUSY;
                mitsumi->cmd_time = 2;
                break;
                
                case CMD_GET_DISC_INFO:
                mitsumi->state = CMD_STATE_BUSY;
                mitsumi->cmd_time = 2;
                break;

                case CMD_GET_DRIVE_MODE:
                mitsumi->state = CMD_STATE_BUSY;
                mitsumi->cmd_time = 2;
                break;

                case CMD_GET_VOL_INFO:
                mitsumi->state = CMD_STATE_BUSY;
                mitsumi->cmd_time = 2;
                break;

                case CMD_GET_Q_CHANNEL:
                mitsumi->state = CMD_STATE_BUSY;
                mitsumi->cmd_time = 2;
                break;

                case CMD_STOP:
                mitsumi->state = CMD_STATE_BUSY;
                mitsumi->cmd_time = 2;
                break;

                case CMD_STOP_AUDIO:
                mitsumi->state = CMD_STATE_BUSY;
                mitsumi->cmd_time = 2;
                break;

                case CMD_EJECT:
                mitsumi->state = CMD_STATE_BUSY;
                mitsumi->cmd_time = 2;
                break;

                case CMD_SET_MODE:
                mitsumi_reset_params(mitsumi);
                mitsumi->nr_params = 1;
                mitsumi->state = CMD_STATE_PARAMS;
                break;

                case CMD_SET_DRIVE_MODE:
                mitsumi_reset_params(mitsumi);
                mitsumi->nr_params = 1;
                mitsumi->state = CMD_STATE_PARAMS;
                break;

                case CMD_CONFIG_DRIVE:
                mitsumi_reset_params(mitsumi);
                mitsumi->nr_params = 2;
                mitsumi->state = CMD_STATE_PARAMS;
                break;

                case CMD_PLAY_READ:
                mitsumi_reset_params(mitsumi);
                mitsumi->nr_params = 6;
                mitsumi->state = CMD_STATE_PARAMS;
                mitsumi->sector_count = 0;
                break;

                case CMD_PLAY_READ_2X:
                mitsumi_reset_params(mitsumi);
                mitsumi->nr_params = 6;
                mitsumi->state = CMD_STATE_PARAMS;
                mitsumi->sector_count = 0;
                break;

                default:
                cdfatal("mitsumi_start_command: Unknown MCD command %02x\n", cmd);
                break;
        }
        cdlog("mitsumi_command_start: cmd=%02x cmd_time=%i\n", cmd, mitsumi->cmd_time);
}

static int msf_to_sector(int m, int s, int f)
{
        /*Convert BCD -> bin*/
        m = (m & 0xf) + ((m >> 4) * 10);
        s = (s & 0xf) + ((s >> 4) * 10);
        f = (f & 0xf) + ((f >> 4) * 10);
                
        return ((((m * 60) + s) * 75) + f) - 150;
}

static uint8_t bin_to_bcd(uint8_t in)
{
        uint8_t out = in % 10;
        out += ((in / 10) << 4);
        
        return out;
}

static void mitsumi_add_status_disc_changed(mitsumi_t *mitsumi)
{
        mitsumi_add_status(mitsumi, STATUS_CMD_CHECK | STATUS_DISC_CHANGE | STATUS_DOOR_OPEN);
        mitsumi->state = CMD_STATE_STATUS;
        cdlog("Disc changed\n");
}

static void mitsumi_command_run(mitsumi_t *mitsumi)
{
        cdlog("mitsumi_command_run %02x\n", mitsumi->cmd);
        switch (mitsumi->cmd)
        {
                case CMD_GET_VERSION:
                mitsumi_reset_status(mitsumi);
                mitsumi_add_status(mitsumi, 0);
                mitsumi_add_status(mitsumi, 'D');
                mitsumi_add_status(mitsumi, 3);
                mitsumi->state = CMD_STATE_STATUS;
                break;

                case CMD_GET_STATUS:
                mitsumi_reset_status(mitsumi);
                if (atapi->status() == CD_STATUS_EMPTY)
                        mitsumi_add_status(mitsumi, STATUS_DOOR_OPEN);
                else if (atapi->medium_changed())
                        mitsumi_add_status(mitsumi, STATUS_DISC_CHANGE | STATUS_DOOR_OPEN);
                else if (atapi->status() == CD_STATUS_PLAYING)
                        mitsumi_add_status(mitsumi, STATUS_BUSY | STATUS_DISC_READY);
                else
                        mitsumi_add_status(mitsumi, mitsumi->status);
                mitsumi->state = CMD_STATE_STATUS;
                break;

                case CMD_SOFT_RESET:
                mitsumi_reset_status(mitsumi);
                mitsumi_add_status(mitsumi, 0);
                mitsumi->state = CMD_STATE_STATUS;
                break;

                case CMD_GET_DISC_INFO:
                mitsumi_reset_status(mitsumi);
                if (atapi->medium_changed())
                        mitsumi_add_status_disc_changed(mitsumi);
                else
                {
                        mitsumi_add_status(mitsumi, 0);
                        mitsumi_add_status(mitsumi, 0);
                        mitsumi_add_status(mitsumi, 0);
                        mitsumi_add_status(mitsumi, 0);
                        mitsumi_add_status(mitsumi, 0);
                        mitsumi->state = CMD_STATE_STATUS;
                }
                break;

                case CMD_GET_VOL_INFO:
                mitsumi_reset_status(mitsumi);
                if (atapi->medium_changed())
                        mitsumi_add_status_disc_changed(mitsumi);
                else
                {
                        /*CD-ROM interface returns TOC suitable for ATAPI. Mangle a bit for MCD*/
                        uint8_t buf[12];
                        uint32_t size;
                        
                        /*Read TOC of first track*/
                        atapi->readtoc(buf, 1, 1, 12, 1);
                        size = atapi->size();
                        cdlog("ATAPI size = %i\n", size);

                        mitsumi_add_status(mitsumi, 0);
                
                        mitsumi_add_status(mitsumi, buf[2]); /*First track nr*/
                        mitsumi_add_status(mitsumi, buf[3]); /*Last track nr*/
                        mitsumi_add_status(mitsumi, bin_to_bcd(size / 4500)); /*Volume length*/
                        mitsumi_add_status(mitsumi, bin_to_bcd((size % 4500) / 75));
                        mitsumi_add_status(mitsumi, bin_to_bcd(size % 75));
                        mitsumi_add_status(mitsumi, bin_to_bcd(buf[9])); /*Track 1 start*/
                        mitsumi_add_status(mitsumi, bin_to_bcd(buf[10]));
                        mitsumi_add_status(mitsumi, bin_to_bcd(buf[11]));
                }
                mitsumi->state = CMD_STATE_STATUS;
                break;

                case CMD_GET_Q_CHANNEL:
                mitsumi_reset_status(mitsumi);

                if (atapi->medium_changed())
                        mitsumi_add_status_disc_changed(mitsumi);
                else if (mitsumi->mode & MODE_TOC)
                {
                        int toc_len;
                        int offset;
                        uint32_t size;
                        
                        atapi->medium_changed();
                        toc_len = atapi->readtoc_raw(mitsumi->toc, 2048);
                        size = atapi->size();
                        
                        offset = (mitsumi->read_toc_track*11) + 4;
                        if (offset >= toc_len)
                        {
                                cdlog("CMD_GET_Q_CHANNEL: end of CD! track %i\n", mitsumi->read_toc_track);
//                                mitsumi_add_status(mitsumi, 0x41);
                                mitsumi->read_toc_track = 0;
//                                offset = (mitsumi->read_toc_track*8) + 4;
                                mitsumi_add_status(mitsumi, 0);
                                
                                mitsumi_add_status(mitsumi, 0x54);
                                mitsumi_add_status(mitsumi, /*2*/0xd);
                                mitsumi_add_status(mitsumi, 0);
                                mitsumi_add_status(mitsumi, 0);
                                mitsumi_add_status(mitsumi, 0);
                                mitsumi_add_status(mitsumi, 0);
                                mitsumi_add_status(mitsumi, 0);
                                mitsumi_add_status(mitsumi, 0);
                                mitsumi_add_status(mitsumi, 0);
                                mitsumi_add_status(mitsumi, 0);
                        }
                        else
                        {
                                cdlog("TOC track %02x %02x %02x %02x %02x %02x %02x %02x   %i %i\n",
                                        mitsumi->toc[offset+0],mitsumi->toc[offset+1],mitsumi->toc[offset+2],mitsumi->toc[offset+3],
                                        mitsumi->toc[offset+4],mitsumi->toc[offset+5],mitsumi->toc[offset+6],mitsumi->toc[offset+7], offset, toc_len);
                                mitsumi_add_status(mitsumi, 0);
                                mitsumi_add_status(mitsumi, mitsumi->toc[offset+1]); /*Control + Adr*/
                                mitsumi_add_status(mitsumi, 0);
                                mitsumi_add_status(mitsumi, mitsumi->toc[offset+3]); /*Point index*/
                                mitsumi_add_status(mitsumi, bin_to_bcd(mitsumi->toc[offset+4])); /*Track M*/
                                mitsumi_add_status(mitsumi, bin_to_bcd(mitsumi->toc[offset+5])); /*Track S*/
                                mitsumi_add_status(mitsumi, bin_to_bcd(mitsumi->toc[offset+6])); /*Track F*/
                                mitsumi_add_status(mitsumi, 0);
                                mitsumi_add_status(mitsumi, bin_to_bcd(mitsumi->toc[offset+8]));
                                mitsumi_add_status(mitsumi, bin_to_bcd(mitsumi->toc[offset+9]));
                                mitsumi_add_status(mitsumi, bin_to_bcd(mitsumi->toc[offset+10]));

                                mitsumi->read_toc_track++;
                        }
                }
                else
                {
                        uint8_t subchannel_data[11];
                        
                        atapi->getcurrentsubchannel(subchannel_data, 1);
                        cdlog("CMD_GET_Q_CHANNEL: read Q channel\n");
                        if (atapi->status() == CD_STATUS_PLAYING)
                                mitsumi_add_status(mitsumi, STATUS_BUSY | STATUS_DISC_READY);
                        else
                                mitsumi_add_status(mitsumi, 0);
                        mitsumi_add_status(mitsumi, subchannel_data[0]); /*Control + ADR*/
                        mitsumi_add_status(mitsumi, subchannel_data[1]); /*Track*/
                        mitsumi_add_status(mitsumi, subchannel_data[2]); /*Index*/
                        mitsumi_add_status(mitsumi, bin_to_bcd(subchannel_data[8])); /*Track M*/
                        mitsumi_add_status(mitsumi, bin_to_bcd(subchannel_data[9])); /*Track S*/
                        mitsumi_add_status(mitsumi, bin_to_bcd(subchannel_data[10])); /*Track F*/
                        mitsumi_add_status(mitsumi, 0); /*Unused*/
                        mitsumi_add_status(mitsumi, bin_to_bcd(subchannel_data[4])); /*Disc M*/
                        mitsumi_add_status(mitsumi, bin_to_bcd(subchannel_data[5])); /*Disc M*/
                        mitsumi_add_status(mitsumi, bin_to_bcd(subchannel_data[6])); /*Disc M*/
                }

                mitsumi->state = CMD_STATE_STATUS;
                break;


                case CMD_SET_MODE:
                if ((mitsumi->params[0] & 4) && !(mitsumi->mode & 4))
                        mitsumi->read_toc_track = 0;
                mitsumi->mode = mitsumi->params[0];
                cdlog("CMD_SET_MODE: mode=%02x\n", mitsumi->mode);
                mitsumi_reset_status(mitsumi);
                mitsumi_add_status(mitsumi, 0);
                mitsumi->state = CMD_STATE_STATUS;
                break;

                case CMD_SET_DRIVE_MODE:
                mitsumi->drv_mode = mitsumi->params[0];
                cdlog("CMD_SET_DRIVE_MODE: mode=%02x\n", mitsumi->drv_mode);
                mitsumi_reset_status(mitsumi);
                mitsumi_add_status(mitsumi, 0);
                mitsumi->state = CMD_STATE_STATUS;
                break;

                case CMD_GET_DRIVE_MODE:
                mitsumi_reset_status(mitsumi);
                mitsumi_add_status(mitsumi, 0);
                mitsumi_add_status(mitsumi, mitsumi->drv_mode);
                mitsumi->state = CMD_STATE_STATUS;
                break;

                case CMD_CONFIG_DRIVE:
                cdlog("CMD_CONFIG_DRIVE: %02x %02x\n", mitsumi->params[0], mitsumi->params[1]);
                mitsumi_reset_status(mitsumi);
                mitsumi_add_status(mitsumi, 0);
                mitsumi->state = CMD_STATE_STATUS;
                break;

                case CMD_STOP:
                atapi->stop();
                mitsumi_reset_status(mitsumi);
                mitsumi_add_status(mitsumi, 0);
                mitsumi->state = CMD_STATE_STATUS;
                break;

                case CMD_STOP_AUDIO:
                atapi->stop();
                mitsumi_reset_status(mitsumi);
                mitsumi_add_status(mitsumi, 0);
                mitsumi->state = CMD_STATE_STATUS;
                break;

                case CMD_EJECT:
                atapi->eject();
                mitsumi_reset_status(mitsumi);
                mitsumi_add_status(mitsumi, 0);
                mitsumi->state = CMD_STATE_STATUS;
                break;

                case CMD_PLAY_READ:
                case CMD_PLAY_READ_2X:
                if (atapi->medium_changed())
                {
                        mitsumi_add_status_disc_changed(mitsumi);
                        mitsumi->state = CMD_STATE_STATUS;
                        break;
                }
                if (!mitsumi->sector_count)
                {
                        /*Load parameters*/
                        mitsumi->sector_pos = msf_to_sector(mitsumi->params[0], mitsumi->params[1], mitsumi->params[2]);
                        cdlog("nr_sectors=%i sector_pos=%i\n", mitsumi->nr_sectors_to_read, mitsumi->sector_pos);
                        if (atapi->is_track_audio(mitsumi->sector_pos, 0) && (mitsumi->mode & MODE_PLAY_AUDIO))
                        {
                                mitsumi->sector_end = msf_to_sector(mitsumi->params[3], mitsumi->params[4], mitsumi->params[5]);
                                cdlog("Playing audio track %08x - %08x\n", mitsumi->sector_pos, mitsumi->sector_end);
                                mitsumi->play_read = CMD_PLAY;
                        }
                        else
                        {
                                mitsumi->nr_sectors_to_read = mitsumi->params[5] |
                                                             (mitsumi->params[4] << 8) |
                                                             (mitsumi->params[3] << 16);
                                if (mitsumi->mode & MODE_DATA_LENGTH)
                                        mitsumi->play_read = CMD_READ_RAW;
                                else
                                        mitsumi->play_read = CMD_READ;
                        }
//                        if (mitsumi->nr_sectors_to_read > 20000)
//                                cdfatal("Here\n");
                }
                if (mitsumi->play_read == CMD_PLAY)
                {
                        if (mitsumi->sector_count)
                                cdfatal("Audio play should not get here...\n");
                        if (mitsumi->sector_pos >= mitsumi->sector_end)
                        {
                                cdlog("Play done\n");
                                mitsumi_reset_status(mitsumi);
                                mitsumi_add_status(mitsumi, 0);
                                mitsumi->state = CMD_STATE_STATUS;
                                break;
                        }
                        cdlog("Play audio!\n");
                        mitsumi->sector_count = 1;
                        atapi->playaudio(mitsumi->sector_pos, mitsumi->sector_end-mitsumi->sector_pos, 0);
                        mitsumi_reset_status(mitsumi);
                        mitsumi_add_status(mitsumi, STATUS_BUSY | STATUS_DISC_READY);
                        mitsumi->state = CMD_STATE_STATUS;
                }
                else if (mitsumi->play_read == CMD_READ)
                {
                        if (mitsumi->sector_count == mitsumi->nr_sectors_to_read)
                        {
                                cdlog("Read done\n");
                                mitsumi_reset_status(mitsumi);
                                mitsumi_add_status(mitsumi, 0);
                                mitsumi->state = CMD_STATE_STATUS;
                                break;
                        }
                        if (atapi->readsector(mitsumi->data_buf, mitsumi->sector_pos, 1))
                                cdfatal("Read failed\n");
                        mitsumi->sector_count++;
                        mitsumi->sector_pos++;
                        mitsumi->data_pos = 0;
                        mitsumi->data_len = 2048;
                        mitsumi->state = CMD_STATE_TRANSFER;
                }
                else
                {
                        if (mitsumi->sector_count == mitsumi->nr_sectors_to_read)
                        {
                                cdlog("Read done\n");
                                mitsumi_reset_status(mitsumi);
                                mitsumi_add_status(mitsumi, 0);
                                mitsumi->state = CMD_STATE_STATUS;
                                break;
                        }
                        atapi->readsector_raw(mitsumi->data_buf, mitsumi->sector_pos);
                        mitsumi->sector_count++;
                        mitsumi->sector_pos++;
                        mitsumi->data_pos = 0;
                        mitsumi->data_len = 2352;
                        mitsumi->state = CMD_STATE_TRANSFER;
                }
                break;

                default:
                cdfatal("mitsumi_command_run: Unknown MCD command %02x\n", mitsumi->cmd);
                break;
        }
}

void mitsumi_write_b(mitsumi_t *mitsumi, uint32_t addr, uint8_t val)
{
        switch (addr & 0xfc)
        {
                case 0x00: /*Command/data register*/
                cdlog("Write command reg: state=%i val=%02x\n", mitsumi->state, val);
                switch (mitsumi->state)
                {
                        case CMD_STATE_IDLE:
                        mitsumi_command_start(mitsumi, val);
                        break;
                        case CMD_STATE_PARAMS:
                        if (mitsumi->params_wp < mitsumi->nr_params)
                        {
                                mitsumi->params[mitsumi->params_wp++] = val;
                                if (mitsumi->params_wp == mitsumi->nr_params)
                                {
                                        mitsumi->state = CMD_STATE_BUSY;
                                        mitsumi->cmd_time = 2;
                                }
                        }
                        break;
                        case CMD_STATE_BUSY:
                        cdlog("mitsumi_write_b: CMD_STATE_BUSY\n");
                        break;
                        case CMD_STATE_STATUS:
                        cdlog("mitsumi_write_b: CMD_STATE_STATUS\n");
                        break;
                        case CMD_STATE_TRANSFER:
                        cdlog("mitsumi_write_b: CMD_STATE_TRANSFER\n");
                        break;
                }
                break;
                
                case 0x20: /*Flags register*/
                /*Read only?*/
                break;
        }
}

uint8_t mitsumi_read_b(mitsumi_t *mitsumi, uint32_t addr)
{
        uint8_t temp = 0xff;
        
        switch (addr & 0xfc)
        {
                case 0x00: /*Command/data register*/
                switch (mitsumi->state)
                {
                        case CMD_STATE_IDLE:
                        cdlog("mitsumi_read_b: CMD_STATE_IDLE\n");
                        temp = 0xff;
                        break;
                        case CMD_STATE_PARAMS:
                        cdlog("mitsumi_read_b: CMD_STATE_PARAMS\n");
                        temp = 0xff;
                        break;
                        case CMD_STATE_BUSY:
                        cdlog("mitsumi_read_b: CMD_STATE_BUSY\n");
                        temp = 0xff;
                        break;
                        case CMD_STATE_STATUS:
                        if (mitsumi->status_rp < mitsumi->status_wp)
                        {
                                temp = mitsumi->status_dat[mitsumi->status_rp++];
                                if (mitsumi->status_rp == mitsumi->status_wp)
                                        mitsumi->state = CMD_STATE_IDLE;
                        }
                        break;
                        case CMD_STATE_TRANSFER:
                        if (mitsumi->data_pos < mitsumi->data_len)
                        {
                                temp = mitsumi->data_buf[mitsumi->data_pos++];
                                if (mitsumi->data_pos == mitsumi->data_len)
                                {
                                        mitsumi->state = CMD_STATE_BUSY;
                                        mitsumi->cmd_time = 2;
                                }
                        }
//                        cdfatal("mitsumi_read_b: CMD_STATE_TRANSFER\n");
                        break;
                }
                break;
                
                case 0x20: /*Flags register*/
                switch (mitsumi->state)
                {
                        case CMD_STATE_IDLE:
                        temp = ~0;
                        break;
                        
                        case CMD_STATE_STATUS:
                        temp = ~FLAGS_STATUS_AVAIL;
                        break;
                        
                        case CMD_STATE_PARAMS:
                        temp = ~0;
                        break;

                        case CMD_STATE_BUSY:
                        temp = ~0;
                        break;

                        case CMD_STATE_TRANSFER:
                        temp = ~FLAGS_DATA_AVAIL;
                        break;
                }
                break;
        }
        
//        if ((addr != 0x3220 || temp != 0xff) && !(addr == 0x3200 && mitsumi->state == CMD_STATE_TRANSFER))
//                cdlog("mitsumi_read_b: addr=%04x temp=%02x\n", addr, temp);
        return temp;
}

void mitsumi_poll(mitsumi_t *mitsumi)
{
        if (mitsumi->cmd_time)
        {
                mitsumi->cmd_time--;
                if (!mitsumi->cmd_time)
                {
                        mitsumi_command_run(mitsumi);
                }
        }
}

void mitsumi_reset(mitsumi_t *mitsumi, const char *drive_path)
{
        mitsumi->status = STATUS_DISC_READY;
        
        ioctl_set_drive(drive_path);
        ioctl_reset();
        ioctl_open(0);
}

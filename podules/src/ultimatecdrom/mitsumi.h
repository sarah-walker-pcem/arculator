typedef enum cmd_state
{
        CMD_STATE_IDLE = 0,
        CMD_STATE_PARAMS,
        CMD_STATE_BUSY,
        CMD_STATE_STATUS,
        CMD_STATE_TRANSFER
} cmd_state;

typedef enum cmd_play_read
{
        CMD_PLAY = 0,
        CMD_READ,
        CMD_READ_RAW
} cmd_play_read;

typedef struct mitsumi_t
{
        uint8_t status, flags;
        uint8_t cmd;
        
        int status_rp, status_wp;
        uint8_t status_dat[16];
        
        int params_wp, nr_params;
        uint8_t params[16];
        
        int cmd_time;
        cmd_state state;
        
        uint8_t mode, drv_mode;
        
        int sector_count, nr_sectors_to_read;
        int sector_pos, sector_end;
        cmd_play_read play_read;
        
        int data_pos, data_len;
        uint8_t data_buf[2352];
        
        int read_toc_track;
        uint8_t toc[2048];
} mitsumi_t;

void mitsumi_write_b(mitsumi_t *mitsumi, uint32_t addr, uint8_t val);
uint8_t mitsumi_read_b(mitsumi_t *mitsumi, uint32_t addr);

void mitsumi_poll(mitsumi_t *mitsumi);
void mitsumi_reset(mitsumi_t *mitsumi, const char *drive_path);

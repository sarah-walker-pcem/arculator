#include "scsi.h"

typedef struct wd33c93a_t
{
        uint8_t aux_status;
        uint8_t cdb[12];
        uint8_t command;
        uint8_t command_phase;
        uint8_t ctrl;
        uint8_t destid;
        uint8_t ownid;
        uint8_t cdb_size;
        uint8_t status;
        uint8_t target_status;
        uint32_t transfer_count;

        uint8_t fifo[12];
        int fifo_read, fifo_write;

        int disconnect_pending;

        uint8_t info;

        int cdb_idx, cdb_len;
        uint8_t last_status;
        int bytes_transferred;

        int addr_reg;
        podule_t *podule;
        struct d71071l_t *dma;
        struct scsi_bus_t *bus;
        scsi_state_t scsi_state;
} wd33c93a_t;

void wd33c93a_init(wd33c93a_t *wd, podule_t *podule, d71071l_t *dma, struct scsi_bus_t *bus);
void wd33c93a_close(wd33c93a_t *wd);
void wd33c93a_write(wd33c93a_t *wd, uint32_t addr, uint8_t val);
uint8_t wd33c93a_read(wd33c93a_t *wd, uint32_t addr);
void wd33c93a_poll(wd33c93a_t *wd);
void wd33c93a_reset(wd33c93a_t *wd);
void wd33c93a_process_scsi(wd33c93a_t *wd);

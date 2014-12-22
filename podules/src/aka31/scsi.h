typedef struct scsi_device_t
{
        void *(*init)();
        void (*close)(void *p);
        
        int (*command)(uint8_t *cdb, void *p);
} scsi_device_t;

int scsi_add_data(uint8_t val);
int scsi_get_data();
void scsi_set_phase(uint8_t phase);
void scsi_set_irq(uint8_t status);

#define SCSI_TEST_UNIT_READY              0x00
#define SCSI_READ_6                       0x08
#define SCSI_WRITE_6                      0x0a
#define SCSI_INQUIRY                      0x12
#define SCSI_MODE_SENSE_6                 0x1a
#define SCSI_PREVENT_ALLOW_MEDIUM_REMOVAL 0x1e
#define SCSI_READ_CAPACITY_10             0x25
#define SCSI_READ_10                      0x28
#define SCSI_WRITE_10                     0x2a

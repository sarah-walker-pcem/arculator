#include "scsi.h"

typedef struct ncr5380_t
{
	uint8_t output_data;
	uint8_t icr;
	uint8_t mode;
	uint8_t tcr;
	uint8_t ser;

//	int target_id;
	//int target_bsy;
//	int target_req;

	uint8_t bus_status;

	podule_t *podule;
	struct scsi_bus_t *bus;
} ncr5380_t;

void ncr5380_init(ncr5380_t *ncr, podule_t *podule, const podule_callbacks_t *podule_callbacks, struct scsi_bus_t *bus);
void ncr5380_write(ncr5380_t *ncr, uint32_t addr, uint8_t val);
uint8_t ncr5380_read(ncr5380_t *ncr, uint32_t addr);
void ncr5380_dack(ncr5380_t *ncr);
int ncr5380_drq(ncr5380_t *ncr);
int ncr5380_bsy(ncr5380_t *ncr);
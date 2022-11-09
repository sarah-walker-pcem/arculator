#include "arc.h"
#include "bmu.h"

/*BMU event triggers bit 1 in IOC IRQA*/

#define BMU_VERSION           0x50
#define BMU_TEMPERATURE       0x52
#define BMU_CURRENT           0x54  /*units of 10.3 mA*/
#define BMU_VOLTAGE           0x56
#define BMU_STATUS            0x5c
#define BMU_CHARGE_RATE       0x5e
#define BMU_CAPACITY_NOMINAL  0x80
#define BMU_CAPACITY_MEASURED 0x82
#define BMU_CAPACITY_USED     0x88
#define BMU_CAPACITY_USABLE   0x8a
#define BMU_CHARGE_ESTIMATE   0x8e  /*units of 11.7 mAh*/
#define BMU_COMMAND           0x90
#define BMU_AUTOSTART         0x9e

#define BMU_STATUS_LID_OPEN           (1 << 1)
#define BMU_STATUS_THRESHOLD_2        (1 << 2)
#define BMU_STATUS_THRESHOLD_1        (1 << 3)
#define BMU_STATUS_CHARGING_FAULT     (1 << 4)
#define BMU_STATUS_CHARGE_STATE_KNOWN (1 << 5)
#define BMU_STATUS_BATTERY_PRESENT    (1 << 6)
#define BMU_STATUS_CHARGER_CONNECTED  (1 << 7)

static struct
{
	uint8_t estimate;
} bmu;

uint8_t bmu_read(int addr)
{
	uint8_t ret = 0xff;

	switch (addr & 0xff)
	{
		case BMU_VERSION:
		ret = 0x03;
		break;

		case BMU_STATUS:
		ret = BMU_STATUS_CHARGER_CONNECTED |
			BMU_STATUS_BATTERY_PRESENT |
			BMU_STATUS_CHARGE_STATE_KNOWN |
			BMU_STATUS_LID_OPEN;
		break;

		case BMU_CAPACITY_USED:
		ret = 0x40;
		break;

		case BMU_CHARGE_ESTIMATE:
		ret = bmu.estimate;
		break;

		case BMU_COMMAND:
		ret = 0; /*Command not in progress*/
		break;
	}
//        rpclog("bmu_read: addr=%02x ret=%02x PC=%07x\n", addr, ret, PC);

	return ret;
}

void bmu_write(int addr, uint8_t val)
{
//        rpclog("bmu_write: addr=%02x val=%02x PC=%07x\n", addr, val, PC);

	switch (addr & 0xff)
	{
		case BMU_CHARGE_ESTIMATE:
		bmu.estimate = val;
		break;
	}
}

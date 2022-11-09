#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdarg.h>
#include <string.h>
#include "podule_api.h"
#include "ncr5380.h"
#include "scsi.h"


#define ICR_DBP             0x01
#define ICR_SEL             0x04
#define ICR_BSY             0x08
#define ICR_ACK             0x10
#define ICR_ARB_LOST        0x20
#define ICR_ARB_IN_PROGRESS 0x40

#define MODE_ARBITRATE 0x01
#define MODE_DMA       0x02
#define MODE_TARGET    0x40

#define STATUS_ACK 0x01
#define STATUS_DRQ 0x40

#define TCR_IO  0x01
#define TCR_CD  0x02
#define TCR_MSG 0x04
#define TCR_REQ 0x08


void ncr5380_init(ncr5380_t *ncr, podule_t *podule, const podule_callbacks_t *podule_callbacks, struct scsi_bus_t *bus)
{
	memset(ncr, 0, sizeof(ncr));
	ncr->podule = podule;
	ncr->bus = bus;
	scsi_bus_init(ncr->bus, podule, podule_callbacks);
}

static int get_bus_host(ncr5380_t *ncr, int match)
{
	int bus_host = 0;

	if (ncr->icr & ICR_DBP)
		bus_host |= BUS_DBP;
	if (ncr->icr & ICR_SEL)
		bus_host |= BUS_SEL;
	if ((ncr->mode & MODE_TARGET) || match)
	{
		if (ncr->tcr & TCR_IO)
			bus_host |= BUS_IO;
		if (ncr->tcr & TCR_CD)
			bus_host |= BUS_CD;
		if (ncr->tcr & TCR_MSG)
			bus_host |= BUS_MSG;
		if (ncr->tcr & TCR_REQ)
			bus_host |= BUS_REQ;
	}
	if (ncr->icr & ICR_BSY)
		bus_host |= BUS_BSY;
	if (ncr->icr & ICR_ACK)
		bus_host |= BUS_ACK;
	if (ncr->mode & MODE_ARBITRATE)
		bus_host |= BUS_ARB;

	return bus_host | BUS_SETDATA(ncr->output_data);
}

void ncr5380_write(ncr5380_t *ncr, uint32_t addr, uint8_t val)
{
	int bus_host = 0;

//	scsi_log("ncr5380_write: addr=%06x val=%02x\n", addr, val);
	switch (addr & 7)
	{
//#if 0
		case 0: /*Output data register*/
		ncr->output_data = val;
		break;
//#endif
		case 1: /*Initiator Command Register*/
		ncr->icr = val;
		break;

		case 2: /*Mode register*/
		if ((val & MODE_ARBITRATE) && !(ncr->mode & MODE_ARBITRATE))
		{
			ncr->icr &= ~ICR_ARB_LOST;
			ncr->icr |=  ICR_ARB_IN_PROGRESS;
		}

		ncr->mode = val;
		break;

		case 3: /*Target Command Register*/
		ncr->tcr = val;
		break;
		case 4: /*Select Enable Register*/
		ncr->ser = val;
		break;

		case 5: /*Start DMA Send*/
		break;

		case 6: /*Start DMA Target Receive*/
		break;

		case 7: /*Start DMA Initiator Receive*/
		break;


		default:
		scsi_fatal("Bad NCR5380 write %06x %02x\n", addr, val);
	}

	bus_host = get_bus_host(ncr, 0);

	scsi_bus_update(ncr->bus, bus_host);
}

uint8_t ncr5380_read(ncr5380_t *ncr, uint32_t addr)
{
	int bus = 0;
	uint8_t temp;

	//scsi_log("ncr5380_read: addr=%06x\n", addr);
	switch (addr & 7)
	{
		case 0: /*Current SCSI Data*/
		bus = scsi_bus_read(ncr->bus);
		return BUS_GETDATA(bus);//ncr->output_data;
		case 1: /*Initiator Command Register*/
		return ncr->icr;
		case 2: /*Mode Register*/
		return ncr->mode;
		case 3: /*Target Command Register*/
		return ncr->tcr;

		case 4: /*Current SCSI Bus Status*/
		temp = get_bus_host(ncr, 0);
		bus = scsi_bus_read(ncr->bus);
//		scsi_log("Current SCSI Bus Status: temp=%02x bus=%02x\n", temp, bus & 0xff);
		temp |= (bus & 0xff);
		return temp;

		case 5: /*Bus and Status Register*/
		temp = 0;
		bus = get_bus_host(ncr, 1);
		if (scsi_bus_match(ncr->bus, bus))
			temp |= 8;
		bus = scsi_bus_read(ncr->bus);
		if (bus & BUS_ACK)
			temp |= STATUS_ACK;
		if ((bus & BUS_REQ) && (ncr->mode & MODE_DMA))
			temp |= STATUS_DRQ;
		return temp;

		case 7: /*Reset Parity/Interrupt*/
		break;

		default:
		scsi_fatal("Bad NCR5380 read %06x\n", addr);
	}
}

void ncr5380_dack(ncr5380_t *ncr)
{
	if (ncr->mode & MODE_DMA)
	{
		int bus = get_bus_host(ncr, 0);

		scsi_bus_update(ncr->bus, bus | BUS_ACK);
		scsi_bus_update(ncr->bus, bus & ~BUS_ACK);
	}
}

int ncr5380_drq(ncr5380_t *ncr)
{
	int bus = scsi_bus_read(ncr->bus);

	return ((bus & BUS_REQ) && (ncr->mode & MODE_DMA) && !(bus & (BUS_MSG | BUS_CD)));
}

int ncr5380_bsy(ncr5380_t *ncr)
{
	int bus = scsi_bus_read(ncr->bus);

	return bus & (BUS_MSG | BUS_CD);
}

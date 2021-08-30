#include <stdint.h>
#include <string.h>
#include "93c06.h"

enum
{
	EEPROM_IDLE = 0,
	EEPROM_WAIT_START,
	EEPROM_GET_COMMAND,
	EEPROM_GET_ADDR,
	EEPROM_GET_MISC,
	EEPROM_READ_DATA,
	EEPROM_WRITE_DATA,
	EEPROM_WRITE_DATA_ALL
};

int eeprom_93c06_update(eeprom_93c06_t *eeprom, int cs, int clk, int di)
{
	if (!cs)
	{
		eeprom->state = EEPROM_IDLE;
		eeprom->data_out = 1;
	}
	else
	{
		if (eeprom->state == EEPROM_IDLE)
		{
			//oak_scsi_log("EEPROM CS gone high\n");
			eeprom->state = EEPROM_WAIT_START;
			eeprom->data_out = 1;
		}

		if (!eeprom->clk && clk)
		{
			switch (eeprom->state)
			{
				case EEPROM_GET_COMMAND:
				case EEPROM_GET_ADDR:
				case EEPROM_GET_MISC:
				eeprom->data <<= 1;
				if (di)
					eeprom->data |= 1;

				eeprom->nr_bits--;
				if (!eeprom->nr_bits)
				{
					switch (eeprom->state)
					{
						case EEPROM_GET_COMMAND:
						eeprom->cmd = eeprom->data;
						//oak_scsi_log("EEPROMGET_GET_COMMAND %i\n", eeprom->cmd);
						switch (eeprom->cmd)
						{
							case 0: /*Misc*/
							eeprom->state = EEPROM_GET_MISC;
							eeprom->data = 0;
							eeprom->nr_bits = 2;
							break;

							case 1: /*Write*/
							case 2: /*Read*/
							case 3: /*Erase*/
							eeprom->state = EEPROM_GET_ADDR;
							eeprom->data = 0;
							eeprom->nr_bits = 6;
							break;

//							default:
							//fatal("Unknown EEPROM command %i\n", eeprom->cmd);
						}
						break;

						case EEPROM_GET_ADDR:
						eeprom->addr = eeprom->data;
						//oak_scsi_log("EEPROMGET_GET_ADDR %i\n", eeprom->addr);
						switch (eeprom->cmd)
						{
							case 1: /*Write*/
							eeprom->state = EEPROM_WRITE_DATA;
							eeprom->data = 0;
							eeprom->nr_bits = 16;
							break;

							case 2: /*Read*/
							eeprom->state = EEPROM_READ_DATA;
							eeprom->data = eeprom->buffer[eeprom->addr];
							//oak_scsi_log("EEPROM_READ_DATA: addr=%x data=%04x\n", eeprom->addr, eeprom->data);
							eeprom->nr_bits = 16;
							break;

							case 3: /*Erase*/
							eeprom->buffer[eeprom->addr] = 0;
							eeprom->state = EEPROM_IDLE;
							break;

							//default:
//							fatal("Unknown EEPROM GET_ADDR command %i\n", eeprom->cmd);
						}
						break;

						case EEPROM_GET_MISC:
						eeprom->cmd = eeprom->data;
						//oak_scsi_log("EEPROMGET_GET_MISC %i\n", eeprom->cmd);
						switch (eeprom->cmd)
						{
							case 0: /*EWDS*/
							eeprom->write_enable = 0;
							eeprom->state = EEPROM_IDLE;
							break;

							case 1: /*ERAL*/
							eeprom->state = EEPROM_WRITE_DATA_ALL;
							eeprom->addr = 0;
							eeprom->data = 0;
							eeprom->nr_bits = 16;
							break;

							case 2: /*ERAL*/
							memset(eeprom->buffer, 0xff, sizeof(eeprom->buffer));
							break;

							case 3: /*EWEN*/
							eeprom->write_enable = 1;
							eeprom->state = EEPROM_IDLE;
							break;

//							default:
//							fatal("Bad EEPROM_GET_MISC %i\n", eeprom->data);
						}
						break;

//						default:
//						fatal("Unknown EEPROM Read done state %i\n", eeprom->state);
					}
				}
				break;

				case EEPROM_WAIT_START:
				if (di)
				{
					eeprom->state = EEPROM_GET_COMMAND;
					eeprom->data = 0;
					eeprom->nr_bits = 2;
				}
				break;


				case EEPROM_READ_DATA:
				eeprom->data_out = (eeprom->data & 0x8000) ? 1 : 0;
				eeprom->data <<= 1;
				eeprom->data |= 1;
				break;

				case EEPROM_WRITE_DATA:
				eeprom->data <<= 1;
				if (di)
					eeprom->data |= 1;
				eeprom->nr_bits--;
				if (!eeprom->nr_bits)
				{
					//oak_scsi_log("EEPROM_WRITE_DATA: addr=%x data=%04x\n", eeprom->addr, eeprom->data);
					if (eeprom->write_enable)
						eeprom->buffer[eeprom->addr] = eeprom->data;
					eeprom->dirty = 1;
					eeprom->state = EEPROM_IDLE;
				}
				break;

				case EEPROM_WRITE_DATA_ALL:
				eeprom->data <<= 1;
				if (di)
					eeprom->data |= 1;
				eeprom->nr_bits--;
				if (!eeprom->nr_bits)
				{
					//oak_scsi_log("EEPROM_WRITE_DATA: addr=%x data=%04x\n", eeprom->addr, eeprom->data);
					if (eeprom->write_enable)
						eeprom->buffer[eeprom->addr] = eeprom->data;
					eeprom->addr++;
					if (eeprom->addr == 16)
					{
						eeprom->dirty = 1;
						eeprom->state = EEPROM_IDLE;
					}
					else
					{
						eeprom->data = 0;
						eeprom->nr_bits = 16;
					}
				}
				break;

				case EEPROM_IDLE:
				break;

				//default:
				//fatal("Unknown EEPROM state %i\n", eeprom->state);
			}
		}
	}
	eeprom->clk = clk ? 1 : 0;

	return eeprom->data_out ? 1 : 0;
}

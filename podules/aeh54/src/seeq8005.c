#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <winbase.h>
#include "net.h"
#include "podule_api.h"
#include "seeq8005.h"

#define COMMAND_DMA_IRQ_ENA           (1 << 0)
#define COMMAND_RX_IRQ_ENA            (1 << 1)
#define COMMAND_TX_IRQ_ENA            (1 << 2)
#define COMMAND_BUFFER_WINDOW_IRQ_ENA (1 << 3)
#define COMMAND_DMA_IRQ_ACK           (1 << 4)
#define COMMAND_RX_IRQ_ACK            (1 << 5)
#define COMMAND_TX_IRQ_ACK            (1 << 6)
#define COMMAND_BUFFER_WINDOW_IRQ_ACK (1 << 7)
#define COMMAND_SET_DMA_ON            (1 << 8)
#define COMMAND_SET_RX_ON             (1 << 9)
#define COMMAND_SET_TX_ON             (1 << 10)
#define COMMAND_SET_DMA_OFF           (1 << 11)
#define COMMAND_SET_RX_OFF            (1 << 12)
#define COMMAND_SET_TX_OFF            (1 << 13)
#define COMMAND_FIFO_READ             (1 << 14)
#define COMMAND_FIFO_WRITE            (1 << 15)

#define STATUS_DMA_IRQ_ENA           (1 << 0)
#define STATUS_RX_IRQ_ENA            (1 << 1)
#define STATUS_TX_IRQ_ENA            (1 << 2)
#define STATUS_BUFFER_WINDOW_IRQ_ENA (1 << 3)
#define STATUS_DMA_IRQ               (1 << 4)
#define STATUS_RX_IRQ                (1 << 5)
#define STATUS_TX_IRQ                (1 << 6)
#define STATUS_BUFFER_WINDOW_IRQ     (1 << 7)
#define STATUS_DMA_ON                (1 << 8)
#define STATUS_RX_ON                 (1 << 9)
#define STATUS_TX_ON                 (1 << 10)
#define STATUS_FIFO_FULL             (1 << 13)
#define STATUS_FIFO_EMPTY            (1 << 14)
#define STATUS_FIFO_DIR              (1 << 15)

#define CONFIG1_BUFFER_CODE_MASK         (0xf << 0)
#define CONFIG1_BUFFER_CODE_STATION_ADDR_0 (0 << 0)
#define CONFIG1_BUFFER_CODE_STATION_ADDR_1 (1 << 0)
#define CONFIG1_BUFFER_CODE_STATION_ADDR_2 (2 << 0)
#define CONFIG1_BUFFER_CODE_STATION_ADDR_3 (3 << 0)
#define CONFIG1_BUFFER_CODE_STATION_ADDR_4 (4 << 0)
#define CONFIG1_BUFFER_CODE_STATION_ADDR_5 (5 << 0)
#define CONFIG1_BUFFER_CODE_TX_END_PTR     (7 << 0)
#define CONFIG1_BUFFER_CODE_LOCAL_MEMORY   (8 << 0)
#define CONFIG1_STATION_REGISTER_SET_0     (1 << 8)
#define CONFIG1_STATION_REGISTER_SET_1     (1 << 9)
#define CONFIG1_STATION_REGISTER_SET_2     (1 << 10)
#define CONFIG1_STATION_REGISTER_SET_3     (1 << 11)
#define CONFIG1_STATION_REGISTER_SET_4     (1 << 12)
#define CONFIG1_STATION_REGISTER_SET_5     (1 << 13)
#define CONFIG1_MATCH_MODE_MASK            (3 << 14)
#define CONFIG1_MATCH_MODE_SPECIFIC_ONLY   (0 << 14)
#define CONFIG1_MATCH_MODE_BROADCAST       (1 << 14)
#define CONFIG1_MATCH_MODE_MULTICAST       (2 << 14)
#define CONFIG1_MATCH_MODE_PROMISCUOUS     (3 << 14)

#define CONFIG2_REC_CRC             (1 << 9)
#define CONFIG2_XMIT_NO_CRC         (1 << 10)
#define CONFIG2_LOOPBACK            (1 << 11)

#define TX_COMMAND_BABBLE_IRQ       (1 << 0)
#define TX_COMMAND_COLLISION_IRQ    (1 << 1)
#define TX_COMMAND_16_COLLISION_IRQ (1 << 2)
#define TX_COMMAND_SUCCESS_IRQ      (1 << 3)
#define TX_COMMAND_DATA_FOLLOWS     (1 << 5)
#define TX_COMMAND_CHAIN_CONTINUE   (1 << 6)
#define TX_COMMAND_XMIT_RECEIVE     (1 << 7)

#define TX_STATUS_DONE (1 << 7)

#define RX_COMMAND_CHAIN_CONTINUE (1 << 6)

#define RX_STATUS_CRC_ERROR       (1 << 1)
#define RX_STATUS_DONE            (1 << 7)

void seeq8005_init(seeq8005_t *seeq8005, void (*set_irq)(void *p, int state), void *p, net_t *net)
{
	seeq8005->p = p;
	seeq8005->net = net;
	seeq8005->set_irq = set_irq;
}

void seeq8005_close(seeq8005_t *seeq8005)
{
}

static void update_irqs(seeq8005_t *seeq8005)
{
	int irq = 0;

	if ((seeq8005->status & STATUS_DMA_IRQ) && (seeq8005->command & COMMAND_DMA_IRQ_ENA))
		irq = 1;
	if ((seeq8005->status & STATUS_RX_IRQ) && (seeq8005->command & COMMAND_RX_IRQ_ENA))
		irq = 1;
	if ((seeq8005->status & STATUS_TX_IRQ) && (seeq8005->command & COMMAND_TX_IRQ_ENA))
		irq = 1;
	if ((seeq8005->status & STATUS_BUFFER_WINDOW_IRQ) && (seeq8005->command & COMMAND_BUFFER_WINDOW_IRQ_ENA))
		irq = 1;

//	aeh54_log("update_irqs: %i  %04x %04x\n", irq, seeq8005->status, seeq8005->command);
	seeq8005->set_irq(seeq8005->p, irq);
}

uint16_t seeq8005_read(seeq8005_t *seeq8005, uint32_t addr)
{
	uint16_t ret = 0xff;

	switch (addr & 0x7)
	{
		case 0x0: /*Status register*/
		ret = seeq8005->status;
		break;

		case 0x1: /*Config register 1*/
		ret = seeq8005->config_1;
		break;

		case 0x2: /*Config register 2*/
		ret = seeq8005->config_2;
		break;

		case 0x3: /*Receive End Area register*/
		ret = seeq8005->rx_end_area >> 8;
		break;

		case 0x4: /*Buffer Window register*/
		switch (seeq8005->config_1 & CONFIG1_BUFFER_CODE_MASK)
		{
			case CONFIG1_BUFFER_CODE_LOCAL_MEMORY:
//			aeh54_log("RAM read %04x\n", seeq8005->dma_addr);
			ret = seeq8005->ram[seeq8005->dma_addr];
			seeq8005->dma_addr++;
			if (seeq8005->dma_addr == 0)
				seeq8005->dma_addr = seeq8005->tx_end_ptr + 0x100;
			ret |= seeq8005->ram[seeq8005->dma_addr] << 8;
			seeq8005->dma_addr++;
			if (seeq8005->dma_addr == 0)
				seeq8005->dma_addr = seeq8005->tx_end_ptr + 0x100;
			break;
		}
		break;

		case 0x5: /*Receive pointer register*/
		ret = seeq8005->rx_ptr;
		break;

		case 0x6: /*Transmit pointer register*/
		ret = seeq8005->tx_ptr;
		break;

		case 0x7: /*DMA address register*/
		ret = seeq8005->dma_addr;
		break;
	}
//	if ((addr & 7) != 4)
//		aeh54_log("seeq8005_read: addr=%07x val=%04x  status=%04x\n", addr, ret, seeq8005->status);

	return ret;
}

void seeq8005_write(seeq8005_t *seeq8005, uint32_t addr, uint16_t val)
{
//	if ((addr & 7) != 4)
//		aeh54_log("seeq8005_write: addr=%07x val=%04x\n", addr, val);

	switch (addr & 0x7)
	{
		case 0x0: /*Command register*/
		seeq8005->command = val;
		seeq8005->status = (seeq8005->status & ~0xf) | (seeq8005->command & 0xf);

		if (seeq8005->command & COMMAND_DMA_IRQ_ACK)
			seeq8005->status &= ~STATUS_DMA_IRQ;
		if (seeq8005->command & COMMAND_RX_IRQ_ACK)
			seeq8005->status &= ~STATUS_RX_IRQ;
		if (seeq8005->command & COMMAND_TX_IRQ_ACK)
			seeq8005->status &= ~STATUS_TX_IRQ;
		if (seeq8005->command & COMMAND_BUFFER_WINDOW_IRQ_ACK)
			seeq8005->status &= ~STATUS_BUFFER_WINDOW_IRQ;

		if ((seeq8005->command & COMMAND_SET_DMA_ON) && (seeq8005->command & COMMAND_SET_DMA_OFF))
			seeq8005->status |= STATUS_DMA_IRQ;
		else if (seeq8005->command & COMMAND_SET_DMA_ON)
			seeq8005->status |= STATUS_DMA_ON;
		else if (seeq8005->command & COMMAND_SET_DMA_OFF)
			seeq8005->status &= ~STATUS_DMA_ON;

		if ((seeq8005->command & COMMAND_SET_RX_ON) && (seeq8005->command & COMMAND_SET_RX_OFF))
			seeq8005->status |= STATUS_RX_IRQ;
		else if (seeq8005->command & COMMAND_SET_RX_ON)
		{
			//aeh54_log("Rx on\n");
			seeq8005->status |= STATUS_RX_ON;
		}
		else if (seeq8005->command & COMMAND_SET_RX_OFF)
			seeq8005->status &= ~STATUS_RX_ON;

		if ((seeq8005->command & COMMAND_SET_TX_ON) && (seeq8005->command & COMMAND_SET_TX_OFF))
			seeq8005->status |= STATUS_TX_IRQ;
		else if (seeq8005->command & COMMAND_SET_TX_ON)
			seeq8005->status |= STATUS_TX_ON;
		else if (seeq8005->command & COMMAND_SET_TX_OFF)
			seeq8005->status &= ~STATUS_TX_ON;

		if (seeq8005->command & COMMAND_FIFO_READ)
			seeq8005->status |= STATUS_FIFO_DIR;
		if (seeq8005->command & COMMAND_FIFO_WRITE)
			seeq8005->status &= ~STATUS_FIFO_DIR;

		/*Hack*/
		if (seeq8005->command & COMMAND_FIFO_READ)
		{
			seeq8005->status |= STATUS_FIFO_FULL;
			seeq8005->status &= ~STATUS_FIFO_EMPTY;
//			aeh54_log("Set FIFO read %04x\n", seeq8005->status);
		}
		if (seeq8005->command & COMMAND_FIFO_WRITE)
		{
			seeq8005->status &= ~STATUS_FIFO_FULL;
			seeq8005->status |= STATUS_FIFO_EMPTY;
//			aeh54_log("Set FIFO write %04x\n", seeq8005->status);
		}

		update_irqs(seeq8005);
		break;

		case 0x1: /*Config register 1*/
		seeq8005->config_1 = val;
		break;

		case 0x2: /*Config register 2*/
		seeq8005->config_2 = val;
		break;

		case 0x3: /*Receive End Area register*/
		seeq8005->rx_end_area = val << 8;
		break;

		case 0x4: /*Buffer Window register*/
		switch (seeq8005->config_1 & CONFIG1_BUFFER_CODE_MASK)
		{
			case CONFIG1_BUFFER_CODE_STATION_ADDR_0:
			case CONFIG1_BUFFER_CODE_STATION_ADDR_1:
			case CONFIG1_BUFFER_CODE_STATION_ADDR_2:
			case CONFIG1_BUFFER_CODE_STATION_ADDR_3:
			case CONFIG1_BUFFER_CODE_STATION_ADDR_4:
			case CONFIG1_BUFFER_CODE_STATION_ADDR_5:
			{
				int addr_nr = seeq8005->config_1 & CONFIG1_BUFFER_CODE_MASK;

				seeq8005->station_addr[addr_nr][0] = seeq8005->station_addr[addr_nr][1];
				seeq8005->station_addr[addr_nr][1] = seeq8005->station_addr[addr_nr][2];
				seeq8005->station_addr[addr_nr][2] = seeq8005->station_addr[addr_nr][3];
				seeq8005->station_addr[addr_nr][3] = seeq8005->station_addr[addr_nr][4];
				seeq8005->station_addr[addr_nr][4] = seeq8005->station_addr[addr_nr][5];
				seeq8005->station_addr[addr_nr][5] = val;

/*				aeh54_log("Station addr %i now %02x:%02x:%02x:%02x:%02x:%02x\n", addr_nr,
					seeq8005->station_addr[addr_nr][0],
					seeq8005->station_addr[addr_nr][1],
					seeq8005->station_addr[addr_nr][2],
					seeq8005->station_addr[addr_nr][3],
					seeq8005->station_addr[addr_nr][4],
					seeq8005->station_addr[addr_nr][5]);*/
			}
			break;

			case CONFIG1_BUFFER_CODE_TX_END_PTR:
			seeq8005->tx_end_ptr = val << 8;
//			aeh54_log("TX end ptr=%04x\n", val << 8);
			break;

			case CONFIG1_BUFFER_CODE_LOCAL_MEMORY:
//			aeh54_log("  write to %04x\n",  seeq8005->dma_addr);
			seeq8005->ram[seeq8005->dma_addr] = val & 0xff;
			seeq8005->dma_addr++;
			if (seeq8005->dma_addr == (seeq8005->tx_end_ptr + 0x100))
				seeq8005->dma_addr = 0;
			seeq8005->ram[seeq8005->dma_addr] = val >> 8;
			seeq8005->dma_addr++;
			if (seeq8005->dma_addr == (seeq8005->tx_end_ptr + 0x100))
				seeq8005->dma_addr = 0;
			break;
		}
		break;

		case 0x5: /*Receive pointer register*/
		seeq8005->rx_ptr = val;
		break;

		case 0x6: /*Transmit pointer register*/
		seeq8005->tx_ptr = val;
		break;

		case 0x7: /*DMA address register*/
		seeq8005->dma_addr = val;
		break;
	}
}

const uint32_t ethernet_poly = 0xedb88320;

static uint32_t gen_crc(uint8_t *packet, int size)
{
	uint32_t crc = 0xffffffff;

	for (int i = 0; i < size; i++)
	{
		crc ^= packet[i];
		for (int j = 0; j < 8; j++)
			crc = (crc >> 1) ^ ((crc & 1) ? ethernet_poly : 0);
	}

	return ~crc;
}

static int write_rx_data(seeq8005_t *seeq8005, uint16_t *write_ptr, uint8_t val)
{
//	aeh54_log("write_rx_data: write_ptr=%04x val=%02x\n", *write_ptr, val);
	seeq8005->ram[*write_ptr] = val;
	(*write_ptr)++;
	if (!(*write_ptr)) /*Overflow*/
		*write_ptr = seeq8005->tx_end_ptr + 0x100;
	if (*write_ptr == seeq8005->rx_end_area)
	{
		//aeh54_log("Hit end of rx buffer\n");
		seeq8005->status &= ~STATUS_RX_ON;
		seeq8005->status |= STATUS_RX_IRQ;
		update_irqs(seeq8005);
		return 1; /*Hit end of buffer*/
	}
	return 0;
}

static const uint8_t broadcast_mac[6] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
static const uint8_t multicast_mac_header[6] = {0x01, 0x00, 0x5e};

static int filter_mac(seeq8005_t *seeq8005, uint8_t *destination_mac)
{
	uint32_t match_mode = seeq8005->config_1 & CONFIG1_MATCH_MODE_MASK;

	if (match_mode == CONFIG1_MATCH_MODE_PROMISCUOUS)
		return 1; /*Match all packets*/

	for (int i = 0; i < 6; i++)
	{
		if (seeq8005->config_1 & (CONFIG1_STATION_REGISTER_SET_0 << i))
		{
			if (!memcmp(destination_mac, seeq8005->station_addr[i], 6))
				return 1;
		}

	}

	if (match_mode >= CONFIG1_MATCH_MODE_BROADCAST)
	{
		if (!memcmp(destination_mac, broadcast_mac, 6))
			return 1;
	}

	if (match_mode >= CONFIG1_MATCH_MODE_MULTICAST)
	{
		if (!memcmp(destination_mac, multicast_mac_header, 3))
			return 1;
	}

	return 0;
}

static void receive_packet(seeq8005_t *seeq8005, uint8_t *packet, int packet_size)
{
	uint16_t write_ptr;
	uint16_t header_ptr;
	uint8_t status = 0;

//	aeh54_log("Receive packet, size=%i\n", packet_size);

//	aeh54_log("Rx ptr = %04x, end area = %04x\n", seeq8005->rx_ptr, seeq8005->rx_end_area);

	if (!filter_mac(seeq8005, packet))
		return;

	write_ptr = seeq8005->rx_ptr;
	header_ptr = write_ptr;

	if (write_rx_data(seeq8005, &write_ptr, 0))
		return;
	if (write_rx_data(seeq8005, &write_ptr, 0))
		return;
	if (write_rx_data(seeq8005, &write_ptr, 0))
		return;
	if (write_rx_data(seeq8005, &write_ptr, 0))
		return;

	for (int i = 0; i < packet_size; i++)
	{
		if (write_rx_data(seeq8005, &write_ptr, packet[i]))
			return;
	}

	status = RX_STATUS_DONE;

	if ((seeq8005->config_2 & CONFIG2_LOOPBACK) && (seeq8005->config_2 & CONFIG2_XMIT_NO_CRC))
	{
		uint32_t received_crc = packet[packet_size - 4] | (packet[packet_size - 3] << 8) |
				   (packet[packet_size - 2] << 16) | (packet[packet_size - 1] << 24);
		uint32_t crc = gen_crc(packet, packet_size - 4);

//		aeh54_log("crc=%08x rec_crc=%08x\n", crc, received_crc);
		if (crc != received_crc)
			status |= RX_STATUS_CRC_ERROR;
	}
	else if (seeq8005->config_2 & CONFIG2_REC_CRC)
	{
		uint32_t crc = gen_crc(packet, packet_size - 4);

		if (write_rx_data(seeq8005, &write_ptr, crc & 0xff))
			return;
		if (write_rx_data(seeq8005, &write_ptr, crc >> 8))
			return;
		if (write_rx_data(seeq8005, &write_ptr, crc >> 16))
			return;
		if (write_rx_data(seeq8005, &write_ptr, crc >> 24))
			return;
	}

	if (write_rx_data(seeq8005, &header_ptr, write_ptr >> 8))
		return;
	if (write_rx_data(seeq8005, &header_ptr, write_ptr & 0xff))
		return;
	if (write_rx_data(seeq8005, &header_ptr, RX_COMMAND_CHAIN_CONTINUE))
		return;
	if (write_rx_data(seeq8005, &header_ptr, status))
		return;

	seeq8005->rx_ptr = write_ptr;
	if (write_rx_data(seeq8005, &write_ptr, 0))
		return;
	if (write_rx_data(seeq8005, &write_ptr, 0))
		return;
	if (write_rx_data(seeq8005, &write_ptr, 0))
		return;
	if (write_rx_data(seeq8005, &write_ptr, 0))
		return;
	/*aeh54_log("CRC=%08x  %02x %02x %02x %02x\n", gen_crc(packet, packet_size - 4),
			packet[packet_size - 4],packet[packet_size - 3],packet[packet_size - 2],packet[packet_size - 1]);*/

	//aeh54_log("  now rx_ptr=%04x rx_end_area=%04x\n", seeq8005->rx_ptr, seeq8005->rx_end_area);
	seeq8005->status |= STATUS_RX_IRQ;
	update_irqs(seeq8005);
}

static void transmit_packet(seeq8005_t *seeq8005, uint8_t *packet, int packet_size)
{
	//aeh54_log("Transmit packet, size=%i\n", packet_size);

//	for (int i = 0; i < packet_size; i++)
//		aeh54_log("%02x ", packet[i]);
//	aeh54_log("\n");

	if ((seeq8005->config_2 & CONFIG2_LOOPBACK) && (seeq8005->status & STATUS_RX_ON))
		receive_packet(seeq8005, packet, packet_size);
	else if (!(seeq8005->config_2 & CONFIG2_LOOPBACK))
		seeq8005->net->write(seeq8005->net, packet, packet_size);
}

static uint8_t read_tx_data(seeq8005_t *seeq8005)
{
	uint8_t data = seeq8005->ram[seeq8005->tx_ptr];

	seeq8005->tx_ptr++;
	if (seeq8005->tx_ptr >= (seeq8005->tx_end_ptr+0x100))
		seeq8005->tx_ptr = 0;

	return data;
}

void seeq8005_poll(seeq8005_t *seeq8005)
{
	if (seeq8005->status & STATUS_TX_ON)
	{
		uint16_t next_packet_ptr;
		uint8_t command;
//		int packet_size;
		int status_ptr;

		/*Read next header*/
		next_packet_ptr = read_tx_data(seeq8005) << 8;
		next_packet_ptr |= read_tx_data(seeq8005);

		command = read_tx_data(seeq8005);
		status_ptr = seeq8005->tx_ptr;
		read_tx_data(seeq8005);

		if ((command & TX_COMMAND_DATA_FOLLOWS) && (command & TX_COMMAND_XMIT_RECEIVE))
		{
			uint8_t packet[0x10000];
			int packet_size = 0;
//			packet_size = (next_packet_ptr - seeq8005->tx_ptr) & 0xffff;

			//for (int i = 0; i < packet_size; i++)
			while (seeq8005->tx_ptr != next_packet_ptr && packet_size < 0x10000)
			{
				packet[packet_size] = read_tx_data(seeq8005);
				packet_size++;
			}

			transmit_packet(seeq8005, packet, packet_size);

			seeq8005->ram[status_ptr] = TX_STATUS_DONE;

			if (command & TX_COMMAND_SUCCESS_IRQ)
			{
				seeq8005->status |= STATUS_TX_IRQ;
				update_irqs(seeq8005);
			}
		}
//		aeh54_log("next_packet_ptr=%04x command=%02x\n", next_packet_ptr, command);
		seeq8005->tx_ptr = next_packet_ptr;

		if (!(command & TX_COMMAND_CHAIN_CONTINUE))
		{
//			aeh54_log("chain over\n");
			seeq8005->status &= ~STATUS_TX_ON;
		}
	}

	packet_t packet;

	if (!seeq8005->net->read(seeq8005->net, &packet))
	{
		//aeh54_log("ne2000 inQ:%d  got a %dbyte packet @%d\n",QueuePeek(slirpq),qp->len,qp);
	//	aeh54_log("  rx_ptr=%04x rx_end_area=%04x\n", seeq8005->rx_ptr, seeq8005->rx_end_area);
		if ((seeq8005->status & STATUS_RX_ON) && !(seeq8005->config_2 & CONFIG2_LOOPBACK))
		{
//			for (int i = 0; i < qp->len; i++)
//				aeh54_log("%02x ", qp->data[i]);
//			aeh54_log("\n");

			if (packet.len < 64)
			{
				uint8_t temp_packet[64];

				memcpy(temp_packet, packet.data, packet.len);
				memset(temp_packet+packet.len, 0, 64-packet.len);

				receive_packet(seeq8005, temp_packet, 64);
			}
			else
				receive_packet(seeq8005, packet.data, packet.len);
		}
		seeq8005->net->free(seeq8005->net, &packet);
	}
}

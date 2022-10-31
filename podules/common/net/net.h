#ifndef _NET_H_
#define _NET_H_

#include <stdint.h>
#include "podule_api.h"

typedef struct
{
	uint8_t *data;
	int len;
	void *p;
} packet_t;

struct net_t;

typedef struct
{
	void (*close)(struct net_t *net);
	int (*read)(struct net_t *net, packet_t *packet);
	void (*write)(struct net_t *net, uint8_t *data, int size);
	void (*free)(struct net_t *net, packet_t *packet);

	void *p;
} net_t;

#define NETWORK_DEVICE_DEFAULT "slirp"

net_t *net_init(const char *network_device, uint8_t *mac_addr);
podule_config_selection_t *net_get_networks(void);

#endif /* _NET_H_ */
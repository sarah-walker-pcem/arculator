#ifndef _NET_H_
#define _NET_H_

#include <stdint.h>

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

	void *p;
} net_t;

net_t *net_init(void);

#endif /* _NET_H_ */
#include <pthread.h>
#include "net.h"
#include "net_slirp.h"

#include "slirp/slirp.h"
#include "slirp/queue.h"

static queueADT *g_slirpq;
typedef struct net_slirp_t
{
	pthread_t poll_thread;
	volatile int exit_poll;
	queueADT slirpq;
} net_slirp_t;

static void *slirp_poll_thread(void *p)
{
	net_t *net = p;
	net_slirp_t *slirp = net->p;

	while (!slirp->exit_poll)
	{
		int ret2, nfds;
		struct timeval tv;
		fd_set rfds, wfds, xfds;
		int timeout;

		nfds = -1;
		FD_ZERO(&rfds);
		FD_ZERO(&wfds);
		FD_ZERO(&xfds);
		timeout = slirp_select_fill(&nfds, &rfds, &wfds, &xfds);

		if(timeout < 0)
			timeout = 500;
		tv.tv_sec = 0;
		tv.tv_usec = timeout;

		ret2 = select(nfds + 1, &rfds, &wfds, &xfds, &tv);
		if (ret2 >= 0)
			slirp_select_poll(&rfds, &wfds, &xfds);
	}

	return NULL;
}

static int net_slirp_read(net_t *net, packet_t *packet)
{
	net_slirp_t *slirp = net->p;

	if (QueuePeek(slirp->slirpq) > 0)
	{
		struct queuepacket *qp = QueueDelete(slirp->slirpq);

		packet->p = qp;
		packet->data = qp->data;
		packet->len = qp->len;

		return 0;
	}

	return -1;
}

static void net_slirp_write(net_t *net, uint8_t *data, int size)
{
	slirp_input(data, size);
}

static void net_slirp_free(net_t *net, packet_t *packet)
{
	packet->data = NULL;
	free(packet->p);
}


void slirp_output(const unsigned char *pkt, int pkt_len)
{
	struct queuepacket *p;
	p = (struct queuepacket *)malloc(sizeof(struct queuepacket));
	p->len = pkt_len;
	memcpy(p->data, pkt, pkt_len);
	QueueEnter(*g_slirpq, p);
//        aeh54_log("slirp_output %d @%d\n",pkt_len,p);
}
int slirp_can_output(void)
{
//        aeh54_log("slirp_can_output\n");
	return 1;
}


void net_slirp_close(net_t *net)
{
	net_slirp_t *slirp = net->p;

	slirp->exit_poll = 1;
	pthread_join(slirp->poll_thread, NULL);

	free(slirp);
	free(net);
}

net_t *slirp_net_init(void)
{
	struct in_addr myaddr;
	int rc;
	net_t *net = malloc(sizeof(*net));
	net_slirp_t *slirp = malloc(sizeof(*slirp));

	memset(net, 0, sizeof(*net));
	memset(slirp, 0, sizeof(*slirp));

	net->close = net_slirp_close;
	net->read = net_slirp_read;
	net->write = net_slirp_write;
	net->free = net_slirp_free;
	net->p = slirp;

	rc = slirp_init();
//        aeh54_log("ne2000 slirp_init returned: %d\n",rc);
	if (rc)
	{
		free(slirp);
		free(net);
		return NULL;
	}

//	aeh54_log("ne2000 slirp initalized!\n");
	inet_aton("10.0.2.15", &myaddr);
	//YES THIS NEEDS TO PULL FROM A CONFIG FILE... but for now.
	rc = slirp_redir(0, 42323, myaddr, 23);
//	aeh54_log("ne2000 slirp redir returned %d on port 42323 -> 23\n", rc);
	rc = slirp_redir(0, 42380, myaddr, 80);
//	aeh54_log("ne2000 slirp redir returned %d on port 42380 -> 80\n", rc);
	rc = slirp_redir(0, 42443, myaddr, 443);
//	aeh54_log("ne2000 slirp redir returned %d on port 42443 -> 443\n", rc);
	rc = slirp_redir(0, 42322, myaddr, 22);
//	aeh54_log("ne2000 slirp redir returned %d on port 42322 -> 22\n", rc);

	slirp->slirpq = QueueCreate();
	g_slirpq = &slirp->slirpq;

	pthread_create(&slirp->poll_thread, 0, slirp_poll_thread, net);

	return net;
}


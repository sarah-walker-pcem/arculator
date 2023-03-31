#include <stdlib.h>
#include <string.h>
#include "net.h"
#if defined(WIN32) || defined(ENABLE_PCAP)
#include "net_pcap.h"
#endif
#include "net_slirp.h"

net_t *net_init(const char *network_device, uint8_t *mac_addr)
{
	if (!strcmp(network_device, "slirp"))
		return slirp_net_init();
#if defined(WIN32) || defined(ENABLE_PCAP)
	return pcap_net_init(network_device, mac_addr);
#endif
	return NULL;
}

static const char *null_string = "";

podule_config_selection_t *net_get_networks(void)
{
	podule_config_selection_t *config;
#if defined(WIN32) || defined(ENABLE_PCAP)
	int nr_pcap_devs = pcap_net_get_devs(NULL, 0);
#else
	int nr_pcap_devs = 0;
#endif

	config = malloc(sizeof(*config) * (nr_pcap_devs + 1 + 1));
	if (!config)
		return NULL;

	config[0].description = "SLiRP";
	config[0].value_string = "slirp";

#if defined(WIN32) || defined(ENABLE_PCAP)
	pcap_net_get_devs(&config[1], nr_pcap_devs);
#endif

	config[nr_pcap_devs + 1].description = null_string;

	return config;
}
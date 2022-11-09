#include <pcap.h>
#include <string.h>
#include <windows.h>
#include "net.h"
#include "net_pcap.h"

//void aeh54_log(const char *format, ...);

typedef struct net_pcap_t
{
	pcap_t *pcap;
	uint8_t mac[6];
} net_pcap_t;

#define ETH_DEV_NAME_MAX     256                        /* maximum device name size */
#define ETH_DEV_DESC_MAX     256                        /* maximum device description size */
#define ETH_MAX_DEVICE        30                        /* maximum ethernet devices */
#define ETH_PROMISC            1                        /* promiscuous mode = true */
#define ETH_MAX_PACKET      1514                        /* maximum ethernet packet size */
#define PCAP_READ_TIMEOUT -1

static HINSTANCE net_hLib = 0;                      /* handle to DLL */
static const char *net_lib_name = "wpcap.dll";

typedef pcap_t* (__cdecl * PCAP_OPEN_LIVE)(const char *, int, int, int, char *);
typedef int (__cdecl * PCAP_SENDPACKET)(pcap_t *handle, const u_char *msg, int len);
typedef int (__cdecl * PCAP_SETNONBLOCK)(pcap_t *, int, char *);
typedef const u_char*(__cdecl *PCAP_NEXT)(pcap_t *, struct pcap_pkthdr *);
typedef const char*(__cdecl *PCAP_LIB_VERSION)(void);
typedef void (__cdecl *PCAP_CLOSE)(pcap_t *);
typedef int  (__cdecl *PCAP_GETNONBLOCK)(pcap_t *p, char *errbuf);
typedef int (__cdecl *PCAP_COMPILE)(pcap_t *p, struct bpf_program *fp, const char *str, int optimize, bpf_u_int32 netmask);
typedef int (__cdecl *PCAP_SETFILTER)(pcap_t *p, struct bpf_program *fp);

static PCAP_LIB_VERSION _pcap_lib_version;
static PCAP_OPEN_LIVE _pcap_open_live;
static PCAP_SENDPACKET _pcap_sendpacket;
static PCAP_SETNONBLOCK _pcap_setnonblock;
static PCAP_NEXT _pcap_next;
static PCAP_CLOSE _pcap_close;
static PCAP_GETNONBLOCK _pcap_getnonblock;
static PCAP_COMPILE _pcap_compile;
static PCAP_SETFILTER _pcap_setfilter;

static int (*_pcap_findalldevs)(pcap_if_t **, char *);
static void (*_pcap_freealldevs)(pcap_if_t *);
static int (*_pcap_datalink)(pcap_t *);

static int get_network_name(char *dev_name, char *regval)
{
	if (dev_name[strlen( "\\Device\\NPF_" )] == '{')
	{
		char regkey[2048];
		HKEY reghnd;

		sprintf(regkey, "SYSTEM\\CurrentControlSet\\Control\\Network\\"
			"{4D36E972-E325-11CE-BFC1-08002BE10318}\\%s\\Connection", dev_name+
			strlen("\\Device\\NPF_"));

		if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, regkey, 0, KEY_QUERY_VALUE, &reghnd) == ERROR_SUCCESS)
		{
			DWORD reglen = 2048;
			DWORD regtype;

			if (RegQueryValueExA(reghnd, "Name", NULL, &regtype, (LPBYTE)regval, &reglen) == ERROR_SUCCESS)
			{
				RegCloseKey (reghnd);

				if ((regtype != REG_SZ) || (reglen > 2048))
					return -1;

				/*Name now in regval*/
				return 0;
			}
			RegCloseKey (reghnd);
		}
	}
	return -1;
}

static int pcap_open_library(void)
{
	net_hLib = LoadLibraryA(net_lib_name);
	if(net_hLib==0)
	{
		//aeh54_log("ne2000 Failed to load %s\n",net_lib_name);
		return -1;
	}

	_pcap_lib_version  = (PCAP_LIB_VERSION)GetProcAddress(net_hLib, "pcap_lib_version");
	_pcap_open_live = (PCAP_OPEN_LIVE)GetProcAddress(net_hLib, "pcap_open_live");
	_pcap_sendpacket = (PCAP_SENDPACKET)GetProcAddress(net_hLib, "pcap_sendpacket");
	_pcap_setnonblock = (PCAP_SETNONBLOCK)GetProcAddress(net_hLib, "pcap_setnonblock");
	_pcap_next = (PCAP_NEXT)GetProcAddress(net_hLib, "pcap_next");
	_pcap_close = (PCAP_CLOSE)GetProcAddress(net_hLib, "pcap_close");
	_pcap_getnonblock = (PCAP_GETNONBLOCK)GetProcAddress(net_hLib, "pcap_getnonblock");
	_pcap_compile = (PCAP_COMPILE)GetProcAddress(net_hLib, "pcap_compile");
	_pcap_setfilter = (PCAP_SETFILTER)GetProcAddress(net_hLib, "pcap_setfilter");
	_pcap_findalldevs = (void *)GetProcAddress(net_hLib, "pcap_findalldevs");
	_pcap_freealldevs = (void *)GetProcAddress(net_hLib, "pcap_freealldevs");
	_pcap_datalink = (void *)GetProcAddress(net_hLib, "pcap_datalink");

	return 0;
}

static void pcap_close_library(void)
{
	FreeLibrary(net_hLib);
}

static int net_pcap_read(net_t *net, packet_t *packet)
{
	net_pcap_t *pcap = net->p;
	struct pcap_pkthdr h;
	unsigned char *data;

	data = _pcap_next(pcap->pcap, &h);
//	if (data)
//		aeh54_log("net_pcap_read: data=%x\n", data);
	if (data)
	{
		packet->data = data;
		packet->len = h.caplen;

		return 0;
	}

	return -1;
}

static void net_pcap_write(net_t *net, uint8_t *data, int size)
{
	net_pcap_t *pcap = net->p;

	//aeh54_log("net_pcap_write: data=%x size=%x\n", data, size);
	_pcap_sendpacket(pcap->pcap, data, size);
}

static void net_pcap_free(net_t *net, packet_t *packet)
{
	packet->data = NULL;
}

static void net_pcap_close(net_t *net)
{
	net_pcap_t *pcap = net->p;

	_pcap_close(pcap->pcap);

	free(pcap);
	free(net);

	pcap_close_library();
}

net_t *pcap_net_init(const char *network_device, uint8_t *mac_addr)
{
	char errbuf[PCAP_ERRBUF_SIZE];
	pcap_if_t *alldevs;
	int rc;

	net_t *net = malloc(sizeof(*net));
	net_pcap_t *pcap = malloc(sizeof(*pcap));

	memset(net, 0, sizeof(*net));
	memset(pcap, 0, sizeof(*pcap));

	net->close = net_pcap_close;
	net->read = net_pcap_read;
	net->write = net_pcap_write;
	net->free = net_pcap_free;
	net->p = pcap;

	memcpy(pcap->mac, mac_addr, 6);

	if (pcap_open_library())
	{
		//aeh54_log("pcap_open_library failed\n");
		return NULL;
	}

	pcap->pcap = _pcap_open_live(network_device, ETH_MAX_PACKET, ETH_PROMISC, PCAP_READ_TIMEOUT, errbuf);
	if (!pcap->pcap)
	{
		//aeh54_log("pcap_open_live fail\n");
		goto err_out;
	}

	//aeh54_log("pcap_open_live succeed\n");

	rc = _pcap_getnonblock(pcap->pcap, errbuf);
	//aeh54_log("ne2000 pcap is currently in %s mode\n", rc ? "non-blocking" : "blocking");
	switch (rc)
	{
		case 0:
		//aeh54_log("ne2000 Setting interface to non-blocking mode..");
		rc = _pcap_setnonblock(pcap->pcap, 1, errbuf);
		if (rc==0)
		{
			//aeh54_log("..");
			rc = _pcap_getnonblock(pcap->pcap, errbuf);
			//if (rc == 1)
				//aeh54_log("..!", rc);
			//else
				//aeh54_log("\tunable to set pcap into non-blocking mode!\nContinuining without pcap.\n");
		}
		else
			//aeh54_log("There was an unexpected error of [%s]\n\nexiting.\n",errbuf);
		//aeh54_log("\n");
		break;
		case 1:
		//aeh54_log("non blocking\n");
		break;
		default:
		//aeh54_log("this isn't right!!!\n");
		break;
	}

	if(_pcap_compile && _pcap_setfilter)
	{
		struct bpf_program fp;
		char filter_exp[255];

		//aeh54_log("ne2000 Building packet filter...");
		sprintf(filter_exp,"( ((ether dst ff:ff:ff:ff:ff:ff) or (ether dst %02x:%02x:%02x:%02x:%02x:%02x)) and not (ether src %02x:%02x:%02x:%02x:%02x:%02x) )",
			pcap->mac[0], pcap->mac[1], pcap->mac[2], pcap->mac[3], pcap->mac[4], pcap->mac[5],
			pcap->mac[0], pcap->mac[1], pcap->mac[2], pcap->mac[3], pcap->mac[4], pcap->mac[5]);

		//I'm doing a MAC level filter so TCP/IP doesn't matter.
		if (_pcap_compile(pcap->pcap, &fp, filter_exp, 0, 0xffffffff) == -1)
		{
			//aeh54_log("\nne2000 Couldn't compile filter\n");
		}
		else
		{
			//aeh54_log("...");
			rc = _pcap_setfilter(pcap->pcap, &fp);
			//if (rc == -1)
				//aeh54_log("\nError installing pcap filter.\n");
			//else
				//aeh54_log("...!\n");
		}
		//aeh54_log("ne2000 Using filter\t[%s]\n", filter_exp);
	}
	//else
		//aeh54_log("ne2000 Your platform lacks pcap_compile & pcap_setfilter\n");

	return net;

err_out:
	free(pcap);
	free(net);

	pcap_close_library();

	return NULL;
}

int pcap_net_get_devs(podule_config_selection_t *config, int max_devs)
{
	char errbuf[PCAP_ERRBUF_SIZE];
	pcap_if_t *alldevs;
	int nr_devs = 0;

	if (pcap_open_library())
	{
		//aeh54_log("pcap_net_nr_devs: failed to open pcap library\n");
		return 0;
	}

	if (_pcap_findalldevs(&alldevs, errbuf) != -1)
	{
		pcap_if_t *dev;

		for (dev = alldevs; dev; dev = dev->next)
		{
			pcap_t *conn = _pcap_open_live(dev->name, ETH_MAX_PACKET, ETH_PROMISC, PCAP_READ_TIMEOUT, errbuf);
			int datalink = 0;

			if (conn)
			{
				datalink = _pcap_datalink(conn);
				_pcap_close(conn);
			}

			if (conn && datalink == DLT_EN10MB)
			{
				if ((dev->flags & PCAP_IF_LOOPBACK) || (!strcmp("any", dev->name)))
					continue;

				if (nr_devs < max_devs)
				{
					char desc[2048];

					char *desc_s;
					char *value;

					desc_s = malloc(256);
					value = malloc(256);

					if (get_network_name(dev->name, desc))
						snprintf(desc_s, 255, "PCAP device (%s)\n", dev->name);
					else
						snprintf(desc_s, 255, "PCAP device \"%s\" (%s)\n", desc, dev->name);
					desc_s[255] = 0;

					strncpy(value, dev->name, 255);
					value[255] = 0;

					config[nr_devs].description = (const char *)desc_s;
					config[nr_devs].value_string = value;
				}

				nr_devs++;
			}
		}

		_pcap_freealldevs(alldevs);
	}

	pcap_close_library();

	return nr_devs;
}

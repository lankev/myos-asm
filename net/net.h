#ifndef _NET_H
#define _NET_H
#include "stdint.h"

extern int      net_ok;
extern int      net_dhcp_ok;
extern uint32_t net_my_ip;
extern uint32_t net_gw_ip;
extern uint32_t net_dns_ip;
extern uint32_t net_netmask;
extern uint8_t  net_mac[6];

int  net_init(void);
int  net_dhcp(void);
int  net_reconnect(void);
int  net_ping_gw(void);
int  net_http_get(const char *url, char *buf, int bufsize);
#endif

#ifndef _NET_H
#define _NET_H
#include <stdint.h>
extern int net_ok;
int  net_init(void);
int  net_http_get(const char *url, char *buf, int bufsize);
#endif

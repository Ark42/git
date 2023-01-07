/*--------------------------------------------------------------------------*\
|| packet.h by Ryan Rubley <ryan@morpheussoftware.net>                      ||
||                                                                          ||
|| (v0.90) 5-26-2001                                                        ||
|| (v0.95) 8-04-2002                                                        ||
|| (v0.96) 8-17-2002                                                        ||
|| (v0.97) 3-09-2004                                                        ||
|| (v0.98) 9-12-2004                                                        ||
|| (v0.99) 9-12-2004                                                        ||
|| Part of Gamer's Internet Tunnel                                          ||
|| Packet Adapter functions                                                 ||
||                                                                          ||
\*--------------------------------------------------------------------------*/

#ifndef _GIT_PACKET_H_
#define _GIT_PACKET_H_

#include <packet32.h>
#include <ntddndis.h>

#define MAX_ADAPTERS 10
#define ADAPTER_NAME_BUF 8192
extern unsigned AdapterCount;
extern char *AdapterList[MAX_ADAPTERS];
extern char *AdapterDesc[MAX_ADAPTERS];

void PacketFillAdapterList(void);
char *AdapterListNoGUID(int num);

#endif

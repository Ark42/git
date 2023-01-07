/*--------------------------------------------------------------------------*\
|| tun2eth.h by Ryan Rubley <ryan@morpheussoftware.net>                     ||
||                                                                          ||
|| (v0.91) 8-25-2001                                                        ||
|| (v0.95) 8-04-2002                                                        ||
|| (v0.96) 8-17-2002                                                        ||
|| (v0.97) 3-09-2004                                                        ||
|| (v0.98) 9-12-2004                                                        ||
|| (v0.99) 9-12-2004                                                        ||
|| Part of Gamer's Internet Tunnel                                          ||
|| Data shared betwen git.c and tun2eth.c                                   ||
||                                                                          ||
\*--------------------------------------------------------------------------*/

#ifndef _TUN2ETH_H_
#define _TUN2ETH_H_

//PT extern pthread_mutex_t tun2eth_mutex;
extern CRITICAL_SECTION tun2eth_mutex;
//PT extern pthread_mutex_t multi_mutex;
extern CRITICAL_SECTION multi_mutex;
extern int tun2eth_enabled;
//PT void *tun2eth(void *arg);
DWORD WINAPI tun2eth(void *arg);

#endif

/*--------------------------------------------------------------------------*\
|| eth2tun.h by Ryan Rubley <ryan@morpheussoftware.net>                     ||
||                                                                          ||
|| (v0.91) 8-25-2001                                                        ||
|| (v0.95) 8-04-2002                                                        ||
|| (v0.96) 8-17-2002                                                        ||
|| (v0.97) 3-09-2004                                                        ||
|| (v0.98) 9-12-2004                                                        ||
|| (v0.99) 9-12-2004                                                        ||
|| Gamer's Internet Tunnel                                                  ||
|| Data shared betwen git.c and eth2tun.c                                   ||
||                                                                          ||
\*--------------------------------------------------------------------------*/

#ifndef _ETH2TUN_H_
#define _ETH2TUN_H_

//PT extern pthread_mutex_t eth2tun_mutex;
extern CRITICAL_SECTION eth2tun_mutex;
extern int eth2tun_enabled;
//PT void *eth2tun(void *arg);
DWORD WINAPI eth2tun(void *arg);

#endif


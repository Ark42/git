/*--------------------------------------------------------------------------*\
|| pktcomp.h by Ryan Rubley <ryan@morpheussoftware.net>                     ||
||                                                                          ||
|| (v0.91) 8-25-2001                                                        ||
|| (v0.95) 8-04-2002                                                        ||
|| (v0.96) 8-17-2002                                                        ||
|| (v0.97) 3-09-2004                                                        ||
|| (v0.98) 9-12-2004                                                        ||
|| (v0.99) 9-12-2004                                                        ||
|| Part of Gamer's Internet Tunnel                                          ||
|| Packet Compresssion/Decompression                                        ||
||                                                                          ||
\*--------------------------------------------------------------------------*/

#ifndef _GIT_PKTCOMP_H_
#define _GIT_PKTCOMP_H_

unsigned int compress_packet(unsigned char *inbuf, unsigned int inlen, unsigned char *outbuf);
unsigned int decompress_packet(unsigned char *inbuf, unsigned int inlen, unsigned char *outbuf, unsigned int *advance);

#endif

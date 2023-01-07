/*--------------------------------------------------------------------------*\
|| pktcomp.cpp by Ryan Rubley <ryan@morpheussoftware.net>                   ||
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

#include "git.h"
#include "pktcomp.h"
#include "zlib/zlib.h"

#define PKTHDR_UNCOMP		0x0000
#define PKTHDR_ZLIB			0x4000
#define PKTHDR_RESERVED		0x8000
#define PKTHDR_EXTENDED		0xC000
#define PKTHDR_COMPMASK		0xC000
#define PKTHDR_SIZEMASK		0x3FFF
#include <stdio.h>

//compress a packet. if we don't have enough buffer space this will return 0, causing the packet to be dropped
unsigned int compress_packet(unsigned char *inbuf, unsigned int inlen, unsigned char *outbuf)
{
	unsigned int outlen = 0;
	if( options & OPT_COMP_ZLIB ) {
		unsigned long sz = MAX_PACKET_SIZE - 2;
		if( compress(outbuf + 2, &sz, inbuf, inlen) == Z_OK ) {
			*(unsigned short *)outbuf = htons(PKTHDR_ZLIB | (unsigned short)(sz & PKTHDR_SIZEMASK));
			outlen = sz + 2;
			if( outlen >= inlen + 2 ) outlen = 0; //ignore compression of we made it bigger
		}
	}
	if( !outlen ) {
		if( inlen + 2 <= MAX_PACKET_SIZE ) {
			memcpy(outbuf + 2, inbuf, inlen);
			*(unsigned short *)outbuf = htons(PKTHDR_UNCOMP | (unsigned short)(inlen & PKTHDR_SIZEMASK));
			outlen = inlen + 2;
		}
	}
	return outlen;
}

//decompress a packet, return 0 if we don't have enough stuff in inbuf to do so yet, or if decompressing failed.
//advance will be set to the number of bytes to advance the buffer the packet came from so that it points to the next
//packet if needed, or advance will be 0 if we don't have enough stuff in inbuf yet.
unsigned int decompress_packet(unsigned char *inbuf, unsigned int inlen, unsigned char *outbuf, unsigned int *advance)
{
	*advance = 0;
	unsigned int outlen = 0;
	if( inlen < 2 ) return 0;
	unsigned short compsize = ntohs(*(unsigned short *)inbuf);
	unsigned short comptype = compsize & PKTHDR_COMPMASK;
	compsize &= PKTHDR_SIZEMASK;
	if( inlen - 2 < compsize ) return 0;
	inbuf += 2;
	switch( comptype ) {
	default:
	case PKTHDR_UNCOMP:
		memcpy(outbuf, inbuf, compsize);
		outlen = compsize;
		break;

	case PKTHDR_ZLIB: {
		unsigned long sz = MAX_PACKET_SIZE;
		if( uncompress(outbuf, &sz, inbuf, compsize) == Z_OK ) {
			outlen = sz;
		}
		break; }
	}
	*advance = compsize + 2;
	return outlen;
}

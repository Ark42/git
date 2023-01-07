/*--------------------------------------------------------------------------*\
|| eth2tun.c by Ryan Rubley <ryan@morpheussoftware.net>                     ||
||                                                                          ||
|| (v0.91) 8-25-2001                                                        ||
|| (v0.95) 8-04-2002                                                        ||
|| (v0.96) 8-17-2002                                                        ||
|| (v0.97) 3-09-2004                                                        ||
|| (v0.98) 9-12-2004                                                        ||
|| (v0.99) 9-12-2004                                                        ||
|| Part of Gamer's Internet Tunnel                                          ||
|| Grabs IPX packets from raw packet device and sends them thru each tunnel ||
||                                                                          ||
\*--------------------------------------------------------------------------*/

#include "git.h"
#include "utils.h"
#include "packet.h"
#include "eth2tun.h"
#include "pktcomp.h"

LPADAPTER recvAdapter;
LPPACKET recvPacket;
unsigned char recvBuffer[256*1024];
//PT pthread_mutex_t eth2tun_mutex = PTHREAD_MUTEX_INITIALIZER;
CRITICAL_SECTION eth2tun_mutex;
int eth2tun_enabled;

void ParsePackets(LPPACKET pkts);

//PT void *eth2tun(void *arg)
DWORD WINAPI eth2tun(void *arg)
{
	struct hostent *he;

	EnterCriticalSection(&eth2tun_mutex);

	//if we init winpcap twice at the same time
	//it might cause a fatal exception and lock the system
	//so we have to wait for them to init one at a time
	EnterCriticalSection(&initing);

	if(device>=0 && (unsigned)device<AdapterCount) {
		recvAdapter = PacketOpenAdapter(AdapterList[device]);
	} else {
		recvAdapter = NULL;
	}
	if(!recvAdapter || (recvAdapter->hFile == INVALID_HANDLE_VALUE)) {
		MessageBox(NULL,"Unable to open the packet device.\n"
			"Please select a different one under Advanced Configuration options.",
			GIT_NAME,MB_ICONSTOP);
		LeaveCriticalSection(&eth2tun_mutex);
		eth2tun_enabled = 1;
		LeaveCriticalSection(&initing);
		return NULL;
	}	
	if(!PacketSetHwFilter(recvAdapter,NDIS_PACKET_TYPE_PROMISCUOUS)) {
		MessageBox(NULL,"Failed to set device to promiscuous mode.\n",
			GIT_NAME,MB_ICONSTOP);
		PacketCloseAdapter(recvAdapter);
		LeaveCriticalSection(&eth2tun_mutex);
		eth2tun_enabled = 1;
		LeaveCriticalSection(&initing);
		return NULL;
	}
	if(!PacketSetBuff(recvAdapter,512*1024)) {
		MessageBox(NULL,"Failed to allocate receive buffer.\n",
			GIT_NAME,MB_ICONSTOP);
		PacketCloseAdapter(recvAdapter);
		LeaveCriticalSection(&eth2tun_mutex);
		eth2tun_enabled = 1;
		LeaveCriticalSection(&initing);
		return NULL;
	}
	PacketSetReadTimeout(recvAdapter,100);
	if((recvPacket = PacketAllocatePacket())==NULL) {
		MessageBox(NULL,"Failed to allocate the packet structure memory.",
			GIT_NAME,MB_ICONSTOP);
		PacketCloseAdapter(recvAdapter);
		LeaveCriticalSection(&eth2tun_mutex);
		eth2tun_enabled = 1;
		LeaveCriticalSection(&initing);
		return NULL;
	}
	PacketInitPacket(recvPacket,recvBuffer,sizeof(recvBuffer));

	eth2tun_enabled = 1;
	LeaveCriticalSection(&initing);

	if( options & OPT_IPV4_NAT ) {
		if( (he = gethostbyname((char *)natinfo.internal)) ) {
			natinfo.intaddr.s_addr =  *(long *)(he->h_addr);
		} else {
			natinfo.intaddr.s_addr = inet_addr((char *)natinfo.internal);
		}
		if( (he = gethostbyname((char *)natinfo.external)) ) {
			natinfo.extaddr.s_addr =  *(long *)(he->h_addr);
		} else {
			natinfo.extaddr.s_addr = inet_addr((char *)natinfo.external);
		}
	}

	while(eth2tun_enabled) {
		if(!PacketReceivePacket(recvAdapter,recvPacket,TRUE)){
			MessageBox(NULL,"Failed to receive packet.",
				GIT_NAME,MB_ICONSTOP);
			eth2tun_enabled = 0;
			break;
		}
		ParsePackets(recvPacket);
	}

	PacketFreePacket(recvPacket);
	PacketCloseAdapter(recvAdapter);

	LeaveCriticalSection(&eth2tun_mutex);
	return NULL;
}

void ParsePackets(LPPACKET pkts)
{
	static unsigned char *base;
	static unsigned char cmpBuf[MAX_PACKET_SIZE];
	static char *msg;
	static unsigned off,frame;
	static struct bpf_hdr *hdr;
	static struct ipx_s *ipx;
	static struct ipv4_s ipv4;
	static struct arp_s *arp;
	static unsigned int h, cmpLen;

	for( off=0; off<pkts->ulBytesReceived; off=Packet_WORDALIGN(off+hdr->bh_datalen) ) {	
		hdr = (struct bpf_hdr *)((char *)pkts->Buffer+off);
		off += hdr->bh_hdrlen;
		base = (unsigned char *)pkts->Buffer+off;
		analyze_frame(base,hdr->bh_caplen,&frame,&ipx,&ipv4,&arp,&msg);
		if( ipx || ipv4.header || arp ) { //if its an ipx or ipv4 frame or arp
			if( (!(options & OPT_OTHER_ORFP) && (
				//if hw src addr not in list (new reforward prevention method)
				!check_hwsrcaddr_is_in_list(base + 6, 6)
				//if its not traveled too far (old reforward prevention method)
				)) || ((options & OPT_OTHER_ORFP) && (
				(ipx && ipx->tc<8) || (ipv4.header && ipv4.header->ttl>8) || (arp && !check_hwsrcaddr_is_in_list(base + 6, 6))
				)) ) {

				if( !msg ) { //if we fwd, check some other things that might make us not send
					if( ipv4.header ) {
						if( ipv4.header->dstaddr==0xFFFFFFFF ) {
							if( options & OPT_IPV4_NOBC ) {
								msg = "destination is broadcast";
							}
						} else {
							if( options & OPT_IPV4_NOUC ) {
								msg = "destination is unicast";
							} else if( (options & OPT_IPV4_NORT) && addr_is_routable(ipv4.header->dstaddr) ) {
								msg = "destination is routable";
							}
						}
					}
				}
				if( !msg ) { //if we fwd this

					//if using old reforward prevention method, modify packets
					if( (options & OPT_OTHER_ORFP) ) {
						if( ipx ) {
							ipx->tc += 8;
						} else if( ipv4.header ) {
							ipv4.header->ttl = 8;
						} else {
							//nothing to modify for arp
						}
					}
					if( ipv4.header ) {
						//adjust for NAT
						if( (options & OPT_IPV4_NAT) && ipv4.header->srcaddr == natinfo.intaddr.s_addr ) {
							ipv4.header->srcaddr = natinfo.extaddr.s_addr;
						}
						//recalculate checksums if packet modifed from old reforward prevention method or from nat adjustment
						if( (options & (OPT_IPV4_NAT|OPT_OTHER_ORFP)) ) {
							calc_ipv4_sum(ipv4.header);
							switch(ipv4.header->proto) {
							case PROTO_ICMP:
								calc_icmp_sum(ipv4.header);
								break;
							case PROTO_TCP:
								calc_tcp_sum(ipv4.header);
								break;
							case PROTO_UDP:
								calc_udp_sum(ipv4.header);
								break;
							}
						}
					}

					//compress packet
					cmpLen = compress_packet(base, hdr->bh_caplen, cmpBuf);

					//send to each host
					for( h=0; h<numhosts; h++ ) {
						if( hosts[h].state==FH_STATE_UP ) {
							if( send(hosts[h].s, (char *)cmpBuf, cmpLen, 0) <= 0 ) {
								hosts[h].state = FH_STATE_DOWN;
								hosts[h].error = FH_FAIL_SEND;
							} else {
								hosts[h].lastfwd = time(NULL);
								if( options & OPT_LOG_FWD ) {
									add_to_log("network",frame,base+6,ipx,ipv4,arp,"ok",LOGFILE_FWD);
								}
							}
						}
					}
				} else {
					if( options & OPT_LOG_UNFWD ) {
						add_to_log("network",frame,base+6,ipx,ipv4,arp,msg,LOGFILE_UNFWD);
					}
				}
			}
		}
	}
}

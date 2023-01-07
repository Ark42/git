/*--------------------------------------------------------------------------*\
|| tun2eth.c by Ryan Rubley <ryan@morpheussoftware.net>                     ||
||                                                                          ||
|| (v0.91) 8-25-2001                                                        ||
|| (v0.95) 8-04-2002                                                        ||
|| (v0.96) 8-17-2002                                                        ||
|| (v0.97) 3-09-2004                                                        ||
|| (v0.98) 9-12-2004                                                        ||
|| (v0.99) 9-12-2004                                                        ||
|| Part of Gamer's Internet Tunnel                                          ||
|| Receives IPX packets from tunnel sockets, retransmits the raw packets    ||
||                                                                          ||
\*--------------------------------------------------------------------------*/

#include "git.h"
#include "utils.h"
#include "packet.h"
#include "tun2eth.h"
#include "pktcomp.h"

LPADAPTER sendAdapter;
LPPACKET sendPacket;
//PT pthread_mutex_t tun2eth_mutex = PTHREAD_MUTEX_INITIALIZER;
CRITICAL_SECTION tun2eth_mutex;
//PT pthread_mutex_t multi_mutex = PTHREAD_MUTEX_INITIALIZER;
CRITICAL_SECTION multi_mutex;
int tun2eth_enabled;

int addr_in_range(unsigned long addr,unsigned long range);
//PT void *connect_socket(void *arg);
DWORD WINAPI connect_socket(void *arg);

//PT void *tun2eth(void *arg)
DWORD WINAPI tun2eth(void *arg)
{
	unsigned char sendBuffer[MAX_PACKET_SIZE];
	char *msg;
	unsigned frame,i;
	int len,addrlen,m;
	//PT pthread_t tid;
	DWORD tid;
	fd_set fds;
	struct timeval tv;
	struct ipx_s *ipx;
	struct ipv4_s ipv4;
	struct arp_s *arp;
	struct sockaddr_in from;

	EnterCriticalSection(&tun2eth_mutex);

	//if we init winpcap twice at the same time
	//it might cause a fatal exception and lock the system
	//so we have to wait for them to init one at a time
	EnterCriticalSection(&initing);

	if(device>=0 && (unsigned)device<AdapterCount) {
		sendAdapter = PacketOpenAdapter(AdapterList[device]);
	} else {
		sendAdapter = NULL;
	}
	if(!sendAdapter || (sendAdapter->hFile == INVALID_HANDLE_VALUE)) {
		MessageBox(NULL,"Unable to open the packet device.\n"
			"Please select a different one under Advanced Configuration options.",
			GIT_NAME,MB_ICONSTOP);
		LeaveCriticalSection(&tun2eth_mutex);
		tun2eth_enabled = 1;
		LeaveCriticalSection(&initing);
		return NULL;
	}	
	if((sendPacket = PacketAllocatePacket())==NULL) {
		MessageBox(NULL,"Failed to allocate the packet structure memory.",
			GIT_NAME,MB_ICONSTOP);
		PacketCloseAdapter(sendAdapter);
		LeaveCriticalSection(&tun2eth_mutex);
		tun2eth_enabled = 1;
		LeaveCriticalSection(&initing);
		return NULL;
	}

	//clear out hw src addr info list
	EnterCriticalSection(&hwsrclist_mutex);
	memset(hw_src_addr_info, 0, sizeof(hw_src_addr_info));
	LeaveCriticalSection(&hwsrclist_mutex);

	tun2eth_enabled = 1;
	LeaveCriticalSection(&initing);

	//spawn threads to connect all our sockets.
	multi.s = -1;
	for( i=0; i<numhosts; i++ ) {
		hosts[i].state = FH_STATE_CONNECTING;
		hosts[i].disconnected = time(NULL);
		//PT pthread_create( &tid, NULL, connect_socket, (void *)i );
		CreateThread(NULL, 0, connect_socket, (void *)i, 0, &tid );
	}

	while(tun2eth_enabled) {
		FD_ZERO(&fds);
		for( i=m=0; i<numhosts; i++ ) {
			//build a list of up sockets
			if( hosts[i].state == FH_STATE_UP ) {
				FD_SET(hosts[i].s,&fds);
				if(hosts[i].s>m) m=hosts[i].s;
			}
			//respawn connect thread on error
			if( hosts[i].state == FH_STATE_DOWN && hosts[i].error ) {
				closesocket(hosts[i].s);
				hosts[i].s = -1;
				hosts[i].state = FH_STATE_CONNECTING;
				hosts[i].disconnected = time(NULL);
				//PT pthread_create( &tid, NULL, connect_socket, (void *)i );
				CreateThread(NULL, 0, connect_socket, (void *)i, 0, &tid );
			}
		}
		//no up sockets, wait 100ms and try again
		if( !m ) {
			Sleep(100);
			continue;
		}
		//wait 100ms on any up sockets for any data
		tv.tv_sec = 0;
		tv.tv_usec = 100000;
		if(select(m+1,&fds,NULL,NULL,&tv)>0) {
			for( i=0; i<numhosts; i++ ) {
				if( FD_ISSET(hosts[i].s,&fds) ) {
					if( hosts[i].method == METH_UDP ) {
						addrlen = sizeof(from);
						len = recvfrom(hosts[i].s, (char *)hosts[i].buf, MAX_PACKET_SIZE, 0, (struct sockaddr *)&from, &addrlen);
						if( len == SOCKET_ERROR && WSAGetLastError() == WSAEMSGSIZE ) len = MAX_PACKET_SIZE;
						if( addr_in_range(from.sin_addr.s_addr,hosts[i].addr.s_addr) ) {
							hosts[i].from.s_addr = from.sin_addr.s_addr;
						} else {
							if( options & OPT_LOG_ADDR ) {
								add_to_log_addr((char *)hosts[i].name,hosts[i].addr,from.sin_addr,"incoming UDP packet denied");
							}
							continue;
						}
					} else {
						//receive into the buffer for this host, adding onto the total length we have so far
						int alreadyhave = hosts[i].pbuf - hosts[i].buf;
						len = recv(hosts[i].s, (char *)hosts[i].pbuf, MAX_PACKET_SIZE - alreadyhave, 0);
						hosts[i].pbuf += len;
						len += alreadyhave;
					}
					if( len <= 0 ) {
						hosts[i].state = FH_STATE_DOWN;
						hosts[i].error = FH_FAIL_RECV;
					} else {
						//decompress packet
						unsigned int adv;
						unsigned int newlen = decompress_packet(hosts[i].buf, len, sendBuffer, &adv);
						if( adv > 0 ) { //eat some of the buffer, move back what we need to keep
							memmove(hosts[i].buf, hosts[i].buf + adv, len - adv);
							hosts[i].pbuf -= adv;
						}
						if( !newlen ) { //if not enough in buf or failed, don't send for now
							continue;
						}
						len = newlen;

						hosts[i].lastrecv = time(NULL);
						analyze_frame(sendBuffer,len,&frame,&ipx,&ipv4,&arp,&msg);
						if( ipx || ipv4.header || arp ) {
							if( !msg ) {
								//add the src addr to the list unless using old reforward prevention method
								if( ((options & OPT_OTHER_ORFP) && !arp) || add_hwsrcaddr(sendBuffer + 6, 6) ) {
									msg = "ok";
								} else {
									msg = "ok, but address table full";
								}
								PacketInitPacket(sendPacket,sendBuffer,len);
								PacketSendPacket(sendAdapter,sendPacket,TRUE);
							}
						} else {
							msg = "error";
						}
						if( options & OPT_LOG_IN ) {
							add_to_log((char *)hosts[i].name,frame,sendBuffer+6,ipx,ipv4,arp,msg,LOGFILE_IN);
						}
					}
				}
			}
		}
	}

	PacketFreePacket(sendPacket);
	PacketCloseAdapter(sendAdapter);

	//close sockets / shutdown threads
	for( i=0;i<numhosts; i++ ) {
		switch(hosts[i].state) {
		case FH_STATE_DOWN:
			if( hosts[i].error ) {
				closesocket(hosts[i].s);
				hosts[i].s = -1;
				hosts[i].error = 0;
				hosts[i].disconnected = time(NULL);
			}
			break;

		case FH_STATE_CONNECTING:
		case FH_STATE_FAILED:
			hosts[i].shutdown = 1;
			break;

		case FH_STATE_UP:
			closesocket(hosts[i].s);
			hosts[i].s = -1;
			hosts[i].state = FH_STATE_DOWN;
			hosts[i].disconnected = time(NULL);
			break;
		}
	}

	//wait for all connecting threads to finish shutdown
	do {
		m = 0;
		for( i=0;i<numhosts; i++ ) {
			if( hosts[i].shutdown ) {
				Sleep(100);
				m = 1;
				break;
			}
		}
	} while( m );

	//close multi listen socket
	if(options & OPT_TCP_MULTI) {
		EnterCriticalSection(&multi_mutex);
		if(multi.s != -1) {
			closesocket(multi.s);
			multi.s = -1;
		}
		LeaveCriticalSection(&multi_mutex);
	}

	LeaveCriticalSection(&tun2eth_mutex);
	return NULL;
}

int addr_in_range(unsigned long addr,unsigned long range)
{
	addr = ntohl(addr);
	range = ntohl(range);
	if( !range ) { //0.0.0.0
		return -1;
	} else if( !(range & 0x00FFFFFF) ) { //x.0.0.0
		return (addr & 0xFF000000) == range;
	} else if( !(range & 0x0000FFFF) ) { //x.x.0.0
		return (addr & 0xFFFF0000) == range;
	} else if( !(range & 0x000000FF) ) { //x.x.x.0
		return (addr & 0xFFFFFF00) == range;
	} else {
		return addr == range;
	}
}

//PT void *connect_socket(void *arg)
DWORD WINAPI connect_socket(void *arg)
{
	struct fwd_host_s *fh = &hosts[(unsigned)arg];
	struct hostent *he;
	struct sockaddr_in sin,from;
	int listensock,addrlen;
	fd_set fds;
	struct timeval tv;

	while(!fh->shutdown && (fh->state==FH_STATE_CONNECTING || fh->state==FH_STATE_FAILED)) {

		sin.sin_family = AF_INET;
		sin.sin_addr.s_addr = INADDR_ANY;
		sin.sin_port = htons(fh->port);
		if( (he = gethostbyname((char *)fh->name)) ) {
			fh->addr.s_addr =  *(long *)(he->h_addr);
		} else {
			fh->addr.s_addr = inet_addr((char *)fh->name);
		}

		switch(fh->method) {
		default:
		case METH_UDP:
			if( (fh->s=socket(AF_INET,SOCK_DGRAM,0)) == -1 ) {
				fh->state = FH_STATE_FAILED;
				fh->error = FH_FAIL_SOCKET;
				break;
			}
			if( bind(fh->s,(struct sockaddr *)&sin,sizeof(struct sockaddr_in)) == -1 ) {
				closesocket(fh->s);
				fh->s = -1;
				fh->state = FH_STATE_FAILED;
				fh->error = FH_FAIL_BIND;
				break;
			}
			sin.sin_addr = fh->addr;
			if( connect(fh->s,(struct sockaddr *)&sin,sizeof(struct sockaddr_in)) == -1 ) {
				closesocket(fh->s);
				fh->s = -1;
				fh->state = FH_STATE_FAILED;
				fh->error = FH_FAIL_CONNECT;
				break;
			}
			fh->state = FH_STATE_UP;
			break;

		case METH_TCP_LISTEN:
			if( (listensock=socket(AF_INET,SOCK_STREAM,0)) == -1 ) {
				fh->state = FH_STATE_FAILED;
				fh->error = FH_FAIL_SOCKET;
				break;
			}
			if( bind(listensock,(struct sockaddr *)&sin,sizeof(struct sockaddr)) == -1 ) {
				closesocket(listensock);
				fh->state = FH_STATE_FAILED;
				fh->error = FH_FAIL_BIND;
				break;
			}
			listen(listensock,0);
			FD_ZERO(&fds);
			FD_SET(listensock,&fds);
			tv.tv_sec = 2;
			tv.tv_usec = 200000;
			if( select(listensock+1,&fds,NULL,NULL,&tv) == 1 ) {
				addrlen = sizeof(from);
				if( (fh->s=accept(listensock,(struct sockaddr *)&from,&addrlen)) == -1 ) {
					closesocket(listensock);
					fh->state = FH_STATE_FAILED;
					fh->error = FH_FAIL_ACCEPT;
					break;
				}
				if( addr_in_range(from.sin_addr.s_addr,fh->addr.s_addr) ) {
					fh->from.s_addr = from.sin_addr.s_addr;
				} else {
					fh->from.s_addr = from.sin_addr.s_addr;
					closesocket(listensock);
					closesocket(fh->s);
					fh->state = FH_STATE_FAILED;
					fh->error = FH_FAIL_WRONGADDR;
					if( options & OPT_LOG_ADDR ) {
						add_to_log_addr((char *)fh->name,fh->addr,from.sin_addr,"TCP listen connection denied");
					}
					break;
				}
				fh->state = FH_STATE_UP;
			}
			closesocket(listensock);
			break;

		case METH_TCP_CONNECT:
			if( (fh->s=socket(AF_INET,SOCK_STREAM,0)) == -1 ) {
				fh->state = FH_STATE_FAILED;
				fh->error = FH_FAIL_SOCKET;
				break;
			}
			sin.sin_addr = fh->addr;
			if( connect(fh->s,(struct sockaddr *)&sin,sizeof(struct sockaddr_in)) == -1 ) {
				closesocket(fh->s);
				fh->s = -1;
				fh->state = FH_STATE_FAILED;
				fh->error = FH_FAIL_CONNECT;
				break;
			}
			fh->state = FH_STATE_UP;
			break;

		case METH_TCP_CONNECT_SOCKS4:
		case METH_TCP_CONNECT_SOCKS5:
			from.sin_family = AF_INET;
			from.sin_port = htons(socksinfo.port);
			if( (he = gethostbyname((char *)socksinfo.hostname)) ) {
				from.sin_addr.s_addr =  *(long *)(he->h_addr);
			} else {
				from.sin_addr.s_addr = inet_addr((char *)socksinfo.hostname);
			}
			if( (fh->s=socket(AF_INET,SOCK_STREAM,0)) == -1 ) {
				fh->state = FH_STATE_FAILED;
				fh->error = FH_FAIL_SOCKET;
				break;
			}
			if( connect(fh->s,(struct sockaddr *)&from,sizeof(struct sockaddr_in)) == -1 ) {
				closesocket(fh->s);
				fh->s = -1;
				fh->state = FH_STATE_FAILED;
				fh->error = FH_FAIL_CONNECT_SOCKS;
				break;
			}
			sin.sin_addr = fh->addr;
			if( (fh->error=connect_socks(fh->s,fh->method==METH_TCP_CONNECT_SOCKS4?S4_VER:S5_VER,&sin)) ) {
				closesocket(fh->s);
				fh->s = -1;
				fh->state = FH_STATE_FAILED;
				break;
			}
			fh->state = FH_STATE_UP;
			break;

		case METH_TCP_MULTI_LISTEN:
			EnterCriticalSection(&multi_mutex);
			if( fh->shutdown ) {
				LeaveCriticalSection(&multi_mutex);
				break;
			}
			if(multi.s == -1) {
				if( (multi.s = socket(AF_INET,SOCK_STREAM,0)) == -1 ) {
					LeaveCriticalSection(&multi_mutex);
					fh->state = FH_STATE_FAILED;
					fh->error = FH_FAIL_SOCKET;
					break;
				}
				if( bind(multi.s,(struct sockaddr *)&sin,sizeof(struct sockaddr)) == -1 ) {
					closesocket(multi.s);
					multi.s = -1;
					LeaveCriticalSection(&multi_mutex);
					fh->state = FH_STATE_FAILED;
					fh->error = FH_FAIL_BIND;
					break;
				}
				listen(multi.s,1);
			}
			FD_ZERO(&fds);
			FD_SET(multi.s,&fds);
			tv.tv_sec = 2;
			tv.tv_usec = 200000;
			if( select(multi.s+1,&fds,NULL,NULL,&tv) == 1 ) {
				addrlen = sizeof(from);
				if( (fh->s=accept(multi.s,(struct sockaddr *)&from,&addrlen)) == -1 ) {
					LeaveCriticalSection(&multi_mutex);
					fh->state = FH_STATE_FAILED;
					fh->error = FH_FAIL_ACCEPT;
					break;
				}
				fh->from.s_addr = from.sin_addr.s_addr;
				fh->state = FH_STATE_UP;
			}
			LeaveCriticalSection(&multi_mutex);
			break;
		}

		if( !fh->shutdown && fh->state==FH_STATE_FAILED && fh->method!=METH_TCP_LISTEN && fh->method!=METH_TCP_MULTI_LISTEN ) {
			Sleep(2000);
		}
	}

	if( fh->state == FH_STATE_UP ) {
		fh->pbuf = fh->buf;
		fh->connected = time(NULL);
	}

	fh->error = 0;

	if( fh->shutdown ) {
		closesocket(fh->s);
		fh->s = -1;
		fh->shutdown = 0;
		fh->state = FH_STATE_DOWN;
		fh->disconnected = time(NULL);
	}

	return NULL;
}

/*--------------------------------------------------------------------------*\
|| utils.c by Ryan Rubley <ryan@morpheussoftware.net>                       ||
||                                                                          ||
|| (v0.91) 8-25-2001                                                        ||
|| (v0.95) 8-04-2002                                                        ||
|| (v0.96) 8-17-2002                                                        ||
|| (v0.97) 3-09-2004                                                        ||
|| (v0.98) 9-12-2004                                                        ||
|| (v0.99) 9-12-2004                                                        ||
|| Part of Gamer's Internet Tunnel                                          ||
|| Just some utility functions                                              ||
||                                                                          ||
\*--------------------------------------------------------------------------*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "utils.h"

//defaults for socket combo box
char *def_socket_list[] = {
"87c2 : Warcraft II",
"17df-17e0 : Starcraft",
"869c : Doom2 / Heretic",
"5100 : Descent",
"",
"0000-ffff : All sockets",
"8000-ffff : All well-known",
/*"",
"0001 : RIP",
"0002 : echo",
"0003 : error",
"0451 : NCP",
"0452 : SAP",
"0453 : RIP",
"0455 : NetBIOS",
"9001 : NLSP",
"9004 : NetBIOS/IPXWAN",
"9093 : IPXF",*/
NULL };

char *def_port_list[] = {
"2299-2300: AoM:Titans",
"27888-27890 : AvP 2:PH",
"5555 : Codename Panzers",
"8086 : C&C Generals",
"19271 : Dungeon Siege",
"47624 : DX7 Games",
"6073 : DX8 Games",
"2300-2400 : DX7/8 Games",
"27015 : Half-life",
"18321 : Medieval Total War",
"27910 : Quake2",
"27960 : Quake3",
"34987 : Rise of Nations",
"26220 : Rome Total War",
"27960 : RTCW/RTCW:ET",
"6112 : Starcraft",
"6112 : Warcraft III",
"3074 : XBox System Link",
"",
"1-212 : All below GIT range",
"217-65535 : All the rest",
"217-1024 : Well-known ports",
"1025-9999 : Common ports",
NULL };

char *method_list[] = {
"UDP - fastest!",
"TCP listen",
"TCP connect",
"TCP connect via socks4",
"TCP connect via socks5",
"TCP multi-listen",
NULL };

//hw src addr info to prevent reforwarding of packets, keep a list of all known-remote addresses
//PT pthread_mutex_t hwsrclist_mutex = PTHREAD_MUTEX_INITIALIZER;
CRITICAL_SECTION hwsrclist_mutex;
struct hw_addr_info_s hw_src_addr_info[MAX_SRC_HW_INFO];

void StrToFwdHost(char *s,struct fwd_host_s *fh)
{
	char *p,*q;
	int i;

	//find a colon
	p = strchr(s,':');
	if( p ) { //if we have a colon, end hostname there, find port after it
		*p++ = '\0';
		fh->port = atoi(p);

		//find a paranthesi
		p = strchr(p,'(');
		if( p ) {
			//find ending paranthesi
			q = strchr(++p,')');
			if( q ) { //if we found one, end the string there
				*q = '\0';
			}
			//search for a matching method name
			for( i=0; method_list[i]; i++ ) {
				if( !strcmp(p,method_list[i]) ) {
					fh->method = i+1;
					break;
				}
			}
		} else { //no paranthesi, default method
			fh->method = METH_UDP;
		}
	} else { //no colon, default port, all of string is hostname
		fh->port = 213;
	}
	strncpy((char *)fh->name,s,MAX_HOSTLEN-1);
	fh->name[MAX_HOSTLEN-1] = '\0';
	fh->addr.s_addr = 0;
	fh->s = -1;
	fh->state = FH_STATE_DOWN;
	fh->error = 0;
	fh->shutdown = 0;
	fh->connected = 0;
	fh->disconnected = 0;
	fh->lastfwd = 0;
	fh->lastrecv = 0;
}

void FwdHostToStr(struct fwd_host_s *fh,char *s)
{
	int i;
	//copy until end of string or we find a reserved char
	for( i=0; fh->name[i] && fh->name[i]!=':' && fh->name[i]!='(' && fh->name[i]!=')'; i++ ) {
		s[i] = fh->name[i];
	}
	if( fh->method < METH_UDP ) {
		fh->method = METH_UDP;
	}
	if( fh->method > METH_TCP_MULTI_LISTEN ) {
		fh->method = METH_TCP_MULTI_LISTEN;
	}
	//add on port and method
	sprintf(&s[i],":%d (%s)",fh->port,method_list[fh->method-1]);
}

void StrToSocketRange(char *s,struct socket_range_s *sr)
{
	unsigned short v;

	//grab first hex value
	sr->min = (unsigned short)strtol(s,&s,16);

	//eat whitespace
	while(*s==' ') s++;

	//if there is a dash, its a range
	if(*s=='-') {

		//eat whitespace and dash
		while(*++s==' ');

		//grab 2nd hex value
		v = (unsigned short)strtol(s,&s,16);
		if(v>=sr->min) {
			sr->max = v;
		} else { //stupid user, backwards order
			sr->max = sr->min;
			sr->min = v;
		}

		//eat whitespace
		while(*s==' ') s++;

	} else { //no dash, no range
		sr->max = sr->min;
	}

	//if there is a colon, there is a description
	if(*s==':') {

		//eat whitespace and colon
		while(*++s==' ');

		//grab description
		strncpy((char *)sr->desc,s,MAX_SOCKET_DESC-1);
		sr->desc[MAX_SOCKET_DESC-1] = '\0';

	} else {
		sr->desc[0] = '\0';
	}
}

void SocketRangeToStr(struct socket_range_s *sr,char *s)
{
	//range
	if( sr->max > sr->min ) {
		sprintf(s,"%04x-%04x",sr->min,sr->max);
	} else { //no range
		sprintf(s,"%04x",sr->min);
	}

	//append description if it exists
	if( sr->desc[0] ) {
		strcat(s," : ");
		strcat(s,(char *)sr->desc);
	}
}

void StrToPortRange(char *s,struct port_range_s *pr)
{
	unsigned short v;

	//grab first dec value
	pr->min = (unsigned short)strtol(s,&s,10);

	//eat whitespace
	while(*s==' ') s++;

	//if there is a dash, its a range
	if(*s=='-') {

		//eat whitespace and dash
		while(*++s==' ');

		//grab 2nd dec value
		v = (unsigned short)strtol(s,&s,10);
		if(v>=pr->min) {
			pr->max = v;
		} else { //stupid user, backwards order
			pr->max = pr->min;
			pr->min = v;
		}

		//eat whitespace
		while(*s==' ') s++;

	} else { //no dash, no range
		pr->max = pr->min;
	}

	//if there is a colon, there is a description
	if(*s==':') {

		//eat whitespace and colon
		while(*++s==' ');

		//grab description
		strncpy((char *)pr->desc,s,MAX_PORT_DESC-1);
		pr->desc[MAX_PORT_DESC-1] = '\0';

	} else {
		pr->desc[0] = '\0';
	}
}

void PortRangeToStr(struct port_range_s *pr,char *s)
{
	//range
	if( pr->max > pr->min ) {
		sprintf(s,"%d-%d",pr->min,pr->max);
	} else { //no range
		sprintf(s,"%d",pr->min);
	}

	//append description if it exists
	if( pr->desc[0] ) {
		strcat(s," : ");
		strcat(s,(char *)pr->desc);
	}
}

int connect_socks(int s,int ver,struct sockaddr_in *sin)
{
	struct socks4_header s4h;
	struct socks5_method s5m;
	struct socks5_method_reply s5m_r;
	struct socks5_userpw s5u;
	struct socks5_userpw_reply s5u_r;
	struct socks5_header s5h;
	struct socks5_header_reply s5h_r;
	fd_set fds;
	struct timeval tv;

	if(ver==S4_VER) {
		s4h.ver = S4_VER;
		s4h.cmd = S4_CMD_CONNECT;
		s4h.dstport = sin->sin_port;
		s4h.dstip = sin->sin_addr.s_addr;
		send(s,(char *)&s4h,sizeof(s4h),0);
		send(s,(char *)socksinfo.username,strlen((char *)socksinfo.username)+1,0);
		FD_ZERO(&fds);
		FD_SET(s,&fds);
		tv.tv_sec = 120;
		tv.tv_usec = 0;
		if( select(s+1,&fds,NULL,NULL,&tv) == 1 &&
			recv(s,(char *)&s4h,sizeof(s4h),0) == sizeof(s4h) &&
			s4h.ver == S4_VER_REPLY ) {

			switch(s4h.cmd) {
			case S4_CMD_REQ_GRANTED: return FH_FAIL_NOT;
			default:
			case S4_CMD_REQ_FAIL: return FH_FAIL_SOCKS_CONNECT;
			case S4_CMD_REQ_FAIL_NO_IDENT:
			case S4_CMD_REQ_FAIL_DIFF_IDENT: return FH_FAIL_SOCKS_IDENT;
			}
		} else {
			return FH_FAIL_SOCKS_CONNECT;
		}
	} else { //S5_VER
		s5m.ver = S5_VER;
		s5m.nmethods = 2;
		s5m.method[0] = S5_METHOD_NO_AUTH;
		s5m.method[1] = S5_METHOD_USERPW;
		send(s,(char *)&s5m,sizeof(s5m),0);
		FD_ZERO(&fds);
		FD_SET(s,&fds);
		tv.tv_sec = 120;
		tv.tv_usec = 0;
		if( select(s+1,&fds,NULL,NULL,&tv) == 1 &&
			recv(s,(char *)&s5m_r,sizeof(s5m_r),0) == sizeof(s5m_r) &&
			s5m_r.ver == S5_VER ) {

			switch(s5m_r.method) {
			case S5_METHOD_NO_AUTH: break;
			case S5_METHOD_USERPW:
				s5u.ver = S5_USERPW_VER;
				s5u.ulen = strlen((char *)socksinfo.username)+1;
				send(s,(char *)&s5u,sizeof(s5u),0);
				send(s,(char *)socksinfo.username,strlen((char *)socksinfo.username)+1,0);
				s5u.ulen = strlen((char *)socksinfo.password)+1;
				send(s,(char *)&s5u.ulen,1,0);
				send(s,(char *)socksinfo.password,strlen((char *)socksinfo.password)+1,0);
				FD_ZERO(&fds);
				FD_SET(s,&fds);
				tv.tv_sec = 120;
				tv.tv_usec = 0;
				if( select(s+1,&fds,NULL,NULL,&tv) == 1 &&
					recv(s,(char *)&s5u_r,sizeof(s5u_r),0) == sizeof(s5u_r) &&
					s5u_r.ver == S5_USERPW_VER ) {

					switch(s5u_r.status) {
					case S5_USERPW_ACCEPTED: break;
					default:
					case S5_USERPW_REJECTED:
						return FH_FAIL_SOCKS_IDENT;
					}
				} else {
					return FH_FAIL_SOCKS_CONNECT;
				}
				break;
			default:
			case S5_METHOD_NONE_ACCEPTABLE: return FH_FAIL_SOCKS_IDENT;
			}
		} else {
			return FH_FAIL_SOCKS_CONNECT;
		}
		s5h.ver = S5_VER;
		s5h.cmd = S5_CMD_CONNECT;
		s5h.rsv = 0;
		s5h.atyp = S5_ATYP_IPV4;
		s5h.dstaddr = sin->sin_addr.s_addr;
		s5h.dstport = sin->sin_port;
		send(s,(char *)&s5h,sizeof(s5h),0);
		FD_ZERO(&fds);
		FD_SET(s,&fds);
		tv.tv_sec = 120;
		tv.tv_usec = 0;
		if( select(s+1,&fds,NULL,NULL,&tv) == 1 &&
			recv(s,(char *)&s5h_r,sizeof(s5h_r),0) == sizeof(s5h_r) &&
			s5h_r.ver == S5_VER ) {

			switch(s5h_r.rep) {
			case S5_CMD_REQ_GRANTED: return FH_FAIL_NOT;
			default:
			case S5_CMD_REQ_FAIL:
			case S5_CMD_REQ_FAIL_RULESET:
			case S5_CMD_REQ_FAIL_NET_UNRCH:
			case S5_CMD_REQ_FAIL_HOST_UNRCH:
			case S5_CMD_REQ_FAIL_CONN_REF:
			case S5_CMD_REQ_FAIL_TTL_EXP:
			case S5_CMD_REQ_FAIL_CMD_NOT_SUP:
			case S5_CMD_REQ_FAIL_ADDR_TYPE: return FH_FAIL_SOCKS_CONNECT;
			}
		} else {
			return FH_FAIL_SOCKS_CONNECT;
		}
	}
}

/* Analyzes network frame at given base of length len.
 * If its an ipx frame, will set ipx to the start of the ipx frame
 * header. If its an ipv4 frame, will set ipv4->header to the start of
 * the ipv4 header, as well as ipv4->ports to the start of the tcp/udp
 * header.  If either ipx or ipv4->header are set, it will also set frame
 * to the frame type it was inside of, otherwise both will be NULL, frame
 * will be 0, and msg will be NULL.
 * If its an ipx of ipv4 frame, it will be checked against the current
 * forwarding rules, and msg will be set to NULL if we forward the frame
 * type, packet/protocol type, and destination-socket/port number, otherwise,
 * msg will point to a message stating the first found reason we dont
 * forward it.
 */
void analyze_frame(unsigned char *base,unsigned len,unsigned *frame,struct ipx_s **ipx,struct ipv4_s *ipv4,struct arp_s **arp,char **msg)
{
	unsigned s;
	unsigned short dstsocket,dstport;

	*frame = 0;
	*ipx = NULL;
	ipv4->header = NULL;
	ipv4->ports = NULL;
	*arp = NULL;
	*msg = NULL;

	//must be at least 14bytes EthII or 802.3 header and 20bytes ipv4 header
	if( len < 34 ) {
		return;
	}

	if( base[12]==0x81 && base[13]==0x37 ) { //if its ethII type ipx
		if( len < 44 ) { //we need at least 30bytes for the ipx headers
			return;
		}
		*ipx = (struct ipx_s *)(base+14);
		*frame = OPT_FRAME_ETH2;
	} else if( base[12]==0x08 && base[13]==0x00 ) { //if its ethII type ipv4
		ipv4->header = (struct ipv4_header_s *)(base+14);
		*frame = OPT_FRAME_ETH2;
	} else if( base[12]==0x08 && base[13]==0x06 ) { //if its ethII type arp
		*arp = (struct arp_s *)(base+14);
		*frame = OPT_FRAME_ETH2;
	} else if( base[12]<5 || (base[12]==5 && base[13]<=0xDC) ) { //if its 802.3
		if( base[14]==0xFF && base[15]==0xFF ) { //if its novell raw
			if( len < 44 ) { //we need at least 30bytes for the ipx headers
				return;
			}
			*ipx = (struct ipx_s *)(base+14);
			*frame = OPT_FRAME_8023;
		} else if( (base[14]&0xFE)==0xE0 && (base[15]&0xFE)==0xE0 ) { //if its 802.2 type ipx
			s = ((base[16]&0x03)==0x03)?17:18;
			if( len < s+30 ) { //make sure at least 47(or 48) bytes total
				return;
			}
			*ipx = (struct ipx_s *)(base+s);
			*frame = OPT_FRAME_8022;
		} else if( (base[14]&0xFE)==0x06 && (base[15]&0xFE)==0x06 ) { //if its 802.2 type ipv4
			s = ((base[16]&0x03)==0x03)?17:18;
			if( len < s+20 ) { //make sure at least 37(or 38) bytes total
				return;
			}
			ipv4->header = (struct ipv4_header_s *)(base+s);
			*frame = OPT_FRAME_8022;
		} else if( (base[14]&0xFE)==0xAA && (base[15]&0xFE)==0xAA && base[16]==0x03 ) { //if its 802.2 SNAP
			if( !base[17] && !base[18] && !base[19] && base[20]==0x81 && base[21]==0x37 ) { //if its snap type ipx
				if( len < 52 ) { //we need 22bytes for the entire snap headers
					return;
				}
				*ipx = (struct ipx_s *)(base+22);
				*frame = OPT_FRAME_SNAP;
			} else  if( !base[17] && !base[18] && !base[19] && base[20]==0x08 && base[21]==0x00 ) { //if its snap type ipv4
				if( len < 42 ) { //we need 22bytes for the entire snap headers
					return;
				}
				ipv4->header = (struct ipv4_header_s *)(base+22);
				*frame = OPT_FRAME_SNAP;
			}
		}
	}

	if( ipv4->header ) {
		ipv4->ports = (struct ipv4_ports_s *)((char *)ipv4->header +
			((ipv4->header->ver_hdrlen & 0x0F) * 4));
	}

	if( *ipx || ipv4->header || *arp ) {
		if( options & *frame ) { //if we fwd this frame type
			if( *ipx ) {
				if( packets==0xFFFFFFFF || ((*ipx)->type<0x1F && (packets&(1<<(*ipx)->type))) ) {
					dstsocket = ntohs( (*ipx)->dstsocket );
					for( s=0; s<numsockets; s++ ) {
						if( dstsocket>=sockets[s].min && dstsocket<=sockets[s].max ) {
							break;
						}
					}
					if( s>=numsockets ) { //if we fwd this destination socket
						*msg = "wrong socket number";
					}
				} else {
					*msg = "wrong packet type";
				}
			} else if( ipv4->header ) {
				if( protos==0xFFFFFFFF || (ipv4->header->proto<0x1F && (protos&(1<<ipv4->header->proto))) ) {
//					if( !(options & OPT_IPV4_OIFB) || ipv4->header->dstaddr==0xFFFFFFFF ) {
						//only check ports for tcp/udp
						if( ipv4->header->proto==PROTO_TCP || ipv4->header->proto==PROTO_UDP ) {
							dstport = ntohs( ipv4->ports->dstport );
							for( s=0; s<numports; s++ ) {
								if( dstport>=ports[s].min && dstport<=ports[s].max ) {
									break;
								}
							}
							if( s>=numports && (options & OPT_IPV4_SRCP) ) { //port not found, maybe try source port
								dstport = ntohs( ipv4->ports->srcport );
								for( s=0; s<numports; s++ ) {
									if( dstport>=ports[s].min && dstport<=ports[s].max ) {
										break;
									}
								}
							}
							if( s>=numports ) { //if we don't fwd this port
								*msg = "wrong port number";
							}
						}
//					} else {
//						*msg = "destination not broadcast";
//					}
				} else {
					*msg = "wrong protocol type";
				}
			} else {
				if( !(options & OPT_OTHER_ARP) ) {
					*msg = "not forwarding arp";
				}
			}
		} else {
			*msg = "wrong frame type";
		}
	}
}

void calc_ipv4_sum(struct ipv4_header_s *header)
{
	int i;
	unsigned int sum = 0;
	unsigned short len = (header->ver_hdrlen & 0x0F) << 2;
	unsigned char *buf = (unsigned char *)header;

	header->checksum = 0;

	//add in 16 bit values from every two bytes
	for(i=0; i<len; i+=2) {
		sum += (buf[i])<<8;
		sum += buf[i+1];
	}

	//keep only the last 16 bits of the 32 bit calculated sum
	while(sum>>16) {
		sum = (sum&0xFFFF) + (sum>>16);
	}

	header->checksum = htons((unsigned short)~sum);
}

void calc_icmp_sum(struct ipv4_header_s *header)
{
	int i;
	unsigned int sum = 0;
	unsigned short len = (header->ver_hdrlen & 0x0F) << 2;
	unsigned char *buf;

	//adjust buf to start of icmp frame, len to length of icmp frame
	buf = (unsigned char *)header + len;
	len = ntohs(header->len) - len;

	//clear out the ICMP checksum field
	((unsigned short *)buf)[1] = 0;

	//this makes it not add the last byte if len is odd
	len--;
	//add in 16 bit values from every two bytes
	for(i=0; i<len; i+=2) {
		sum += (buf[i])<<8;
		sum += buf[i+1];
	}
	//add in the last odd bye here
	if(i==len) {
		sum += (buf[i])<<8;
	}

	//keep only the last 16 bits of the 32 bit calculated sum
	while(sum>>16) {
		sum = (sum&0xFFFF) + (sum>>16);
	}

	((unsigned short *)buf)[1] = htons((unsigned short)~sum);
}

void calc_tcp_sum(struct ipv4_header_s *header)
{
	int i;
	unsigned int sum = 0;
	unsigned short len = (header->ver_hdrlen & 0x0F) << 2;
	unsigned char *buf;

	//add the TCP pseudo header which contains the src and dst ips
	buf = (unsigned char *)&header->srcaddr;
	sum += ((buf[0]<<8)&0xFF00);
	sum += (buf[1]&0xFF);
	sum += ((buf[2]<<8)&0xFF00);
	sum += (buf[3]&0xFF);
	buf = (unsigned char *)&header->dstaddr;
	sum += ((buf[0]<<8)&0xFF00);
	sum += (buf[1]&0xFF);
	sum += ((buf[2]<<8)&0xFF00);
	sum += (buf[3]&0xFF);

	//adjust buf to start of tcp frame, len to length of tcp frame
	buf = (unsigned char *)header + len;
	len = ntohs(header->len) - len;

	//more TCP pseudo header: proto and length
	sum += PROTO_TCP + len;

	//clear out the TCP checksum field
	((unsigned short *)buf)[8] = 0;

	//this makes it not add the last byte if len is odd
	len--;
	//add in 16 bit values from every two bytes
	for(i=0; i<len; i+=2) {
		sum += (buf[i])<<8;
		sum += buf[i+1];
	}
	//add in the last odd bye here
	if(i==len) {
		sum += (buf[i])<<8;
	}

	//keep only the last 16 bits of the 32 bit calculated sum
	while(sum>>16) {
		sum = (sum&0xFFFF) + (sum>>16);
	}

	((unsigned short *)buf)[8] = htons((unsigned short)~sum);
}

void calc_udp_sum(struct ipv4_header_s *header)
{
	int i;
	unsigned int sum = 0;
	unsigned short len = (header->ver_hdrlen & 0x0F) << 2;
	unsigned char *buf;

	//add the UDP pseudo header which contains the src and dst ips
	buf = (unsigned char *)&header->srcaddr;
	sum += ((buf[0]<<8)&0xFF00);
	sum += (buf[1]&0xFF);
	sum += ((buf[2]<<8)&0xFF00);
	sum += (buf[3]&0xFF);
	buf = (unsigned char *)&header->dstaddr;
	sum += ((buf[0]<<8)&0xFF00);
	sum += (buf[1]&0xFF);
	sum += ((buf[2]<<8)&0xFF00);
	sum += (buf[3]&0xFF);

	//adjust buf to start of udp frame, len to length of udp frame
	buf = (unsigned char *)header + len;
	len = ntohs(header->len) - len;

	//more UDP pseudo header: proto and length
	sum += PROTO_UDP + len;

	//clear out the UDP checksum field
	((unsigned short *)buf)[3] = 0;

	//this makes it not add the last byte if len is odd
	len--;
	//add in 16 bit values from every two bytes
	for(i=0; i<len; i+=2) {
		sum += (buf[i])<<8;
		sum += buf[i+1];
	}
	//add in the last odd bye here
	if(i==len) {
		sum += (buf[i])<<8;
	}

	//keep only the last 16 bits of the 32 bit calculated sum
	while(sum>>16) {
		sum = (sum&0xFFFF) + (sum>>16);
	}

	((unsigned short *)buf)[3] = htons((unsigned short)~sum);
}

int add_hwsrcaddr(unsigned char *hwsrcaddr, int addrlen)
{
	int n = 0, len;
	unsigned char addr[MAX_HW_ADDR_LEN];
	time_t now;
	time(&now);
	memset(addr, 0, MAX_HW_ADDR_LEN);
	len = addrlen > MAX_HW_ADDR_LEN ? MAX_HW_ADDR_LEN : addrlen;
	memcpy(addr, hwsrcaddr, len);
	EnterCriticalSection(&hwsrclist_mutex);
	while( n < MAX_SRC_HW_INFO ) {
		if( hw_src_addr_info[n].expire_time > now ) {
			if( !memcmp(addr, hw_src_addr_info[n].addr, MAX_HW_ADDR_LEN) ) {
				hw_src_addr_info[n].expire_time = now + HW_ADDR_EXPIRE;
				LeaveCriticalSection(&hwsrclist_mutex);
				return 2;
			}
		}
		n++;
	}
	n = 0;
	while( n < MAX_SRC_HW_INFO ) {
		if( hw_src_addr_info[n].expire_time <= now ) {
			memset(hw_src_addr_info[n].addr, 0, MAX_HW_ADDR_LEN);
			len = addrlen > MAX_HW_ADDR_LEN ? MAX_HW_ADDR_LEN : addrlen;
			memcpy(hw_src_addr_info[n].addr, hwsrcaddr, len);
			hw_src_addr_info[n].expire_time = now + HW_ADDR_EXPIRE;
			LeaveCriticalSection(&hwsrclist_mutex);
			return 1;
		}
		n++;
	}
	LeaveCriticalSection(&hwsrclist_mutex);
	return 0;
}

int check_hwsrcaddr_is_in_list(unsigned char *hwsrcaddr, int addrlen)
{
	int n = 0, len;
	unsigned char addr[MAX_HW_ADDR_LEN];
	time_t now;
	time(&now);
	memset(addr, 0, MAX_HW_ADDR_LEN);
	len = addrlen > MAX_HW_ADDR_LEN ? MAX_HW_ADDR_LEN : addrlen;
	memcpy(addr, hwsrcaddr, len);
	EnterCriticalSection(&hwsrclist_mutex);
	while( n < MAX_SRC_HW_INFO ) {
		if( hw_src_addr_info[n].expire_time > now ) {
			if( !memcmp(addr, hw_src_addr_info[n].addr, MAX_HW_ADDR_LEN) ) {
				LeaveCriticalSection(&hwsrclist_mutex);
				return 1;
			}
		}
		n++;
	}
	LeaveCriticalSection(&hwsrclist_mutex);
	return 0;
}

int addr_is_routable(unsigned long addr)
{
	addr = ntohl(addr);
	if( (addr & 0xFFFF0000) == 0xC0A80000 ) { //192.168.x.x
		return 0;
	}
	if( (addr & 0xFFF00000) == 0xAC100000 ) { //172.16-31.x.x
		return 0;
	}
	if( (addr & 0xFF000000) == 0x0A000000 ) { //10.x.x.x
		return 0;
	}
	if( (addr & 0xFFFF0000) == 0xA9FE0000 ) { //169.254.x.x
		return 0;
	}
	return 1;
}

void add_to_log(char *host,unsigned frame,unsigned char *mac,struct ipx_s *ipx,struct ipv4_s ipv4,struct arp_s *arp,char *msg,char *logfile)
{
	char logline[LOG_BUF],*p=logline;
	FILE *f;
	time_t t;

	sprintf(p,"[%s",ctime( (time(&t),&t) ));
	p += strlen(p)-1;

	sprintf(p,"] %s: ",host);
	p += strlen(p);

	sprintf(p,"hw:%02x:%02x:%02x:%02x:%02x:%02x ",mac[0],mac[1],mac[2],mac[3],mac[4],mac[5]);
	p += strlen(p);

	if( ipx || ipv4.header || arp ) {
		if( frame & OPT_FRAME_8022 ) {
			strcpy(p,"IEEE802.2 ");
		} else if( frame & OPT_FRAME_8023 ) {
			strcpy(p,"IEEE802.3raw ");
		} else if( frame & OPT_FRAME_ETH2 ) {
			strcpy(p,"EthernetII ");
		} else if( frame & OPT_FRAME_SNAP ) {
			strcpy(p,"EthernetSNAP ");
		} else {
			strcpy(p,"unknown ");
		}
		p += strlen(p);

		if( ipx ) {
			strcpy(p,"IPX ");
			p += strlen(p);
			switch(ipx->type) {
			case PACKET_OLD: strcpy(p,"NLSP/OldIPX"); break;
			case PACKET_RIP: strcpy(p,"RIP"); break;
			case PACKET_ECO: strcpy(p,"Echo"); break;
			case PACKET_ERR: strcpy(p,"Error"); break;
			case PACKET_IPX: strcpy(p,"IPX"); break;
			case PACKET_SPX: strcpy(p,"SPX"); break;
			case PACKET_NCP: strcpy(p,"NCP"); break;
			case PACKET_NTB: strcpy(p,"NetBIOS"); break;
			default: sprintf(p,"unknown(%02x)",ipx->type); break;
			}
			p += strlen(p);
			sprintf(p," to:%02x%02x%02x%02x.%02x%02x%02x%02x%02x%02x:%04x"
				" from:%02x%02x%02x%02x.%02x%02x%02x%02x%02x%02x:%04x '%s'\n",
				ipx->dstnet[0],ipx->dstnet[1],ipx->dstnet[2],ipx->dstnet[3],
				ipx->dstnode[0],ipx->dstnode[1],ipx->dstnode[2],ipx->dstnode[3],ipx->dstnode[4],ipx->dstnode[5],
				ntohs(ipx->dstsocket),
				ipx->srcnet[0],ipx->srcnet[1],ipx->srcnet[2],ipx->srcnet[3],
				ipx->srcnode[0],ipx->srcnode[1],ipx->srcnode[2],ipx->srcnode[3],ipx->srcnode[4],ipx->srcnode[5],
				ntohs(ipx->srcsocket),msg);
			p += strlen(p);
		} else if( ipv4.header ) {
			strcpy(p,"IPv4 ");
			p += strlen(p);
			switch(ipv4.header->proto) {
			case PROTO_ICMP: strcpy(p,"ICMP"); break;
			case PROTO_TCP: strcpy(p,"TCP"); break;
			case PROTO_UDP: strcpy(p,"UDP"); break;
			default: sprintf(p,"unknown(%02x)",ipv4.header->proto); break;
			}
			p += strlen(p);
			sprintf(p," to:%s:%d",
				inet_ntoa(*(struct in_addr *)&ipv4.header->dstaddr),ntohs(ipv4.ports->dstport));
			p += strlen(p);
			sprintf(p," from:%s:%d '%s'\n",
				inet_ntoa(*(struct in_addr *)&ipv4.header->srcaddr),ntohs(ipv4.ports->srcport),msg);
			p += strlen(p);
		} else {
			strcpy(p,"ARP ");
			p += strlen(p);
			if( arp->hwaddrlen == 6 && arp->protaddrlen == 4 && (ntohs(arp->opcode) == 1 || ntohs(arp->opcode) == 2) ) {
				if( ntohs(arp->opcode) == 1 ) {
					sprintf(p,"request to:%02x:%02x:%02x:%02x:%02x:%02x from:%02x:%02x:%02x:%02x:%02x:%02x(%s)",
						arp->dsthwaddr[0], arp->dsthwaddr[1], arp->dsthwaddr[2], arp->dsthwaddr[3], arp->dsthwaddr[4], arp->dsthwaddr[5],
						arp->srchwaddr[0], arp->srchwaddr[1], arp->srchwaddr[2], arp->srchwaddr[3], arp->srchwaddr[4], arp->srchwaddr[5],
						inet_ntoa(*(struct in_addr *)arp->srcprotaddr));
					p += strlen(p);
					sprintf(p," for:%s",inet_ntoa(*(struct in_addr *)arp->dstprotaddr));
					p += strlen(p);
				} else {
					sprintf(p,"reply to:%02x:%02x:%02x:%02x:%02x:%02x(%s) from:%02x:%02x:%02x:%02x:%02x:%02x",
						arp->dsthwaddr[0], arp->dsthwaddr[1], arp->dsthwaddr[2], arp->dsthwaddr[3], arp->dsthwaddr[4], arp->dsthwaddr[5],
						inet_ntoa(*(struct in_addr *)arp->dstprotaddr),
						arp->srchwaddr[0], arp->srchwaddr[1], arp->srchwaddr[2], arp->srchwaddr[3], arp->srchwaddr[4], arp->srchwaddr[5]);
					p += strlen(p);
					sprintf(p," is:%s",inet_ntoa(*(struct in_addr *)arp->srcprotaddr));
					p += strlen(p);
				}
			} else {
				sprintf(p,"Unknown hwtype:%04x prottype:%04x hwlen:%d protlen:%d opcode:%d", ntohs(arp->hwtype),
					ntohs(arp->prottype), arp->hwaddrlen, arp->protaddrlen, ntohs(arp->opcode));
				p += strlen(p);
			}
			sprintf(p," '%s'\n",msg);
			p += strlen(p);
		}
	} else {
		strcpy(p,"invalid\n");
		p += strlen(p);
	}

	//this probably isnt the best way for file access..
	char path[MAX_PATH];
	strcpy(path, g_path);
	strcat(path, logfile);
	if( (f=fopen(path,"a")) ) {
		fputs(logline,f);
		fclose(f);
	}
}

void add_to_log_addr(char *host,struct in_addr want,struct in_addr got,char *msg)
{
	char logline[LOG_BUF],*p=logline;
	FILE *f;
	time_t t;

	sprintf(p,"[%s",ctime( (time(&t),&t) ));
	p += strlen(p)-1;

	sprintf(p,"] %s: wanted:%s ",host,inet_ntoa(want));
	p += strlen(p);

	sprintf(p,"got:%s '%s'\n",inet_ntoa(got),msg);
	p += strlen(p);

	//this probably isnt the best way for file access..
	char path[MAX_PATH];
	strcpy(path, g_path);
	strcat(path, LOGFILE_ADDR);
	if( (f=fopen(path,"a")) ) {
		fputs(logline,f);
		fclose(f);
	}
}

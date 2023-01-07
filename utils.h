/*--------------------------------------------------------------------------*\
|| utils.h by Ryan Rubley <ryan@morpheussoftware.net>                       ||
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

#ifndef _GIT_UTILS_H_
#define _GIT_UTILS_H_

#include "git.h"

#define LOG_BUF	(MAX_HOSTLEN+256)
#define LOGFILE_IN "incoming.log"
#define LOGFILE_FWD "forwarded.log"
#define LOGFILE_UNFWD "unforwarded.log"
#define LOGFILE_ADDR "invalidconnections.log"

struct ipx_s {
	unsigned short checksum,length;
	unsigned char tc,type;
	unsigned char dstnet[4],dstnode[6];
	unsigned short dstsocket;
	unsigned char srcnet[4],srcnode[6];
	unsigned short srcsocket;
};

struct ipv4_header_s {
	unsigned char ver_hdrlen,tos;
	unsigned short len,fragident,fragflags_fragoff;
	unsigned char ttl,proto;
	unsigned short checksum;
	unsigned long srcaddr,dstaddr;
};
struct ipv4_ports_s {
	unsigned short srcport,dstport;
};
struct ipv4_s {
	struct ipv4_header_s *header;
	struct ipv4_ports_s *ports;
};

struct arp_s {
	unsigned short hwtype,prottype;
	unsigned char hwaddrlen,protaddrlen;
	unsigned short opcode;
	unsigned char srchwaddr[6];
	unsigned char srcprotaddr[4];
	unsigned char dsthwaddr[6];
	unsigned char dstprotaddr[4];
};

#ifdef WIN32
#pragma pack(push, 1)
#else
#pragma pack(1)
#endif

struct socks4_header {
	unsigned char ver,cmd;
	unsigned short dstport;
	unsigned long dstip;
	//username, null termianted
};
#define S4_VER						4
#define S4_VER_REPLY				0
#define S4_CMD_CONNECT				1
#define S4_CMD_BIND					2
#define S4_CMD_REQ_GRANTED			90
#define S4_CMD_REQ_FAIL				91
#define S4_CMD_REQ_FAIL_NO_IDENT	92
#define S4_CMD_REQ_FAIL_DIFF_IDENT	93

struct socks5_method {
	unsigned char ver,nmethods,method[2];
};
struct socks5_method_reply {
	unsigned char ver,method;
};
#define S5_VER						5
#define S5_METHOD_NO_AUTH			0
#define S5_METHOD_GSSAPI			1
#define S5_METHOD_USERPW			2
#define S5_METHOD_NONE_ACCEPTABLE	0xFF

struct socks5_userpw {
	unsigned char ver,ulen;//,uname[?],plen,passwd[?];
};
struct socks5_userpw_reply {
	unsigned char ver,status;
};
#define S5_USERPW_VER				1
#define S5_USERPW_ACCEPTED			0
#define S5_USERPW_REJECTED			0xFF

struct socks5_header {
	unsigned char ver,cmd,rsv,atyp;
	unsigned long dstaddr;
	unsigned short dstport;
};
struct socks5_header_reply {
	unsigned char ver,rep,rsv,atyp;
	unsigned long bndaddr;
	unsigned short bndport;
};
#define S5_CMD_CONNECT				1
#define S5_CMD_BIND					2
#define S5_CMD_UDP_ASSOCIATE		3
#define S5_ATYP_IPV4				1
#define S5_ATYP_DOMAIN				3
#define S5_ATYP_IPV6				4
#define S5_CMD_REQ_GRANTED			0
#define S5_CMD_REQ_FAIL				1
#define S5_CMD_REQ_FAIL_RULESET		2
#define S5_CMD_REQ_FAIL_NET_UNRCH	3
#define S5_CMD_REQ_FAIL_HOST_UNRCH	4
#define S5_CMD_REQ_FAIL_CONN_REF	5
#define S5_CMD_REQ_FAIL_TTL_EXP		6
#define S5_CMD_REQ_FAIL_CMD_NOT_SUP	7
#define S5_CMD_REQ_FAIL_ADDR_TYPE	8

struct hw_addr_info_s {
	unsigned char addr[MAX_HW_ADDR_LEN];
	time_t expire_time;
};

#ifdef WIN32
#pragma pack(pop)
#else
#pragma pack()
#endif

extern char *def_socket_list[];
extern char *def_port_list[];
extern char *method_list[];

//hw src addr info to prevent reforwarding of packets, keep a list of all known-remote addresses
//PT extern pthread_mutex_t hwsrclist_mutex;
extern CRITICAL_SECTION hwsrclist_mutex;
extern struct hw_addr_info_s hw_src_addr_info[MAX_SRC_HW_INFO];

//bufs of size MAX_HOSTLEN+64 should suffice for these *ToStr functions
void StrToFwdHost(char *s,struct fwd_host_s *fh);
void FwdHostToStr(struct fwd_host_s *fh,char *s);
void StrToSocketRange(char *s,struct socket_range_s *sr);
void SocketRangeToStr(struct socket_range_s *sr,char *s);
void StrToPortRange(char *s,struct port_range_s *pr);
void PortRangeToStr(struct port_range_s *pr,char *s);
int connect_socks(int s,int ver,struct sockaddr_in *sin);
void analyze_frame(unsigned char *base,unsigned len,unsigned *frame,struct ipx_s **ipx,struct ipv4_s *ipv4,struct arp_s **arp,char **msg);
void calc_ipv4_sum(struct ipv4_header_s *header);
void calc_icmp_sum(struct ipv4_header_s *header);
void calc_tcp_sum(struct ipv4_header_s *header);
void calc_udp_sum(struct ipv4_header_s *header);
int add_hwsrcaddr(unsigned char *hwsrcaddr, int addrlen);
int check_hwsrcaddr_is_in_list(unsigned char *hwsrcaddr, int addrlen);
int addr_is_routable(unsigned long addr);
void add_to_log(char *host,unsigned frame,unsigned char *mac,struct ipx_s *ipx,struct ipv4_s ipv4,struct arp_s *arp,char *msg,char *logfile);
void add_to_log_addr(char *host,struct in_addr want,struct in_addr got,char *msg);

#endif

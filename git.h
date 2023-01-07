/*--------------------------------------------------------------------------*\
|| git.h by Ryan Rubley <ryan@morpheussoftware.net>                         ||
||                                                                          ||
|| (v0.91) 8-25-2001                                                        ||
|| (v0.95) 8-04-2002                                                        ||
|| (v0.96) 8-17-2002                                                        ||
|| (v0.97) 3-09-2004                                                        ||
|| (v0.98) 9-12-2004                                                        ||
|| (v0.99) 9-12-2004                                                        ||
|| Gamer's Internet Tunnel                                                  ||
||                                                                          ||
\*--------------------------------------------------------------------------*/

#ifndef _GIT_H_
#define _GIT_H_

#include <winsock2.h>
#include <windows.h>
#include <time.h>
//PT #include <pthread.h>
#pragma warning (disable:4018) //'==' : signed/unsigned mismatch

#define GIT_NAME	"Gamer's Internet Tunnel"
#define GIT_URL		"http://www.morpheussoftware.net/git/"
#define GIT_FORUM	"http://forums.morpheussoftware.net/"
#define GIT_CLASS	"gamersinternettunnel"

//options
#define OPT_FRAME_8022	0x00000001
#define OPT_FRAME_8023	0x00000002
#define OPT_FRAME_ETH2	0x00000004
#define OPT_FRAME_SNAP	0x00000008
#define OPT_IPV4_NOUC	0x00000010
#define OPT_IPV4_NAT	0x00000020
#define OPT_IPV4_NOBC	0x00000040
#define OPT_IPV4_SRCP	0x00000080
#define OPT_IPV4_NORT	0x00100000
#define OPT_LOG_IN		0x00000100
#define OPT_LOG_FWD		0x00000200
#define OPT_LOG_UNFWD	0x00000400
#define OPT_LOG_ADDR	0x00000800
#define OPT_TCP_MULTI	0x00001000
#define OPT_OTHER_ARP	0x00010000
#define OPT_OTHER_ORFP	0x00020000
#define OPT_COMP_ZLIB	0x01000000

//packet types, these should only range from 00-1F
#define PACKET_OLD	0x00
#define PACKET_RIP	0x01
#define PACKET_ECO	0x02
#define PACKET_ERR	0x03
#define PACKET_IPX	0x04
#define PACKET_SPX	0x05
#define PACKET_NCP	0x11
#define PACKET_NTB	0x14

//protocol types
#define PROTO_ICMP	0x01
#define PROTO_TCP	0x06
#define PROTO_UDP	0x11

//max number of hosts in fwd lists
#define MAX_HOSTS	16

//max number ipx socket and ports ranges in fwd lists
#define MAX_SOCKETS	32
#define MAX_PORTS 32

//max hostname length
#define MAX_HOSTLEN	256

//max socket/port description length
#define MAX_SOCKET_DESC 32
#define MAX_PORT_DESC 32

//max number of src addresses to keep track of
#define MAX_SRC_HW_INFO 256
//max length of a hardware address to look at
#define MAX_HW_ADDR_LEN 6
//number of seconds to remember where the address came from
#define HW_ADDR_EXPIRE	30

//biggest allowed size of a packet, compressed or uncompressed
#define MAX_PACKET_SIZE	16385

//fwd methods
#define METH_UDP					1
#define METH_TCP_LISTEN				2
#define METH_TCP_CONNECT			3
#define METH_TCP_CONNECT_SOCKS4		4
#define METH_TCP_CONNECT_SOCKS5		5
#define METH_TCP_MULTI_LISTEN		6

//fwd host statse
#define FH_STATE_DOWN				0
#define FH_STATE_CONNECTING			1
#define FH_STATE_FAILED				2
#define FH_STATE_UP					3

//how it failed if FH_STATE_FAILED
#define FH_FAIL_NOT					0
#define FH_FAIL_SOCKET				1
#define FH_FAIL_BIND				2
#define FH_FAIL_ACCEPT				3
#define FH_FAIL_CONNECT				4
#define FH_FAIL_CONNECT_SOCKS		5
#define FH_FAIL_SOCKS_CONNECT		6
#define FH_FAIL_SOCKS_IDENT			7
#define FH_FAIL_WRONGADDR			8
#define FH_FAIL_RECV				9
#define FH_FAIL_SEND				10

struct fwd_host_s {
	unsigned char name[MAX_HOSTLEN];
	unsigned short port,method;
	struct in_addr addr,from;
	int s;
	unsigned short state;
	unsigned char error,shutdown;
	time_t connected,disconnected,lastfwd,lastrecv;
	unsigned char buf[MAX_PACKET_SIZE], *pbuf;
};

struct fwd_host_tcpmulti_s {
	unsigned short port;
	unsigned maxhosts;
	int s;
};

struct socket_range_s {
	unsigned short min,max;
	unsigned char desc[MAX_SOCKET_DESC];
};

struct port_range_s {
	unsigned short min,max;
	unsigned char desc[MAX_PORT_DESC];
};

struct socks_info_s {
	unsigned char hostname[MAX_HOSTLEN],username[MAX_HOSTLEN],password[MAX_HOSTLEN];
	unsigned short port;
};

struct nat_info_s {
	unsigned char internal[MAX_HOSTLEN],external[MAX_HOSTLEN];
	struct in_addr intaddr,extaddr;
};

//for dialogs in other .cpp files
extern HINSTANCE hInst;
extern char g_path[MAX_PATH];
int CALLBACK DialogProcStatus(HWND hwndDlg,UINT uMsg,WPARAM wParam,LPARAM lParam);

//so threads can init 1 at a time..
//PT extern pthread_mutex_t initing;
extern CRITICAL_SECTION initing;

//config options
extern struct fwd_host_s hosts[MAX_HOSTS];
extern struct fwd_host_tcpmulti_s multi;
extern unsigned numhosts;
extern struct socket_range_s sockets[MAX_SOCKETS];
extern unsigned numsockets;
extern struct port_range_s ports[MAX_PORTS];
extern unsigned numports;
//advanced options
extern int device;
extern unsigned options;
extern unsigned packets;
extern unsigned protos;
extern struct socks_info_s socksinfo;
extern struct nat_info_s natinfo;

//wizard needs these
extern int enabled;
void SetOptions(int op);
void SaveConfigPageOptions(void);
void SaveAdvCfgPageOptions(void);
void StartupTun(void);
void ShutdownTun(void);

#endif

/*--------------------------------------------------------------------------*\
|| packet.c by Ryan Rubley <ryan@morpheussoftware.net>                      ||
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

#include <winsock2.h>
#include <windows.h>
#include "packet.h"

unsigned AdapterCount;
char *AdapterList[MAX_ADAPTERS];
char *AdapterDesc[MAX_ADAPTERS];

void PacketFillAdapterList(void)
{
	static char	buf[ADAPTER_NAME_BUF];

	char tmp[256];
	WCHAR *pw, *sw;
	char *pa, *sa;
	unsigned long bufsize = ADAPTER_NAME_BUF;
	int i = 0, unicode = 0;

	AdapterCount = 0;
	memset(AdapterList, 0, sizeof(AdapterList));
	memset(AdapterDesc, 0, sizeof(AdapterDesc));

	//WinPcap_3_0.exe			Packet.dll=3.0.0.18		PacketGetVersion()="3.0 alpha3"
	//WinPcap_3_1_beta_3.exe	Packet.dll=3.1.0.23		PacketGetVersion()="3, 1, 0, 23"
	pa = PacketGetVersion();
	if( pa[0] == '1' || pa[0] == '2' || (pa[0] == '3' && pa[1] == '.' && pa[2] == '0') ) {
		if( !(GetVersion() & 0x80000000) ) {//Windows NT/2K/XP
			unicode = 1;
		}
	}

	if(!PacketGetAdapterNames(buf,&bufsize)) {
		return;
	}

	if( unicode ) {
		pw=sw=(WCHAR *)buf;
		while((*pw!='\0')||(*(pw-1)!='\0')) {
			if(*pw=='\0') {
				if(i<MAX_ADAPTERS) {
					if(WideCharToMultiByte( CP_ACP, 0, sw, -1, tmp, 256, NULL, NULL )) {
						strcpy((char *)sw,tmp);
					}
					AdapterList[i]=(char *)sw;
				}
				sw=pw+1;
				i++;
			}
			pw++;
		}
		pw++;
		pa = (char *)pw;
	} else {
		pa=sa=buf;
		while((*pa!='\0')||(*(pa-1)!='\0')) {
			if(*pa=='\0') {
				if(i<MAX_ADAPTERS) {
					AdapterList[i]=sa;
				}
				sa=pa+1;
				i++;
			}
			pa++;
		}
		pa++;
	}
	AdapterCount=i;

	sa=pa;
	i=0;
	while((*pa!='\0')||(*(pa-1)!='\0')) {
		if(*pa=='\0') {
			if(i<MAX_ADAPTERS) {
				AdapterDesc[i]=sa;
			}
			sa=pa+1;
			i++;
		}
		pa++;
	}

	return;
}

char *AdapterListNoGUID(int num)
{
	static char buf[256];
	char *pa,*sa;

	strncpy(buf, AdapterList[num], sizeof(buf) - 1);
	buf[sizeof(buf) - 1] = '\0';

	if( strlen(buf) >= 36 ) {
		for(sa = buf; *sa+36!='\0'; sa++ ) {
			if( isxdigit(sa[0])  && isxdigit(sa[1])  && isxdigit(sa[2])  && isxdigit(sa[3])  && isxdigit(sa[4])  &&
				isxdigit(sa[5])  && isxdigit(sa[6])  && isxdigit(sa[7])  && sa[8]  == '-'    && isxdigit(sa[9])  &&
				isxdigit(sa[10]) && isxdigit(sa[11]) && isxdigit(sa[12]) && sa[13] == '-'    && isxdigit(sa[14]) &&
				isxdigit(sa[15]) && isxdigit(sa[16]) && isxdigit(sa[17]) && sa[18] == '-'    && isxdigit(sa[19]) &&
				isxdigit(sa[20]) && isxdigit(sa[21]) && isxdigit(sa[22]) && sa[23] == '-'    && isxdigit(sa[24]) &&
				isxdigit(sa[25]) && isxdigit(sa[26]) && isxdigit(sa[27]) && isxdigit(sa[28]) && isxdigit(sa[29]) &&
				isxdigit(sa[30]) && isxdigit(sa[31]) && isxdigit(sa[32]) && isxdigit(sa[33]) && isxdigit(sa[34]) &&
				isxdigit(sa[35]) ) {

				pa = sa + 36;
				strcpy(sa, "GUID");
				sa += 4;
				memmove(sa, pa, strlen(pa) + 1);
				break;
			}
		}
	}

	return buf;
}

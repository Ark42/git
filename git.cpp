/*--------------------------------------------------------------------------*\
|| git.c by Ryan Rubley <ryan@morpheussoftware.net>                         ||
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

#include <stdio.h>
#include "resource.h"
#include "git.h"
#include "utils.h"
#include "packet.h"
#include "tun2eth.h"
#include "eth2tun.h"
#include "wizard.h"

#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "packet.lib")
//PT #pragma comment(lib, "pthreadVSE.lib")

NOTIFYICONDATA nid;
HWND hwnd;
HINSTANCE hInst;
HMENU popupextra,popup;
HICON smiconon,smiconoff,smiconup,smicondn,lgiconon;
char g_pathexe[MAX_PATH], g_path[MAX_PATH];
int in_config = 0;
//PT pthread_mutex_t config_lock = PTHREAD_MUTEX_INITIALIZER;
CRITICAL_SECTION config_lock;
//PT pthread_mutex_t initing = PTHREAD_MUTEX_INITIALIZER;
CRITICAL_SECTION initing;

//global states
int autorun = 0;
int enabled = 1;
enum {DISABLED,STARTUP,ENABLED,SHUTDOWN} state = DISABLED;
//config options
struct fwd_host_s hosts[MAX_HOSTS];
struct fwd_host_tcpmulti_s multi;
unsigned numhosts = 0;
struct socket_range_s sockets[MAX_SOCKETS];
unsigned numsockets = 0;
struct port_range_s ports[MAX_PORTS];
unsigned numports = 0;
//advanced options
int device = 0;
unsigned options = OPT_FRAME_8022 | OPT_FRAME_8023 | OPT_FRAME_ETH2 | OPT_IPV4_NOUC;
unsigned packets = (1<<PACKET_IPX);
unsigned protos = (1<<PROTO_UDP);
struct socks_info_s socksinfo;
struct nat_info_s natinfo;
char *g_delaywizard = NULL;


LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
int CALLBACK DialogProcCfg(HWND hwndDlg,UINT uMsg,WPARAM wParam,LPARAM lParam);
int CALLBACK DialogProcAdvCfg(HWND hwndDlg,UINT uMsg,WPARAM wParam,LPARAM lParam);
int CALLBACK DialogProcConnStat(HWND hwndDlg,UINT uMsg,WPARAM wParam,LPARAM lParam);
void SetTrayIcon(void);
void SetAutorun(int ar);
void SetEnabled(int en);
char *MakeStatusString(void);
char *MakeHWSrcListString(void);
char *ctime2(const time_t *timer);


int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
					 LPSTR lpCmdLine, int nShowCmd)
{
	WNDCLASSEX wcex;
	MSG msg;
	HKEY key;
	WSADATA wsad;
	DWORD dw;
	char buf[1024];
	unsigned long type,sz,i;

	//find out if we are already running.
	if( (hwnd = FindWindow(GIT_CLASS, NULL)) ) {
		if( strcmp(lpCmdLine, "-hide") ) { //if not hide, add icon
			SendMessage(hwnd, WM_USER + 2, 0, 0);
			if( strlen(lpCmdLine) > 0 ) { //if any cmd line (other then hide) run wizard
				COPYDATASTRUCT cds;
				cds.dwData = 0;
				cds.cbData = strlen(lpCmdLine) + 1;
				cds.lpData = lpCmdLine;
				SendMessage(hwnd, WM_COPYDATA, 0, (LPARAM)&cds);
			}
		} else { //if hide
			SendMessage(hwnd, WM_USER + 3, 0, 0);
		}
		return 0;
	}

	//put the checkmark on the proper autorun setting in the popup menu
	if(!RegOpenKey(HKEY_CURRENT_USER,"Software\\Microsoft\\Windows\\CurrentVersion\\Run",&key)) {
		sz = sizeof(buf);
		if(!RegQueryValueEx(key,GIT_NAME,0,&type,(LPBYTE)buf,&sz)) {
			if( type==REG_SZ && !strncmp(buf,g_pathexe,strlen(g_pathexe)) ) {
				if( strstr(buf,"-hide") ) {
					SetAutorun(2);
				} else {
					SetAutorun(1);
				}
			}
		}
		RegCloseKey(key);
	}
	
	//grab options from registry
	if(!RegCreateKey(HKEY_CURRENT_USER,"Software\\Git",&key)) {

		//config options
		sz = sizeof(i);
		if(!RegQueryValueEx(key,"numhosts",0,&type,(LPBYTE)&i,&sz)) {
			if( type==REG_DWORD ) {
				numhosts = i>MAX_HOSTS?MAX_HOSTS:i;

				for( i=0; i<numhosts; i++ ) {
					hosts[i].port = 213;
					sprintf(buf,"host%dport",i);
					sz = sizeof(dw);
					if( !RegQueryValueEx(key,buf,0,&type,(LPBYTE)&dw,&sz) && type==REG_DWORD ) {
						hosts[i].port = (unsigned short)dw;
					}

					hosts[i].method = METH_UDP;
					sprintf(buf,"host%dmethod",i);
					sz = sizeof(dw);
					if( !RegQueryValueEx(key,buf,0,&type,(LPBYTE)&dw,&sz) && type==REG_DWORD ) {
						hosts[i].method = (unsigned short)dw;
					}

					sprintf(buf,"host%dname",i);
					sz = MAX_HOSTLEN;
					if( RegQueryValueEx(key,buf,0,&type,hosts[i].name,&sz) || type!=REG_SZ ) {
						hosts[i].name[0] = '\0';
					}

					hosts[i].addr.s_addr = 0;
					hosts[i].s = -1;
					hosts[i].state = FH_STATE_DOWN;
					hosts[i].error = 0;
					hosts[i].shutdown = 0;
					hosts[i].connected = 0;
					hosts[i].disconnected = 0;
					hosts[i].lastfwd = 0;
					hosts[i].lastrecv = 0;
				}
			}
		}

		multi.port = 213;
		sz = sizeof(dw);
		if( !RegQueryValueEx(key,"multiport",0,&type,(LPBYTE)&dw,&sz) && type==REG_DWORD ) {
			multi.port = (unsigned short)dw;
		}
		multi.maxhosts = MAX_HOSTS;
		sz = sizeof(dw);
		if( !RegQueryValueEx(key,"multimax",0,&type,(LPBYTE)&dw,&sz) && type==REG_DWORD ) {
			multi.maxhosts = (unsigned short)dw;
		}

		sz = sizeof(i);
		if(!RegQueryValueEx(key,"numsockets",0,&type,(LPBYTE)&i,&sz) && type==REG_DWORD ) {
			numsockets = i>MAX_SOCKETS?MAX_SOCKETS:i;

			for( i=0; i<numsockets; i++ ) {
				sockets[i].min = 0;
				sprintf(buf,"socket%dmin",i);
				sz = sizeof(dw);
				if( !RegQueryValueEx(key,buf,0,&type,(LPBYTE)&dw,&sz) && type==REG_DWORD ) {
					sockets[i].min = (unsigned short)dw;
				}

				sockets[i].max = sockets[i].min;
				sprintf(buf,"socket%dmax",i);
				sz = sizeof(dw);
				if( !RegQueryValueEx(key,buf,0,&type,(LPBYTE)&dw,&sz) && type==REG_DWORD ) {
					sockets[i].max = (unsigned short)dw;
				}

				sprintf(buf,"socket%ddesc",i);
				sz = MAX_SOCKET_DESC;
				if( RegQueryValueEx(key,buf,0,&type,sockets[i].desc,&sz) || type!=REG_SZ ) {
					sockets[i].desc[0] = '\0';
				}
			}
		}

		sz = sizeof(i);
		if(!RegQueryValueEx(key,"numports",0,&type,(LPBYTE)&i,&sz) && type==REG_DWORD ) {
			numports = i>MAX_PORTS?MAX_PORTS:i;

			for( i=0; i<numports; i++ ) {
				ports[i].min = 0;
				sprintf(buf,"port%dmin",i);
				sz = sizeof(dw);
				if( !RegQueryValueEx(key,buf,0,&type,(LPBYTE)&dw,&sz) && type==REG_DWORD ) {
					ports[i].min = (unsigned short)dw;
				}

				ports[i].max = ports[i].min;
				sprintf(buf,"port%dmax",i);
				sz = sizeof(dw);
				if( !RegQueryValueEx(key,buf,0,&type,(LPBYTE)&dw,&sz) && type==REG_DWORD ) {
					ports[i].max = (unsigned short)dw;
				}

				sprintf(buf,"port%ddesc",i);
				sz = MAX_PORT_DESC;
				if( RegQueryValueEx(key,buf,0,&type,ports[i].desc,&sz) || type!=REG_SZ ) {
					ports[i].desc[0] = '\0';
				}
			}
		}

		//advanced options
		sz = sizeof(i);
		if( !RegQueryValueEx(key,"device",0,&type,(LPBYTE)&i,&sz) && type==REG_DWORD ) {
			device = i;
		}

		sz = sizeof(i);
		if( !RegQueryValueEx(key,"options",0,&type,(LPBYTE)&i,&sz) && type==REG_DWORD ) {
			SetOptions(i);
		}

		sz = sizeof(i);
		if( !RegQueryValueEx(key,"packets",0,&type,(LPBYTE)&i,&sz) && type==REG_DWORD ) {
			packets = i;
		}

		sz = sizeof(i);
		if( !RegQueryValueEx(key,"protos",0,&type,(LPBYTE)&i,&sz) && type==REG_DWORD ) {
			protos = i;
		}

		sz = MAX_HOSTLEN;
		if( RegQueryValueEx(key,"sockshost",0,&type,socksinfo.hostname,&sz) || type!=REG_SZ ) {
			socksinfo.hostname[0] = '\0';
		}
		socksinfo.port = 1080;
		sz = sizeof(i);
		if( !RegQueryValueEx(key,"socksport",0,&type,(LPBYTE)&i,&sz) && type==REG_DWORD ) {
			socksinfo.port = (unsigned short)i;
		}
		sz = MAX_HOSTLEN;
		if( RegQueryValueEx(key,"socksuser",0,&type,socksinfo.username,&sz) || type!=REG_SZ ) {
			socksinfo.username[0] = '\0';
		}
		sz = MAX_HOSTLEN;
		if( RegQueryValueEx(key,"sockspass",0,&type,socksinfo.password,&sz) || type!=REG_BINARY ) {
			socksinfo.password[0] = '\0';
		} else {
			for( i=0; i<sz; i++ ) {
				socksinfo.password[i] ^= 0xAA ^ ((i<<5)&0xFF) ^ strlen((char *)socksinfo.password);
			}
		}

		sz = MAX_HOSTLEN;
		if( RegQueryValueEx(key,"natinternal",0,&type,natinfo.internal,&sz) || type!=REG_SZ ) {
			natinfo.internal[0] = '\0';
		}
		sz = MAX_HOSTLEN;
		if( RegQueryValueEx(key,"natexternal",0,&type,natinfo.external,&sz) || type!=REG_SZ ) {
			natinfo.external[0] = '\0';
		}

		RegCloseKey(key);
	}

	if(WSAStartup(MAKEWORD(2,0),&wsad)) {
		MessageBox(NULL,"Failed to initialize winsock.",GIT_NAME,MB_ICONSTOP);
		return 1;
	}

	InitializeCriticalSection(&config_lock);
	InitializeCriticalSection(&initing);
	InitializeCriticalSection(&eth2tun_mutex);
	InitializeCriticalSection(&tun2eth_mutex);
	InitializeCriticalSection(&multi_mutex);
	InitializeCriticalSection(&hwsrclist_mutex);

	hInst = hInstance;
	smiconon = (HICON)LoadImage(hInstance,MAKEINTRESOURCE(IDI_ENABLED),IMAGE_ICON,16,16,LR_SHARED);
	smiconoff = (HICON)LoadImage(hInstance,MAKEINTRESOURCE(IDI_DISABLED),IMAGE_ICON,16,16,LR_SHARED);
	smiconup = (HICON)LoadImage(hInstance,MAKEINTRESOURCE(IDI_STARTUP),IMAGE_ICON,16,16,LR_SHARED);
	smicondn = (HICON)LoadImage(hInstance,MAKEINTRESOURCE(IDI_SHUTDOWN),IMAGE_ICON,16,16,LR_SHARED);
	lgiconon = (HICON)LoadImage(hInstance,MAKEINTRESOURCE(IDI_ENABLED),IMAGE_ICON,32,32,LR_SHARED);
	popupextra = LoadMenu(hInstance,MAKEINTRESOURCE(IDR_TRAY));
	popup = GetSubMenu(popupextra,0);
	SetMenuDefaultItem(popup,0,TRUE);
	GetModuleFileName(NULL,g_pathexe,MAX_PATH);
	strcpy(g_path, g_pathexe);
	if( strrchr(g_path, '\\') ) {
		strrchr(g_path, '\\')[1] = '\0';
	}

	SetOptions(options); //this sets the checkmarks on the popup menu

	//create a dummy window so we can have a WndProc
	wcex.cbSize = sizeof(WNDCLASSEX); 
	wcex.style			= 0;
	wcex.lpfnWndProc	= (WNDPROC)WndProc;
	wcex.cbClsExtra		= 0;
	wcex.cbWndExtra		= 0;
	wcex.hInstance		= hInstance;
	wcex.hIcon			= lgiconon;
	wcex.hCursor		= LoadCursor(NULL,IDC_ARROW);
	wcex.hbrBackground	= (HBRUSH)(COLOR_BTNFACE);
	wcex.lpszMenuName	= NULL;
	wcex.lpszClassName	= GIT_CLASS;
	wcex.hIconSm		= smiconon;

	RegisterClassEx(&wcex);
	hwnd=CreateWindow( GIT_CLASS, NULL, 0, 0, 0, 0, 0, NULL, NULL, hInstance, NULL );

	PacketFillAdapterList();

	//setup our tray icon
	nid.cbSize = sizeof(nid);
	nid.hWnd = hwnd;
	nid.uID = 0;
	nid.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
	nid.uCallbackMessage = WM_USER+1;
	nid.hIcon = smiconup;
	strcpy(nid.szTip, GIT_NAME " - [STARTING UP]");
	if( strcmp(lpCmdLine, "-hide") ) { //if not hide, add icon
		Shell_NotifyIcon(NIM_ADD, &nid);
		if( strlen(lpCmdLine) > 0 ) { //if any cmd line (other then hide) run wizard
			EnterCriticalSection(&config_lock);
			in_config = 1;
			RunWizard(lpCmdLine);
			in_config = 0;
			LeaveCriticalSection(&config_lock);
		}
	}

	//startup tunneling threads
	StartupTun();

	//main loop
	while(GetMessage(&msg, NULL, 0, 0)) {
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	ShutdownTun();

	WSACleanup();

	Shell_NotifyIcon(NIM_DELETE,&nid);

	DestroyMenu(popup);

	DeleteCriticalSection(&config_lock);
	DeleteCriticalSection(&initing);
	DeleteCriticalSection(&eth2tun_mutex);
	DeleteCriticalSection(&tun2eth_mutex);
	DeleteCriticalSection(&multi_mutex);
	DeleteCriticalSection(&hwsrclist_mutex);

	if( g_delaywizard != NULL ) {
		delete [] g_delaywizard;
		g_delaywizard = NULL;
	}

	return msg.wParam;
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	POINT point;
	switch (message) 
	{
	case WM_USER+1:
		switch(lParam) {
		case WM_LBUTTONDBLCLK:
			SetEnabled(!enabled);
			break;

		case WM_RBUTTONUP:
			GetCursorPos(&point);
			SetForegroundWindow(hWnd);
			switch(TrackPopupMenu(popup,TPM_NONOTIFY|TPM_RETURNCMD,point.x,point.y,0,hWnd,NULL)) {

			//case 0:
				//EndDialog(hWnd,0); //makes menu work right for some reason
				//break;

			case ID_TRAY_TITLE:
				break;

			case ID_TRAY_SUPPORT:
				//ShellExecute( NULL, NULL, "mailto:" GIT_EMAIL "?subject=" GIT_NAME, NULL, NULL, SW_SHOWMAXIMIZED  );
				ShellExecute( NULL, NULL, GIT_FORUM, NULL, NULL, SW_SHOWMAXIMIZED  );
				break;

			case ID_TRAY_HOMEPAGE:
				ShellExecute( NULL, NULL, GIT_URL, NULL, NULL, SW_SHOWMAXIMIZED  );
				break;

			case ID_TRAY_README:
				ShellExecute( NULL, NULL, "Readme.txt", NULL, NULL, SW_SHOWNORMAL );
				break;

			case ID_TRAY_EXIT:
				PostQuitMessage(0);
				break;

			case ID_TRAY_HIDE:
				Shell_NotifyIcon(NIM_DELETE,&nid);
				break;

			case ID_TRAY_WIZARD:
				if(in_config) break;
				EnterCriticalSection(&config_lock);
				in_config = 1;
				RunWizard();
				in_config = 0;
				LeaveCriticalSection(&config_lock);
				break;

			case ID_TRAY_CONFIGURE:
				if(in_config) break;
				EnterCriticalSection(&config_lock);
				in_config = 1;
				DialogBox( hInst, MAKEINTRESOURCE(IDD_CONFIGURE), NULL, DialogProcCfg );
				in_config = 0;
				LeaveCriticalSection(&config_lock);
				break;

			case ID_TRAY_ADVCFG:
				if(in_config) break;
				EnterCriticalSection(&config_lock);
				in_config = 1;
				DialogBox( hInst, MAKEINTRESOURCE(IDD_ADVCFG), NULL, DialogProcAdvCfg );
				in_config = 0;
				LeaveCriticalSection(&config_lock);
				break;

			case ID_TRAY_LOG_IN:
				SetOptions(options^OPT_LOG_IN);
				break;

			case ID_TRAY_LOG_FWD:
				SetOptions(options^OPT_LOG_FWD);
				break;

			case ID_TRAY_LOG_UNFWD:
				SetOptions(options^OPT_LOG_UNFWD);
				break;

			case ID_TRAY_LOG_ADDR:
				SetOptions(options^OPT_LOG_ADDR);
				break;

			case ID_TRAY_STATUS:
				if(in_config) break;
				EnterCriticalSection(&config_lock);
				in_config = 1;
				DialogBox( hInst, MAKEINTRESOURCE(IDD_CONNSTAT), NULL, DialogProcConnStat );
				in_config = 0;
				LeaveCriticalSection(&config_lock);
				break;

			case ID_TRAY_AUTORUN_DONT:
				SetAutorun(0);
				break;

			case ID_TRAY_AUTORUN_TRAY:
				SetAutorun(1);
				break;

			case ID_TRAY_AUTORUN_HIDDEN:
				SetAutorun(2);
				break;

			case ID_TRAY_ENABLED:
				SetEnabled(!enabled);
				break;

			default:
				break;
			}
			break;

		default:
			break;
		}
		break;

	case WM_USER + 2:
		Shell_NotifyIcon(NIM_ADD, &nid);
		break;

	case WM_USER + 3:
		Shell_NotifyIcon(NIM_DELETE, &nid);
		break;

	case WM_USER + 4:
		if( g_delaywizard != NULL ) {
			if(in_config) {
				delete [] g_delaywizard;
				g_delaywizard = NULL;
				break;
			}
			EnterCriticalSection(&config_lock);
			in_config = 1;
			RunWizard(g_delaywizard);
			delete [] g_delaywizard;
			g_delaywizard = NULL;
			in_config = 0;
			LeaveCriticalSection(&config_lock);
		}
		break;

	case WM_COPYDATA: {
		COPYDATASTRUCT *cds = (COPYDATASTRUCT *)lParam;
		if( cds->dwData == 0 ) {
			g_delaywizard = new char[cds->cbData];
			memcpy(g_delaywizard, cds->lpData, cds->cbData);
			PostMessage(hWnd, WM_USER + 4, 0, 0);
		}
		break; }

	case WM_DESTROY:
		PostQuitMessage(0);
		break;

	case WM_QUIT:
		DestroyMenu(popupextra);
		DestroyWindow(hWnd);
		UnregisterClass(GIT_CLASS,hInst);
		break;

	default:
		return DefWindowProc(hWnd, message, wParam, lParam);
	}

	return 0;
}

int CALLBACK DialogProcCfg(HWND hwndDlg,UINT uMsg,WPARAM wParam,LPARAM lParam)
{
	char buf[MAX_HOSTLEN+64];
	struct fwd_host_s fh;
	struct socket_range_s sr;
	struct port_range_s pr;
	unsigned i;
	HWND hwndStat;

	switch( uMsg ) {
	default:
		return FALSE;

	case WM_INITDIALOG:

		SendMessage(hwndDlg, WM_SETICON, ICON_SMALL, (LPARAM)smiconon);
		SendMessage(hwndDlg, WM_SETICON, ICON_BIG, (LPARAM)lgiconon);

		//init hosts frame
		SendDlgItemMessage( hwndDlg, IDC_PORT, CB_ADDSTRING, 0, (LPARAM)"213" );
		SendDlgItemMessage( hwndDlg, IDC_PORT, CB_ADDSTRING, 0, (LPARAM)"214" );
		SendDlgItemMessage( hwndDlg, IDC_PORT, CB_ADDSTRING, 0, (LPARAM)"215" );
		SendDlgItemMessage( hwndDlg, IDC_PORT, CB_ADDSTRING, 0, (LPARAM)"216" );
		SetDlgItemInt( hwndDlg, IDC_PORT, multi.port, FALSE );
		for( i=0; i<METH_TCP_CONNECT_SOCKS5; i++ ) {
			SendDlgItemMessage( hwndDlg, IDC_METH, CB_ADDSTRING, 0, (LPARAM)method_list[i] );
		}
		SendDlgItemMessage( hwndDlg, IDC_METH, CB_SETCURSEL, 0, 0 );
		if(options & OPT_TCP_MULTI) {
			CheckDlgButton( hwndDlg, IDC_TCPMULTI, BST_CHECKED );
			i = TRUE;
		} else {
			for( i=0; i<numhosts; i++ ) {
				FwdHostToStr(&hosts[i],buf);
				SendDlgItemMessage( hwndDlg, IDC_HOSTS, LB_ADDSTRING, 0, (LPARAM)buf );
			}
			i = FALSE;
		}
		EnableWindow( GetDlgItem( hwndDlg, IDC_MAXCONN ), i );
		i = !i;
		EnableWindow( GetDlgItem( hwndDlg, IDC_HOST ), i );
		EnableWindow( GetDlgItem( hwndDlg, IDC_METH ), i );
		EnableWindow( GetDlgItem( hwndDlg, IDC_ADDHOST ), i );
		EnableWindow( GetDlgItem( hwndDlg, IDC_REMOVEHOST ), i );
		EnableWindow( GetDlgItem( hwndDlg, IDC_HOSTS ), i );
		SetDlgItemInt( hwndDlg, IDC_MAXCONN, multi.maxhosts, FALSE );

		//init socket frame
		for( i=0; def_socket_list[i]; i++ ) {
			SendDlgItemMessage( hwndDlg, IDC_SOCKET, CB_ADDSTRING, 0, (LPARAM)def_socket_list[i] );
		}
		for( i=0; i<numsockets; i++ ) {
			SocketRangeToStr(&sockets[i],buf);
			SendDlgItemMessage( hwndDlg, IDC_SOCKETS, LB_ADDSTRING, 0, (LPARAM)buf );
		}

		//init port frame
		for( i=0; def_port_list[i]; i++ ) {
			SendDlgItemMessage( hwndDlg, IDC_PORTC, CB_ADDSTRING, 0, (LPARAM)def_port_list[i] );
		}
		for( i=0; i<numports; i++ ) {
			PortRangeToStr(&ports[i],buf);
			SendDlgItemMessage( hwndDlg, IDC_PORTS, LB_ADDSTRING, 0, (LPARAM)buf );
		}

		//disable add buttons if full
		if( SendDlgItemMessage( hwndDlg, IDC_HOSTS, LB_GETCOUNT, 0, 0 ) >= MAX_HOSTS ) {
			EnableWindow( GetDlgItem( hwndDlg, IDC_HOST ), FALSE );
		}
		if( SendDlgItemMessage( hwndDlg, IDC_SOCKETS, LB_GETCOUNT, 0, 0 ) >= MAX_SOCKETS ) {
			EnableWindow( GetDlgItem( hwndDlg, IDC_SOCKET ), FALSE );
		}
		if( SendDlgItemMessage( hwndDlg, IDC_PORTS, LB_GETCOUNT, 0, 0 ) >= MAX_SOCKETS ) {
			EnableWindow( GetDlgItem( hwndDlg, IDC_PORTC ), FALSE );
		}
		return TRUE;

	case WM_COMMAND:
		switch( wParam ) {
		case IDC_ADDHOST:
			if( GetDlgItemText( hwndDlg, IDC_HOST, (char *)fh.name, MAX_HOSTLEN ) > 0 ) {
				fh.port = GetDlgItemInt( hwndDlg, IDC_PORT, NULL, FALSE );
				if( !fh.port ) fh.port = 213;
				fh.method = (unsigned short)SendDlgItemMessage( hwndDlg, IDC_METH, CB_GETCURSEL, 0, 0 );
				if( ++fh.method < METH_UDP ) fh.method = METH_UDP;
				FwdHostToStr(&fh,buf);
				SendDlgItemMessage( hwndDlg, IDC_HOSTS, LB_ADDSTRING, 0, (LPARAM)buf );
			}
			SetDlgItemText( hwndDlg, IDC_HOST, NULL );
			if( SendDlgItemMessage( hwndDlg, IDC_HOSTS, LB_GETCOUNT, 0, 0 ) >= MAX_HOSTS ) {
				EnableWindow( GetDlgItem( hwndDlg, IDC_HOST ), FALSE );
			}
			break;

		case IDC_REMOVEHOST:
			buf[0] = '\0';
			i = SendDlgItemMessage( hwndDlg, IDC_HOSTS, LB_GETCURSEL, 0, 0 );
			if( i != LB_ERR ) {
				if( SendDlgItemMessage( hwndDlg, IDC_HOSTS, LB_GETTEXTLEN, i, 0 ) < MAX_HOSTLEN+64 ) {
					SendDlgItemMessage( hwndDlg, IDC_HOSTS, LB_GETTEXT, i, (LPARAM)buf );
				}
				StrToFwdHost(buf,&fh);
				SendDlgItemMessage( hwndDlg, IDC_HOSTS, LB_DELETESTRING, i, 0 );
				SetDlgItemText( hwndDlg, IDC_HOST, (char *)fh.name );
				SetDlgItemInt( hwndDlg, IDC_PORT, fh.port, FALSE );
				SendDlgItemMessage( hwndDlg, IDC_METH, CB_SETCURSEL, fh.method-1, 0 );
			}
			if( SendDlgItemMessage( hwndDlg, IDC_HOSTS, LB_GETCOUNT, 0, 0 ) < MAX_HOSTS ) {
				EnableWindow( GetDlgItem( hwndDlg, IDC_HOST ), TRUE );
			}
			break;

		case IDC_ADDSOCKET:
			if( GetDlgItemText( hwndDlg, IDC_SOCKET, buf, MAX_HOSTLEN ) > 0 ) {
				StrToSocketRange(buf,&sr);
				SocketRangeToStr(&sr,buf);
				SendDlgItemMessage( hwndDlg, IDC_SOCKETS, LB_ADDSTRING, 0, (LPARAM)buf );
			}
			SetDlgItemText( hwndDlg, IDC_SOCKET, NULL );
			if( SendDlgItemMessage( hwndDlg, IDC_SOCKETS, LB_GETCOUNT, 0, 0 ) >= MAX_SOCKETS ) {
				EnableWindow( GetDlgItem( hwndDlg, IDC_SOCKET ), FALSE );
			}
			break;

		case IDC_REMOVESOCKET:
			buf[0] = '\0';
			i = SendDlgItemMessage( hwndDlg, IDC_SOCKETS, LB_GETCURSEL, 0, 0 );
			if( i != LB_ERR ) {
				if( SendDlgItemMessage( hwndDlg, IDC_SOCKETS, LB_GETTEXTLEN, i, 0 ) < MAX_HOSTLEN ) {
					SendDlgItemMessage( hwndDlg, IDC_SOCKETS, LB_GETTEXT, i, (LPARAM)buf );
				}
				SendDlgItemMessage( hwndDlg, IDC_SOCKETS, LB_DELETESTRING, i, 0 );
				SetDlgItemText( hwndDlg, IDC_SOCKET, buf );
			}
			if( SendDlgItemMessage( hwndDlg, IDC_SOCKETS, LB_GETCOUNT, 0, 0 ) < MAX_SOCKETS ) {
				EnableWindow( GetDlgItem( hwndDlg, IDC_SOCKET ), TRUE );
			}
			break;

		case IDC_ADDPORT:
			if( GetDlgItemText( hwndDlg, IDC_PORTC, buf, MAX_HOSTLEN ) > 0 ) {
				StrToPortRange(buf,&pr);
				PortRangeToStr(&pr,buf);
				SendDlgItemMessage( hwndDlg, IDC_PORTS, LB_ADDSTRING, 0, (LPARAM)buf );
			}
			SetDlgItemText( hwndDlg, IDC_PORTC, NULL );
			if( SendDlgItemMessage( hwndDlg, IDC_PORTS, LB_GETCOUNT, 0, 0 ) >= MAX_PORTS ) {
				EnableWindow( GetDlgItem( hwndDlg, IDC_PORTC ), FALSE );
			}
			break;

		case IDC_REMOVEPORT:
			buf[0] = '\0';
			i = SendDlgItemMessage( hwndDlg, IDC_PORTS, LB_GETCURSEL, 0, 0 );
			if( i != LB_ERR ) {
				if( SendDlgItemMessage( hwndDlg, IDC_PORTS, LB_GETTEXTLEN, i, 0 ) < MAX_HOSTLEN ) {
					SendDlgItemMessage( hwndDlg, IDC_PORTS, LB_GETTEXT, i, (LPARAM)buf );
				}
				SendDlgItemMessage( hwndDlg, IDC_PORTS, LB_DELETESTRING, i, 0 );
				SetDlgItemText( hwndDlg, IDC_PORTC, buf );
			}
			if( SendDlgItemMessage( hwndDlg, IDC_PORTS, LB_GETCOUNT, 0, 0 ) < MAX_PORTS ) {
				EnableWindow( GetDlgItem( hwndDlg, IDC_PORTC ), TRUE );
			}
			break;

		case IDC_TCPMULTI:
			i = (IsDlgButtonChecked( hwndDlg, IDC_TCPMULTI ) & BST_CHECKED)?TRUE:FALSE;
			EnableWindow( GetDlgItem( hwndDlg, IDC_MAXCONN ), i );
			i = !i;
			EnableWindow( GetDlgItem( hwndDlg, IDC_HOST ), i );
			EnableWindow( GetDlgItem( hwndDlg, IDC_METH ), i );
			EnableWindow( GetDlgItem( hwndDlg, IDC_ADDHOST ), i );
			EnableWindow( GetDlgItem( hwndDlg, IDC_REMOVEHOST ), i );
			EnableWindow( GetDlgItem( hwndDlg, IDC_HOSTS ), i );
			break;

		case IDC_OK:
			//display status window
			EnableWindow( hwndDlg, FALSE );
			if( enabled ) {
				hwndStat = CreateDialog( hInst, MAKEINTRESOURCE(IDD_STATUS), hwndDlg, DialogProcStatus );
				ShutdownTun();
			} else {
				hwndStat = NULL;
			}

			//misc stuff for multi
			i = options & 0xFFFF0FFF;
			if(IsDlgButtonChecked( hwndDlg, IDC_TCPMULTI ) & BST_CHECKED) i|=OPT_TCP_MULTI;
			SetOptions(i);
			multi.port = GetDlgItemInt( hwndDlg, IDC_PORT, NULL, FALSE );
			if( !multi.port ) multi.port = 213;
			multi.maxhosts = GetDlgItemInt( hwndDlg, IDC_MAXCONN, NULL, FALSE );
			if(multi.maxhosts<0) multi.maxhosts = 0;
			if(multi.maxhosts>MAX_HOSTS) multi.maxhosts = MAX_HOSTS;

			if(options & OPT_TCP_MULTI) {
				//fill hosts with multi
				numhosts = multi.maxhosts;
				for( i=0; i<numhosts; i++ ) {
					sprintf(buf,"0.0.0.0:%d (%s)",multi.port,method_list[METH_TCP_MULTI_LISTEN-1]);
					StrToFwdHost(buf,&hosts[i]);
				}
			} else {
				//fill hosts from dialog
				i = SendDlgItemMessage( hwndDlg, IDC_HOSTS, LB_GETCOUNT, 0, 0 );
				if( i != LB_ERR ) {
					numhosts = i>MAX_HOSTS?MAX_HOSTS:i;
					for( i=0; i<numhosts; i++ ) {
						if( SendDlgItemMessage( hwndDlg, IDC_HOSTS, LB_GETTEXTLEN, i, 0 ) < MAX_HOSTLEN+64 ) {
							SendDlgItemMessage( hwndDlg, IDC_HOSTS, LB_GETTEXT, i, (LPARAM)buf );
							StrToFwdHost(buf,&hosts[i]);
						}
					}
				}
			}

			//fill sockets from dialog
			i = SendDlgItemMessage( hwndDlg, IDC_SOCKETS, LB_GETCOUNT, 0, 0 );
			if( i != LB_ERR ) {
				numsockets = i>MAX_SOCKETS?MAX_SOCKETS:i;
				for( i=0; i<numsockets; i++ ) {
					if( SendDlgItemMessage( hwndDlg, IDC_SOCKETS, LB_GETTEXTLEN, i, 0 ) < MAX_HOSTLEN ) {
						SendDlgItemMessage( hwndDlg, IDC_SOCKETS, LB_GETTEXT, i, (LPARAM)buf );
						StrToSocketRange(buf,&sockets[i]);
					}
				}
			}

			//fill ports from dialog
			i = SendDlgItemMessage( hwndDlg, IDC_PORTS, LB_GETCOUNT, 0, 0 );
			if( i != LB_ERR ) {
				numports = i>MAX_PORTS?MAX_PORTS:i;
				for( i=0; i<numports; i++ ) {
					if( SendDlgItemMessage( hwndDlg, IDC_PORTS, LB_GETTEXTLEN, i, 0 ) < MAX_HOSTLEN ) {
						SendDlgItemMessage( hwndDlg, IDC_PORTS, LB_GETTEXT, i, (LPARAM)buf );
						StrToPortRange(buf,&ports[i]);
					}
				}
			}

			SaveConfigPageOptions();

			//remove status window
			if( hwndStat ) {
				if( enabled ) {
					StartupTun();
				}
				DestroyWindow( hwndStat );
			}

			EndDialog( hwndDlg, wParam );
			break;

		case IDCANCEL:
			EndDialog( hwndDlg, wParam );
			break;

		default:
			break;
		}
		return TRUE;
	}
}

int CALLBACK DialogProcAdvCfg(HWND hwndDlg,UINT uMsg,WPARAM wParam,LPARAM lParam)
{
	char buf[MAX_HOSTLEN];
	unsigned i;
	HWND hwndStat;

	switch( uMsg ) {
	default:
		return FALSE;

	case WM_INITDIALOG:

		SendMessage(hwndDlg, WM_SETICON, ICON_SMALL, (LPARAM)smiconon);
		SendMessage(hwndDlg, WM_SETICON, ICON_BIG, (LPARAM)lgiconon);

		//init device frame
		if( AdapterCount < 1 ) {
			SendDlgItemMessage( hwndDlg, IDC_DEVICE, CB_ADDSTRING, 0, (LPARAM)"" );
		} else {
			for( i = 0; i < AdapterCount; i++ ) {
				if( i < MAX_ADAPTERS ) {
					sprintf(buf, "%d - %.64s (%.128s)", i, AdapterListNoGUID(i), AdapterDesc[i]);
				} else {
					sprintf(buf, "%d", i);
				}
				SendDlgItemMessage( hwndDlg, IDC_DEVICE, CB_ADDSTRING, 0, (LPARAM)buf );
			}
		}
		SendDlgItemMessage( hwndDlg, IDC_DEVICE, CB_SETCURSEL, device, 0 );

		//init frame frame
		if(options&OPT_FRAME_8022) CheckDlgButton( hwndDlg, IDC_FRAME_8022, BST_CHECKED );
		if(options&OPT_FRAME_8023) CheckDlgButton( hwndDlg, IDC_FRAME_8023, BST_CHECKED );
		if(options&OPT_FRAME_ETH2) CheckDlgButton( hwndDlg, IDC_FRAME_ETH2, BST_CHECKED );
		if(options&OPT_FRAME_SNAP) CheckDlgButton( hwndDlg, IDC_FRAME_SNAP, BST_CHECKED );

		//init packet frame
		if(packets&(1<<PACKET_OLD)) CheckDlgButton( hwndDlg, IDC_PACKET_OLD, BST_CHECKED );
		if(packets&(1<<PACKET_RIP)) CheckDlgButton( hwndDlg, IDC_PACKET_RIP, BST_CHECKED );
		if(packets&(1<<PACKET_ECO)) CheckDlgButton( hwndDlg, IDC_PACKET_ECO, BST_CHECKED );
		if(packets&(1<<PACKET_ERR)) CheckDlgButton( hwndDlg, IDC_PACKET_ERR, BST_CHECKED );
		if(packets&(1<<PACKET_IPX)) CheckDlgButton( hwndDlg, IDC_PACKET_IPX, BST_CHECKED );
		if(packets&(1<<PACKET_SPX)) CheckDlgButton( hwndDlg, IDC_PACKET_SPX, BST_CHECKED );
		if(packets&(1<<PACKET_NCP)) CheckDlgButton( hwndDlg, IDC_PACKET_NCP, BST_CHECKED );
		if(packets&(1<<PACKET_NTB)) CheckDlgButton( hwndDlg, IDC_PACKET_NTB, BST_CHECKED );

		//init ipv4 protocols frame
		if(protos&(1<<PROTO_ICMP)) CheckDlgButton( hwndDlg, IDC_PROTO_ICMP, BST_CHECKED );
		if(protos&(1<<PROTO_TCP)) CheckDlgButton( hwndDlg, IDC_PROTO_TCP, BST_CHECKED );
		if(protos&(1<<PROTO_UDP)) CheckDlgButton( hwndDlg, IDC_PROTO_UDP, BST_CHECKED );

		//init ipv4 options frame
		if(options&OPT_IPV4_SRCP) CheckDlgButton( hwndDlg, IDC_OPT_SRCP, BST_CHECKED );
		if(options&OPT_IPV4_NOUC) CheckDlgButton( hwndDlg, IDC_OPT_NOUC, BST_CHECKED );
		if(options&OPT_IPV4_NAT) CheckDlgButton( hwndDlg, IDC_OPT_NAT, BST_CHECKED );
		if(options&OPT_IPV4_NOBC) CheckDlgButton( hwndDlg, IDC_OPT_NOBC, BST_CHECKED );
		if(options&OPT_IPV4_NORT) CheckDlgButton( hwndDlg, IDC_OPT_NORT, BST_CHECKED );
		SetDlgItemText( hwndDlg, IDC_OPT_NAT_INT, (char *)natinfo.internal );
		SetDlgItemText( hwndDlg, IDC_OPT_NAT_EXT, (char *)natinfo.external );

		//init socks frame
		SetDlgItemText( hwndDlg, IDC_SOCKSHOST, (char *)socksinfo.hostname );
		SetDlgItemInt( hwndDlg, IDC_SOCKSPORT, socksinfo.port, FALSE );
		SetDlgItemText( hwndDlg, IDC_SOCKSUSER, (char *)socksinfo.username );
		SetDlgItemText( hwndDlg, IDC_SOCKSPASS, (char *)socksinfo.password );

		//init other frame
		if(options&OPT_OTHER_ARP) CheckDlgButton( hwndDlg, IDC_OTHER_ARP, BST_CHECKED );
		if(options&OPT_OTHER_ORFP) CheckDlgButton( hwndDlg, IDC_OTHER_ORFP, BST_CHECKED );
		if(options&OPT_COMP_ZLIB) CheckDlgButton( hwndDlg, IDC_COMP_ZLIB, BST_CHECKED );

		return TRUE;

	case WM_COMMAND:
		switch( wParam ) {
		case IDC_OK:
			//display status window
			EnableWindow( hwndDlg, FALSE );
			if( enabled ) {
				hwndStat = CreateDialog( hInst, MAKEINTRESOURCE(IDD_STATUS), hwndDlg, DialogProcStatus );
				ShutdownTun();
			} else {
				hwndStat = NULL;
			}

			//fill device from dialog
			device = SendDlgItemMessage( hwndDlg, IDC_DEVICE, CB_GETCURSEL, 0, 0 );

			//fill frames from dialog
			i = options & 0x0000FF00;
			if(IsDlgButtonChecked( hwndDlg, IDC_FRAME_8022 ) & BST_CHECKED) i|=OPT_FRAME_8022;
			if(IsDlgButtonChecked( hwndDlg, IDC_FRAME_8023 ) & BST_CHECKED) i|=OPT_FRAME_8023;
			if(IsDlgButtonChecked( hwndDlg, IDC_FRAME_ETH2 ) & BST_CHECKED) i|=OPT_FRAME_ETH2;
			if(IsDlgButtonChecked( hwndDlg, IDC_FRAME_SNAP ) & BST_CHECKED) i|=OPT_FRAME_SNAP;
			if(IsDlgButtonChecked( hwndDlg, IDC_OPT_SRCP ) & BST_CHECKED) i|=OPT_IPV4_SRCP;
			if(IsDlgButtonChecked( hwndDlg, IDC_OPT_NOUC ) & BST_CHECKED) i|=OPT_IPV4_NOUC;
			if(IsDlgButtonChecked( hwndDlg, IDC_OPT_NAT ) & BST_CHECKED) i|=OPT_IPV4_NAT;
			if(IsDlgButtonChecked( hwndDlg, IDC_OPT_NOBC ) & BST_CHECKED) i|=OPT_IPV4_NOBC;
			if(IsDlgButtonChecked( hwndDlg, IDC_OPT_NORT ) & BST_CHECKED) i|=OPT_IPV4_NORT;
			if(IsDlgButtonChecked( hwndDlg, IDC_OTHER_ARP ) & BST_CHECKED) i|=OPT_OTHER_ARP;
			if(IsDlgButtonChecked( hwndDlg, IDC_OTHER_ORFP ) & BST_CHECKED) i|=OPT_OTHER_ORFP;
			if(IsDlgButtonChecked( hwndDlg, IDC_COMP_ZLIB ) & BST_CHECKED) i|=OPT_COMP_ZLIB;
			SetOptions(i);

			//fill packets from dialog
			i = 0;
			if(IsDlgButtonChecked( hwndDlg, IDC_PACKET_OLD ) & BST_CHECKED) i|=(1<<PACKET_OLD);
			if(IsDlgButtonChecked( hwndDlg, IDC_PACKET_RIP ) & BST_CHECKED) i|=(1<<PACKET_RIP);
			if(IsDlgButtonChecked( hwndDlg, IDC_PACKET_ECO ) & BST_CHECKED) i|=(1<<PACKET_ECO);
			if(IsDlgButtonChecked( hwndDlg, IDC_PACKET_ERR ) & BST_CHECKED) i|=(1<<PACKET_ERR);
			if(IsDlgButtonChecked( hwndDlg, IDC_PACKET_IPX ) & BST_CHECKED) i|=(1<<PACKET_IPX);
			if(IsDlgButtonChecked( hwndDlg, IDC_PACKET_SPX ) & BST_CHECKED) i|=(1<<PACKET_SPX);
			if(IsDlgButtonChecked( hwndDlg, IDC_PACKET_NCP ) & BST_CHECKED) i|=(1<<PACKET_NCP);
			if(IsDlgButtonChecked( hwndDlg, IDC_PACKET_NTB ) & BST_CHECKED) i|=(1<<PACKET_NTB);
			packets = i;

			//fill protos from dialog
			i = 0;
			if(IsDlgButtonChecked( hwndDlg, IDC_PROTO_ICMP ) & BST_CHECKED) i|=(1<<PROTO_ICMP);
			if(IsDlgButtonChecked( hwndDlg, IDC_PROTO_UDP ) & BST_CHECKED) i|=(1<<PROTO_UDP);
			if(IsDlgButtonChecked( hwndDlg, IDC_PROTO_TCP ) & BST_CHECKED) i|=(1<<PROTO_TCP);
			protos = i;

			//fill socks from dialog
			GetDlgItemText( hwndDlg, IDC_SOCKSHOST, (char *)socksinfo.hostname, MAX_HOSTLEN );
			socksinfo.port = GetDlgItemInt( hwndDlg, IDC_SOCKSPORT, NULL, FALSE );
			if( !socksinfo.port ) {
				socksinfo.port = 1080;
			}
			GetDlgItemText( hwndDlg, IDC_SOCKSUSER, (char *)socksinfo.username, MAX_HOSTLEN );
			GetDlgItemText( hwndDlg, IDC_SOCKSPASS, (char *)socksinfo.password, MAX_HOSTLEN );

			//fill nat from dialog
			GetDlgItemText( hwndDlg, IDC_OPT_NAT_INT, (char *)natinfo.internal, MAX_HOSTLEN );
			GetDlgItemText( hwndDlg, IDC_OPT_NAT_EXT, (char *)natinfo.external, MAX_HOSTLEN );

			SaveAdvCfgPageOptions();

			//remove status window
			if( hwndStat ) {
				if( enabled ) {
					StartupTun();
				}
				DestroyWindow( hwndStat );
			}

			EndDialog( hwndDlg, wParam );
			break;

		case IDCANCEL:
			EndDialog( hwndDlg, wParam );
			break;

		default:
			break;
		}
		return TRUE;
	}
}

int CALLBACK DialogProcStatus(HWND hwndDlg,UINT uMsg,WPARAM wParam,LPARAM lParam)
{
	switch( uMsg ) {
	default:
		return FALSE;

	case WM_INITDIALOG:
		return TRUE;

	case WM_COMMAND:
		switch( wParam ) {
		case IDOK:
		case IDCANCEL:
			break;

		default:
			break;
		}
		return TRUE;
	}
}

int CALLBACK DialogProcConnStat(HWND hwndDlg,UINT uMsg,WPARAM wParam,LPARAM lParam)
{
	switch( uMsg ) {
	default:
		return FALSE;

	case WM_INITDIALOG:
		SendMessage(hwndDlg, WM_SETICON, ICON_SMALL, (LPARAM)smiconon);
		SendMessage(hwndDlg, WM_SETICON, ICON_BIG, (LPARAM)lgiconon);

		SetDlgItemText( hwndDlg, IDC_CONNTEXT, MakeStatusString() );
		return TRUE;

	case WM_COMMAND:
		switch( wParam ) {
		case IDC_REFRESH:
		case IDC_HWSRCLIST:
			if( IsDlgButtonChecked( hwndDlg, IDC_HWSRCLIST) ) {
				SetDlgItemText( hwndDlg, IDC_CONNTEXT, MakeHWSrcListString() );
			} else {
				SetDlgItemText( hwndDlg, IDC_CONNTEXT, MakeStatusString() );
			}
			break;

		case IDOK:
		case IDCANCEL:
			EndDialog( hwndDlg, wParam );
			break;

		default:
			break;
		}
		return TRUE;
	}
}

void SetTrayIcon(void)
{
	switch(state) {
	default:
	case DISABLED:
		nid.hIcon = smiconoff;
		strcpy(nid.szTip, GIT_NAME " - [DISABLED]");
		break;
	case STARTUP:
		nid.hIcon = smiconup;
		strcpy(nid.szTip, GIT_NAME " - [STARTING UP]");
		break;
	case ENABLED:
		nid.hIcon = smiconon;
		strcpy(nid.szTip, GIT_NAME " - [ENABLED]");
		break;
	case SHUTDOWN:
		nid.hIcon = smicondn;
		strcpy(nid.szTip, GIT_NAME " - [SHUTTING DOWN]");
		break;
	}

	Shell_NotifyIcon(NIM_MODIFY,&nid);
}

void SetAutorun(int ar)
{
	HKEY key;
	MENUITEMINFO mii;
	char buf[1024];
	int id;

	mii.cbSize = sizeof(mii);
	mii.fMask = MIIM_STATE;

	mii.fState = MFS_UNCHECKED;
	SetMenuItemInfo(popup,ID_TRAY_AUTORUN_DONT,FALSE,&mii);
	SetMenuItemInfo(popup,ID_TRAY_AUTORUN_TRAY,FALSE,&mii);
	SetMenuItemInfo(popup,ID_TRAY_AUTORUN_HIDDEN,FALSE,&mii);

	mii.fState = MFS_CHECKED;
	switch(autorun = ar) {
	default:
	case 0:
		id = ID_TRAY_AUTORUN_DONT;
		if(!RegOpenKey(HKEY_CURRENT_USER,"Software\\Microsoft\\Windows\\CurrentVersion\\Run",&key)) {
			RegDeleteValue(key,GIT_NAME);
			RegCloseKey(key);
		}
		break;
	case 1:
		id = ID_TRAY_AUTORUN_TRAY;
		if(!RegOpenKey(HKEY_CURRENT_USER,"Software\\Microsoft\\Windows\\CurrentVersion\\Run",&key)) {
			RegSetValueEx(key,GIT_NAME,0,REG_SZ,(LPBYTE)g_pathexe,strlen(g_pathexe)+1);
			RegCloseKey(key);
		}
		break;
	case 2:
		id = ID_TRAY_AUTORUN_HIDDEN;
		if(!RegOpenKey(HKEY_CURRENT_USER,"Software\\Microsoft\\Windows\\CurrentVersion\\Run",&key)) {
			strcpy(buf,g_pathexe);
			strcat(buf," -hide");
			RegSetValueEx(key,GIT_NAME,0,REG_SZ,(LPBYTE)buf,strlen(buf)+1);
			RegCloseKey(key);
		}
		break;
	}
	SetMenuItemInfo(popup,id,FALSE,&mii);
	DrawMenuBar(hwnd);
}

void SetEnabled(int en)
{
	MENUITEMINFO mii;

	mii.cbSize = sizeof(mii);
	mii.fMask = MIIM_STATE;

	enabled = en;

	if(state==DISABLED && enabled) {
		StartupTun();
	}

	if(state==ENABLED && !enabled) {
		ShutdownTun();
	}

	if(enabled) {
		mii.fState = MFS_CHECKED;
	} else {
		mii.fState = MFS_UNCHECKED;
	}

	SetMenuItemInfo(popup,ID_TRAY_ENABLED,FALSE,&mii);
	SetTrayIcon();
	DrawMenuBar(hwnd);
}

void SetOptions(int op)
{
	HKEY key;
	MENUITEMINFO mii;
	DWORD dw;

	mii.cbSize = sizeof(mii);
	mii.fMask = MIIM_STATE;
	options = op;

	mii.fState = (options & OPT_LOG_IN) ? MFS_CHECKED : MFS_UNCHECKED;
	SetMenuItemInfo(popup,ID_TRAY_LOG_IN,FALSE,&mii);

	mii.fState = (options & OPT_LOG_FWD) ? MFS_CHECKED : MFS_UNCHECKED;
	SetMenuItemInfo(popup,ID_TRAY_LOG_FWD,FALSE,&mii);

	mii.fState = (options & OPT_LOG_UNFWD) ? MFS_CHECKED : MFS_UNCHECKED;
	SetMenuItemInfo(popup,ID_TRAY_LOG_UNFWD,FALSE,&mii);

	mii.fState = (options & OPT_LOG_ADDR) ? MFS_CHECKED : MFS_UNCHECKED;
	SetMenuItemInfo(popup,ID_TRAY_LOG_ADDR,FALSE,&mii);

	if(!RegCreateKey(HKEY_CURRENT_USER,"Software\\Git",&key)) {
		dw = options;
		RegSetValueEx(key,"options",0,REG_DWORD,(LPBYTE)&dw,sizeof(dw));
		RegCloseKey(key);
	}

	DrawMenuBar(hwnd);
}

void SaveConfigPageOptions(void)
{
	char buf[MAX_HOSTLEN+64];
	HKEY key;
	unsigned i;
	DWORD dw;

	if(!RegCreateKey(HKEY_CURRENT_USER,"Software\\Git",&key)) {

		//save hosts to registry
		i = numhosts;
		RegSetValueEx(key,"numhosts",0,REG_DWORD,(LPBYTE)&i,sizeof(i));
		for( i=0; i<numhosts; i++ ) {
			sprintf(buf,"host%dport",i);
			dw = hosts[i].port;
			RegSetValueEx(key,buf,0,REG_DWORD,(LPBYTE)&dw,sizeof(dw));
			sprintf(buf,"host%dmethod",i);
			dw = hosts[i].method;
			RegSetValueEx(key,buf,0,REG_DWORD,(LPBYTE)&dw,sizeof(dw));
			sprintf(buf,"host%dname",i);
			RegSetValueEx(key,buf,0,REG_SZ,hosts[i].name,strlen((char *)hosts[i].name));
		}

		//save multi stuff
		i = multi.port;
		RegSetValueEx(key,"multiport",0,REG_DWORD,(LPBYTE)&i,sizeof(i));
		i = multi.maxhosts;
		RegSetValueEx(key,"multimax",0,REG_DWORD,(LPBYTE)&i,sizeof(i));

		//save sockets to registry
		i = numsockets;
		RegSetValueEx(key,"numsockets",0,REG_DWORD,(LPBYTE)&i,sizeof(i));
		for( i=0; i<numsockets; i++ ) {
			sprintf(buf,"socket%dmin",i);
			dw = sockets[i].min;
			RegSetValueEx(key,buf,0,REG_DWORD,(LPBYTE)&dw,sizeof(dw));
			sprintf(buf,"socket%dmax",i);
			dw = sockets[i].max;
			RegSetValueEx(key,buf,0,REG_DWORD,(LPBYTE)&dw,sizeof(dw));
			sprintf(buf,"socket%ddesc",i);
			RegSetValueEx(key,buf,0,REG_SZ,sockets[i].desc,strlen((char *)sockets[i].desc));
		}

		//save ports to registry
		i = numports;
		RegSetValueEx(key,"numports",0,REG_DWORD,(LPBYTE)&i,sizeof(i));
		for( i=0; i<numports; i++ ) {
			sprintf(buf,"port%dmin",i);
			dw = ports[i].min;
			RegSetValueEx(key,buf,0,REG_DWORD,(LPBYTE)&dw,sizeof(dw));
			sprintf(buf,"port%dmax",i);
			dw = ports[i].max;
			RegSetValueEx(key,buf,0,REG_DWORD,(LPBYTE)&dw,sizeof(dw));
			sprintf(buf,"port%ddesc",i);
			RegSetValueEx(key,buf,0,REG_SZ,ports[i].desc,strlen((char *)ports[i].desc));
		}

		RegCloseKey(key);
	}
}

void SaveAdvCfgPageOptions(void)
{
	char buf[MAX_HOSTLEN];
	HKEY key;
	unsigned i;

	if(!RegCreateKey(HKEY_CURRENT_USER,"Software\\Git",&key)) {

		//save device to registry
		i = device;
		RegSetValueEx(key,"device",0,REG_DWORD,(LPBYTE)&i,sizeof(i));

		//frames already saved to registry from SetOptions()

		//save packets to registry
		i = packets;
		RegSetValueEx(key,"packets",0,REG_DWORD,(LPBYTE)&i,sizeof(i));

		//save protos to registry
		i = protos;
		RegSetValueEx(key,"protos",0,REG_DWORD,(LPBYTE)&i,sizeof(i));

		//save socks to registry
		RegSetValueEx(key,"sockshost",0,REG_SZ,socksinfo.hostname,strlen((char *)socksinfo.hostname));
		i = socksinfo.port;
		RegSetValueEx(key,"socksport",0,REG_DWORD,(LPBYTE)&i,sizeof(i));
		RegSetValueEx(key,"socksuser",0,REG_SZ,socksinfo.username,strlen((char *)socksinfo.username));
		for( i=0; i<strlen((char *)socksinfo.password); i++ ) {
			buf[i] = socksinfo.password[i] ^ 0xAA ^ ((i<<5)&0xFF) ^ strlen((char *)socksinfo.password);
		}
		RegSetValueEx(key,"sockspass",0,REG_BINARY,(LPBYTE)buf,strlen((char *)socksinfo.password));

		//save nat to registry
		RegSetValueEx(key,"natinternal",0,REG_SZ,natinfo.internal,strlen((char *)natinfo.internal));
		RegSetValueEx(key,"natexternal",0,REG_SZ,natinfo.external,strlen((char *)natinfo.external));

		RegCloseKey(key);
	}
}

void StartupTun(void)
{
//PT	pthread_t tid;
	DWORD tid;

	if(state!=DISABLED||!enabled) return;
	state = STARTUP;
	SetTrayIcon();

//PT	pthread_create( &tid, NULL, eth2tun, NULL );
	CreateThread(NULL, 0, eth2tun, NULL, 0, &tid);
//PT	pthread_create( &tid, NULL, tun2eth, NULL );
	CreateThread(NULL, 0, tun2eth, NULL, 0, &tid);
	while(!eth2tun_enabled || !tun2eth_enabled) {
		Sleep(100);
	}

	state = ENABLED;
	SetTrayIcon();
}

void ShutdownTun(void)
{
	if(state!=ENABLED) return;
	state = SHUTDOWN;
	SetTrayIcon();

	//lock their mutex's so we wait for them to shutdown
	//after seeing their enabled set to 0
	eth2tun_enabled = 0;
	tun2eth_enabled = 0;
	EnterCriticalSection(&eth2tun_mutex);
	LeaveCriticalSection(&eth2tun_mutex);
	EnterCriticalSection(&tun2eth_mutex);
	LeaveCriticalSection(&tun2eth_mutex);

	state = DISABLED;
	SetTrayIcon();
}

char *MakeStatusString(void)
{
	//hostname+256bytes for msgs/times per host
	static char buf[MAX_HOSTS*(MAX_HOSTLEN+256)],*p;
	unsigned int i;

	p=buf;
	for( i=0; i<numhosts; i++ ) {

		sprintf(p,"%s:%d (",hosts[i].name,hosts[i].port);
		p += strlen(p);

		switch(hosts[i].method) {
		case METH_UDP:
			sprintf(p,"udp");
			break;
		case METH_TCP_LISTEN:
			sprintf(p,"tcp listen");
			break;
		case METH_TCP_CONNECT:
			sprintf(p,"tcp connect");
			break;
		case METH_TCP_CONNECT_SOCKS4:
			sprintf(p,"tcp connect via socks4");
			break;
		case METH_TCP_CONNECT_SOCKS5:
			sprintf(p,"tcp connect via socks5");
			break;
		case METH_TCP_MULTI_LISTEN:
			sprintf(p,"tcp multi-listen");
			break;
		}
		p += strlen(p);
		sprintf(p,") - ");
		p += strlen(p);

		switch(hosts[i].state) {
		case FH_STATE_DOWN:
			sprintf(p,"DOWN since %s",ctime2(&hosts[i].disconnected));
			break;

		case FH_STATE_CONNECTING:
			switch(hosts[i].method) {
			case METH_UDP:
				sprintf(p,"WAITING");
				break;
			case METH_TCP_LISTEN:
			case METH_TCP_MULTI_LISTEN:
				sprintf(p,"LISTENING");
				break;
			default:
				sprintf(p,"CONNECTING");
				break;
			}
			p += strlen(p);
			sprintf(p," since %s",ctime2(&hosts[i].disconnected));
			break;

		case FH_STATE_FAILED:
			sprintf(p,"FAILED (");
			p += strlen(p);
			switch(hosts[i].error) {
			default: sprintf(p,"unknown error"); break;
			case FH_FAIL_SOCKET: sprintf(p,"could not create socket"); break;
			case FH_FAIL_BIND: sprintf(p,"could not bind to port"); break;
			case FH_FAIL_ACCEPT: sprintf(p,"failed to accept"); break;
			case FH_FAIL_CONNECT: sprintf(p,"could not connect to host"); break;
			case FH_FAIL_CONNECT_SOCKS: sprintf(p,"could not connect to socks server"); break;
			case FH_FAIL_SOCKS_CONNECT: sprintf(p,"socks server failed or could not connect to host"); break;
			case FH_FAIL_SOCKS_IDENT: sprintf(p,"socks server reported bad user/pass/ident"); break;
			case FH_FAIL_WRONGADDR: sprintf(p,"connection from wrong ip (%s)",inet_ntoa(hosts[i].from)); break;
			case FH_FAIL_RECV: sprintf(p,"receive failed"); break;
			case FH_FAIL_SEND: sprintf(p,"send failed"); break;
			}
			p += strlen(p);
			sprintf(p,") retrying...");
			if( hosts[i].connected ) {
				p += strlen(p);
				sprintf(p,"\r\n\tlast connected at %s",ctime2(&hosts[i].connected));
			}
			if( hosts[i].disconnected ) {
				p += strlen(p);
				sprintf(p,"\r\n\tdisconnected at %s",ctime2(&hosts[i].disconnected));
			}
			break;

		case FH_STATE_UP:
			sprintf(p,"UP since %s",ctime2(&hosts[i].connected));
			p += strlen(p);
			switch(hosts[i].method) {
			case METH_UDP:
				if( hosts[i].from.s_addr ) {
					sprintf(p,"\r\n\tlast packet received from %s",inet_ntoa(hosts[i].from));
				}
				break;
			case METH_TCP_LISTEN:
			case METH_TCP_MULTI_LISTEN:
				sprintf(p,"\r\n\taccepted connection from %s",inet_ntoa(hosts[i].from));
				break;
			default:
				break;
			}
			break;
		}
		p += strlen(p);
		sprintf(p,"\r\n");
		p += strlen(p);
		if( hosts[i].lastfwd ) {
			sprintf(p,"\tlast packet forwarded at %s\r\n",ctime2(&hosts[i].lastfwd));
			p += strlen(p);
		}
		if( hosts[i].lastrecv ) {
			sprintf(p,"\tlast packet received at %s\r\n",ctime2(&hosts[i].lastrecv));
			p += strlen(p);
		}
		sprintf(p,"\r\n");
		p += strlen(p);
	}
	if( !numhosts ) {
		sprintf(p,"No connections defined, go to Configuration\r\n");
	}

	return buf;
}

char *MakeHWSrcListString(void)
{
	static char buf[(MAX_SRC_HW_INFO * 24)+1];
	char *p = buf;
	int n = 0;
	time_t now;
	time(&now);
	EnterCriticalSection(&hwsrclist_mutex);
	sprintf(p, "List is empty");
	while( n < MAX_SRC_HW_INFO ) {
		if( hw_src_addr_info[n].expire_time > now ) {
			sprintf(p,"%02X:%02X:%02X:%02X:%02X:%02X - %d\r\n", hw_src_addr_info[n].addr[0], hw_src_addr_info[n].addr[1], hw_src_addr_info[n].addr[2],
				hw_src_addr_info[n].addr[3], hw_src_addr_info[n].addr[4], hw_src_addr_info[n].addr[5],
				hw_src_addr_info[n].expire_time - now);
			p += strlen(p);
		}
		n++;
	}
	LeaveCriticalSection(&hwsrclist_mutex);
	return buf;
}

char *ctime2(const time_t *timer)
{
	static char buf[26];

	if(timer&&*timer) {
		strcpy(buf,ctime(timer));
		buf[24] = '\0';
	} else {
		strcpy(buf,"forever");
	}
	return buf;
}

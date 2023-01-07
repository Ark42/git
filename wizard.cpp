/*--------------------------------------------------------------------------*\
|| wizard.cpp by Ryan Rubley <ryan@morpheussoftware.net>                    ||
||                                                                          ||
|| (v0.91) 8-25-2001                                                        ||
|| (v0.95) 8-04-2002                                                        ||
|| (v0.96) 8-17-2002                                                        ||
|| (v0.97) 3-09-2004                                                        ||
|| (v0.98) 9-12-2004                                                        ||
|| (v0.99) 9-12-2004                                                        ||
|| Part of Gamer's Internet Tunnel                                          ||
|| Configuration Wizard                                                     ||
||                                                                          ||
\*--------------------------------------------------------------------------*/

//was 108K exe in 0.98
#include "git.h"
#include "wizard.h"
#include "utils.h"
#include "packet.h"
#include "resource.h"

#pragma warning(disable:4786) //identifier was truncated to '255' characters in the browser information
#include <fstream>
#include <sstream>
#include <iomanip>
#include <string>
#include <vector>
#include <map>
#include <algorithm>
using namespace std;

#define DWORDALIGN(p) (LPWORD)((UINT)(p)+3&~3)
#define WORD(w) if(pass)*pWord++=(w);else size+=2;
#define STRING(s) if(pass)pWord+=MultiByteToWideChar(CP_ACP,0,(s),-1,(LPWSTR)pWord,(strlen(s)+1)*2);else size+=(strlen(s)+1)*2;
#define DLGITEM(ix,iy,icx,icy,iid,istyle) if(pass){pItem=(LPDLGITEMTEMPLATE)DWORDALIGN(pWord);pItem->x=(ix);pItem->y=(iy);pItem->cx=(icx);pItem->cy=(icy);pItem->id=(iid);pItem->style=WS_CHILD|WS_VISIBLE|(istyle);pWord=(LPWORD)(pItem+1);}else{size=size+3&~3;size+=sizeof(DLGITEMTEMPLATE);controls++;}
#define BUTTON() WORD(0xFFFF)WORD(0x0080)
#define EDIT() WORD(0xFFFF)WORD(0x0081)
#define STATIC() WORD(0xFFFF)WORD(0x0082)
#define LISTBOX() WORD(0xFFFF)WORD(0x0083)
#define SCROLLBAR() WORD(0xFFFF)WORD(0x0084)
#define COMBOBOX() WORD(0xFFFF)WORD(0x0085)

#define ID_HEADING			30
#define ID_SUBHEADING		31
#define ID_ICON				32
#define ID_AUTOCONF			33
#define ID_SAVEPROF			34
#define ID_IPFWNAT			35
#define ID_USERBASE			50
#define MAX_WIZARD_LINES	36

#define VARTYPE_INT			0
#define VARTYPE_STRING		1
#define VARTYPE_VAR			2
#define VARTYPE_SYM			3

#define PARAM1V() if( wizcmd.GetParam(1).GetType() != VARTYPE_VAR ) err = "Parameter 1 is missing or invalid"; else
#define PARAM2V() PARAM1V() if( GetDerefType(wizcmd.GetParam(2)) < 0 ) err = "Parameter 2 is missing or invalid"; else
#define PARAM3V() PARAM2V() if( GetDerefType(wizcmd.GetParam(3)) < 0 ) err = "Parameter 3 is missing or invalid"; else
#define PARAM4V() PARAM3V() if( GetDerefType(wizcmd.GetParam(4)) < 0 ) err = "Parameter 4 is missing or invalid"; else
#define PARAMSYM() if( GetDerefType(wizcmd.GetParam(1)) < 0 ) err = "Parameter 1 is missing or invalid"; else \
					if( GetDerefType(wizcmd.GetParam(3)) < 0 ) err = "Parameter 3 is missing or invalid"; else
#define PARAMSYMV() PARAM1V() if( GetDerefType(wizcmd.GetParam(2)) < 0 ) err = "Parameter 2 is missing or invalid"; else \
					if( GetDerefType(wizcmd.GetParam(4)) < 0 ) err = "Parameter 4 is missing or invalid"; else

HFONT boldFont = NULL;

int ShowPage(int next = 1);
int CALLBACK DialogProcWizard(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam);
void split(string &text, string &separators, vector<string> &pieces);
void join(vector<string> &pieces, string &separators, string &text);
void trim(string &text);
void cleanvarname(string &var);

//a variable, can be int or string
class WizVar
{
public:
	WizVar() { SetString(""); }
	WizVar(int val) { SetInt(val); }
	WizVar(string val) { SetString(val); }

	int GetType() { return type; }
	void SetInt(int val) { type = VARTYPE_INT; intval = val; strval.erase(); }
	void SetString(string val) { type = VARTYPE_STRING; intval = 0; strval = val; }
	int GetInt(void) { return type==VARTYPE_INT?intval:atoi(strval.c_str()); }
	string GetString(void) { if(type==VARTYPE_INT) { stringstream ss; ss << intval; return ss.str(); } return strval; }

	operator =(int val) { SetInt(val); }
	operator =(string val) { SetString(val); }
	operator =(char *val) { SetString(val); }
	operator =(const char *val) { SetString(val); }
	operator int(void) { return GetInt(); }
	operator string(void) { return GetString(); }
	operator const char *(void) { return GetString().c_str(); }

	void PrependString(string val) { SetString(val + GetString()); }
	void AppendString(string val) { SetString(GetString() + val); }

	int NumLines(void) { string str = GetString(); vector<string> pieces; split(str, string("\r\n"), pieces);
							return pieces.size();
						}
	string GetLine(int num = 0) { string str = GetString(); vector<string> pieces; split(str, string("\r\n"), pieces);
									return num>=0&&num<pieces.size() ? pieces[num] : "";
								}
	void SetLine(int num, string val) { string str = GetString(); vector<string> pieces; split(str, string("\r\n"), pieces);
										if( num>=0&&num<pieces.size() ) pieces[num] = val; else pieces.push_back(val);
										join(pieces, string("\r\n"), str); SetString(str);
									}
	void AddLine(string val) { SetString(GetString() + "\r\n" + val); }
	void DeleteLine(int num = 0) { string str = GetString(); vector<string> pieces; split(str, string("\r\n"), pieces);
									if( num>=0&&num<pieces.size() ) pieces.erase(pieces.begin() + num);
									join(pieces, string("\r\n"), str); SetString(str);
								}
	void SortLines(void) { string str = GetString(); vector<string> pieces; split(str, string("\r\n"), pieces);
							sort(pieces.begin(), pieces.end()); join(pieces, string("\r\n"), str); SetString(str);
						}
	void SplitLines(string separators) { string str = GetString(); vector<string> pieces; split(str, separators, pieces);
									join(pieces, string("\r\n"), str); SetString(str);
								}
	void JoinLines(string glue) { string str = GetString(); vector<string> pieces; split(str, string("\r\n"), pieces);
									join(pieces, glue, str); SetString(str);
								}
	void RemoveBlankLines() { string str = GetString(); vector<string> pieces; split(str, string("\r\n"), pieces);
								for(int i=0;i<pieces.size();i++) if(pieces[i].length()<=0) pieces.erase(pieces.begin()+i--);
								join(pieces, string("\r\n"), str); SetString(str);
							}

protected:
	int type;
	int intval;
	string strval;
};

//a command parameter, can be int, string, variable name, or a symbol
class WizParam: public WizVar
{
public:
	WizParam() : WizVar() { type = -1; }
	WizParam(int val) : WizVar(val) { }
	WizParam(string val) : WizVar(val) { }

	void SetVar(string val) { type = VARTYPE_VAR; intval = 0; cleanvarname(val); strval = val; }
	void SetSym(string val) { type = VARTYPE_SYM; intval = 0; strval = val; }
	string GetVar(void) { return GetString(); }
	string GetSym(void) { return GetString(); }
};

//a command (one line from the script file)
class WizCmd
{
public:
	WizCmd() { linenum = -1; }
	void ClearParams() { param.clear(); }
	void AddParam(WizParam val) { param.push_back(val); }
	int GetNumParams(void) { return param.size(); }
	WizParam GetParam(int num) { return num>=0&&num<param.size()?param[num]:WizParam(); }
	string GetCmd(void) { if(param.size() <= 0) return ""; if(param[0].GetType() != VARTYPE_VAR) return ""; return param[0].GetVar(); }
	void SetLineNum(int num) { linenum = num; }
	int GetLineNum(void) { return linenum; }

protected:
	string cmd;
	vector<WizParam> param;
	int linenum;
};

//a line from the 'control' variable
class WizCtrl
{
public:
	WizCtrl() { size = 1; id = 0; }
	WizCtrl(string val) { SetFromLine(val); id = 0; }
	void SetFromLine(string val) { int i; vector<string> pieces; split(val, string("|"), pieces);
								for( i = 0; i < pieces.size(); i++ ) trim(pieces[i]);
								ctrl = pieces.size() > 0 ? pieces[0] : "";
								var = pieces.size() > 1 ? pieces[1] : ""; cleanvarname(var); if( var.length() < 1 ) var = "#";
								text = pieces.size() > 2 ? pieces[2] : "";
								size = pieces.size() > 3 ? atoi(pieces[3].c_str()) : 1; if( size < 1 ) size = 1;
								choices.clear(); for( i = 4; i < pieces.size(); i++ ) choices.push_back(pieces[i]);
							}
	void SetID(int val) { id = val; }
	int GetID(void) { return id; }
	string GetCtrl(void) { return ctrl; }
	string GetVar(void) { return var; }
	string GetText(void) { return text; }
	int GetSize(void) { return size; }
	int GetNumChoices(void) { return choices.size(); }
	string GetChoice(int num) { return num>=0&&num<choices.size()?choices[num]:""; }

protected:
	string ctrl, var, text;
	int size, id;
	vector<string> choices;
};
int GetDerefType(WizParam &param);
int GetDerefInt(WizParam &param);
string GetDerefString(WizParam &param);
void TestCmd(WizCmd &wizcmd, bool &test, string &err);
void SetConfigVars(void);
void SetFlagVars(void);
void SaveConfigVars(void);

//this is the list of commands
vector<WizCmd> g_WizCmds;
//this is the global variable table
map<string, WizVar> g_WizVars;
//this is the list of controls on the current page
vector<WizCtrl> g_WizCtrls;

void RunWizard(char *filename)
{
start_over:
	int i;
	g_WizCmds.clear();
	g_WizVars.clear();
	g_WizCtrls.clear();

	SetConfigVars();
	SetFlagVars();

	string scriptname, scriptfile;
	if( filename == NULL ) {
		char buf[MAX_PATH];
		strcpy(buf, g_path);
		strcat(buf, "*.git");
		HANDLE hFF;
		WIN32_FIND_DATA ffd;
		if( (hFF = FindFirstFile(buf, &ffd)) != INVALID_HANDLE_VALUE ) {
			BOOL bOK = TRUE;
			while( bOK ) {
				if( !(ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) ) {
					string script(ffd.cFileName);
					script = script.substr(0, script.length() - 4);
					g_WizVars["scripts"].AddLine(script);
				}
				bOK = FindNextFile(hFF, &ffd);
			}
			FindClose(hFF);
		}
		g_WizVars["scripts"].DeleteLine();
		g_WizVars["scripts"].SortLines();
		g_WizVars["scriptcontrol"] = g_WizVars["scripts"];
		g_WizVars["scriptcontrol"].JoinLines("|");
		g_WizVars["scriptcontrol"].PrependString("dropd|script|Select the program to configure GIT for:||");
	} else {
		scriptfile = filename;
		if( scriptfile[0] == '"' ) scriptfile = scriptfile.substr(1, scriptfile.length() - 2);
		scriptname = scriptfile;
		int pos;
		if( (pos = scriptname.rfind("\\")) != string::npos ) scriptname.erase(0, pos + 1);
		if( (pos = scriptname.rfind(".")) != string::npos ) scriptname.erase(pos);
		g_WizVars["scripts"] = scriptname;
		g_WizVars["script"] = 0;
		g_WizVars["scriptcontrol"] = "label||GIT will be configured for the program:";
		g_WizVars["scriptcontrol"].AddLine("label||" + scriptname);
		g_WizVars["scriptcontrol"].AddLine("space");
	}

	for( i = 0; i < AdapterCount; i++ ) {
		char buf[1024];
		if( i < MAX_ADAPTERS ) {
			sprintf(buf, "%d - %.64s (%.128s)", i, AdapterListNoGUID(i), AdapterDesc[i]);
		} else {
			sprintf(buf, "%d", i);
		}
		g_WizVars["devices"].AddLine(buf);
	}
	g_WizVars["devices"].DeleteLine();
	g_WizVars["devicecontrol"] = g_WizVars["devices"];
	g_WizVars["devicecontrol"].JoinLines("|");
	g_WizVars["devicecontrol"].PrependString("dropd|device|Verify that your network packet device is selected:||");

	g_WizVars["heading"] = "Program and Network Device";
	g_WizVars["subheading"] = "Please select the program to configure GIT for and make sure your network packet device is selected.";
	g_WizVars["control"] = g_WizVars["scriptcontrol"];
	g_WizVars["control"].AddLine("label||Use auto configure to bypass the rest of the wizard, if possible, using values stored from the previous time GIT was configured for the program selected.|2");
	g_WizVars["control"].AddLine("space|||3");
	g_WizVars["control"].AddLine(g_WizVars["devicecontrol"]);
	g_WizVars["control"].AddLine("space|||2");
	g_WizVars["control"].AddLine("hline");
	g_WizVars["control"].AddLine("space");
	g_WizVars["control"].AddLine("text1|filename|Or, enter a profile name to save the current settings to:");
	g_WizVars["control"].AddLine("check|savehost|Save host list to profile.");
	g_WizVars["control"].AddLine("specl||autoconf");
	g_WizVars["control"].AddLine("specl||saveprof");

	int done; //which button user presses when script is complete
	bool autoconf = false; //if a loadvar fails, this becomes false
	if( (done = ShowPage(-1)) == IDCANCEL ) {
		return;
	}
	if( done == ID_AUTOCONF ) {
		autoconf = true;
		done = IDOK;
	}

	if( filename == NULL ) {
		scriptname = g_WizVars["scripts"].GetLine(g_WizVars["script"]);
		scriptfile = string(g_path) + scriptname + ".git";
	}

	g_WizVars["scriptname"] = scriptname;
	//g_WizVars["heading"] = scriptname;
	//g_WizVars["subheading"] = "Configure your settings for ";
	//g_WizVars["subheading"].AppendString(g_WizVars["heading"]);
	//g_WizVars["subheading"].AppendString(" below.");

	if( scriptname.length() <= 0 ) {
		MessageBox(NULL, "No script selected.", GIT_NAME, MB_ICONWARNING);
		return;
	}
	ifstream file(scriptfile.c_str());
	if( !file ) {
		scriptfile = "Unable to open script: " + scriptfile;
		MessageBox(NULL, scriptfile.c_str(), GIT_NAME, MB_ICONWARNING);
		return;
	}
	string line;
	int linenum = 1;
	//parse script lines
	while( getline(file, line) ) {
		//remove any garbage newline pieces from the end of the line
		while( line[line.length() - 1] == '\r' || line[line.length() - 1] == '\n' ) {
			line = line.substr(0, line.length() - 1);
		}

		WizCmd wizcmd;
		WizParam param;
		int instring = 0, inwhite = 1;
		const char *p = line.c_str(), *s;
		//parse one line
		for(;;) {
			if( instring ) {
				if( *p == '"' ) {
					if( p[1] == '"' ) {
						p++;
					} else {
						string str(s, p - s);
						int pos = str.find("\"\"");
						while( pos != string::npos ) {
							str.replace(pos, 2, "\"");
							pos = str.find("\"\"", pos + 1);
						}
						param.SetString(str);
						wizcmd.AddParam(param);
						instring = 0;
						inwhite = 1;
					}
				}
				if( *p == '\0' ) {
					break;
				}
			} else {
				if( inwhite ) {
					if( *p != ' ' && *p != '\t' && *p != '\r' && *p != '\n' ) {
						s = p;
						inwhite = 0;
						if( *s == '"' ) {
							s++;
							instring = 1;
						}
					}
				} else {
					if( *p == ' ' || *p == '\t' || *p == '\r' || *p == '\n' || *p == '#' || *p == '\0' ) {
						string str(s, p - s);
						if( str.length() > 0 ) {
							if( (str[0] >= 'A' && str[0] <= 'Z') || (str[0] >= 'a' && str[0] <= 'z') || str[0] == '_' ) {
								param.SetVar(str);
							} else if( str[0] >= '0' && str[0] <= '9' ) {
								if( str.length() >= 2 && str[0] == '0' && str[1] == 'x' ) {
									param.SetInt(strtol(str.c_str() + 2, NULL, 16));
								} else {
									param.SetInt(atoi(str.c_str()));
								}
							} else if( str.length() >= 2 && str[0] == '-' && str[1] >= '0' && str[1] <= '9' ) {
								param.SetInt(atoi(str.c_str()));
							} else {
								param.SetSym(str);
							}
							wizcmd.AddParam(param);
						}
						inwhite = 1;
					}
				}
				if( *p == '#' || *p == '\0' ) {
					break;
				}
			}
			p++;
		}
		//done parsing line

		if( wizcmd.GetCmd().length() > 0 ) {
			wizcmd.SetLineNum(linenum);
			g_WizCmds.push_back(wizcmd);
		}
		linenum++;
	}
	file.close();
	//done parsing

	//implied finish at end
	WizCmd wizcmd;
	WizParam param;
	param.SetVar("finish");
	wizcmd.AddParam(param);
	g_WizCmds.push_back(wizcmd);

	vector<int> whilelocs; //positions to jump back to for while/endwhile
	vector<int> backlocs; //positions to jump back to for back button
	backlocs.push_back(-1);
	map<string, WizVar> savevars; //list of vars requested to be saved, only done if script not canceled
	//run script
	for( i = 0; i < g_WizCmds.size(); i++ ) {
		WizCmd &wizcmd = g_WizCmds[i];
		string cmd = wizcmd.GetCmd();
		string err;
		if( cmd == "setvar" ) {
			PARAM2V() {
				if( GetDerefType(wizcmd.GetParam(2)) == VARTYPE_INT ) {
					g_WizVars[wizcmd.GetParam(1).GetVar()] = GetDerefInt(wizcmd.GetParam(2));
				} else {
					g_WizVars[wizcmd.GetParam(1).GetVar()] = GetDerefString(wizcmd.GetParam(2));
				}
			}
		} else if( cmd == "math" ) {
			PARAMSYMV() {
				int val, left = GetDerefInt(wizcmd.GetParam(2)),
					right = GetDerefInt(wizcmd.GetParam(4));
				string op = wizcmd.GetParam(3).GetSym();
				bool ok = true;
				if( op == "+" ) val = left + right;
				else if( op == "-" ) val = left - right;
				else if( op == "*" ) val = left * right;
				else if( op == "/" ) val = right == 0 ? 0 : left / right;
				else if( op == "%" ) val = left % right;
				else { err = "Unknown operator"; ok = false; }
				if( ok ) g_WizVars[wizcmd.GetParam(1).GetVar()] = val;
			}
		} else if( cmd == "addflag" ) {
			PARAM2V() {
				g_WizVars[wizcmd.GetParam(1).GetVar()] = GetDerefInt(wizcmd.GetParam(1)) | GetDerefInt(wizcmd.GetParam(2));
			}
		} else if( cmd == "removeflag" ) {
			PARAM2V() {
				g_WizVars[wizcmd.GetParam(1).GetVar()] = GetDerefInt(wizcmd.GetParam(1)) &~ GetDerefInt(wizcmd.GetParam(2));
			}
		} else if( cmd == "prependstring" ) {
			PARAM2V() {
				g_WizVars[wizcmd.GetParam(1).GetVar()].PrependString(GetDerefString(wizcmd.GetParam(2)));
			}
		} else if( cmd == "appendstring" ) {
			PARAM2V() {
				g_WizVars[wizcmd.GetParam(1).GetVar()].AppendString(GetDerefString(wizcmd.GetParam(2)));
			}
		} else if( cmd == "findstring" ) {
			PARAM4V() {
				g_WizVars[wizcmd.GetParam(1).GetVar()] = GetDerefString(wizcmd.GetParam(2)).find(GetDerefString(wizcmd.GetParam(3)), GetDerefInt(wizcmd.GetParam(4)));
			}
		} else if( cmd == "reversefindstring" ) {
			PARAM4V() {
				g_WizVars[wizcmd.GetParam(1).GetVar()] = GetDerefString(wizcmd.GetParam(2)).rfind(GetDerefString(wizcmd.GetParam(3)), GetDerefInt(wizcmd.GetParam(4)));
			}
		} else if( cmd == "stringlength" ) {
			PARAM2V() {
				g_WizVars[wizcmd.GetParam(1).GetVar()] = GetDerefString(wizcmd.GetParam(2)).length();
			}
		} else if( cmd == "substring" ) {
			PARAM4V() {
				g_WizVars[wizcmd.GetParam(1).GetVar()] = GetDerefString(wizcmd.GetParam(2)).substr(GetDerefInt(wizcmd.GetParam(3)), GetDerefInt(wizcmd.GetParam(4)));
			}
		} else if( cmd == "numlines" ) {
			PARAM2V() {
				g_WizVars[wizcmd.GetParam(1).GetVar()] = WizParam(GetDerefString(wizcmd.GetParam(2))).NumLines();
			}
		} else if( cmd == "getline" ) {
			PARAM3V() {
				g_WizVars[wizcmd.GetParam(1).GetVar()] = WizParam(GetDerefString(wizcmd.GetParam(2))).GetLine(GetDerefInt(wizcmd.GetParam(3)));
			}
		} else if( cmd == "setline" ) {
			PARAM3V() {
				g_WizVars[wizcmd.GetParam(1).GetVar()].SetLine(GetDerefInt(wizcmd.GetParam(2)), GetDerefString(wizcmd.GetParam(3)));
			}
		} else if( cmd == "addline" ) {
			PARAM2V() {
				g_WizVars[wizcmd.GetParam(1).GetVar()].AddLine(GetDerefString(wizcmd.GetParam(2)));
			}
		} else if( cmd == "deleteline" ) {
			PARAM2V() {
				g_WizVars[wizcmd.GetParam(1).GetVar()].DeleteLine(GetDerefInt(wizcmd.GetParam(2)));
			}
		} else if( cmd == "sortlines" ) {
			PARAM1V() {
				g_WizVars[wizcmd.GetParam(1).GetVar()].SortLines();
			}
		} else if( cmd == "splitlines" ) {
			PARAM2V() {
				g_WizVars[wizcmd.GetParam(1).GetVar()].SplitLines(GetDerefString(wizcmd.GetParam(2)));
			}
		} else if( cmd == "joinlines" ) {
			PARAM2V() {
				g_WizVars[wizcmd.GetParam(1).GetVar()].JoinLines(GetDerefString(wizcmd.GetParam(2)));
			}
		} else if( cmd == "removeblanklines" ) {
			PARAM1V() {
				g_WizVars[wizcmd.GetParam(1).GetVar()].RemoveBlankLines();
			}
		} else if( cmd == "if" ) {
			bool test;
			TestCmd(g_WizCmds[i], test, err);
			int nest = 0;
			//search the elseif/else cases
			while( !test ) {
				do {
					if( ++i >= g_WizCmds.size() ) break;
					cmd = g_WizCmds[i].GetCmd();
				} while( cmd != "if" && cmd != "elseif" && cmd != "else" && cmd != "endif" );
				//if we ran off the end, set so that the next cmd will be the implied finish
				if( i >= g_WizCmds.size() ) {
					i = g_WizCmds.size() - 2;
					break;
				}
				//nested if inside a block we aren't executing, skip until endif
				if( cmd == "if" ) {
					nest++;
				}
				//if we are in the proper nest, find the next elseif to test
				if( nest == 0 ) {
					if( cmd == "elseif" ) {
						TestCmd(g_WizCmds[i], test, err);
					} else if( cmd == "else" || cmd == "endif" ) {
						//break out of here at the else or endif and start executing
						test = true;
					}
				}
				if( cmd == "endif" ) {
					nest--;
					if( nest < 0 ) nest = 0;
				}
			}
		} else if( cmd == "elseif" || cmd == "else" ) {
			bool test = false;
			int nest = 0;
			//skip until the proper endif
			while( !test ) {
				do {
					if( ++i >= g_WizCmds.size() ) break;
					cmd = g_WizCmds[i].GetCmd();
				} while( cmd != "if" && cmd != "endif" );
				//if we ran off the end, set so that the next cmd will be the implied finish
				if( i >= g_WizCmds.size() ) {
					i = g_WizCmds.size() - 2;
					break;
				}
				if( cmd == "if" ) {
					nest++;
				} else if( nest == 0 ) {
					test = true;
				} else {
					nest--;
				}
			}
		} else if( cmd == "endif" ) {
			//do nothing
		} else if( cmd == "while" ) {
			bool test;
			TestCmd(g_WizCmds[i], test, err);
			if( test ) {
				whilelocs.push_back(i);
			}
			int nest = 0;
			//skip until the proper endwhile
			while( !test ) {
				do {
					if( ++i >= g_WizCmds.size() ) break;
					cmd = g_WizCmds[i].GetCmd();
				} while( cmd != "while" && cmd != "endwhile" );
				//if we ran off the end, set so that the next cmd will be the implied finish
				if( i >= g_WizCmds.size() ) {
					i = g_WizCmds.size() - 2;
					break;
				}
				if( cmd == "while" ) {
					nest++;
				} else if( nest == 0 ) {
					test = true;
				} else {
					nest--;
				}
			}
		} else if( cmd == "endwhile" ) {
			if( whilelocs.size() > 0 ) {
				i = whilelocs[whilelocs.size() - 1] - 1;
				whilelocs.pop_back();
			}
		} else if( cmd == "loadvar" ) {
			PARAM1V() {
				string varname = wizcmd.GetParam(1).GetVar();
				if( savevars.find(varname) != savevars.end() ) {
					//this is in case we used the back button - load from save vars not registry
					g_WizVars[varname] = savevars[varname];
				} else {
					HKEY hKey;
					if( !RegOpenKey(HKEY_CURRENT_USER, "Software\\Git\\Wizard", &hKey) ) {
						DWORD type, size;

						if( !RegQueryValueEx(hKey, varname.c_str(), 0, &type, NULL, &size ) ) {
							if( type == REG_DWORD ) {
								DWORD val;
								if( !RegQueryValueEx(hKey, varname.c_str(), 0, &type, (LPBYTE)&val, &size ) ) {
									g_WizVars[varname] = val;
								} else {
									autoconf = false;
								}
							} else if( type == REG_SZ ) {
								char *str = new char[size];
								if( !RegQueryValueEx(hKey, varname.c_str(), 0, &type, (LPBYTE)str, &size ) ) {
									g_WizVars[varname] = str;
								} else {
									autoconf = false;
								}
								delete [] str;
							} else {
								autoconf = false;
							}
						} else {
							autoconf = false;
						}
						RegCloseKey(hKey);
					} else {
						autoconf = false;
					}
				}
			}
		} else if( cmd == "savevar" ) {
			PARAM1V() {
				string varname = wizcmd.GetParam(1).GetVar();
				savevars[varname] = g_WizVars[varname];
			}
		} else if( cmd == "showpage" ) {
			if( !autoconf ) {
				done = ShowPage();
				if( done == IDOK ) {
					backlocs.push_back(i);
				}
			}
		} else if( cmd == "finish" ) {
			if( autoconf ) {
				done = IDOK;
			} else {
				g_WizVars["heading"] = "Wizard completed successfully";
				g_WizVars["subheading"] = "GIT is ready for " + scriptname;
				g_WizVars["control"] = "label||The wizard has completed successfully and GIT is ready to be configured for the program " + scriptname + "|2";
				g_WizVars["control"].AddLine("label||Press Finish to apply the settings calculated by the wizard or "
					"Cancel to revert to the previous settings.|2");
				WizParam msg = WizParam(GetDerefString(wizcmd.GetParam(1)));
				msg.SplitLines("|");
				msg.JoinLines(" ");
				g_WizVars["control"].AddLine("label||" + msg.GetString() + "|10");
				done = ShowPage(2);
			}
		} else if( cmd == "cancel" ) {
			g_WizVars["heading"] = "Wizard did not complete successfully";
			g_WizVars["subheading"] = "Unable to configure for " + scriptname;
			g_WizVars["control"] = "label||The wizard was unable to configure GIT for " + scriptname + "|2";
			WizParam msg = WizParam(GetDerefString(wizcmd.GetParam(1)));
			msg.SplitLines("|");
			msg.JoinLines(" ");
			g_WizVars["control"].AddLine("label||" + msg.GetString() + "|10");
			done = ShowPage(0);
		} else {
			err = "Unknown command '" + cmd + "' found";
		}
		if( err.length() > 0 ) {
			stringstream ss;
			ss << err << " on line " << wizcmd.GetLineNum();
			MessageBox(NULL, ss.str().c_str(), GIT_NAME, MB_ICONWARNING);
			err.erase();
		}
		if( done == IDCANCEL ) {
			break;
		} else if( done == IDRETRY ) {
			if( backlocs.size() > 1 ) {
				backlocs.pop_back();
				i = backlocs[backlocs.size() - 1];
				done = IDOK;
			} else {
				backlocs.clear();
				goto start_over; //heh, really, is there a better way?
			}
		}
	}

	if( done == IDOK ) {
		//save vars that were requested to registry
		HKEY hKey;
		if( !RegCreateKey(HKEY_CURRENT_USER, "Software\\Git\\Wizard", &hKey) ) {
			for( map<string, WizVar>::iterator var = savevars.begin(); var != savevars.end(); var++ ) {
				if( var->second.GetType() == VARTYPE_INT ) {
					DWORD val = var->second.GetInt();
					RegSetValueEx(hKey, var->first.c_str(), 0, REG_DWORD, (LPBYTE)&val, sizeof(val));
				} else {
					string str = var->second.GetString();
					RegSetValueEx(hKey, var->first.c_str(), 0, REG_SZ, (LPBYTE)str.c_str(), str.length() + 1);
				}
			}
			RegCloseKey(hKey);
		}

		//display status window
		HWND hwndStat;
		if( enabled ) {
			hwndStat = CreateDialog( hInst, MAKEINTRESOURCE(IDD_STATUS), NULL, DialogProcStatus );
			ShutdownTun();
		} else {
			hwndStat = NULL;
		}

		SaveConfigVars();
		SaveConfigPageOptions();
		SaveAdvCfgPageOptions();

		//remove status window
		if( hwndStat ) {
			if( enabled ) {
				StartupTun();
			}
			DestroyWindow( hwndStat );
		}
	}
}

int ShowPage(int next)
{
    HGLOBAL hGlobal;
    LPDLGTEMPLATE pDlg;
	LPDLGITEMTEMPLATE pItem;
    LPWORD pWord;

	vector<string> control;
	split(g_WizVars["control"].GetString(), string("\r\n"), control);
	g_WizCtrls.clear();
	for( int i = 0; i < control.size(); i++ ) {
		g_WizCtrls.push_back(control[i]);
	}
	control.clear();
	g_WizVars["control"] = "";

	int size = 0, controls = 0;
	for(int pass = 0; pass < 2; pass++) {
		if( pass ) {
			if( !(hGlobal = GlobalAlloc(GMEM_ZEROINIT, size)) ) {
				MessageBox(NULL, "Out of memory creating dialog", GIT_NAME, MB_ICONERROR);
				return IDCANCEL;
			}

			//divide x by 1.5 and y by 1.625 to convert from pixels at a normal 8pt Tahoma
			pDlg = (LPDLGTEMPLATE)GlobalLock(hGlobal);
			pDlg->style = DS_MODALFRAME | DS_NOIDLEMSG | DS_SETFOREGROUND | DS_CENTER | DS_SETFONT | WS_POPUP | WS_CAPTION | WS_SYSMENU;
			pDlg->cdit = controls;
			pDlg->cx = 332;
			pDlg->cy = 218;
			pWord = (LPWORD)(pDlg + 1);
		} else {
			size += sizeof(DLGTEMPLATE);
		}
		WORD(0)
		WORD(0)
		STRING(GIT_NAME " Wizard")
		WORD(8)
		STRING("Tahoma")

		//white box at top
		DLGITEM(0, 0, 332, 36, 0, SS_WHITERECT)
		STATIC() WORD(0) WORD(0)

		//heading
		DLGITEM(6, 3, 272, 8, ID_HEADING, SS_LEFT)
		STATIC() STRING(g_WizVars["heading"]) WORD(0)

		//subheading
		DLGITEM(18, 15, 260, 16, ID_SUBHEADING, SS_LEFT)
		STATIC() STRING(g_WizVars["subheading"]) WORD(0)

		//icon
		DLGITEM(289, 3, 0, 0, ID_ICON, SS_ICON) //294,8 for 32x32
		STATIC() WORD(0xFFFF) WORD(IDI_WIZARD) WORD(0)

		//horizontal lines under white box
		DLGITEM(0, 36, 334, 0, 0, SS_ETCHEDHORZ)
		STATIC() WORD(0) WORD(0)

		//user controls
		int line = 0; //really 1/2 of a text line per increment (1 line is 8 DLUs)
		int nextid = ID_USERBASE;
		for( int i = 0; i < g_WizCtrls.size(); i++ ) {
			string ctrl = g_WizCtrls[i].GetCtrl();
			if( ctrl == "space" ) {
				line += g_WizCtrls[i].GetSize();
			} else if( ctrl == "label" ) {
				if( line + (g_WizCtrls[i].GetSize() * 2) >= MAX_WIZARD_LINES ) break;
				DLGITEM(18, 40 + (line * 4), 296, g_WizCtrls[i].GetSize() * 8, nextid, SS_LEFT)
				STATIC() STRING(g_WizCtrls[i].GetText().c_str()) WORD(0)
				g_WizCtrls[i].SetID(nextid);
				nextid++;
				line += (g_WizCtrls[i].GetSize() * 2) + 1;
			} else if( ctrl == "check" ) {
				if( line + 2 >= MAX_WIZARD_LINES ) break;
				DLGITEM(18, 40 + (line * 4), 296, 8, nextid, WS_TABSTOP | BS_AUTOCHECKBOX)
				BUTTON() STRING(g_WizCtrls[i].GetText().c_str()) WORD(0)
				g_WizCtrls[i].SetID(nextid);
				nextid++;
				line += 3;
			} else if( ctrl == "radio" ) {
				int rows = g_WizCtrls[i].GetSize();
				if( rows > 16) rows = 16;

				int height = g_WizCtrls[i].GetNumChoices();
				if( g_WizCtrls[i].GetText().length() > 0 ) height++;
				height = (height + rows - 1) / rows;
				if( line + (height * 3) - 1 >= MAX_WIZARD_LINES ) break;

				int width = 296 / rows;
				int left = 0;
				if( g_WizCtrls[i].GetText().length() > 0 ) {
					DLGITEM(18, 40 + (line * 4), width, 8, nextid, SS_LEFT)
					STATIC() STRING(g_WizCtrls[i].GetText().c_str()) WORD(0)
					nextid++;
					left++;
				}
				g_WizCtrls[i].SetID(nextid);

				for( int c = 0; c < g_WizCtrls[i].GetNumChoices(); c++ ) {
					if( left >= rows ) {
						line += 3;
						left = 0;
					}
					DLGITEM(18 + (left * width), 40 + (line * 4), width, 8, nextid, WS_TABSTOP | BS_AUTORADIOBUTTON | (c?0:WS_GROUP))
					BUTTON() STRING(g_WizCtrls[i].GetChoice(c).c_str()) WORD(0)
					nextid++;
					left++;
				}
				line += 3;
			} else if( ctrl == "dropd" || ctrl == "combo" || ctrl == "text1"  || ctrl == "textr" ) {
				if( line + (g_WizCtrls[i].GetText().length()>0&&g_WizCtrls[i].GetSize()>1?3:6) >= MAX_WIZARD_LINES ) break;

				int width = 296 / g_WizCtrls[i].GetSize();
				int left = 0;
				if( width < 16 ) width = 16;
				if( g_WizCtrls[i].GetText().length() > 0 ) {
					if( g_WizCtrls[i].GetSize() > 1 ) {
						DLGITEM(18, 40 + (line * 4) + 4, 296 - width, 8, nextid, SS_LEFT)
						left += 296 - width;
					} else {
						DLGITEM(18, 40 + (line * 4), 296, 8, nextid, SS_LEFT)
						line += 3;
					}
					STATIC() STRING(g_WizCtrls[i].GetText().c_str()) WORD(0)
					nextid++;
				}

				if( ctrl == "text1" || ctrl == "textr" ) {
					DLGITEM(18 + left, 40 + (line * 4), width, 12, nextid, WS_TABSTOP | WS_BORDER | ES_LEFT | ES_AUTOHSCROLL | (ctrl=="textr"?ES_READONLY:0))
					EDIT() WORD(0) WORD(0)
				} else {
					DLGITEM(18 + left, 40 + (line * 4), width, 76, nextid, WS_TABSTOP | WS_VSCROLL | (ctrl=="dropd"?CBS_SIMPLE:0) | CBS_DROPDOWN | CBS_AUTOHSCROLL)
					COMBOBOX() WORD(0) WORD(0)
				}
				g_WizCtrls[i].SetID(nextid);
				nextid++;
				line += 4;
			} else if( ctrl == "textm" ) {
				if( line + (g_WizCtrls[i].GetText().length()>0?3:0) + (g_WizCtrls[i].GetSize() * 2) + 1 >= MAX_WIZARD_LINES ) break;

				if( g_WizCtrls[i].GetText().length() > 0 ) {
					DLGITEM(18, 40 + (line * 4), 296, 8, nextid, SS_LEFT)
					STATIC() STRING(g_WizCtrls[i].GetText().c_str()) WORD(0)
					nextid++;
					line += 3;
				}

				DLGITEM(18, 40 + (line * 4), 296, (g_WizCtrls[i].GetSize() * 8) + 4, nextid, WS_TABSTOP | WS_BORDER | ES_LEFT | ES_MULTILINE | ES_WANTRETURN)
				EDIT() WORD(0) WORD(0)
				g_WizCtrls[i].SetID(nextid);
				nextid++;
				line += (g_WizCtrls[i].GetSize() * 2) + 2;
			} else if( ctrl == "hline" ) {
				if( line >= MAX_WIZARD_LINES ) break;
				DLGITEM(18, 40 + (line * 4), 296, 0, nextid, SS_ETCHEDHORZ)
				STATIC() WORD(0) WORD(0)
				g_WizCtrls[i].SetID(nextid);
				nextid++;
				line += 1;
			} else if( ctrl == "specl" ) {
				ctrl = g_WizCtrls[i].GetText();
				if( ctrl == "autoconf" ) {
					//if( line + 3 >= MAX_WIZARD_LINES ) break;
					//DLGITEM(18, 40 + (line * 4), 70, 14, ID_AUTOCONF, WS_TABSTOP)
					DLGITEM(18, 196, 60, 14, ID_AUTOCONF, WS_TABSTOP)
					BUTTON() STRING("&Auto Configure") WORD(0)
					//line += 4;
				} else if( ctrl == "saveprof" ) {
					DLGITEM(85, 196, 50, 14, ID_SAVEPROF, WS_TABSTOP)
					BUTTON() STRING("&Save Profile") WORD(0)
				} else if( ctrl == "ipfwnat" ) {
					if( line + 3 >= MAX_WIZARD_LINES ) break;
					DLGITEM(18, 40 + (line * 4), 120, 14, ID_IPFWNAT, WS_TABSTOP)
					BUTTON() STRING("Try to guess these settings") WORD(0)
					line += 4;
				}
			} else if( ctrl.length() > 0 ) {
				string err = "Unknown control type '" + ctrl + "' specified.";
				MessageBox(NULL, err.c_str(), GIT_NAME, MB_ICONWARNING);
			}
		}

		//horizontal lines near bottom
		DLGITEM(0, 186, 334, 0, 0, SS_ETCHEDHORZ)
		STATIC() WORD(0) WORD(0)

		//back button
		if( next>=0 ) {
			DLGITEM(165, 196, 50, 14, IDRETRY, WS_TABSTOP)
			BUTTON() STRING("< &Back") WORD(0)
		}

		//next button
		if( next ) {
			DLGITEM(215, 196, 50, 14, IDOK, WS_TABSTOP | BS_DEFPUSHBUTTON)
			BUTTON() STRING(next>1?"&Finish":"&Next >") WORD(0)
		}

		//cancel button
		DLGITEM(272, 196, 50, 14, IDCANCEL, WS_TABSTOP)
		BUTTON() STRING("Cancel") WORD(0)
	}

	g_WizVars["heading"] = "";
	g_WizVars["subheading"] = "";

	GlobalUnlock(hGlobal);
	int ret = DialogBoxIndirect(hInst, (LPDLGTEMPLATE)hGlobal, NULL, (DLGPROC)DialogProcWizard);
	GlobalFree(hGlobal);

	return ret;
}

int CALLBACK DialogProcWizard(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch( uMsg ) {
	default:
		return FALSE;

	case WM_INITDIALOG: {
		HDC hDC = GetDC(hwndDlg);
		boldFont = CreateFont(-MulDiv(8, GetDeviceCaps(hDC, LOGPIXELSY), 72), 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
			DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH, "Tahoma");
		ReleaseDC(hwndDlg, hDC);
		SendDlgItemMessage(hwndDlg, ID_HEADING, WM_SETFONT, (WPARAM)boldFont, 0);
		SendMessage(hwndDlg, WM_SETICON, ICON_SMALL, (LPARAM)LoadImage(hInst, MAKEINTRESOURCE(IDI_WIZARD), IMAGE_ICON, 16, 16, LR_SHARED));
		SendMessage(hwndDlg, WM_SETICON, ICON_BIG, (LPARAM)LoadImage(hInst, MAKEINTRESOURCE(IDI_WIZARD), IMAGE_ICON, 32, 32, LR_SHARED));
		SendDlgItemMessage(hwndDlg, ID_ICON, STM_SETICON, (WPARAM)LoadImage(hInst, MAKEINTRESOURCE(IDI_WIZARD), IMAGE_ICON, 48, 48, LR_SHARED), 0);

		for( int i = 0; i < g_WizCtrls.size(); i++ ) {
			string ctrl = g_WizCtrls[i].GetCtrl();
			string var =  g_WizCtrls[i].GetVar();
			int id = g_WizCtrls[i].GetID();
			if( ctrl == "label" ) {
				if( var != "#" ) {
					SetDlgItemText( hwndDlg, id, g_WizVars[var].GetString().c_str() );
				}
			} else if( ctrl == "check" ) {
				CheckDlgButton( hwndDlg, id, g_WizVars[var].GetInt() ? BST_CHECKED : BST_UNCHECKED );
			} else if( ctrl == "radio" ) {
				int choice = g_WizVars[var].GetInt(), max = g_WizCtrls[i].GetNumChoices() - 1;
				if( choice > max ) choice = 0;
				CheckRadioButton( hwndDlg, id, id + max, id + choice );
			} else if( ctrl == "dropd" ) {
				if( g_WizCtrls[i].GetNumChoices() < 1 ) {
					SendDlgItemMessage( hwndDlg, id, CB_ADDSTRING, 0, (LPARAM)"" );
				} else {
					for( int p = 0; p < g_WizCtrls[i].GetNumChoices(); p++ ) {
						SendDlgItemMessage( hwndDlg, id, CB_ADDSTRING, 0, (LPARAM)g_WizCtrls[i].GetChoice(p).c_str() );
					}
				}
				int choice = g_WizVars[var].GetInt(), max = g_WizCtrls[i].GetNumChoices() - 1;
				if( choice > max ) choice = 0;
				SendDlgItemMessage( hwndDlg, id, CB_SETCURSEL, choice, 0 );
			} else if( ctrl == "combo" ) {
				if( g_WizCtrls[i].GetNumChoices() < 1 ) {
					SendDlgItemMessage( hwndDlg, id, CB_ADDSTRING, 0, (LPARAM)"" );
				} else {
					for( int p = 0; p < g_WizCtrls[i].GetNumChoices(); p++ ) {
						SendDlgItemMessage( hwndDlg, id, CB_ADDSTRING, 0, (LPARAM)g_WizCtrls[i].GetChoice(p).c_str() );
					}
				}
				SetDlgItemText( hwndDlg, id, g_WizVars[var].GetLine().c_str() );
			} else if( ctrl == "text1" || ctrl == "textr" ) {
				SetDlgItemText( hwndDlg, id, g_WizVars[var].GetLine().c_str() );
			} else if( ctrl == "textm" ) {
				SetDlgItemText( hwndDlg, id, g_WizVars[var].GetString().c_str() );
			}
		}
		return TRUE; }

	case WM_DESTROY:
		if( boldFont != NULL ) {
			DeleteObject(boldFont);
			boldFont = NULL;
		}
		return TRUE;

	case WM_CTLCOLORSTATIC: {
		int id = GetDlgCtrlID((HWND)lParam);
		if( id == ID_HEADING || id == ID_SUBHEADING ) {
			return (int)(HBRUSH)GetStockObject(HOLLOW_BRUSH);
		} else if( id == ID_ICON ) {
			SetBkMode((HDC)wParam, TRANSPARENT);
			return (int)(HBRUSH)GetStockObject(WHITE_BRUSH);
		} else {
			return DefWindowProc(hwndDlg, uMsg, wParam, lParam);
		} }

	case WM_COMMAND:
		switch( wParam ) {
		case ID_AUTOCONF:
		case IDOK: {
			for( int i = 0; i < g_WizCtrls.size(); i++ ) {
				string ctrl = g_WizCtrls[i].GetCtrl();
				string var =  g_WizCtrls[i].GetVar();
				int id = g_WizCtrls[i].GetID();
				if( ctrl == "check" ) {
					g_WizVars[var] = IsDlgButtonChecked( hwndDlg, id ) ? 1 : 0;
				} else if( ctrl == "radio" ) {
					int max = g_WizCtrls[i].GetNumChoices() - 1;
					for( int choice = 0; choice <= max; choice++ ) {
						if( IsDlgButtonChecked( hwndDlg, id + choice ) ) {
							g_WizVars[var] = choice;
							break;
						}
					}
					if( choice > max ) {
						g_WizVars[var] = 0;
					}
				} else if( ctrl == "dropd" ) {
					 int choice = SendDlgItemMessage( hwndDlg, id, CB_GETCURSEL, 0, 0 );
					 if( choice < 0 ) choice = 0;
					 g_WizVars[var] = choice;
				} else if( ctrl == "combo" || ctrl == "text1" || ctrl == "textm" ) {
					int size = SendDlgItemMessage( hwndDlg, id, WM_GETTEXTLENGTH, 0, 0 ) + 1;
					char *str = new char[size];
					GetDlgItemText( hwndDlg, id, str, size );
					g_WizVars[var] = str;
					delete [] str;
				}
			}
			EndDialog( hwndDlg, wParam );
			break; }

		case IDRETRY:
		case IDCANCEL:
			EndDialog( hwndDlg, wParam );
			break;

		case ID_SAVEPROF: {
			int size = SendDlgItemMessage( hwndDlg, ID_USERBASE+7, WM_GETTEXTLENGTH, 0, 0 ) + 1;
			if( size <= 1 ) return TRUE;
			char *str = new char[size];
			GetDlgItemText( hwndDlg, ID_USERBASE+7, str, size );
			for( char *p = str; *p; p++ ) {
				if( strchr("\\/:*?\"<>|", *p) ) *p = '_';
			}
			string scriptfile = string(g_path) + "Profile - " + str + ".git";
			delete [] str;
			ifstream fileexists(scriptfile.c_str());
			if( fileexists ) {
				fileexists.close();
				if( MessageBox(hwndDlg, "Profile already exists, overwrite?", GIT_NAME, MB_ICONQUESTION | MB_YESNO | MB_DEFBUTTON2) != IDYES ) {
					return TRUE;
				}
			}
			ofstream file(scriptfile.c_str());
			if( !file ) {
				scriptfile = "Unable to write to file: " + scriptfile;
				MessageBox(hwndDlg, scriptfile.c_str(), GIT_NAME, MB_ICONWARNING);
				return TRUE;
			}
			file << "# GIT Wizard generated profile" << endl << endl;
			file << "setvar multiport " << g_WizVars["multiport"] << endl;
			file << "setvar multimax " << g_WizVars["multimax"] << endl;
			file.setf(ios::hex);
			file.unsetf(ios::dec);
			file << "setvar options 0x" << setfill('0') << setw(8) << g_WizVars["options"].GetInt() << endl;
			file << "setvar packets 0x" << setfill('0') << setw(8) << g_WizVars["packets"].GetInt() << endl;
			file << "setvar protos 0x" << setfill('0') << setw(8) << g_WizVars["protos"].GetInt() << endl;
			file << "setvar natinternal \"" << g_WizVars["natinternal"].GetString() << "\"" << endl;
			file << "setvar natexternal \"" << g_WizVars["natexternal"].GetString() << "\"" << endl;
			int i, max;
			if( IsDlgButtonChecked( hwndDlg, ID_USERBASE+8 ) ) {
				file << "setvar hosts \"" << g_WizVars["hosts"].GetLine() << "\"" << endl;
				max = g_WizVars["hosts"].NumLines();
				for( i = 1; i < max; i++ ) {
					file << "addline hosts \"" << g_WizVars["hosts"].GetLine(i) << "\"" << endl;
				}
			} else {
				file << "setvar heading \"Host List\"" << endl;
				file << "setvar subheading \"Please enter the hosts to connect to.\"" << endl;
				file << "setvar control \"textm|hosts|Configure hosts to connect to here, or in the normal Config window.|10\"" << endl;
				file << "showpage" << endl;
			}
			file << "setvar sockets \"" << g_WizVars["sockets"].GetLine() << "\"" << endl;
			max = g_WizVars["sockets"].NumLines();
			for( i = 1; i < max; i++ ) {
				file << "addline sockets \"" << g_WizVars["sockets"].GetLine(i) << "\"" << endl;
			}
			file << "setvar ports \"" << g_WizVars["ports"].GetLine() << "\"" << endl;
			max = g_WizVars["ports"].NumLines();
			for( i = 1; i < max; i++ ) {
				file << "addline ports \"" << g_WizVars["ports"].GetLine(i) << "\"" << endl;
			}
			file.close();
			MessageBox(NULL, "Profile saved successfully", GIT_NAME, MB_ICONINFORMATION);
			EndDialog( hwndDlg, IDCANCEL );
			break; }

		case ID_IPFWNAT: {
			char buf[4096];
			if( gethostname(buf, sizeof(buf)) == SOCKET_ERROR ) {
				MessageBox(hwndDlg, "Unable to locate your computer's name.", GIT_NAME, MB_ICONWARNING);
			} else {
				struct hostent *he;
				he = gethostbyname((char *)buf);
				SetDlgItemText( hwndDlg, ID_USERBASE+1, inet_ntoa(*(struct in_addr *)he->h_addr) );
				int routable = addr_is_routable(*(unsigned long *)he->h_addr);
				CheckRadioButton( hwndDlg, ID_USERBASE+3, ID_USERBASE+4, ID_USERBASE+(routable?3:4) );
				CheckRadioButton( hwndDlg, ID_USERBASE+6, ID_USERBASE+7, ID_USERBASE+(routable?6:7) );	
				char *msg;
				if( (*(unsigned long *)he->h_addr & 0xFFFF0000) == 0xA9FE0000 ) { //169.254.x.x
					msg = "Your computer appears to have assigned itself a 169.254.x.x address.\n"
						"This address is typically invalid and likely means you are not on the Internet at this time.";
				} else if( !routable ) {
					msg = "You appear to have a non-routable address and are probably using NAT to access the Internet.\n"
						"NAT also acts like a firewall, unless your computer is in the DMZ, in which case you should\n"
						"select Open instead of Firewalled.  You will also need to fill in your external IP address.";
				} else {
					msg = "You appear to have an open routable address. If you behind a firewall,\n"
						"please select Firewalled instead of Open.";
				}
				MessageBox(hwndDlg, msg, GIT_NAME, MB_ICONINFORMATION);
			}
			break; }

		default:
			break;
		}
		return TRUE;
	}
}

/*
//this version splits on any char in separators and removes empty pieces
void split(string &text, string &separators, vector<string> &pieces)
{
	int n = text.length();
	int start, stop;

	start = text.find_first_not_of(separators);
	while( (start >= 0) && (start < n) ) {
		stop = text.find_first_of(separators, start);
		if( (stop < 0) || (stop > n) ) stop = n;
		pieces.push_back(text.substr(start, stop - start));
		start = text.find_first_not_of(separators, stop+1);
	}
}
*/

//this version splits on the entire separator string and keeps empty pieces
void split(string &text, string &separator, vector<string> &pieces)
{
	int n = text.length();
	int start, stop;

	start = 0;
	while( (start >= 0) && (start < n) ) {
		stop = text.find(separator, start);
		if( (stop < 0) || (stop > n) ) stop = n;
		pieces.push_back(text.substr(start, stop - start));
		start = stop + separator.length();
	}
}

void join(vector<string> &pieces, string &glue, string &text)
{
	int i;
	text.erase();
	if( pieces.size() < 1 ) {
		return;
	}
	for( i = 0; i < pieces.size() - 1; i++ ) {
		text += pieces[i] + glue;
	}
	if( pieces.size() >= 1 ) {
		text += pieces[i];
	}
}

void trim(string &text)
{
	text.erase(0, text.find_first_not_of(" \t\r\n"));
	text.erase(text.find_last_not_of(" \t\r\n") + 1);
}

void cleanvarname(string &var)
{
	trim(var);
	while( var.length() > 0 && !((var[0] >= 'A' && var[0] <= 'Z') || (var[0] >= 'a' && var[0] <= 'z')) && var[0] != '_' ) {
		var.erase(0, 1);
	}
	int bad;
	while( (bad = var.find_first_not_of("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789_")) != string::npos ) {
		var.erase(bad, 1);
	}
}

int GetDerefType(WizParam &param)
{
	if(param.GetType() == VARTYPE_VAR) {
		return g_WizVars[param.GetVar()].GetType();
	} else if(param.GetType() == VARTYPE_SYM) {
		return -1;
	} else {
		return param.GetType();
	}
}

int GetDerefInt(WizParam &param)
{
	if(param.GetType() == VARTYPE_VAR) {
		return g_WizVars[param.GetVar()].GetInt();
	} else if(param.GetType() == VARTYPE_SYM) {
		return 0;
	} else {
		return param.GetInt();
	}
}

string GetDerefString(WizParam &param)
{
	if(param.GetType() == VARTYPE_VAR) {
		return g_WizVars[param.GetVar()].GetString();
	} else if(param.GetType() == VARTYPE_SYM) {
		return "";
	} else {
		return param.GetString();
	}
}

void TestCmd(WizCmd &wizcmd, bool &test, string &err)
{
	PARAMSYM() {
		bool neg = false;
		test = false;
		string op = wizcmd.GetParam(2).GetSym();
		if( op[0] == '!' ) { op.erase(0, 1); neg = true; }
		int type = GetDerefType(wizcmd.GetParam(1));
		if( type == VARTYPE_INT && GetDerefType(wizcmd.GetParam(3)) == VARTYPE_STRING ) type = VARTYPE_STRING;
		if( type == VARTYPE_INT ) {
			int left = GetDerefInt(wizcmd.GetParam(1)),
				right = GetDerefInt(wizcmd.GetParam(3));
			if( op == "=" ) test = left == right;
			else if( op == "<" ) test = left < right;
			else if( op == ">" ) test = left > right;
			else if( op == "<=" ) test = left <= right;
			else if( op == ">=" ) test = left >= right;
			else if( op == "con" ) test = (left & right) == right;
			else { err = "Unknown operator"; test = true; return; }
		} else {
			string left = GetDerefString(wizcmd.GetParam(1)),
				right = GetDerefString(wizcmd.GetParam(3));
			if( op == "=" ) test = left == right;
			else if( op == "<" ) test = left < right;
			else if( op == ">" ) test = left > right;
			else if( op == "<=" ) test = left <= right;
			else if( op == ">=" ) test = left >= right;
			else if( op == "con" ) test = left.find(right) != string::npos;
			else { err = "Unknown operator"; test = true; return; }
		}
		if( neg ) test = !test;
	}
}

void SetConfigVars(void)
{
	//ints
	g_WizVars["device"] = device;
	g_WizVars["multiport"] = multi.port;
	g_WizVars["multimax"] = multi.maxhosts;

	//flags
	g_WizVars["options"] = options & 0xFFFFF0FF; //remove log bits
	g_WizVars["packets"] = packets;
	g_WizVars["protos"] = protos;

	//strings
	g_WizVars["natinternal"] = (char *)natinfo.internal;
	g_WizVars["natexternal"] = (char *)natinfo.external;

	//multiline
	char buf[MAX_HOSTLEN + 64];
	int i;

	g_WizVars["hosts"] = "";
	for( i = 0; i < numhosts; i++ ) {
		FwdHostToStr(&hosts[i], buf);
		g_WizVars["hosts"].AddLine(buf);
	}
	g_WizVars["hosts"].DeleteLine();

	g_WizVars["sockets"] = "";
	for( i = 0; i < numsockets; i++ ) {
		SocketRangeToStr(&sockets[i], buf);
		g_WizVars["sockets"].AddLine(buf);
	}
	g_WizVars["sockets"].DeleteLine();

	g_WizVars["ports"] = "";
	for( i = 0; i < numports; i++ ) {
		PortRangeToStr(&ports[i], buf);
		g_WizVars["ports"].AddLine(buf);
	}
	g_WizVars["ports"].DeleteLine();
}

void SetFlagVars(void)
{
	g_WizVars["OPT_FRAME_8022"] = 0x00000001;
	g_WizVars["OPT_FRAME_8023"] = 0x00000002;
	g_WizVars["OPT_FRAME_ETH2"] = 0x00000004;
	g_WizVars["OPT_FRAME_SNAP"] = 0x00000008;
	g_WizVars["OPT_IPV4_NOUC"] = 0x00000010;
	g_WizVars["OPT_IPV4_NAT"] = 0x00000020;
	g_WizVars["OPT_IPV4_NOBC"] = 0x00000040;
	g_WizVars["OPT_IPV4_SRCP"] = 0x00000080;
	g_WizVars["OPT_IPV4_NORT"] = 0x00100000;
	g_WizVars["OPT_TCP_MULTI"] = 0x00001000;
	g_WizVars["OPT_OTHER_ARP"] = 0x00010000;
	g_WizVars["OPT_OTHER_ORFP"] = 0x00020000;
	g_WizVars["OPT_COMP_ZLIB"] = 0x01000000;
	g_WizVars["PACKET_OLD"] = 1<<0x00;
	g_WizVars["PACKET_RIP"] = 1<<0x01;
	g_WizVars["PACKET_ECO"] = 1<<0x02;
	g_WizVars["PACKET_ERR"] = 1<<0x03;
	g_WizVars["PACKET_IPX"] = 1<<0x04;
	g_WizVars["PACKET_SPX"] = 1<<0x05;
	g_WizVars["PACKET_NCP"] = 1<<0x11;
	g_WizVars["PACKET_NTB"] = 1<<0x14;
	g_WizVars["PROTO_ICMP"] = 1<<0x01;
	g_WizVars["PROTO_TCP"] = 1<<0x06;
	g_WizVars["PROTO_UDP"] = 1<<0x11;

	g_WizVars["METH_UDP"] = string(" (") + method_list[METH_UDP-1] + ")";
	g_WizVars["METH_TCP_LISTEN"] = string(" (") + method_list[METH_TCP_LISTEN-1] + ")";
	g_WizVars["METH_TCP_CONNECT"] = string(" (") + method_list[METH_TCP_CONNECT-1] + ")";
	g_WizVars["METH_TCP_CONNECT_SOCKS4"] = string(" (") + method_list[METH_TCP_CONNECT_SOCKS4-1] + ")";
	g_WizVars["METH_TCP_CONNECT_SOCKS5"] = string(" (") + method_list[METH_TCP_CONNECT_SOCKS5-1] + ")";
}

void SaveConfigVars(void)
{
	//ints
	device = g_WizVars["device"];
	multi.port = g_WizVars["multiport"];
	multi.maxhosts = g_WizVars["multimax"];

	//flags
	int i = (g_WizVars["options"] & 0xFFFFF0FF) | (options & 0x00000F00); //keep log bits
	SetOptions(i);
	packets = g_WizVars["packets"];
	protos = g_WizVars["protos"];

	//strings
	string str;
	str = g_WizVars["natinternal"].GetLine().substr(0, MAX_HOSTLEN-1); strcpy((char *)natinfo.internal, str.c_str());
	str = g_WizVars["natexternal"].GetLine().substr(0, MAX_HOSTLEN-1); strcpy((char *)natinfo.external, str.c_str());

	//multiline
	char buf[MAX_HOSTLEN + 64];

	g_WizVars["hosts"].RemoveBlankLines();
	numhosts = g_WizVars["hosts"].NumLines();
	if( numhosts > MAX_HOSTS ) numhosts = MAX_HOSTS;
	for( i = 0; i < numhosts; i++ ) {
		str = g_WizVars["hosts"].GetLine(i).substr(0, sizeof(buf)-1);
		strcpy(buf, str.c_str());
		StrToFwdHost(buf, &hosts[i]);
	}

	g_WizVars["sockets"].RemoveBlankLines();
	numsockets = g_WizVars["sockets"].NumLines();
	if( numsockets > MAX_SOCKETS ) numsockets = numsockets;
	for( i = 0; i < numsockets; i++ ) {
		str = g_WizVars["sockets"].GetLine(i).substr(0, sizeof(buf)-1);
		strcpy(buf, str.c_str());
		StrToSocketRange(buf, &sockets[i]);
	}

	g_WizVars["ports"].RemoveBlankLines();
	numports = g_WizVars["ports"].NumLines();
	if( numports > MAX_PORTS ) numports = MAX_PORTS;
	for( i = 0; i < numports; i++ ) {
		str = g_WizVars["ports"].GetLine(i).substr(0, sizeof(buf)-1);
		strcpy(buf, str.c_str());
		StrToPortRange(buf, &ports[i]);
	}
}

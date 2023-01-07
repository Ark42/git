
[Setup]
AppName=Gamer's Internet Tunnel
AppId=GamersInternetTunnel
AppVerName=GIT v0.99 BETA 4
AppPublisher=Morpheus Software, LLC
AppPublisherURL=http://www.morpheussoftware.net/git/
AppSupportURL=http://forums.morpheussoftware.net/
AppUpdatesURL=http://www.morpheussoftware.net/git/
AppCopyright=Copyright © 2001-2004 Morpheus Software, LLC
Compression=lzma/ultra
SolidCompression=yes
DefaultDirName={pf}\GIT
DefaultGroupName=GIT
UsePreviousGroup=no
DisableReadyPage=yes
DisableProgramGroupPage=yes
LicenseFile=
OutputBaseFilename=Git-099b4
OutputDir=C:\Src\VC\GIT\Installers
PrivilegesRequired=admin
ChangesAssociations=yes
UninstallDisplayIcon={app}\Git.exe
SourceDir=C:\Src\VC\GIT
WizardSmallImageFile=Git.bmp
WizardImageStretch=no

[Files]
Source: "Release\Git.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "Readme.txt"; DestDir: "{app}"; Flags: ignoreversion
Source: "Wizard Scripts.txt"; DestDir: "{app}"; Flags: ignoreversion
Source: "WinPcap_3_0.exe"; DestDir: "{app}"; Flags: ignoreversion
;Source: "C:\WINNT\system32\pthreadVSE.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "Age of Mythology - Titans.git"; DestDir: "{app}"; Flags: ignoreversion
Source: "Generic IPX.git"; DestDir: "{app}"; Flags: ignoreversion
Source: "Generic TCP or UDP.git"; DestDir: "{app}"; Flags: ignoreversion
Source: "Warcraft II.git"; DestDir: "{app}"; Flags: ignoreversion
Source: "Warcraft III.git"; DestDir: "{app}"; Flags: ignoreversion

[INI]
Filename: "{app}\GIT Homepage.url"; Section: "InternetShortcut"; Key: "URL"; String: "http://www.morpheussoftware.net/git/"
Filename: "{app}\GIT Support Forum.url"; Section: "InternetShortcut"; Key: "URL"; String: "http://forums.morpheussoftware.net/"

[Icons]
Name: "{group}\GIT"; Filename: "{app}\Git.exe"; WorkingDir: "{app}"
Name: "{group}\Install WinPcap v3.0"; Filename: "{app}\WinPcap_3_0.exe"; WorkingDir: "{app}"
Name: "{group}\GIT Readme"; Filename: "{app}\Readme.txt"; WorkingDir: "{app}"
Name: "{group}\GIT Homepage"; Filename: "{app}\GIT Homepage.url"; WorkingDir: "{app}"
Name: "{group}\GIT Support Forum"; Filename: "{app}\GIT Support Forum.url"; WorkingDir: "{app}"
Name: "{group}\Uninstall GIT"; Filename: "{uninstallexe}"; WorkingDir: "{app}"

[Registry]
Root: HKCR; Subkey: ".git"; ValueType: string; ValueName: ""; ValueData: "Gamers.Internet.Tunnel"; Flags: uninsdeletekey
Root: HKCR; Subkey: "Gamers.Internet.Tunnel"; ValueType: string; ValueName: ""; ValueData: "Gamer's Internet Tunnel Wizard Script"; Flags: uninsdeletekey
Root: HKCR; Subkey: "Gamers.Internet.Tunnel\DefaultIcon"; ValueType: string; ValueName: ""; ValueData: "{app}\Git.exe,4";
Root: HKCR; Subkey: "Gamers.Internet.Tunnel\Shell\Open"; ValueType: string; ValueName: ""; ValueData: "";
Root: HKCR; Subkey: "Gamers.Internet.Tunnel\Shell\Open\Command"; ValueType: string; ValueName: ""; ValueData: """{app}\Git.exe"" ""%1""";
Root: HKCR; Subkey: "Gamers.Internet.Tunnel\Shell\Edit"; ValueType: string; ValueName: ""; ValueData: "";
Root: HKCR; Subkey: "Gamers.Internet.Tunnel\Shell\Edit\Command"; ValueType: string; ValueName: ""; ValueData: "Notepad.exe ""%1""";
Root: HKCU; Subkey: "SOFTWARE\Git"; ValueType: none; ValueName: ""; ValueData: ""; Flags: uninsdeletekey

[Run]
Filename: "{app}\WinPcap_3_0.exe"; Description: "Install WinPcap v3.0"; Parameters: ""; WorkingDir: "{app}"; Flags: nowait postinstall skipifsilent unchecked

[UninstallDelete]
Type: files; Name: "{app}\GIT Homepage.url"
Type: files; Name: "{app}\GIT Support Forum.url"

[code]
procedure CurPageChanged(CurPage: Integer);
var Exists, HasVer: Boolean;
var MS, LS, Major, Minor: Cardinal;
var Str: String;
begin
  case CurPage of
    wpSelectDir:
    begin
      Str := WizardForm.SelectDirLabel.Caption;
      StringChange(Str, 'Next', 'Install');
      WizardForm.SelectDirLabel.Caption := Str;
      WizardForm.NextButton.Caption := '&Install';
    end;
    wpFinished:
    begin
      MS := 0;
      LS := 0;
      Str := ExpandConstant('{sys}')+'\packet.dll';
      Exists := FileExists(Str)
      if Exists then HasVer := GetVersionNumbers(Str, MS, LS);
      Major := MS shr 16;
      Minor := MS and $FFFF;
      Str := 'GIT requires WinPcap v3.0 or higher installed in order to function properly.'+#13+#10+#13+#10;
      if Exists then begin
        if HasVer then begin
            Str := Str + 'You currently have WinPcap v' + IntToStr(Major) + '.' + IntToStr(Minor) + ' intalled.';
          end else begin
            Str := Str + 'You currently have an older version of WinPcap installed.';
          end;
      end else begin
        Str := Str + 'You currently do not have WinPcap installed at all.';
      end;
      Str := Str + #13+#10+#13+#10+'If you experience crashes, freezes, or problems finding your packet device in GIT, follow these instructions:'+#13+#10+
        '1) Reboot your computer.'+#13+#10+
        '2) Uninstall WinPcap from control panel.'+#13+#10+
        '3) Reboot your computer.'+#13+#10+
        '4) Install WinPcap v3.0 or higher.'+#13+#10+
        '5) Reboot your computer.';
      WizardForm.RunList.Top := 250; //150
      WizardForm.FinishedLabel.Height := 179; //79
      WizardForm.FinishedLabel.Caption := Str
      if (Major < 3) or ((Major = 3) and (Minor < 0)) then begin
        WizardForm.RunList.Checked[0] := true;
      end;
    end;
  end;
end;

procedure CurStepChanged(CurStep: Integer);
begin
  case CurStep of
    csStart:
    begin
      if not FileExists(ExpandConstant('{sys}')+'\ws2_32.dll') then begin
        MsgBox('Your system is missing the winsock2 upgrade.'+#13+#10+
          'You should install winsock2 or GIT will be unable to function properly.', mbError, 0);
      end;
    end;
  end;
end;


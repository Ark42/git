@echo off

set SrcPath=C:\Src\VC\Git\
set Program=Git
set Installer=Git-099b4

set DSW=%Program%.dsw
set Project=%Program% - Win32 Release
set OutExe=Release\%Program%.exe
set MainH=%Program%.h
set ISS=%Program%.iss

set MSDEV=C:\Program Files\Microsoft Visual Studio\Common\MSDev98\Bin\MSDev.com
set RC=C:\Program Files\Microsoft Visual Studio\Common\MSDev98\Bin\rc.exe
set Awk=C:\unix\awk.exe
set ISCC=C:\Program Files\Inno Setup 4\ISCC.exe
set ResHack=C:\Program Files\ResHack\ResHacker.exe

:: compile
echo Compiling...
"%MSDEV%" %SrcPath%%DSW% /MAKE "%Project%" /REBUILD
echo Done compiling!

:: extract version resource from program exe, insert into installer
"%ISCC%" "%SrcPath%%ISS%"
"%ResHack%" -extract %SrcPath%%OutExe%,tmp.rc,versioninfo,,
"%RC%" tmp.rc
"%ResHack%" -addoverwrite %Installer%.exe,tmp.exe,tmp.res,,,
del tmp.rc tmp.res %Installer%.exe
ren tmp.exe %Installer%.exe

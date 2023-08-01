@echo off
setlocal

mkdir bin
pushd bin
mkdir debug
mkdir release
popd

where /q cl || (
  	echo ERROR: "cl" not found - please run this from the MSVC x64 native tools command prompt.
	pause
  	exit /b 1
)

if "%Platform%" neq "x64" (
	echo ERROR: Platform is not "x64" - please run this from the MSVC x64 native tools command prompt.
	pause
	exit /b 1
)

set SourceFiles=../../src/main.cpp

set MSVCCompileFlags=/DPLATFORM_WIN32 /nologo /W3 /Z7 /GS- /Gs999999
set MSVCLinkFlags=/incremental:no /opt:icf /opt:ref /subsystem:windows

set MSVCProfileCompileFlags=
set MSVCProfileLinkFlags=

if "%1"=="" (
	set Profile=debug
	goto compile
)
set Profile=%1

if %Profile% == debug (
	set MSVCProfileCompileFlags=/Od /DDEBUG
	set MSVCProfileLinkFlags=
) else if %Profile% == release (
	set MSVCProfileCompileFlags=/O2
	set MSVCProfileLinkFlags=
) else (
	echo ERROR: You should either specify debug or release as the first argument.
	exit /b 1
)

if not exist ./bin/%Profile% (
	echo ERROR: Directory ./bin/%Profile% does not exist.
	exit /b 1
)

:compile
pushd bin
pushd %Profile%
call cl %MSVCCompileFlags% %MSVCProfileCompileFlags% -Femain_msvc.exe %SourceFiles% /link %MSVCLinkFlags% %MSVCProfileLinkFlags%
popd
popd

rem pause
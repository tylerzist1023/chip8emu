@echo off
setlocal

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

set CompileFlags=/I../../include /DPLATFORM_WIN32 /DCOMPILER_MSVC /nologo /W3 /Z7 /GS- /Gs999999
set LinkFlags=/link /LIBPATH:../../lib /incremental:no /opt:icf /opt:ref /subsystem:windows

set DebugCompileFlags=/Od /DDEBUG
set DebugLinkFlags=

set ReleaseCompileFlags=/O2
set ReleaseLinkFlags=

set Compiler=cl
set CompilerName=msvc

set ExeCommand=-Fe

call build_win32.bat %1
@echo off
setlocal

mkdir bin
pushd bin
mkdir debug
mkdir release
popd

set SourceFiles=../../src/main.cpp

set ClangCompileFlags=-DPLATFORM_WIN32
set ClangLinkFlags=-fuse-ld=lld -Wl,-subsystem:windows -nostdlib++

set ClangProfileCompileFlags=
set ClangProfileLinkFlags=

if "%1"=="" (
	set Profile=debug
	goto compile
)
set Profile=%1

if %Profile% == debug (
	set ClangProfileCompileFlags=-g -O0 -DDEBUG
	set ClangProfileLinkFlags=
) else if %Profile% == release (
	set ClangProfileCompileFlags=-O3
	set ClangProfileLinkFlags=
) else (
	echo ERROR: You should either specify debug or release as the first argument.
	exit /b 1
)

if not exist ./bin/%Profile% (
	echo ERROR: Bin directory does not exist. You should either specify debug or release as the first argument.
	exit /b 1
)

:compile
pushd bin
pushd %Profile%
call clang++ %ClangCompileFlags% %ClangProfileCompileFlags% %ClangLinkFlags% %ClangProfileLinkFlags% %SourceFiles% -o main_clang.exe
popd
popd

pause
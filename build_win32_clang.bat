@echo off
setlocal

set CompileFlags=-I../../include -DPLATFORM_WIN32 -DCOMPILER_CLANG
set LinkFlags=-L../../lib -fuse-ld=lld -Wl,-subsystem:console -nostdlib++

set DebugCompileFlags=-g -O0 -DDEBUG
set DebugLinkFlags=

set ReleaseCompileFlags=-O3
set ReleaseLinkFlags=

set Compiler=clang++
set CompilerName=clang

set ExeCommand=-o

call build_win32.bat %1
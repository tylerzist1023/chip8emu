@echo off
setlocal

mkdir bin
pushd bin
mkdir debug
mkdir release
popd

set SourceFiles=../../src/main.cpp

if "%1"=="" (
    set Profile=debug
    goto setflags
)
set Profile=%1

:setflags
if not exist ./bin/%Profile% (
    echo ERROR: Bin directory does not exist. You should either specify debug or release as the first argument.
    exit /b 1
)

if %Profile% == debug (
    set ProfileCompileFlags=%DebugCompileFlags%
    set ProfileLinkFlags=%DebugLinkFlags%
) else if %Profile% == release (
    set ProfileCompileFlags=%ReleaseCompileFlags%
    set ProfileLinkFlags=%ReleaseLinkFlags%
) else (
    echo ERROR: You should either specify debug or release as the first argument.
    exit /b 1
)

pushd bin
pushd %Profile%
call %Compiler% %SourceFiles% %CompileFlags% %ProfileCompileFlags% %ExeCommand%main_%CompilerName%.exe %LinkFlags% %ProfileLinkFlags%
popd
popd

pause
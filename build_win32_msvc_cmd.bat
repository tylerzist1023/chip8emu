@echo off
set VCVarsAllLocation="C:/Program Files/Microsoft Visual Studio/2022/Enterprise/VC/Auxiliary/Build"
pushd %VCVarsAllLocation%
call vcvarsall.bat x64
popd

cmd /k
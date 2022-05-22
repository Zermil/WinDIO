@echo off

set MY_PATH=%~dp0

REM Change this to your visual studio's 'vcvars64.bat' script path
set MSVC_PATH="MY_MSVC_PATH"
call %MSVC_PATH%\vcvars64.bat

pushd %MY_PATH%

cl main.cpp /std:c++17 /EHsc %* /Fe:windio-test.exe

popd
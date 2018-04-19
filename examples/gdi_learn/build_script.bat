@echo off

set errno=0
set INCDIR=../../
set CFLAGS=-Wall -I%INCDIR%
set LFLAGS=-luser32 -lgdi32

if exist script.dll (
   del /Q /F script.dll
)

REM test if gcc is install
cmd /c "gcc -dumpversion" >NUL 2>NUL

REM begin build
if %ERRORLEVEL% neq 0 (
   tcc\tcc.exe -shared -o script.dll script.c app.def %CFLAGS% %LFLAGS%	
) else (
   gcc --shared -o script.dll script.c app.dll %CFLAGS% %LFLAGS%
)

if %ERRORLEVEL% neq 0 (
   pause
   set /A errno=1
)

REM return error number	
set /A ERRORLEVEL=%errno%

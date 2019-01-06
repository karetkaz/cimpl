@echo off

:: test build and run on a 32 bit platform.
SET WATCOM=C:\Workspace\ow_daily\rel2
IF NOT EXIST "%WATCOM%" (
	echo WATCOM not found
	exit
)

SET INCLUDE=%WATCOM%\h\nt;%WATCOM%\h
SET LIB=%WATCOM%\lib386
SET PATH=%WATCOM%\binnt;%PATH%

pushd "%~dp0\.."
SET CMPL=%CD%
popd
echo cmpl home is: %CMPL%

SET BIN=%CMPL%\bin\w32.wcc
IF NOT EXIST %BIN% mkdir %BIN%
pushd %BIN%
owcc -xc -std=c99 -o %BIN%\cmpl.exe %CMPL%\src\*.c
REM ~ owcc -xc -std=c99 -shared -I %CMPL%\src -o %BIN%\libFile.dll %CMPL%\lib\cmplFile\src\*.c
REM ~ owcc -xc -std=c99 -shared -Wc,-aa -I %CMPL%\src -o %BIN%\libGfx.dll %CMPL%\lib\cmplGfx\src\*.c %CMPL%\lib\cmplGfx\src\os_win32\*.c
del %BIN%\*.o
popd

:: test the virtual machine
%BIN%\cmpl --test-vm

:: dump symbols, assembly, syntax tree and global variables
SET DUMP_FILE=%BIN%.ci
%BIN%\cmpl -std"%CMPL%\lib\stdlib.ci" -X+steps -profile/p/G/M -api/a/m/d/p/u -asm/n/s -ast -log/15 "%DUMP_FILE%" -dump "%DUMP_FILE%" "%CMPL%\test\test.ci"

REM ~ SET DUMP_FILE=%BIN%.all.ci
REM ~ %BIN%\cmpl -std"%CMPL%\lib\stdlib.ci" -X+steps -profile/p/G/M -api/a/m/d/p/u -asm/n/s -ast -log/15 "%DUMP_FILE%" -dump "%DUMP_FILE%" "%BIN%\libFile.dll" "%BIN%\libGfx.dll" "%CMPL%\lib\cmplGfx\gfxlib.ci" "%CMPL%\test\test.ci"

pause

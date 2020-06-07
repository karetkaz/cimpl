@echo off

:: test build and run on windows platform.
REM SET MINGW_HOME=C:\Workspace\MinGw-5.3.0
SET WATCOM=C:\Workspace\ow_daily\rel2

REM pushd "%~dp0\.."
REM SET CMPL_HOME=%CD%
REM popd
echo cmpl home is: %CMPL_HOME%

SET BIN=%CMPL_HOME%\bin
SET BINW=%CMPL_HOME%\bin\win.wcc

IF EXIST "%MINGW_HOME%" (
	SET "PATH=%MINGW_HOME%\bin;%PATH%"
	IF NOT EXIST "%BIN%" mkdir "%BIN%"
	mingw32-make -C "%CMPL_HOME%" BINDIR="%BIN%" clean
	mingw32-make -C "%CMPL_HOME%" -j 12 BINDIR="%BIN%" cmpl.exe libFile.dll libGfx.dll
)

IF EXIST "%WATCOM%" (
	SET "INCLUDE=%WATCOM%\h\nt;%WATCOM%\h"
	SET "LIB=%WATCOM%\lib386"
	SET "PATH=%WATCOM%\binnt;%PATH%"

	IF NOT EXIST "%BINW%\obj.cc" mkdir "%BINW%\obj.cc"
	pushd "%BINW%\obj.cc"
	owcc -xc -std=c99 -o %BINW%\cmpl.exe %CMPL_HOME%\src\*.c
	REM ~ owcc -xc -std=c99 -shared -I %CMPL_HOME%\src -o %BIN%\libFile.dll %CMPL_HOME%\cmplFile\src\*.c
	REM ~ owcc -xc -std=c99 -shared -Wc,-aa -I %CMPL_HOME%\src -o %BIN%\libGfx.dll %CMPL_HOME%\cmplGfx\src\*.c %CMPL_HOME%\cmplGfx\src\os_win32\*.c
	popd
)

cd %CMPL_HOME%
:: dump symbols, assembly, syntax tree and global variables
SET TEST_FLAGS=-X+steps-stdin-offsets -asm/m/n/s -debug/g "%CMPL_HOME%\test\test.ci"
%BINW%\cmpl -log/d "%BINW%.ci" %TEST_FLAGS%
%BIN%\cmpl -log/d "%BIN%.ci" %TEST_FLAGS%

:: test the virtual machine
%BINW%\cmpl --test-vm
%BIN%\cmpl --test-vm

SET TEST_FILES=%CMPL_HOME%\test\*.ci
SET TEST_FILES=%TEST_FILES%;%CMPL_HOME%\test\lang\*.ci
SET TEST_FILES=%TEST_FILES%;%CMPL_HOME%\test\std\*.ci
SET TEST_FILES=%TEST_FILES%;%CMPL_HOME%\cmplFile\test\*.ci
SET TEST_FILES=%TEST_FILES%;%CMPL_HOME%\cmplGfx\test\*.ci
REM ~ SET TEST_FILES=%TEST_FILES%;%CMPL_HOME%\cmplGL\test\*.ci

SET DUMP_FILE=%BIN%.dump.ci
%BIN%\cmpl -X-stdin+steps -asm/n/s -run/g -log/d "%DUMP_FILE%" "%BIN%\libFile.dll" "%BIN%\libGfx.dll" "%CMPL_HOME%\lib\gfxlib.ci" "%CMPL_HOME%\test\test.ci"
FOR %%f IN (%TEST_FILES%) DO (
	pushd "%%~dpf"
	echo **** running test: %%f
	%BIN%\cmpl -X-stdin+steps -run/g -log/a/d "%DUMP_FILE%" "%BIN%\libFile.dll" "%BIN%\libGfx.dll" "%CMPL_HOME%\lib\gfxlib.ci" "%%f"
	IF ERRORLEVEL 1 echo ******** failed: %%f
	popd
)

pause

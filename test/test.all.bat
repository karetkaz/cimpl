@echo off

:: test build and run on windows platform.
SET WATCOM=C:\Workspace\ow_daily\rel2
SET MINGW=C:\Workspace\MinGw-5.3.0

pushd "%~dp0\.."
SET CMPL=%CD%
popd
echo cmpl home is: %CMPL%

SET BIN=%CMPL%\bin\win.gcc
SET BINW=%CMPL%\bin\win.wcc

IF EXIST "%MINGW%" (
	SET PATH=%MINGW%\bin;%PATH%
	IF NOT EXIST "%BIN%" mkdir "%BIN%"
	mingw32-make -C "%CMPL%" BINDIR="%BIN%" clean
	mingw32-make -C "%CMPL%" -j 12 BINDIR="%BIN%" cmpl.exe libFile.dll libGfx.dll libOpenGL.dll
)

IF EXIST "%WATCOM%" (
	SET INCLUDE=%WATCOM%\h\nt;%WATCOM%\h
	SET LIB=%WATCOM%\lib386
	SET PATH=%WATCOM%\binnt;%PATH%

	IF NOT EXIST "%BINW%\obj.cc" mkdir "%BINW%\obj.cc"
	pushd "%BINW%\obj.cc"
	owcc -xc -std=c99 -o %BINW%\cmpl.exe %CMPL%\src\*.c
	REM ~ owcc -xc -std=c99 -shared -I %CMPL%\src -o %BIN%\libFile.dll %CMPL%\cmplFile\src\*.c
	REM ~ owcc -xc -std=c99 -shared -Wc,-aa -I %CMPL%\src -o %BIN%\libGfx.dll %CMPL%\cmplGfx\src\*.c %CMPL%\cmplGfx\src\os_win32\*.c
	popd
)

cd %CMPL%
:: dump symbols, assembly, syntax tree and global variables
SET TEST_FLAGS=-X+steps-stdin-offsets -asm/m/n/s -debug/g "%CMPL%\test\test.ci"
%BINW%\cmpl -log/d "%BINW%.ci" %TEST_FLAGS%
%BIN%\cmpl -log/d "%BIN%.ci" %TEST_FLAGS%
:: test the virtual machine
%BINW%\cmpl --test-vm
%BIN%\cmpl --test-vm

:: pause && exit

SET TEST_FILES=%CMPL%\test\*.ci
SET TEST_FILES=%TEST_FILES%;%CMPL%\test\lang\*.ci
SET TEST_FILES=%TEST_FILES%;%CMPL%\test\stdc\*.ci
SET TEST_FILES=%TEST_FILES%;%CMPL%\cmplFile\test\*.ci
SET TEST_FILES=%TEST_FILES%;%CMPL%\cmplGfx\test\*.ci
REM ~ SET TEST_FILES=%TEST_FILES%;%CMPL%\cmplGL\test\*.ci

SET DUMP_FILE=%BIN%.dump.ci
%BIN%\cmpl -X-stdin+steps -asm/n/s -run/g -log/d "%DUMP_FILE%" -std"%CMPL%\lib\stdlib.ci" "%BIN%\libFile.dll" "%BIN%\libGfx.dll" "%CMPL%\cmplGfx\lib\gfxlib.ci" "%CMPL%\test\test.ci"
FOR %%f IN (%TEST_FILES%) DO (
	pushd "%%~dpf"
	echo **** running test: %%f
	%BIN%\cmpl -X-stdin+steps -run/g -log/a/d "%DUMP_FILE%" -std"%CMPL%\lib\stdlib.ci" "%BIN%\libFile.dll" "%BIN%\libOpenGL.dll" "%BIN%\libGfx.dll" "%CMPL%\cmplGfx\lib\gfxlib.ci" "%%f"
	IF ERRORLEVEL 1 echo ******** failed: %%f
	popd
)

pause

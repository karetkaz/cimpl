@echo off

cd "%~dp0\.."
SET CMPL_HOME=%CD%
echo cmpl home is: %CMPL_HOME%

SET BIN=%CMPL_HOME%\bin\windows
SET BIN_WCC=%CMPL_HOME%\bin\windows.wcc

@REM SET "MINGW_HOME=C:\Program Files\mingw-w64\x86_64-8.1.0-posix-seh-rt_v6-rev0\mingw64"
SET "MINGW_HOME=C:\Workspace\MinGw-5.3.0"
SET "WATCOM=C:\Workspace\ow_daily\rel2"

IF EXIST "%MINGW_HOME%" (
	SET "PATH=%MINGW_HOME%\bin;%PATH%"

	IF NOT EXIST "%BIN%" mkdir "%BIN%"
	mingw32-make -C "%CMPL_HOME%" BINDIR="%BIN%" clean
	mingw32-make -C "%CMPL_HOME%" -j 12 BINDIR="%BIN%" cmpl libFile.dll libGfx.dll libOpenGL.dll
)

IF EXIST "%WATCOM%" (
	SET "INCLUDE=%WATCOM%\h\nt;%WATCOM%\h"
	SET "LIB=%WATCOM%\lib386"
	SET "PATH=%WATCOM%\binnt;%PATH%"

	IF NOT EXIST "%BIN_WCC%" mkdir "%BIN_WCC%"
	pushd "%BIN_WCC%"
	owcc -xc -std=c99 -o %BIN_WCC%\cmpl.exe %CMPL_HOME%\src\*.c
	popd
)

:: test the virtual machine
%BIN_WCC%\cmpl --test-vm
%BIN%\cmpl --test-vm

:: dump symbols, assembly, syntax tree and global variables
SET TEST_FLAGS=-X+steps-stdin-offsets -asm/m/n/s -debug/g "%CMPL_HOME%\cmplStd\test\test.ci"
%BIN_WCC%\cmpl -log/d "%BIN_WCC%.ci" %TEST_FLAGS%
%BIN%\cmpl -log/d "%BIN%.ci" %TEST_FLAGS%

SET TEST_FILES=%CMPL_HOME%\cmplStd\test\test.ci
SET TEST_FILES=%TEST_FILES%;%CMPL_HOME%\cmplStd\test\demo\*.ci
SET TEST_FILES=%TEST_FILES%;%CMPL_HOME%\cmplStd\test\lang\*.ci
SET TEST_FILES=%TEST_FILES%;%CMPL_HOME%\cmplStd\test\std\*.ci
SET TEST_FILES=%TEST_FILES%;%CMPL_HOME%\cmplFile\test\*.ci
SET TEST_FILES=%TEST_FILES%;%CMPL_HOME%\cmplGfx\test\*.ci
SET TEST_FILES=%TEST_FILES%;%CMPL_HOME%\cmplGfx\test\demo\*.ci
SET TEST_FILES=%TEST_FILES%;%CMPL_HOME%\cmplGfx\test\demo.procedural\*.ci
SET TEST_FILES=%TEST_FILES%;%CMPL_HOME%\cmplGfx\test\demo.widget\*.ci
SET TEST_FILES=%TEST_FILES%;%CMPL_HOME%\temp\todo.cmplGfx\*.ci
SET TEST_FILES=%TEST_FILES%;%CMPL_HOME%\temp\todo.cmplGfx\demo\*.ci
SET TEST_FILES=%TEST_FILES%;%CMPL_HOME%\cmplGL\test\*.ci

SET DUMP_FILE=%BIN%.dump.ci
%BIN%\cmpl -log/d "%DUMP_FILE%"
FOR %%f IN (%TEST_FILES%) DO (
	pushd "%%~dpf"
	echo **** running test: %%f
	%BIN%\cmpl -X-stdin+steps -run -log/a/d "%DUMP_FILE%" "%BIN%\libFile.dll" "%BIN%\libGfx.dll" "%BIN%\libOpenGL.dll" "%%f"
	IF ERRORLEVEL 1 echo ******** failed: %%f
	popd
)

pause

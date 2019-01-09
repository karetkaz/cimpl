@echo off

:: test build and run on windows platform.
SET MINGW=C:\Workspace\MinGw-5.3.0
IF NOT EXIST "%MINGW%" (
	echo MinGW not found
	exit
)

SET PATH=%MINGW%\bin;%PATH%

pushd "%~dp0\.."
SET CMPL=%CD%
popd
echo cmpl home is: %CMPL%

SET BIN=%CMPL%\bin\w32.gcc
IF NOT EXIST %BIN% mkdir %BIN%
mingw32-make -C "%CMPL%" BINDIR="%BIN%" clean
mingw32-make -C "%CMPL%" -j 12 BINDIR="%BIN%" cmpl.exe libFile.dll libGfx.dll

:: test the virtual machine
%BIN%\cmpl --test-vm

:: dump symbols, assembly, syntax tree and global variables
SET DUMP_FILE=%BIN%.ci
%BIN%\cmpl -X-stdin+steps -std"%CMPL%\lib\stdlib.ci" -profile/p/G/M -api/a/m/d/p/u -asm/n/s -ast -log/15 "%DUMP_FILE%" -dump "%DUMP_FILE%" "%CMPL%\test\test.ci"

SET DUMP_FILE=%BIN%.all.ci
%BIN%\cmpl -X-stdin+steps -std"%CMPL%\lib\stdlib.ci" -profile/p/G/M -api/a/m/d/p/u -asm/n/s -ast -log/15 "%DUMP_FILE%" -dump "%DUMP_FILE%" "%BIN%\libFile.dll" "%BIN%\libGfx.dll" "%CMPL%\lib\cmplGfx\gfxlib.ci" "%CMPL%\test\test.ci"

SET TEST_FILES=%CMPL%\test\*.ci
SET TEST_FILES=%TEST_FILES%;%CMPL%\test\lang\*.ci
SET TEST_FILES=%TEST_FILES%;%CMPL%\test\stdc\*.ci
SET TEST_FILES=%TEST_FILES%;%CMPL%\test\cmplFile\*.ci
SET TEST_FILES=%TEST_FILES%;%CMPL%\test\cmplGfx\*.ci
REM ~ SET TEST_FILES=%TEST_FILES%;%CMPL%\test\cmplGL\*.ci

SET DUMP_FILE=%CMPL%/bin/test.dump.win32.ci
FOR %%f IN (%TEST_FILES%) DO (
	pushd "%%~dpf"
	echo **** running test: %%f
	%BIN%\cmpl -std"%CMPL%\lib\stdlib.ci" -asm/n/s -run/g -log/a "%DUMP_FILE%" -dump "%DUMP_FILE%" "%BIN%\libFile.dll" "%BIN%\libGfx.dll" "%CMPL%\lib\cmplGfx\gfxlib.ci" "%%f"
	IF ERRORLEVEL 1 echo ******** failed: %%f
	popd
)

pause
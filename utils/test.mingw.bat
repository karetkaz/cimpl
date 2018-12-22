@echo off

SET BINDIR=c:/Workspace/tmp
mingw32-make BINDIR="%BINDIR%" clean
mingw32-make -j 12 BINDIR="%BINDIR%" cmpl.exe libFile.dll libOpenGL.dll libGfx.dll

:: dump api for scite
%BINDIR%\cmpl -dump.scite extras/cmpl.api
%BINDIR%\cmpl -dump.scite %BINDIR%/cmplGfx.api %BINDIR%/libGfx.dll lib/cmplGfx/gfxlib.ci

%BINDIR%\cmpl -dump.scite %BINDIR%/libFile.api %BINDIR%/libFile.dll
%BINDIR%\cmpl -dump.scite %BINDIR%/libOpenGL.api %BINDIR%/libOpenGL.dll

:: dump symbols, assembly, syntax tree and global variables (to be compared with previous version to test if the code is generated properly)
%BINDIR%\cmpl -X+steps+fold+fast+asgn-stdin-glob-offsets -debug/G/M -api/d/p/a -asm/n/s -ast -log/15 extras/test.dump.ci -dump extras/test.dump.ci -dump.ast.xml extras/test.dump.xml "test.ci"

:: dump symbols, assembly, syntax tree and global variables (to be compared with previous version to test if the code is generated properly)
%BINDIR%\cmpl -X+steps+fold+fast+asgn-stdin-glob-offsets -debug/G/M -api/d/p/a -asm/n/s -ast -log/15 extras/libs.dump.ci -dump extras/libs.dump.ci %BINDIR%/libFile.dll %BINDIR%/libGfx.dll lib/cmplGfx/gfxlib.ci

:: dump profile data in json format
REM ~ %BINDIR%\cmpl -X-stdin-steps -dump.json extras/test.dump.json -api/a/m/d/p/u -asm/a/n/s -ast/t -profile/t/P/G/M "test.ci"

:: dump profile data in text format
REM ~ %BINDIR%\cmpl -X-stdin+steps -log/15 %BINDIR%/test.dump.ci -dump %BINDIR%/test.dump.ci -api/a/m/d/p/u -asm/a/n/s -ast/t -profile/P/G/M "test.ci"

:: test the virtual machine
REM ~ %BINDIR%\cmpl -vmTest

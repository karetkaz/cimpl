@echo off

SET BINDIR=c:/Workspace/tmp
mingw32-make clean BINDIR="%BINDIR%"
mingw32-make -j 12 cmpl.exe libFile.dll libOpenGL.dll libGfx.dll BINDIR="%BINDIR%"

:: dump api for scite
%BINDIR%\cmpl -dump.scite extras/cmpl.api
%BINDIR%\cmpl -dump.scite %BINDIR%/cmplGfx.api %BINDIR%/libGfx.dll libGfx/gfxlib.ci

%BINDIR%\cmpl -dump.scite %BINDIR%/libFile.api %BINDIR%/libFile.dll
%BINDIR%\cmpl -dump.scite %BINDIR%/libOpenGL.api %BINDIR%/libOpenGL.dll

:: dump symbols, assembly, syntax tree and global variables (to be compared with previous version to test if the code is generated properly)
%BINDIR%\cmpl -X+steps+fold+fast+asgn-stdin-glob-offsets -debug/G/H -api/d/p/a -asm/n/s -ast -log/15 extras/test.dump.ci -dump extras/test.dump.ci -dump.ast.xml extras/test.dump.xml "test.ci"

:: dump symbols, assembly, syntax tree and global variables (to be compared with previous version to test if the code is generated properly)
%BINDIR%\cmpl -X+steps+fold+fast+asgn-stdin-glob-offsets -debug/G/H -api/d/p/a -asm/n/s -ast -log/15 extras/libs.dump.ci -dump extras/libs.dump.ci %BINDIR%/libFile.dll %BINDIR%/libGfx.dll libGfx/gfxlib.ci

:: dump profile data in json format
REM ~ %BINDIR%\cmpl -X-stdin-steps -dump.json extras/test.dump.json -api/a/m/d/p/u -asm/a/n/s -ast/t -profile/t/P/G/H "test.ci"

:: dump profile data in text format
REM ~ %BINDIR%\cmpl -X-stdin+steps -log/15 %BINDIR%/test.dump.ci -dump %BINDIR%/test.dump.ci -api/a/m/d/p/u -asm/a/n/s -ast/t -profile/P/G/H "test.ci"

:: test the virtual machine
REM ~ %BINDIR%\cmpl -vmTest

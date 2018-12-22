SET WATCOM=C:\Users\Admin\Workspace\ow_daily\rel2
SET INCLUDE=%WATCOM%\h
SET LIB=%WATCOM%\lib386
SET PATH=%WATCOM%\binnt;%PATH%

SET SOURCE=%~dp0\src\*.c
SET ROOTDIR=%~dp0

SET OBJECT=%~dp0\out\obj
SET OUTPUT=%~dp0\out

mkdir %OUTPUT%
mkdir %OBJECT%
cd %OBJECT%
owcc -std=c99 %SOURCE% -o %OUTPUT%\cmpl

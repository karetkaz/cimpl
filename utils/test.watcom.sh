#!/bin/sh

# test build and run on a 32 bit platform.
BIN=$PWD/bin
OBJ=$BIN/obj.cc

export WATCOM="$(echo ~/bin/ow_daily/rel2)"
if [ ! -z $WATCOM ] && [ -x $WATCOM ]
then
	export INCLUDE="$WATCOM/lh"
	export LIB="$WATCOM/lib386"
	PATH="$WATCOM/binl:$PATH"
	mkdir -p $OBJ

	ROOT=$PWD
	cd $OBJ
	owcc -std=c99 $ROOT/src/*.c -o $BIN/cmpl
	#owcc -shared -std=c99 -I $PWD/src -o $BIN/libFile.so $PWD/lib/cmplFile/src/file.c
	cd $ROOT
fi

## dump api for scite
$BIN/cmpl -dump.scite $BIN/cmpl.api
#$BIN/cmpl -dump.scite $BIN/libFile.api $BIN/libFile.so
#$BIN/cmpl -dump.scite $BIN/libOpenGL.api $BIN/libOpenGL.so
#$BIN/cmpl -dump.scite $BIN/libGfx.api $BIN/libGfx.so lib/cmplGfx/gfxlib.ci

# dump symbols, assembly, syntax tree and global variables (to be compared with previous version to test if the code is generated properly)
$BIN/cmpl -X+steps+fold+fast+asgn-stdin-glob-offsets -debug/G/M -api/d/p/a -asm/n/s -ast -log/15 extras/test.dump.ci -dump extras/test.dump.ci -dump.ast.xml extras/test.dump.xml "test/test.ci"

# dump symbols, assembly, syntax tree and global variables (to be compared with previous version to test if the code is generated properly)
#$BIN/cmpl -X+steps+fold+fast+asgn-stdin-glob-offsets -debug/G/M -api/d/p/a -asm/n/s -ast -log/15 $BIN/libs.dump.ci -dump extras/libs.dump.ci $BIN/libFile.so $BIN/libGfx.so lib/cmplGfx/gfxlib.ci

# dump profile data in json format
$BIN/cmpl -X-stdin-steps -dump.json $BIN/test.dump.json -api/a/m/d/p/u -asm/a/n/s -ast/t -profile/t/P/G/M "test/test.ci"

# dump profile data in text format
$BIN/cmpl -X-stdin+steps -log/15 $BIN/test.dump.ci -dump $BIN/test.dump.ci -api/a/m/d/p/u -asm/a/n/s -ast/t -profile/P/G/M "test/test.ci"

# test the virtual machine
$BIN/cmpl -vmTest

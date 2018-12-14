#!/bin/sh

# test build and run on a 32 bit platform.

SOURCE=$PWD/src/*.c
ROOTDIR=$PWD

OBJECT=$PWD/out/obj
OUTPUT=$PWD/out

export WATCOM="$(echo ~/bin/ow_daily/rel2)"
if [ ! -z $WATCOM ] && [ -x $WATCOM ]
then
	export INCLUDE="$WATCOM/lh"
	export LIB="$WATCOM/lib386"
	PATH="$WATCOM/binl:$PATH"
	mkdir -p $OBJECT

	ROOT=$PWD
	cd $OBJECT
	owcc -std=c99 $SOURCE -o $OUTPUT/cmpl
	#owcc -shared -std=c99 -I $ROOTDIR/src -o $OUTPUT/libFile.so $ROOTDIR/lib/src/file.c
	cd $ROOT
fi

## dump api for scite
./out/cmpl -dump.scite extras/cmpl.api
#./out/cmpl -dump.scite out/libFile.api out/libFile.so
#./out/cmpl -dump.scite out/libOpenGL.api out/libOpenGL.so
#./out/cmpl -dump.scite out/libGfx.api out/libGfx.so libGfx/gfxlib.ci

# dump symbols, assembly, syntax tree and global variables (to be compared with previous version to test if the code is generated properly)
./out/cmpl -X+steps+fold+fast+asgn-stdin-glob-offsets -debug/G/H -api/d/p/a -asm/n/s -ast -log/15 extras/test.dump.ci -dump extras/test.dump.ci -dump.ast.xml extras/test.dump.xml "test.ci"

# dump symbols, assembly, syntax tree and global variables (to be compared with previous version to test if the code is generated properly)
#./out/cmpl -X+steps+fold+fast+asgn-stdin-glob-offsets -debug/G/H -api/d/p/a -asm/n/s -ast -log/15 extras/libs.dump.ci -dump extras/libs.dump.ci out/libFile.so out/libGfx.so libGfx/gfxlib.ci

# dump profile data in json format
./out/cmpl -X-stdin-steps -dump.json extras/test.dump.json -api/a/m/d/p/u -asm/a/n/s -ast/t -profile/t/P/G/H "test.ci"

# dump profile data in text format
./out/cmpl -X-stdin+steps -log/15 out/test.dump.ci -dump out/test.dump.ci -api/a/m/d/p/u -asm/a/n/s -ast/t -profile/P/G/H "test.ci"

# test the virtual machine
./out/cmpl -vmTest

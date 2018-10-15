#!/bin/sh

# test build and run on a 32 bit platform.

SRCDIR=$PWD/src/
SOURCE=$PWD/src/*.c
PLUGIN=$PWD/src.ext/*.c

OBJECT=$PWD/out/obj
OUTPUT=$PWD/out/cmpl

export WATCOM="$(echo ~/bin/ow_daily/rel2)"
if [ ! -z $WATCOM ] && [ -x $WATCOM ]
then
	export INCLUDE="$WATCOM/lh"
	export LIB="$WATCOM/lib386"
	PATH="$WATCOM/binl:$PATH"
	mkdir -p $OBJECT

	ROOT=$PWD
	cd $OBJECT
	owcc -std=c99 $SOURCE -o $OUTPUT
	cd $ROOT
else
	gcc -m32 -o $OUTPUT $SOURCE -lm -ldl
fi

# dump symbols, syntax tree and global variables (to be compared with files from `extras`)
"$OUTPUT" -debug/G/h -X+steps+fold+fast+asgn-stdin-glob-offsets -api/d/p/a -asm/n/s -ast -dump out/dump.test.ci -dump.ast.xml out/dump.test.xml test.ci

# dump profile data in json format
"$OUTPUT" -X-stdin-steps -dump.json out/dump.test.prof.json -api/a/m/d/p/u -asm/a/n/s -ast/t -profile/t/P/G/H "test.ci"

# dump profile data in text format
"$OUTPUT" -X-stdin+steps -dump out/dump.test.prof.ci -api/a/m/d/p/u -asm/a/n/s -ast/t -profile/P/G/H "test.ci"
#"$OUTPUT" -X+steps -api/a/m/d/p/u -asm/a/n/s -ast/t test.ci

# dump api for scite
"$OUTPUT" -api/a/d -dump.scite extras/stdlib.api

# test the virtual machine
"$OUTPUT" -vmTest

# test File plugin: "$OUTPUT" -run out/libFile.so test/plugin/file.write.ci
# test OpenGL plugin: "$OUTPUT" -run out/libOpenGL.so test/plugin/openGL.triangle.ci

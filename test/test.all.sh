#!/bin/sh

CMPL_HOME="$(dirname "$(dirname "$(readlink -f "$0")")")"
echo "cmpl home is: $CMPL_HOME"
cd $CMPL_HOME

BIN=bin/lnx.gcc
CMPL_BIN=$CMPL_HOME/$BIN

BINW=$CMPL_HOME/bin/lnx.wcc

make clean BINDIR="$BIN"
make -j 12 cmpl libFile.so libGfx.so libOpenGL.so cmpl.js libFile.wasm libGfx.wasm cmpl.dbg.js BINDIR="$BIN"

# build and run test on 32 bit platform
WATCOM="$(echo ~/bin/ow_daily/rel2)"
if [ ! -z $WATCOM ] && [ -d $WATCOM ]
then
	export WATCOM="$WATCOM"
	export INCLUDE="$WATCOM/lh"
	export LIB="$WATCOM/lib386"
	PATH="$WATCOM/binl:$PATH"

	mkdir -p $BINW/obj.cc && cd $BINW/obj.cc
	owcc -xc -std=c99 -o "$BINW/cmpl" $CMPL_HOME/src/*.c

	#owcc -xc -std=c99 -shared -I $CMPL/src -o $BIN/libFile.so $CMPL/lib/cmplFile/src/*.c

	#mkdir -p $BINW/obj.gx && cd $BINW/obj.gx
	#owcc -xc -std=c99 -shared -Wc,-aa -I $CMPL/src -o $BIN/libGfx.dll $CMPL/cmplGfx/src/*.c $CMPL/cmplGfx/src/os_linux/*.c
fi

cd $CMPL_HOME
# dump symbols, assembly, syntax tree and global variables
TEST_FLAGS="$(echo -X+steps-stdin-offsets -asm/m/n/s -debug/g "$CMPL_HOME/test/test.ci")"
$BINW/cmpl -log/d "$BINW.ci" $TEST_FLAGS
$BIN/cmpl -log/d "$BIN.ci" $TEST_FLAGS

## dump api for scite
$CMPL_BIN/cmpl -dump.scite extras/cmpl.api
$CMPL_BIN/cmpl -dump.scite extras/cmplGfx.api "$BIN/libGfx.so" cmplGfx/lib/gfxlib.ci
$CMPL_BIN/cmpl -dump.scite extras/cmplAll.api "$BIN/libFile.so" "$BIN/libOpenGL.so" "$BIN/libGfx.so" cmplGfx/lib/gfxlib.ci

# dump symbols, documentation, assembly, syntax tree and global variables (to be compared with previous version to test if the code is generated properly)
$CMPL_BIN/cmpl -X+steps+fold+fast+asgn-stdin-glob-offsets -debug/G/M -api/a/m/d/p -asm/n/s -ast -doc -log/d/15 "extras/test.dump.ci" -dump.ast.xml "extras/test.dump.xml" -stdlib/stdlib.ci "test/test.ci"

# dump symbols, documentation, assembly, syntax tree and global variables (to be compared with previous version to test if the code is generated properly)
$CMPL_BIN/cmpl -X+steps+fold+fast+asgn-stdin-glob-offsets -debug/G/M -api/a/m/d/p/u -asm/n/s -ast -doc -log/d "extras/libs.dump.ci" -stdlib/stdlib.ci "$BIN/libFile.so" "$BIN/libGfx.so" "cmplGfx/lib/gfxlib.ci"

# dump profile data in text format including function tracing
$CMPL_BIN/cmpl -X-stdin+steps -profile/t/P/G/M -api/a/m/d/p/u -asm/a/n/s -ast/t -doc -log/15 "extras/test.prof.ci" -dump "extras/test.prof.ci" -stdlib/stdlib.ci "test/test.ci"

# dump profile data in json format
$CMPL_BIN/cmpl -X-stdin-steps -profile/t/P/G/M -api/a/m/d/p/u -asm/a/n/s -ast/t -doc -dump.json "extras/test.prof.json" -stdlib/stdlib.ci "test/test.ci"

# test the virtual machine
$CMPL_BIN/cmpl --test-vm
$CMPL_BIN/cmpl>tmp/Execution.md --dump-vm

TEST_FILES="$CMPL_HOME/test/*.ci"
TEST_FILES="$TEST_FILES $CMPL_HOME/test/lang/*.ci"
TEST_FILES="$TEST_FILES $CMPL_HOME/test/stdc/*.ci"
TEST_FILES="$TEST_FILES $CMPL_HOME/cmplFile/test/*.ci"
TEST_FILES="$TEST_FILES $CMPL_HOME/cmplGfx/test/*.ci"
TEST_FILES="$TEST_FILES $CMPL_HOME/cmplGL/test/*.ci"

DUMP_FILE=$CMPL_BIN.dump.ci
$CMPL_BIN/cmpl -X-stdin+steps -asm/n/s -run/g -log/d "$DUMP_FILE" "$CMPL_BIN/libFile.so" "$CMPL_BIN/libGfx.so" "$CMPL_HOME/cmplGfx/lib/gfxlib.ci" "$CMPL_HOME/test/test.ci"
for file in $(echo "$TEST_FILES")
do
	cd $(dirname "$file")
	echo "**** running test: $file"
	$CMPL_BIN/cmpl -X-stdin+steps -run/g -log/a/d "$DUMP_FILE" "$CMPL_BIN/libFile.so" "$CMPL_BIN/libOpenGL.so" "$CMPL_BIN/libGfx.so" "$CMPL_HOME/cmplGfx/lib/gfxlib.ci" "$file"
	if [ "$?" -ne "0" ]; then
		echo "******** failed: $file"
	fi
done

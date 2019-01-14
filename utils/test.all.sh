#!/bin/sh

CMPL="$(dirname "$(dirname "$(readlink -f "$0")")")"
echo "cmpl home is: $CMPL"
cd $CMPL

BIN=bin/lnx.gcc
CMPL_LIB=$CMPL/lib
CMPL_BIN=$CMPL/$BIN

BINW=$CMPL/bin/lnx.wcc

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
	owcc -xc -std=c99 -o "$BINW/cmpl" $CMPL/src/*.c

	#owcc -xc -std=c99 -shared -I $CMPL/src -o $BIN/libFile.so $CMPL/lib/cmplFile/src/*.c

	#mkdir -p $BINW/obj.gx && cd $BINW/obj.gx
	#owcc -xc -std=c99 -shared -Wc,-aa -I $CMPL/src -o $BIN/libGfx.dll $CMPL/lib/cmplGfx/src/*.c $CMPL/lib/cmplGfx/src/os_linux/*.c
fi

cd $CMPL
# dump symbols, assembly, syntax tree and global variables
TEST_FLAGS="$(echo -X+steps-stdin-offsets -asm/m/n/s -debug/g "$CMPL/test/test.ci")"
$BINW/cmpl -log/d "$BINW.ci" $TEST_FLAGS
$BIN/cmpl -log/d "$BIN.ci" $TEST_FLAGS

## dump api for scite
$CMPL_BIN/cmpl -dump.scite extras/cmpl.api
$CMPL_BIN/cmpl -dump.scite extras/cmplGfx.api "$BIN/libGfx.so" lib/cmplGfx/gfxlib.ci
$CMPL_BIN/cmpl -dump.scite extras/cmplAll.api "$BIN/libFile.so" "$BIN/libOpenGL.so" "$BIN/libGfx.so" lib/cmplGfx/gfxlib.ci

# dump symbols, assembly, syntax tree and global variables (to be compared with previous version to test if the code is generated properly)
$CMPL_BIN/cmpl -X+steps+fold+fast+asgn-stdin-glob-offsets -debug/G/M -api/d/p/a/m -asm/n/s -ast -log/d/15 "extras/test.dump.ci" -dump.ast.xml "extras/test.dump.xml" "test/test.ci"

# dump symbols, assembly, syntax tree and global variables (to be compared with previous version to test if the code is generated properly)
$CMPL_BIN/cmpl -X+steps+fold+fast+asgn-stdin-glob-offsets -debug/G/M -api/d/p/a/m -asm/n/s -ast -log/d "extras/libs.dump.ci" "$BIN/libFile.so" "$BIN/libGfx.so" "lib/cmplGfx/gfxlib.ci"

# dump profile data in text format including function tracing
$CMPL_BIN/cmpl -X-stdin+steps -profile/t/P/G/M -api/a/m/d/p/u -asm/a/n/s -ast/t -log/15 "extras/test.prof.ci" -dump "extras/test.prof.ci" "test/test.ci"

# dump profile data in json format
$CMPL_BIN/cmpl -X-stdin-steps -profile/t/P/G/M -api/a/m/d/p/u -asm/a/n/s -ast/t -dump.json "extras/test.prof.json" "test/test.ci"

# test the virtual machine
$CMPL_BIN/cmpl --test-vm
#$CMPL_BIN/cmpl>extras/Reference/Execution.md --dump-vm

TEST_FILES="$CMPL/test/*.ci"
TEST_FILES="$TEST_FILES $CMPL/test/lang/*.ci"
TEST_FILES="$TEST_FILES $CMPL/test/stdc/*.ci"
TEST_FILES="$TEST_FILES $CMPL/test/cmplFile/*.ci"
TEST_FILES="$TEST_FILES $CMPL/test/cmplGfx/*.ci"
TEST_FILES="$TEST_FILES $CMPL/test/cmplGL/*.ci"

DUMP_FILE=$CMPL_BIN.dump.ci
$CMPL_BIN/cmpl -X-stdin+steps -asm/n/s -run/g -log/d "$DUMP_FILE" -std"$CMPL/lib/stdlib.ci" "$CMPL_BIN/libFile.so" "$CMPL_BIN/libGfx.so" "$CMPL_LIB/cmplGfx/gfxlib.ci" "$CMPL/test/test.ci"
for file in $(echo "$TEST_FILES")
do
	cd $(dirname "$file")
	echo "**** running test: $file"
	$CMPL_BIN/cmpl -X-stdin+steps -run/g -log/a/d "$DUMP_FILE" -std"$CMPL/lib/stdlib.ci" "$CMPL_BIN/libFile.so" "$CMPL_BIN/libOpenGL.so" "$CMPL_BIN/libGfx.so" "$CMPL/lib/cmplGfx/gfxlib.ci" "$file"
	if [ "$?" -ne "0" ]; then
		echo "******** failed: $file"
	fi
done

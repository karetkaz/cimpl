#!/bin/sh

CMPL_HOME="$(dirname "$(dirname "$(readlink -f "$0")")")"
echo "cmpl home is: $CMPL_HOME"
cd $CMPL_HOME

BIN=bin/lnx.gcc
CMPL_BIN=$CMPL_HOME/$BIN

BINW=$CMPL_HOME/bin/lnx.wcc

make clean BINDIR="$BIN"
make -j 12 cmpl libFile.so libGfx.so libOpenGL.so cmpl.js libFile.wasm libGfx.wasm BINDIR="$BIN"

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

# test the virtual machine
$BINW/cmpl --test-vm
$BIN/cmpl --test-vm
if [ "$?" -ne "0" ]; then
	exit 1
fi

## dump api for scite including all libraries
$CMPL_BIN/cmpl -dump.scite extras/cmpl.api "$BIN/libFile.so" "$BIN/libOpenGL.so" "$BIN/libGfx.so"
if [ "$?" -ne "0" ]; then
	exit 1
fi

# create reference
$CMPL_BIN/cmpl>'cmplStd/doc/todo/Instructions.md' --dump-vm
rm extras/Cmpl.md
DOC_FILES="$CMPL_HOME/cmplStd/doc/lang/*.md"             # Introduction
DOC_FILES="$DOC_FILES $CMPL_HOME/cmplStd/doc/expr/*.md"  # Expressions
DOC_FILES="$DOC_FILES $CMPL_HOME/cmplStd/doc/stmt/*.md"  # Statements
DOC_FILES="$DOC_FILES $CMPL_HOME/cmplStd/doc/decl/*.md"  # Declarations
DOC_FILES="$DOC_FILES $CMPL_HOME/cmplStd/doc/type/*.md"  # Type system
#DOC_FILES="$DOC_FILES $CMPL_HOME/cmplStd/doc/exec/*.md"  # Execution and vm
printf '%s\n' $DOC_FILES | while read file
do
	cat>>extras/Cmpl.md "$file"
done

# dump symbols, assembly, syntax tree and global variables
TEST_FLAGS="$(echo -X+steps-stdin-offsets -asm/m/n/s -debug/g "$CMPL_HOME/cmplStd/test/test.ci")"
$BINW/cmpl -log/d "$BINW.ci" $TEST_FLAGS
$BIN/cmpl -log/d "$BIN.ci" $TEST_FLAGS

# dump symbols, documentation, assembly, syntax tree and global variables (to be compared with previous version to test if the code is generated properly)
$CMPL_BIN/cmpl -X+steps+fold+fast-stdin-glob-offsets -debug/G/M -api/A/m/d/p -asm/n/s -ast -doc -log/d/15 "extras/dump/test.dump.ci" -dump.ast.xml "extras/dump/test.dump.xml" -stdcmplStd/stdlib.ci "cmplStd/test/test.ci"
# dump symbols, documentation, assembly, syntax tree and global variables (to be compared with previous version to test if the code is generated properly)
$CMPL_BIN/cmpl -X+steps+fold+fast-stdin-glob-offsets -debug/G/M -api/A/m/d/p -asm/n/s -ast -doc -use -log/d "extras/dump/libs.dump.ci" -stdcmplStd/stdlib.ci "$BIN/libFile.so" "$BIN/libGfx.so"
# dump profile data in text format including function tracing
$CMPL_BIN/cmpl -X-stdin+steps -profile/t/P/G/M -api/A/m/d/p -asm/g/n/s -ast/t -doc -use -log/d/15 "extras/dump/test.prof.ci" -stdcmplStd/stdlib.ci "cmplStd/test/test.ci"
# dump profile data in json format
$CMPL_BIN/cmpl -X-stdin-steps -profile/t/P/G/M -api/A/m/d/p -asm/g/n/s -ast/t -doc -use -dump.json "extras/dump/test.prof.json" -stdcmplStd/stdlib.ci "cmplStd/test/test.ci"

TEST_FILES="$CMPL_HOME/cmplStd/test/test.ci"
TEST_FILES="$TEST_FILES $CMPL_HOME/cmplStd/test/demo/*.ci"
TEST_FILES="$TEST_FILES $CMPL_HOME/cmplStd/test/lang/*.ci"
TEST_FILES="$TEST_FILES $CMPL_HOME/cmplStd/test/std/*.ci"

TEST_FILES="$TEST_FILES $CMPL_HOME/cmplFile/test/*.ci"

TEST_FILES="$TEST_FILES $CMPL_HOME/cmplGfx/test/*.ci"
TEST_FILES="$TEST_FILES $CMPL_HOME/cmplGfx/test/demo/*.ci"
TEST_FILES="$TEST_FILES $CMPL_HOME/cmplGfx/todo/*.ci"
TEST_FILES="$TEST_FILES $CMPL_HOME/cmplGfx/todo/demo/*.ci"

TEST_FILES="$TEST_FILES $CMPL_HOME/cmplGL/test/*.ci"

DUMP_FILE=$CMPL_BIN.dump.ci
$CMPL_BIN/cmpl -X-stdin+steps -asm/n/s -run/g -log/d "$DUMP_FILE" "$CMPL_BIN/libFile.so" "$CMPL_BIN/libOpenGL.so" "$CMPL_BIN/libGfx.so" "$CMPL_HOME/cmplStd/test/test.ci"
for file in $(echo "$TEST_FILES")
do
	cd $(dirname "$file")
	echo "**** running test: $file"
	$CMPL_BIN/cmpl -X-stdin+steps -run/g -log/a/d "$DUMP_FILE" "$CMPL_BIN/libFile.so" "$CMPL_BIN/libOpenGL.so" "$CMPL_BIN/libGfx.so" "$file"
	if [ "$?" -ne "0" ]; then
		echo "******** failed: $file"
	fi
done

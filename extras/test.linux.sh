#!/bin/sh

export "CMPL_HOME=$(dirname "$(dirname "$(readlink -f "$0")")")"
echo "cmpl home is: $CMPL_HOME"
cd "$CMPL_HOME" || exit 1

BIN=bin/linux
BIN_WCC=$CMPL_HOME/bin/linux.wcc
BIN_EMC=$CMPL_HOME/extras/demo/emscripten

make clean BINDIR="$BIN"
make -j 12 cmpl libFile.so libGfx.so libOpenGL.so BINDIR="$BIN"

if [ -n "$BIN_EMC" ]; then
  make -j 12 cmpl.js libFile.wasm libGfx.wasm BINDIR="$BIN_EMC"
fi

# build and run test on 32 bit platform
WATCOM="$(echo ~/bin/ow_daily/rel2)"
if [ -n "$BIN_WCC" ] && [ -d "$WATCOM" ]; then
  export WATCOM="$WATCOM"
  export INCLUDE="$WATCOM/lh"
  export LIB="$WATCOM/lib386"
  PATH="$WATCOM/binl:$PATH"

  mkdir -p $BIN_WCC && cd $BIN_WCC
  owcc -xc -std=c99 -o "$BIN_WCC/cmpl" $(echo "$CMPL_HOME/src/*.c")
  cd $CMPL_HOME || exit 1
fi

# test the virtual machine
$BIN_WCC/cmpl --test-vm
if ! $BIN/cmpl --test-vm; then
  echo "virtual machine test failed"
	exit 1
fi

## dump api for scite including all libraries
if ! $BIN/cmpl -dump.scite extras/cmpl.api "$BIN/libFile.so" "$BIN/libGfx.so"; then
  echo "failed to dump compiler api"
	exit 1
fi

# create reference todo: cmpl -doc.md extras/Cmpl.md cmplStd/doc/cmpl.ci
$BIN/cmpl>temp/Instructions.md --dump-vm
DOC_FILES="$CMPL_HOME/cmplStd/doc/lang/*.md"             # Introduction
DOC_FILES="$DOC_FILES $CMPL_HOME/cmplStd/doc/expr/*.md"  # Expressions
DOC_FILES="$DOC_FILES $CMPL_HOME/cmplStd/doc/stmt/*.md"  # Statements
DOC_FILES="$DOC_FILES $CMPL_HOME/cmplStd/doc/decl/*.md"  # Declarations
DOC_FILES="$DOC_FILES $CMPL_HOME/cmplStd/doc/type/*.md"  # Type system
#DOC_FILES="$DOC_FILES $CMPL_HOME/cmplStd/doc/exec/*.md"  # Execution and vm
rm extras/Cmpl.md
printf '%s\n' $DOC_FILES | while read file; do
	cat>>extras/Cmpl.md "$file"
done

# dump symbols, assembly, syntax tree and global variables
TEST_FLAGS="$(echo -X+steps-stdin-offsets -asm/m/n/s -debug/g "$CMPL_HOME/cmplStd/test/test.ci")"
$BIN_WCC/cmpl -log/d "$BIN_WCC.ci" $TEST_FLAGS
if ! $BIN/cmpl -log/d "$BIN.ci" $TEST_FLAGS; then
  echo "main test failed: $TEST_FLAGS"
	exit 1
fi

# dump symbols, documentation, assembly, syntax tree and global variables (to be compared with previous version to test if the code is generated properly)
$BIN/cmpl -X+steps+fold+fast-stdin-glob-offsets -debug/G/M -api/A/m/d/p -asm/n/s -ast -doc -log/d/15 "extras/dump/test.dump.ci" -dump.ast.xml "extras/dump/test.dump.xml" "cmplStd/test/test.ci"
# dump symbols, documentation, assembly, syntax tree and global variables (to be compared with previous version to test if the code is generated properly)
$BIN/cmpl -X+steps+fold+fast-stdin-glob-offsets -debug/G/M -api/A/m/d/p -asm/n/s -ast -doc -use -log/d "extras/dump/libs.dump.ci" "$BIN/libFile.so" "$BIN/libGfx.so"
# dump profile data in text format including function tracing
$BIN/cmpl -X-stdin+steps -profile/t/P/G/M -api/A/m/d/p -asm/g/n/s -ast/t -doc -use -log/d/15 "extras/dump/test.prof.ci" "cmplStd/test/test.ci"
# dump profile data in json format
$BIN/cmpl -X-stdin-steps -profile/t/P/G/M -api/A/m/d/p -asm/g/n/s -ast/t -doc -use -dump.json "extras/dump/test.prof.json" "cmplStd/test/test.ci"

TEST_FILES="$CMPL_HOME/cmplStd/test/test.ci"
TEST_FILES="$TEST_FILES $(echo $CMPL_HOME/cmplStd/test/demo/*.ci)"
TEST_FILES="$TEST_FILES $(echo $CMPL_HOME/cmplStd/test/lang/*.ci)"
TEST_FILES="$TEST_FILES $(echo $CMPL_HOME/cmplStd/test/std/*.ci)"
TEST_FILES="$TEST_FILES $(echo $CMPL_HOME/cmplFile/test/*.ci)"
TEST_FILES="$TEST_FILES $(echo $CMPL_HOME/cmplGfx/test/*.ci)"
TEST_FILES="$TEST_FILES $(echo $CMPL_HOME/cmplGfx/test/demo/*.ci)"
TEST_FILES="$TEST_FILES $(echo $CMPL_HOME/cmplGfx/test/demo.procedural/*.ci)"
TEST_FILES="$TEST_FILES $(echo $CMPL_HOME/cmplGfx/test/demo.widget/*.ci)"
TEST_FILES="$TEST_FILES $(echo $CMPL_HOME/temp/todo.cmplGfx/*.ci)"
TEST_FILES="$TEST_FILES $(echo $CMPL_HOME/temp/todo.cmplGfx/demo/*.ci)"

BIN="$CMPL_HOME/$BIN"
DUMP_FILE=$BIN.dump.ci
$BIN/cmpl -log/d "$DUMP_FILE"
for file in $(echo "$TEST_FILES")
do
	if ! cd "$(dirname "$file")"; then
    echo "**** cannot run test: $file"
    continue
  fi
	if ! $BIN/cmpl -X-stdin+steps -run -log/a/d "$DUMP_FILE" "$BIN/libFile.so" "$BIN/libGfx.so" "$file"; then
		echo "****** test failed: $file"
		continue
	fi
	echo "**** test finished: $file"
done

TEST_FILES="$(echo $CMPL_HOME/cmplGL/test/*.ci)"
for file in $(echo "$TEST_FILES")
do
	if ! cd "$(dirname "$file")"; then
    echo "**** cannot run test: $file"
    continue
  fi
	if ! $BIN/cmpl -X-stdin+steps -run -log/a/d "$DUMP_FILE" "$BIN/libFile.so" "$BIN/libOpenGL.so" "$file"; then
		echo "****** test failed: $file"
		continue
	fi
	echo "**** test finished: $file"
done

# requirements to build and run on linux
# sudo apt install build-essential libjpeg-dev libpng-dev libsdl2-dev freeglut3-dev

#!/bin/sh

export "CMPL_HOME=$(dirname "$(dirname "$(stat -f %N "$0")")")"
echo "cmpl home is: $CMPL_HOME"
cd "$CMPL_HOME" || exit 1

BIN=bin/macos

make clean BINDIR="$BIN"
make -j 12 cmpl libFile.dylib libGfx.dylib BINDIR="$BIN"

# test the virtual machine
if ! $BIN/cmpl --test-vm; then
  echo "virtual machine test failed"
	exit 1
fi

## dump api for scite including all libraries
if ! $BIN/cmpl -dump.scite extras/cmpl.api "$BIN/libFile.dylib" "$BIN/libGfx.dylib"; then
  echo "failed to dump compiler api"
	exit 1
fi

# dump symbols, assembly, syntax tree and global variables
$BIN/cmpl -X+steps-stdin-offsets -log/d "$BIN.ci" -asm/m/n/s -debug/g "$CMPL_HOME/cmplStd/test/test.ci"
# dump symbols, documentation, assembly, syntax tree and global variables (to be compared with previous version to test if the code is generated properly)
$BIN/cmpl -X+steps+fold+fast-stdin-glob-offsets -debug/G/M -api/A/m/d/p -asm/n/s -ast -doc -log/d/15 "extras/dump/test.dump.ci" -dump.ast.xml "extras/dump/test.dump.xml" "cmplStd/test/test.ci"
# dump symbols, documentation, assembly, syntax tree and global variables (to be compared with previous version to test if the code is generated properly)
$BIN/cmpl -X+steps+fold+fast-stdin-glob-offsets -debug/G/M -api/A/m/d/p -asm/n/s -ast -doc -use -log/d "extras/dump/libs.dump.ci" "$BIN/libFile.dylib" "$BIN/libGfx.dylib"
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
	if ! $BIN/cmpl -X-stdin+steps -run -log/a/d "$DUMP_FILE" "$BIN/libFile.dylib" "$BIN/libGfx.dylib" "$file"; then
		echo "****** test failed: $file"
		continue
	fi
	echo "**** test finished: $file"
done

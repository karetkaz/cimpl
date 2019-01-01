#!/bin/sh

BIN=bin

make clean BINDIR="$BIN"
make -j 12 cmpl libFile.so libOpenGL.so libGfx.so cmpl.js cmpl.dbg.js BINDIR="$BIN"

## dump api for scite
$BIN/cmpl -dump.scite extras/cmpl.api
$BIN/cmpl -dump.scite extras/cmplGfx.api "$BIN/libGfx.so" lib/cmplGfx/gfxlib.ci

$BIN/cmpl -dump.scite "$BIN/libFile.api" "$BIN/libFile.so"
$BIN/cmpl -dump.scite "$BIN/libOpenGL.api" "$BIN/libOpenGL.so"

# dump symbols, assembly, syntax tree and global variables (to be compared with previous version to test if the code is generated properly)
$BIN/cmpl -X+steps+fold+fast+asgn-stdin-glob-offsets -debug/G/M -api/d/p/a/m -asm/n/s -ast -log/15 "extras/test.dump.ci" -dump "extras/test.dump.ci" -dump.ast.xml "extras/test.dump.xml" "test/test.ci"

# dump symbols, assembly, syntax tree and global variables (to be compared with previous version to test if the code is generated properly)
$BIN/cmpl -X+steps+fold+fast+asgn-stdin-glob-offsets -debug/G/M -api/d/p/a/m -asm/n/s -ast -log "extras/libs.dump.ci" -dump "extras/libs.dump.ci" "$BIN/libFile.so" "$BIN/libGfx.so" "lib/cmplGfx/gfxlib.ci"

# dump profile data in text format including function tracing
$BIN/cmpl -X-stdin+steps -profile/t/P/G/M -api/a/m/d/p/u -asm/a/n/s -ast/t -log/15 "extras/test.prof.ci" -dump "extras/test.prof.ci" "test/test.ci"

# dump profile data in json format
$BIN/cmpl -X-stdin-steps -profile/t/P/G/M -api/a/m/d/p/u -asm/a/n/s -ast/t -dump.json "extras/test.prof.json" "test/test.ci"

# test the virtual machine
$BIN/cmpl --test-vm
#$BIN/cmpl>extras/Reference/Execution.md --dump-vm

CMPL_HOME="$PWD"
DUMP_FILE="$PWD/bin/test.dump.ci"

TEST_FILES="$CMPL_HOME/test/*.ci"
TEST_FILES="$TEST_FILES $CMPL_HOME/test/lang/*.ci"
TEST_FILES="$TEST_FILES $CMPL_HOME/test/stdc/*.ci"
TEST_FILES="$TEST_FILES $CMPL_HOME/test/cmplFile/*.ci"
TEST_FILES="$TEST_FILES $CMPL_HOME/test/cmplGfx/*.ci"
TEST_FILES="$TEST_FILES $CMPL_HOME/test/cmplGL/*.ci"

for file in $(echo "$TEST_FILES")
do
	cd $(dirname "$file")
	echo "**** running test: $file"
	$CMPL_HOME/bin/cmpl -X-stdin+steps -asm/n/s -run/g -log/a "$DUMP_FILE" -dump "$DUMP_FILE" -std"$CMPL_HOME/lib/stdlib.ci" "$CMPL_HOME/$BIN/libFile.so" "$CMPL_HOME/$BIN/libOpenGL.so" "$CMPL_HOME/$BIN/libGfx.so" "$CMPL_HOME/lib/cmplGfx/gfxlib.ci" "$file"
	if [ "$?" -ne "0" ]; then
		echo "******** failed: $file"
	fi
done

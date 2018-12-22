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
$BIN/cmpl -X+steps+fold+fast+asgn-stdin-glob-offsets -debug/G/M -api/d/p/a -asm/n/s -ast -log/15 extras/test.dump.ci -dump extras/test.dump.ci -dump.ast.xml extras/test.dump.xml "test/test.ci"

# dump symbols, assembly, syntax tree and global variables (to be compared with previous version to test if the code is generated properly)
$BIN/cmpl -X+steps+fold+fast+asgn-stdin-glob-offsets -debug/G/M -api/d/p/a -asm/n/s -ast -log/15 extras/libs.dump.ci -dump extras/libs.dump.ci "$BIN/libFile.so" "$BIN/libGfx.so" lib/cmplGfx/gfxlib.ci

# dump profile data in json format
$BIN/cmpl -X-stdin-steps -dump.json extras/test.dump.json -api/a/m/d/p/u -asm/a/n/s -ast/t -profile/t/P/G/M "test/test.ci"

# dump profile data in text format
$BIN/cmpl -X-stdin+steps -log/15 "$BIN/test.dump.ci" -dump "$BIN/test.dump.ci" -api/a/m/d/p/u -asm/a/n/s -ast/t -profile/P/G/M "test/test.ci"

# test the virtual machine
$BIN/cmpl --test-vm
#$BIN/cmpl>extras/Reference/Execution.md --dump-vm

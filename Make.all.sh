#!/bin/sh

make clean
make -j 12 cmpl libFile.so libOpenGL.so libGfx.so cmpl.js cmpl.dbg.js

## dump api for scite
./out/cmpl -dump.scite extras/cmpl.api
./out/cmpl -dump.scite out/libFile.api out/libFile.so
./out/cmpl -dump.scite out/libOpenGL.api out/libOpenGL.so
./out/cmpl -dump.scite out/libGfx.api out/libGfx.so libGfx/gfxlib.ci

# dump symbols, assembly, syntax tree and global variables (to be compared with previous version to test if the code is generated properly)
./out/cmpl -X+steps+fold+fast+asgn-stdin-glob-offsets -debug/G/H -api/d/p/a -asm/n/s -ast -log/0 extras/dump.test.log -dump extras/dump.test.ci -dump.ast.xml extras/dump.test.xml "test.ci"

# dump profile data in json format
./out/cmpl -X-stdin-steps -dump.json extras/dump.test.json -api/a/m/d/p/u -asm/a/n/s -ast/t -profile/t/P/G/H "test.ci"

# dump profile data in text format
./out/cmpl -X-stdin+steps -log15 out/dump.test.ci -dump out/dump.test.ci -api/a/m/d/p/u -asm/a/n/s -ast/t -profile/P/G/H "test.ci"

# test the virtual machine
./out/cmpl -vmTest

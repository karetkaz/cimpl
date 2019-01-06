#!/bin/sh

# test build and run on a 32 bit platform
export WATCOM="$(echo ~/bin/ow_daily/rel2)"
if [ ! -d $WATCOM ]
then
	echo WATCOM not found
	exit
fi

export INCLUDE="$WATCOM/lh"
export LIB="$WATCOM/lib386"
PATH="$WATCOM/binl:$PATH"

CMPL="$(dirname "$(dirname "$(readlink -f "$0")")")"
echo "cmpl home is: $CMPL"

BIN=$CMPL/bin/l32.wcc
mkdir -p $BIN
cd $BIN

owcc -xc -std=c99 -o "$BIN/cmpl" $CMPL/src/*.c
#owcc -xc -std=c99 -shared -I $CMPL/src -o $BIN/libFile.so $CMPL/lib/cmplFile/src/*.c
#owcc -xc -std=c99 -shared -Wc,-aa -I $CMPL/src -o $BIN/libGfx.dll $CMPL/lib/cmplGfx/src/*.c $CMPL/lib/cmplGfx/src/os_linux/*.c
#cd "$(dirname "$(readlink -f "$0")")"
rm $BIN/*.o

# test the virtual machine
$BIN/cmpl --test-vm

## dump symbols, assembly, syntax tree and global variables
DUMP_FILE=$BIN.ci
$BIN/cmpl -std"$CMPL/lib/stdlib.ci" -X+steps -profile/p/G/M -api/a/m/d/p/u -asm/n/s -ast -log/15 "$DUMP_FILE" -dump "$DUMP_FILE" "$CMPL/test/test.ci"

#~ DUMP_FILE=$BIN.all.ci
#~ $BIN/cmpl -std"$CMPL/lib/stdlib.ci" -X+steps -profile/p/G/M -api/a/m/d/p/u -asm/n/s -ast -log/15 "$DUMP_FILE" -dump "$DUMP_FILE"  "$BIN/libFile.so" "$BIN/libGfx.so" "$CMPL/lib/cmplGfx/gfxlib.ci" "$CMPL/test/test.ci"

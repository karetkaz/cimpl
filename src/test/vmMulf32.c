#include "cmpl.h"
#include "cmplVm.h"
#include "printer.h"

/// callback executed after vm execution is finished
static vmError onHalt(nfcContext args) {
	printFmt(stdout, NULL, "value = %f\n", argf32(args, 0));
	return noError;
}

/// print the generated instructions
static void dumpAsm(rtContext rt) {
	for (size_t pc = rt->vm.pc; pc <= rt->vm.px; ) {
		unsigned char *ip = vmPointer(rt, pc);
		if (ip == NULL) {
			// invalid offset
			return;
		}

		int size = opcode_tbl[*ip].size;
		if (size <= 0) {
			// invalid instruction
			return;
		}

		printFmt(stdout, NULL, "%06x  %.9A\n", pc, ip);
		pc += size;
	}
}

#define dieif(__EXP) do { if (__EXP) { printFmt(stdout, NULL, "%?s:%?u: %s(" #__EXP "): failed to emit instruction\n", __FILE__, __LINE__, __FUNCTION__); abort(); } } while(0)

/// demonstrate how to use of the virtual machine: multiply two float numbers.
/// memory used by the virtual machine is 520 bytes.
/// stack size used for execution is 12 bytes.
int main() {
	// initialize
	char mem[520];         // 0.5 Kb memory
	rtContext rt = rtInit(mem, sizeof(mem));

	// override optimization flags
	//rt->foldCasts = 0;

	// start emitting the instructions
	size_t start = vmInit(rt, 0, onHalt);
	dieif(!emitF64(rt, 16.5));              // push 16.5 as double
	dieif(!emit(rt, f64_f32));              // convert double to float
	dieif(!emitI64(rt, 13));                // push 13 as int64
	dieif(!emit(rt, i64_f32));              // convert int64 to float
	dieif(!emit(rt, f32_mul));              // multiply the values

	// prepare for execution
	rt->vm.px = emitInt(rt, opc_nfc, 0);
	rt->vm.pc = start;
	rt->vm.ss = 12;

	// print emitted instructions
	printFmt(stdout, NULL, "\n-- assembly:\n");
	dumpAsm(rt);

	// execute the bytecode
	printFmt(stdout, NULL, "\n-- execute:\n");
	execute(rt, 0, NULL, NULL);

	return rtClose(rt);
}

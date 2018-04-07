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
		size_t ppc = pc;

		if (ip == NULL) {
			break;
		}
		printFmt(stdout, NULL, "%A\n", ip);
		pc += opcode_tbl[*ip].size;
		if (pc == ppc) {
			break;
		}
	}
}

int main() {
	// initialize
	char mem[512];         // 0.5 Kb memory
	rtContext rt = rtInit(mem, sizeof(mem));

	// override optimization flags
	//rt->foldCasts = 0;

	// start emitting the instructions
	size_t start = vmInit(rt, 0, onHalt);
	emitF64(rt, 16.5);
	emit(rt, f64_f32);
	emitI64(rt, 13);
	emit(rt, i64_f32);
	emit(rt, f32_mul);

	// prepare for execution
	rt->vm.px = emitInt(rt, opc_nfc, 0);
	rt->vm.pc = start;
	rt->vm.ss = 8;

	// print emitted instructions
	printFmt(stdout, NULL, "\n-- assembly:\n");
	dumpAsm(rt);

	// execute the bytecode
	printFmt(stdout, NULL, "\n-- execute:\n");
	execute(rt, 0, NULL, NULL);

	return rtClose(rt);
}
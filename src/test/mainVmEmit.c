#include "cmpl.h"
#include "vmCore.h"
#include "printer.h"

static vmError onHalt(nfcContext args) {
	printf("value = %f", argf32(args, 0));
	return noError;
}

int main() {
	// initialize
	char mem[1024];         // 1 Kb memory
	rtContext rt = rtInit(mem, sizeof(mem));
	vmInit(rt, onHalt);

	// override optimization flags
	rt->foldInstr = 1;

	// start emitting some instructions
	size_t start = emit(rt, markIP);
	emitI64(rt, 16);
	emit(rt, i64_f32);
	emitI64(rt, 13);
	emit(rt, i64_f32);
	emit(rt, f32_mul);

	// prepare for execution
	rt->vm.px = emitInt(rt, opc_nfc, 0);
	rt->vm.pc = start;
	rt->vm.ss = 0;

	// print emitted instructions
	for (size_t pc = start; pc < rt->vm.px; ) {
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

	// execute the bytecode
	execute(rt, 0, NULL, NULL);
	return rtClose(rt);
}
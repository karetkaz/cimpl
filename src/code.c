/*******************************************************************************
 *   File: code.c
 *   Date: 2011/06/23
 *   Desc: bytecode
 *******************************************************************************
 * generate and execute instructions
 */

#include "internal.h"
#include <math.h>
#include <time.h>

const struct opcodeRec opcode_tbl[256] = {
	#define OPCODE_DEF(Name, Code, Size, In, Out, Text) {Code, Size, In, Out, Text},
	#include "defs.inl"
};


/// Bit scan forward. (get the index of the highest bit set)
static inline int32_t bitsf(uint32_t x) {
	int result = 0;
	if ((x & 0x0000ffff) == 0) {
		result += 16;
		x >>= 16;
	}
	if ((x & 0x000000ff) == 0) {
		result += 8;
		x >>= 8;
	}
	if ((x & 0x0000000f) == 0) {
		result += 4;
		x >>= 4;
	}
	if ((x & 0x00000003) == 0) {
		result += 2;
		x >>= 2;
	}
	if ((x & 0x00000001) == 0) {
		result += 1;
	}
	return x ? result : -1;
}

// instead of void *
typedef uint8_t *memptr;
typedef uint32_t *stkptr;

// method tracing
typedef struct trcptr {
	size_t caller;      // Instruction pointer of caller
	size_t callee;      // Instruction pointer of callee
	clock_t func;       // time when the function was invoked
	clock_t stmt;       // time when the statement execution started
	stkptr sp;          // Stack pointer
} *trcptr;

// execution unit
typedef struct vmProcessor {
	/* Stack pointers
	 * the stacks are located at the end of the heap.
	 * base pointer
	 *    is the base of the stack, and should not be modified during execution.
	 * trace pointer
	 *    is available only in debug mode, and gets incremented on each function call.
	 *    it points to debug data for tracing and profiling (struct traceRec)
	 *    the trace pointer must be greater than the base pointer
	 * stack pointer
	 *     is initialized with base pointer + stack size, and is decremented on push.
	 *     it points to local variables, arguments.
	 *     the stack pointer must be greater than the trace pointer
	 */
	stkptr sp;		// Stack pointer
	trcptr tp;		// Trace pointer
	memptr bp;		// Stack base
	size_t ss;		// Stack size

	// Instruction pointer
	memptr ip;

	// multi processing
	unsigned int	cp;			// slaves (join == 0)
	unsigned int	pp;			// parent (main == 0)
} *vmProcessor;

#pragma pack(push, 1)
/// bytecode instruction
typedef struct vmInstruction {
	uint8_t opc;
	union {
		vmValue arg;		// argument (load const)
		uint8_t idx;		// usually index for stack
		int32_t rel:24;		// relative offset (ip, sp) +- 7MB
		struct {
			// used when starting a new parallel task
			uint8_t  dl;	// data to to be copied to stack
			uint16_t cl;	// code to to be executed parallel: 0 means fork
		};
	};
} *vmInstruction;
#pragma pack(pop)

/// Check if the pointer is inside the vm.
static inline int isValidOffset(rtContext rt, void *ptr) {
	if ((unsigned char*)ptr > rt->_mem + rt->_size) {
		return 0;
	}
	if ((unsigned char*)ptr < rt->_mem) {
		return 0;
	}
	return 1;
}

static inline vmInstruction lastIp(rtContext rt) {
	vmInstruction result = vmPointer(rt, rt->vm.pc);
	if (result == NULL) {
		result = (vmInstruction) rt->_beg;
		result->opc = opc_nop;
	}
	return result;
}

// rollback the last emitted instruction.
static inline void rollbackPc(rtContext rt) {
	vmInstruction ip = lastIp(rt);
	if ((void*)ip != rt->_beg) {
		const struct opcodeRec *info = &opcode_tbl[ip->opc];
		rt->vm.ss -= info->stack_out - info->stack_in;
		rt->_beg = (memptr)ip;
	}
}

size_t vmOffset(rtContext rt, void *ptr) {
	if (ptr == NULL) {
		return 0;
	}
	dieif(!isValidOffset(rt, ptr), ERR_INVALID_OFFSET, ((unsigned char*)ptr - rt->_mem));
	return (unsigned char*)ptr - rt->_mem;
}

int testOcp(rtContext rt, size_t offs, vmOpcode opc, vmValue *arg) {
	vmInstruction ip = vmPointer(rt, offs);
	if (opc >= opc_last) {
		trace("invalid opc_x%x", opc);
		return 0;
	}
	if (ip != NULL && ip->opc == opc) {
		if (arg != NULL) {
			*arg = ip->arg;
		}
		return 1;
	}
	return 0;
}

static int removeOpc(rtContext rt, size_t offs) {
	if (offs >= rt->vm.ro && offs <= rt->vm.pc) {
		vmInstruction ip = vmPointer(rt, offs);
		unsigned char *dst = (unsigned char *)ip;
		size_t size = opcode_tbl[ip->opc].size;
		memcpy(dst, dst + size, rt->_beg - dst);
		rt->_beg -= size;
		rt->vm.pc = rt->_beg - rt->_mem;
		return 1;
	}
	return 0;
}

static int decrementStackAccess(rtContext rt, size_t offsBegin, size_t offsEnd, int count) {
	size_t offs;
	for (offs = offsBegin; offs < offsEnd; ) {
		vmInstruction ip = vmPointer(rt, offs);
		int size = opcode_tbl[ip->opc].size;
		if (size <= 0) {
			fatal(ERR_INTERNAL_ERROR": %.*A", offs, ip);
			return 0;
		}
		offs += size;
		switch (ip->opc) {
			default:
				break;

			case opc_set1:
			case opc_set2:
			case opc_set4:
			case opc_dup1:
			case opc_dup2:
			case opc_dup4:
				ip->idx -= count;
				break;

			case opc_ldsp:
				ip->rel -= count * vm_size;
				break;
		}
	}
	return 1;
}

int optimizeAssign(rtContext rt, size_t offsBegin, size_t offsEnd) {
	vmValue arg;
	if (testOcp(rt, offsBegin, opc_dup1, &arg)) {
		// duplicate top of stack then set top of stack
		if (arg.u08 != 0) {
			return 0;
		}
		if (!testOcp(rt, offsEnd, opc_set1, &arg)) {
			return 0;
		}
		if (arg.u08 != 1) {
			return 0;
		}

		if (!removeOpc(rt, offsEnd)) {
			fatal(ERR_INTERNAL_ERROR": %.*A", offsEnd, vmPointer(rt, offsEnd));
			return 0;
		}
		if (!removeOpc(rt, offsBegin)) {
			fatal(ERR_INTERNAL_ERROR": %.*A", offsEnd, vmPointer(rt, offsEnd));
			return 0;
		}
		if (!decrementStackAccess(rt, offsBegin, offsEnd, 1)) {
			fatal(ERR_INTERNAL_ERROR": %.*A", offsBegin, vmPointer(rt, offsBegin));
			return 0;
		}
		return 1;
	}
	if (testOcp(rt, offsBegin, opc_dup2, &arg)) {
		// duplicate top of stack then set top of stack
		if (arg.u08 != 0) {
			return 0;
		}
		if (!testOcp(rt, offsEnd, opc_set2, &arg)) {
			return 0;
		}
		if (arg.u08 != 2) {
			return 0;
		}
		if (!removeOpc(rt, offsEnd)) {
			fatal(ERR_INTERNAL_ERROR": %.*A", offsEnd, vmPointer(rt, offsEnd));
			return 0;
		}
		if (!removeOpc(rt, offsBegin)) {
			fatal(ERR_INTERNAL_ERROR": %.*A", offsEnd, vmPointer(rt, offsEnd));
			return 0;
		}
		if (!decrementStackAccess(rt, offsBegin, offsEnd, 2)) {
			fatal(ERR_INTERNAL_ERROR": %.*A", offsBegin, vmPointer(rt, offsBegin));
			return 0;
		}
		return 1;
	}
	return 0;
}

size_t emitOpc(rtContext rt, vmOpcode opc, vmValue arg) {
	libc *nativeCalls = rt->vm.nfc;
	vmInstruction ip = (vmInstruction)rt->_beg;

	dieif((unsigned char*)ip + 16 >= rt->_end, ERR_MEMORY_OVERRUN);

	/* TODO: set max stack used.
	switch (opc) {
		case opc_dup1:
		case opc_set1:
			if (rt->vm.su < arg.i32 + 1)
				rt->vm.su = arg.i32 + 1;
			break;
		case opc_dup2:
		case opc_set2:
			if (rt->vm.su < arg.i32 + 2)
				rt->vm.su = arg.i32 + 2;
			break;
		case opc_dup4:
		case opc_set4:
			if (rt->vm.su < arg.i32 + 4)
				rt->vm.su = arg.i32 + 4;
			break;
		case opc_ldi:
		case opc_ldi1:
		case opc_ldi2:
		case opc_ldi4:
		case opc_ldi8:
		case opc_ldiq:
		case opc_sti:
		case opc_sti1:
		case opc_sti2:
		case opc_sti4:
		case opc_sti8:
		case opc_stiq:
			ip = lastIp(rt);
			if (ip->opc == opc_ldsp) {
				if (rt->vm.su < arg.i32 / 4)
					rt->vm.su = arg.i32 / 4;
			}
			break;
	}
	// */

	if (opc == markIP) {
		return rt->_beg - rt->_mem;
	}

	// load / store
	else if (opc == opc_ldi) {
		dieif (arg.i32 != arg.i64, ERR_INTERNAL_ERROR);
		switch (arg.i32) {
			case 1:
				opc = opc_ldi1;
				break;

			case 2:
				opc = opc_ldi2;
				break;

			case 4:
				opc = opc_ldi4;
				break;

			case 8:
				opc = opc_ldi8;
				break;

			case 16:
				opc = opc_ldiq;
				break;

			default: // we have src on the stack
				// push dst
				if (!emitInt(rt, opc_ldsp, 4 - arg.i32)) {
					trace(ERR_INTERNAL_ERROR);
					return 0;
				}
				// copy
				if (!emitInt(rt, opc_move, -arg.i32)) {
					trace(ERR_INTERNAL_ERROR);
					return 0;
				}
				// create n bytes on stack
				opc = opc_spc;
				break;
		}
	}
	else if (opc == opc_sti) {
		dieif (arg.i32 != arg.i64, ERR_INTERNAL_ERROR);
		switch (arg.i32) {
			case 1:
				opc = opc_sti1;
				break;

			case 2:
				opc = opc_sti2;
				break;

			case 4:
				opc = opc_sti4;
				break;

			case 8:
				opc = opc_sti8;
				break;

			case 16:
				opc = opc_stiq;
				break;

			default: // we have dst on the stack
				// push dst
				if (!emitInt(rt, opc_ldsp, sizeof(vmOffs))) {
					trace(ERR_INTERNAL_ERROR);
					return 0;
				}
				// copy
				if (!emitInt(rt, opc_move, arg.i32)) {
					trace(ERR_INTERNAL_ERROR);
					return 0;
				}
				ip = lastIp(rt);
				if (ip->opc == opc_ldsp) {
					//~ if we copy from the stack to the stack
					//~ do not remove more elements than we copy
					if (arg.i64 > ip->rel) {
						arg.i64 = ip->rel;
					}
				}
				// remove n bytes from stack
				arg.i64 = -arg.i64;
				opc = opc_spc;
				break;
		}
	}
	else if (opc == opc_drop) {
		opc = opc_spc;
		arg.i64 = -arg.i64;
	}

	if (opc >= opc_last) {
		trace("invalid opc_x%x", opc);
		return 0;
	}

	// Optimize
	switch (opc) {
		default:
			break;

		case opc_lc32:
			if (!rt->foldInstr) {
				break;
			}
			if (arg.i64 == 0) {
				opc = opc_lzx1;
				ip = lastIp(rt);
				if (ip->opc == opc_lzx1) {
					opc = opc_lzx2;
					rollbackPc(rt);
				}
			}
			break;

		case opc_lc64:
			if (!rt->foldInstr) {
				break;
			}
			if (arg.i64 == 0) {
				opc = opc_lzx2;
				ip = lastIp(rt);
				if (ip->opc == opc_lzx2) {
					opc = opc_lzx4;
					rollbackPc(rt);
				}
			}
			break;

		case opc_ldi1:/* TODO: sign or zero extend
			ip = lastIp(rt);
			if (ip->opc == opc_ldsp && ((ip->rel & (vm_size-1)) == 0) && ((ip->rel / vm_size) < vm_regs)) {
				arg.i32 = ip->rel / vm_size;
				opc = opc_dup1;
				rt->_beg = (memptr)ip;
				rt->vm.ss -= 1;
			}*/
			break;

		case opc_ldi2:/* TODO: sign or zero extend
			ip = lastIp(rt);
			if (ip->opc == opc_ldsp && ((ip->rel & (vm_size-1)) == 0) && ((ip->rel / vm_size) < vm_regs)) {
				arg.i32 = ip->rel / vm_size;
				opc = opc_dup1;
				rt->_beg = (memptr)ip;
				rt->vm.ss -= 1;
			}*/
			break;

		case opc_ldi4:
			if (!rt->fastMemory) {
				break;
			}
			ip = lastIp(rt);
			if (ip->opc == opc_ldsp && ((ip->rel & (vm_size - 1)) == 0) && ((ip->rel / vm_size) < vm_regs)) {
				arg.i32 = ip->rel / vm_size;
				opc = opc_dup1;
				rollbackPc(rt);
			}
			else if (ip->opc == opc_lref) {
				arg.i64 = ip->arg.u32;
				if (arg.i64 == arg.i24) {
					opc = opc_ld32;
					rollbackPc(rt);
				}
			}
			break;

		case opc_sti4:
			if (!rt->fastMemory) {
				break;
			}
			ip = lastIp(rt);
			if (ip->opc == opc_ldsp && ((ip->rel & (vm_size - 1)) == 0) && ((ip->rel / vm_size) < vm_regs)) {
				arg.i32 = ip->rel / vm_size;
				opc = opc_set1;
				rollbackPc(rt);
			}
			else if (ip->opc == opc_lref) {
				arg.i64 = ip->arg.u32;
				if (arg.i64 == arg.i24) {
					opc = opc_st32;
					rollbackPc(rt);
				}
			}
			break;

		case opc_ldi8:
			if (!rt->fastMemory) {
				break;
			}
			ip = lastIp(rt);
			if (ip->opc == opc_ldsp && ((ip->rel & (vm_size - 1)) == 0) && ((ip->rel / vm_size) < vm_regs)) {
				arg.i32 = ip->rel / vm_size;
				opc = opc_dup2;
				rollbackPc(rt);
			}
			else if (ip->opc == opc_lref) {
				arg.i64 = ip->arg.u32;
				if (arg.i64 == arg.i24) {
					opc = opc_ld64;
					rollbackPc(rt);
				}
			}
			break;

		case opc_sti8:
			if (!rt->fastMemory) {
				break;
			}
			ip = lastIp(rt);
			if (ip->opc == opc_ldsp && ((ip->rel & (vm_size - 1)) == 0) && ((ip->rel / vm_size) < vm_regs)) {
				arg.i32 = ip->rel / vm_size;
				opc = opc_set2;
				rollbackPc(rt);
			}
			else if (ip->opc == opc_lref) {
				arg.i64 = ip->arg.u32;
				if (arg.i64 == arg.i24) {
					opc = opc_st64;
					rollbackPc(rt);
				}
			}
			break;

		case opc_ldiq:
			if (!rt->fastMemory) {
				break;
			}
			ip = lastIp(rt);
			if (ip->opc == opc_ldsp && ((ip->rel & (vm_size - 1)) == 0) && ((ip->rel / vm_size) <= vm_regs)) {
				arg.i32 = ip->rel / vm_size;
				opc = opc_dup4;
				rollbackPc(rt);
			}
			break;

		case opc_stiq:
			if (!rt->fastMemory) {
				break;
			}
			ip = lastIp(rt);
			if (ip->opc == opc_ldsp && ((ip->rel & (vm_size - 1)) == 0) && ((ip->rel / vm_size) <= vm_regs)) {
				arg.i32 = ip->rel / vm_size;
				opc = opc_set4;
				rollbackPc(rt);
			}
			break;

		// shl, shr, sar, and with immediate value.
		case b32_shl:
			if (!rt->foldInstr) {
				break;
			}
			ip = lastIp(rt);
			if (ip->opc == opc_lc32) {
				arg.i32 = b32_bit_shl | (ip->arg.i32 & 0x3f);
				opc = b32_bit;
				rollbackPc(rt);
			}
			break;

		case b32_shr:
			if (!rt->foldInstr) {
				break;
			}
			ip = lastIp(rt);
			if (ip->opc == opc_lc32) {
				arg.i32 = b32_bit_shr | (ip->arg.i32 & 0x3f);
				opc = b32_bit;
				rollbackPc(rt);
			}
			break;

		case b32_sar:
			if (!rt->foldInstr) {
				break;
			}
			ip = lastIp(rt);
			if (ip->opc == opc_lc32) {
				arg.i32 = b32_bit_sar | (ip->arg.i32 & 0x3f);
				opc = b32_bit;
				rollbackPc(rt);
			}
			break;

		case b32_and:
			if (!rt->foldInstr) {
				break;
			}
			ip = lastIp(rt);
			if (ip->opc == opc_lc32 && (ip->arg.i32 & (ip->arg.i32 + 1)) == 0) {
				arg.i32 = b32_bit_and | (bitsf(ip->arg.u32 + 1) & 0x3f);
				opc = b32_bit;
				rollbackPc(rt);
			}
			break;

		//* TODO: eval Arithmetic using vm.
		case opc_not:
			if (!rt->foldInstr) {
				break;
			}
			ip = lastIp(rt);
			if (ip->opc == opc_not) {
				rt->_beg = (memptr) ip;
				ip->opc = opc_nop;
				return rt->vm.pc;
			}
			break;

		case i32_neg:
			if (!rt->foldInstr) {
				break;
			}
			ip = lastIp(rt);
			if (ip->opc == i32_neg) {
				rt->_beg = (memptr) ip;
				ip->opc = opc_nop;
				return rt->vm.pc;
			}
			if (ip->opc == opc_lc32 && rt->foldConst) {
				ip->arg.i32 = -ip->arg.i32;
				return rt->vm.pc;
			}
			break;

		case i64_neg:
			if (!rt->foldInstr) {
				break;
			}
			ip = lastIp(rt);
			if (ip->opc == i64_neg) {
				rt->_beg = (memptr) ip;
				ip->opc = opc_nop;
				return rt->vm.pc;
			}
			if (ip->opc == opc_lc64 && rt->foldConst) {
				ip->arg.i64 = -ip->arg.i64;
				return rt->vm.pc;
			}
			break;

		case f32_neg:
			if (!rt->foldInstr) {
				break;
			}
			ip = lastIp(rt);
			if (ip->opc == f32_neg) {
				rt->_beg = (memptr) ip;
				ip->opc = opc_nop;
				return rt->vm.pc;
			}
			if (ip->opc == opc_lf32 && rt->foldConst) {
				ip->arg.f32 = -ip->arg.f32;
				return rt->vm.pc;
			}
			break;

		case f64_neg:
			if (!rt->foldInstr) {
				break;
			}
			ip = lastIp(rt);
			if (ip->opc == f64_neg) {
				rt->_beg = (memptr) ip;
				ip->opc = opc_nop;
				return rt->vm.pc;
			}
			if (ip->opc == opc_lf64 && rt->foldConst) {
				ip->arg.f64 = -ip->arg.f64;
				return rt->vm.pc;
			}
			break;

		case i32_add:
			if (!rt->foldInstr) {
				break;
			}
			ip = lastIp(rt);
			if (ip->opc == opc_lc32) {
				arg.i64 = ip->arg.i32;
				if (arg.i24 == arg.i64) {
					opc = opc_inc;
					rollbackPc(rt);
				}
			}
			break;

		case i32_sub:
			if (!rt->foldInstr) {
				break;
			}
			ip = lastIp(rt);
			if (ip->opc == opc_lc32) {
				arg.i64 = -ip->arg.i32;
				if (arg.i24 == arg.i64) {
					opc = opc_inc;
					rollbackPc(rt);
				}
			}
			break;

		case opc_inc:
			if (arg.i64 == 0 && rt->foldConst) {
				return rt->vm.pc;
			}
			if (!rt->foldInstr) {
				break;
			}
			ip = lastIp(rt);
			if (ip->opc == opc_inc) {
				vmValue tmp;
				tmp.i64 = arg.i64 + ip->rel;
				if (tmp.i24 == tmp.i64) {
					ip->rel = tmp.i24;
					return rt->vm.pc;
				}
			}
			else if (ip->opc == opc_ldsp) {
				vmValue tmp;
				if (arg.i64 < 0) {
					// local variable bound error.
					trace(ERR_INTERNAL_ERROR);
					return 0;
				}
				tmp.i64 = arg.i64 + ip->rel;
				if (tmp.i24 == tmp.i64) {
					ip->rel = tmp.i24;
					return rt->vm.pc;
				}
			}
			else if (ip->opc == opc_lref) {
				vmValue tmp;
				if (arg.i64 < 0) {
					// global variable bound error.
					trace(ERR_INTERNAL_ERROR);
					return 0;
				}
				tmp.i64 = arg.i64 + ip->rel;
				if (tmp.i24 == tmp.i64) {
					ip->rel = tmp.i24;
					return rt->vm.pc;
				}
			}
			break;

		case opc_mad:
			if (!rt->foldInstr) {
				break;
			}
			if (arg.i64 == 0) {
				opc = opc_inc;
				rollbackPc(rt);
			}
			if (arg.i64 == 1) {
				ip = lastIp(rt);
				if (ip->opc == opc_lc32) {
					arg.i64 = ip->arg.i32;
					if (arg.i24 == arg.i64) {
						opc = opc_inc;
						rollbackPc(rt);
					}
				} else {
					opc = i32_add;
				}
			}
			break;

		//* TODO: eval Conversions using vm.
		case u32_i64:
			if (!rt->foldCasts) {
				break;
			}
			ip = lastIp(rt);
			if (ip->opc == opc_lzx1) {
				opc = opc_lzx2;
				rollbackPc(rt);
			}
			else if (ip->opc == opc_lc32) {
				arg.i64 = ip->arg.u32;
				opc = opc_lc64;
				rollbackPc(rt);
			}
			break;

		case i32_bol:
			if (!rt->foldCasts) {
				break;
			}
			ip = lastIp(rt);
			if (ip->opc == opc_lc32) {
				arg.i64 = ip->arg.i32 != 0;
				opc = opc_lc32;
				rollbackPc(rt);
			}
			break;

		case i32_f32:
			if (!rt->foldCasts) {
				break;
			}
			ip = lastIp(rt);
			if (ip->opc == opc_lc32) {
				arg.f32 = (float32_t) ip->arg.i32;
				opc = opc_lf32;
				rollbackPc(rt);
				logif(ip->arg.i32 != arg.f32, "inexact cast: %d => %f", ip->arg.i32, arg.f32);
			}
			break;

		case i32_i64:
			if (!rt->foldCasts) {
				break;
			}
			ip = lastIp(rt);
			if (ip->opc == opc_lzx1) {
				opc = opc_lzx2;
				rollbackPc(rt);
			}
			else if (ip->opc == opc_lc32) {
				arg.i64 = ip->arg.i32;
				opc = opc_lc64;
				rollbackPc(rt);
			}
			break;

		case i32_f64:
			if (!rt->foldCasts) {
				break;
			}
			ip = lastIp(rt);
			if (ip->opc == opc_lc32) {
				arg.f64 = ip->arg.i32;
				opc = opc_lf64;
				rollbackPc(rt);
				logif(ip->arg.i32 != arg.f64, "inexact cast: %d => %F", ip->arg.i32, arg.f64);
			}
			break;

		case i64_i32:
			if (!rt->foldCasts) {
				break;
			}
			ip = lastIp(rt);
			if (ip->opc == opc_lzx2) {
				opc = opc_lzx1;
				rollbackPc(rt);
			}
			else if (ip->opc == opc_lc64) {
				arg.i64 = ip->arg.i64;
				opc = opc_lc32;
				rollbackPc(rt);
				logif(ip->arg.i64 != arg.i32, "inexact cast: %D => %d", ip->arg.i64, arg.i32);
				logif(ip->arg.i64 != arg.u32, "inexact cast: %D => %u", ip->arg.i64, arg.u32);
			}
			break;

		case i64_f32:
			if (!rt->foldCasts) {
				break;
			}
			ip = lastIp(rt);
			if (ip->opc == opc_lc64) {
				arg.f32 = (float32_t) ip->arg.i64;
				opc = opc_lf32;
				rollbackPc(rt);
				logif(ip->arg.i64 != arg.f32, "inexact cast: %D => %f", ip->arg.i64, arg.f32);
			}
			break;

		case i64_bol:
			if (!rt->foldCasts) {
				break;
			}
			ip = lastIp(rt);
			if (ip->opc == opc_lzx2) {
				opc = opc_lzx1;
				rollbackPc(rt);
			}
			if (ip->opc == opc_lc64) {
				arg.i64 = ip->arg.i64 != 0;
				opc = opc_lc32;
				rollbackPc(rt);
			}
			break;

		case i64_f64:
			if (!rt->foldCasts) {
				break;
			}
			ip = lastIp(rt);
			if (ip->opc == opc_lc64) {
				arg.f64 = (float64_t) ip->arg.i64;
				opc = opc_lf64;
				rollbackPc(rt);
				logif(ip->arg.i64 != arg.f64, "inexact cast: %D => %F", ip->arg.i64, arg.f64);
			}
			break;

		case f32_i32:
			if (!rt->foldCasts) {
				break;
			}
			ip = lastIp(rt);
			if (ip->opc == opc_lf32) {
				arg.i64 = (int64_t) ip->arg.f32;
				opc = opc_lc32;
				rollbackPc(rt);
				logif(ip->arg.f32 != arg.i32, "inexact cast: %f => %d", ip->arg.f32, arg.i32);
			}
			break;

		case f32_bol:
			if (!rt->foldCasts) {
				break;
			}
			ip = lastIp(rt);
			if (ip->opc == opc_lf32) {
				arg.i64 = ip->arg.f32 != 0;
				opc = opc_lc32;
				rollbackPc(rt);
			}
			break;

		case f32_i64:
			if (!rt->foldCasts) {
				break;
			}
			ip = lastIp(rt);
			if (ip->opc == opc_lf32) {
				arg.i64 = (int64_t) ip->arg.f32;
				opc = opc_lc64;
				rollbackPc(rt);
				logif(ip->arg.f32 != arg.i64, "inexact cast: %f => %D", ip->arg.f32, arg.i64);
			}
			break;

		case f32_f64:
			if (!rt->foldCasts) {
				break;
			}
			ip = lastIp(rt);
			if (ip->opc == opc_lf32) {
				arg.f64 = ip->arg.f32;
				opc = opc_lf64;
				rollbackPc(rt);
				logif(ip->arg.f32 != arg.f64, "inexact cast: %f => %F", ip->arg.f32, arg.f64);
			}
			break;

		case f64_i32:
			if (!rt->foldCasts) {
				break;
			}
			ip = lastIp(rt);
			if (ip->opc == opc_lf64) {
				arg.i64 = (int64_t) ip->arg.f64;
				opc = opc_lc32;
				rollbackPc(rt);
				logif(ip->arg.f64 != arg.i32, "inexact cast: %F => %d", ip->arg.f64, arg.i32);
			}
			break;

		case f64_f32:
			if (!rt->foldCasts) {
				break;
			}
			ip = lastIp(rt);
			if (ip->opc == opc_lf64) {
				arg.f32 = (float32_t) ip->arg.f64;
				opc = opc_lf32;
				rollbackPc(rt);
				logif(ip->arg.f64 != arg.f32, "inexact cast: %F => %f", ip->arg.f64, arg.f32);
			}
			break;

		case f64_i64:
			if (!rt->foldCasts) {
				break;
			}
			ip = lastIp(rt);
			if (ip->opc == opc_lf64) {
				arg.i64 = (int64_t) ip->arg.f64;
				opc = opc_lc64;
				rollbackPc(rt);
				logif(ip->arg.f64 != arg.i64, "inexact cast: %F => %D", ip->arg.f64, arg.i64);
			}
			break;

		case f64_bol:
			if (!rt->foldCasts) {
				break;
			}
			ip = lastIp(rt);
			if (ip->opc == opc_lf64) {
				arg.i64 = ip->arg.f64 != 0;
				opc = opc_lc32;
				rollbackPc(rt);
			}
			break;

		/* mul, div, mod
		case u32_mod:
			ip = vmPointer(s, s->vm.pc);
			if (ip->opc == opc_lc32) {
				int x = ip->arg.i32;
				// if constant contains only one bit
				if (x > 0 && lobit(x) == x) {
					ip->arg.i32 -= 1;
					opc = b32_and;
				}
			}
			break;

		case u32_div:
			ip = vmPointer(s, s->vm.pc);
			if (ip->opc == opc_lc32) {
				int x = ip->arg.i32;
				// if constant contains only one bit
				if (x > 0 && lobit(x) == x) {
					ip->arg.i32 ?= 1;
					opc = b32_shr;
				}
			}
			break;
		// */

		// conditional jumps
		case opc_jnz:
			if (!rt->foldInstr) {
				break;
			}
			ip = lastIp(rt);
			if (ip->opc == opc_not) {
				rt->_beg = (memptr) ip;
				opc = opc_jz;
			}
			// i32 to boolean can be skipped
			else if (ip->opc == i32_bol) {
				rt->_beg = (memptr) ip;
			}
			break;

		case opc_jz:
			if (!rt->foldInstr) {
				break;
			}
			ip = lastIp(rt);
			if (ip->opc == opc_not) {
				rt->_beg = (memptr) ip;
				opc = opc_jnz;
			}
			// i32 to boolean can be skipped
			else if (ip->opc == i32_bol) {
				rt->_beg = (memptr) ip;
			}
			break;

		case opc_spc:
			if (arg.i64 == 0) {
				return rt->_beg - rt->_mem;
			}
			/* TODO: merge stack allocations
			ip = lastIp(rt);
			if (ip->opc == opc_spc) {
				stkval tmp;
				tmp.i8 = arg.i64 + ip->rel;
				if (tmp.rel == tmp.i8) {
					rt->_beg = (memptr)ip;
					rt->vm.ss -= ip->rel / vm_size;
					arg.i64 += ip->rel;
				}
			} */
			break;

		case opc_jmpi:
			/* TODO: only if we have no jump here
			ip = lastIp(rt);
			if (ip->opc == opc_jmpi) {
				//~ rt->_beg = (memptr)ip;
				return rt->vm.pc;
			}*/
			break;
	}

	ip = (vmInstruction)rt->_beg;

	ip->opc = opc;
	ip->arg = arg;
	rt->vm.pc = rt->_beg - rt->_mem;	// previous program pointer is here

	// post checks
	if ((opc == opc_jmp || opc == opc_jnz || opc == opc_jz) && arg.i64 != 0) {
		ip->rel -= rt->vm.pc;
		dieif((ip->rel + rt->vm.pc) != arg.u64, ERR_INTERNAL_ERROR);
	}
	else switch (opc) {
		default:
			break;

		case opc_spc:
			if (arg.i64 > 0x7fffff) {
				trace("local variable is too large: %D", arg.i64);
				return 0;
			}
			dieif(ip->rel != arg.i64, ERR_INTERNAL_ERROR);
			break;

		case opc_st64:
		case opc_ld64:
			dieif((ip->rel & 7) != 0, ERR_INTERNAL_ERROR);
			dieif(ip->rel != arg.i64, ERR_INTERNAL_ERROR);
			break;

		case opc_st32:
		case opc_ld32:
			dieif((ip->rel & 3) != 0, ERR_INTERNAL_ERROR);
			dieif(ip->rel != arg.i64, ERR_INTERNAL_ERROR);
			break;

		case opc_ldsp:
		case opc_move:
		case opc_inc:
		case opc_mad:
			dieif(ip->rel != arg.i64, ERR_INTERNAL_ERROR);
			break;
	}

	switch (opc) {
		error_opc:
			// invalid instruction
			return 0;

		error_stc:
			// stack underflow
			return 0;

		#define STOP(__ERR, __CHK) if (__CHK) goto __ERR
		#define NEXT(__IP, __SP, __CHK)\
			STOP(error_stc, (ssize_t)rt->vm.ss < (ssize_t)(__CHK));\
			rt->vm.ss += (__SP);\
			rt->_beg += (__IP);
		#include "code.inl"
	}

	if (opc == opc_call) {
		// each call ends with a return,
		// which drops ip from the top of the stack.
		rt->vm.ss -= 1;
	}

	dbgEmit("pc[%d]: sp(%d): %A", rt->vm.pc, rt->vm.ss, ip);
	return rt->vm.pc;
}
int fixJump(rtContext rt, size_t src, size_t dst, ssize_t stc) {
	dieif(stc > 0 && stc & 3, ERR_INTERNAL_ERROR);
	if (src != 0) {
		vmInstruction ip = vmPointer(rt, src);
		switch (ip->opc) {
			default:
				fatal(ERR_INTERNAL_ERROR);
				return 0;

			case opc_task:
				ip->dl = (uint8_t) (stc / 4);
				ip->cl = (uint16_t) (dst - src);
				dieif(ip->dl != stc / 4, ERR_INTERNAL_ERROR);
				dieif(ip->cl != dst - src, ERR_INTERNAL_ERROR);
				return 1;

			//~ case opc_ldsp:
			//~ case opc_call:
			case opc_jmp:
			case opc_jnz:
			case opc_jz:
				ip->rel = (int32_t) (dst - src);
				dieif(ip->rel != (int32_t) (dst - src), ERR_INTERNAL_ERROR);
				break;
		}
	}
	if (stc != -1) {
		rt->vm.ss = stc / vm_size;
	}
	return 1;
}

// TODO: to be removed.
static inline void vmTrace(vmProcessor pu, int cp, char *msg) {
	trace("%s: {pu:%d, ip:%x, bp:%x, sp:%x, stack:%d, parent:%d, children:%d}", msg, cp, pu[cp].ip, pu[cp].bp, pu[cp].sp, pu[cp].ss, pu[cp].pp, pu[cp].cp);
	(void)pu;
	(void)cp;
	(void)msg;
}

/// Try to start a new worker for task.
static inline int vmFork(vmProcessor pu, int n, unsigned master, unsigned cl) {
	// find an empty processor
	int slave = master + 1;
	while (slave < n) {
		if (pu[slave].ip == NULL) {
			break;
		}
		slave += 1;
	}

	if (slave < n) {
		// set workers, parent, ip, sp and copy stack
		pu[master].cp++;
		pu[slave].cp = 0;
		pu[slave].pp = master;

		pu[slave].ip = pu[master].ip;
		pu[slave].sp = (stkptr)pu[slave].bp + pu[slave].ss - cl;
		memcpy(pu[slave].sp, pu[master].sp, cl * vm_size);

		vmTrace(pu, master, "master");
		vmTrace(pu, slave, "slave");

		return slave;
	}
	return 0;
}

/// Wait for worker to finish.
static inline int vmJoin(vmProcessor pu, unsigned slave, int wait) {
	int master = pu[slave].pp;
	vmTrace(pu, slave, "join");

	// slave proc
	if (master != slave) {
		if (pu[slave].cp == 0) {
			pu[slave].ip = NULL;
			pu[master].cp -= 1;
			return 1;
		}
		return 0;
	}

	if (pu[slave].cp > 0) {
		return !wait;
	}

	// processor can continue to work
	return 1;
}

/// Check for stack overflow.
static inline int ovf(vmProcessor pu) {
	return (memptr)pu->sp < (memptr)pu->tp;
}

/** manage function call stack traces
 * @param rt runtime context
 * @param sp tack pointer (for tracing arguments
 * @param caller the address of the caller, NULL on start and end.
 * @param callee the called function address, -1 on leave.
 * @param extra TO BE REMOVED
 */
static inline vmError traceCall(rtContext rt, void *sp, size_t caller, size_t callee) {
	dbgn (*debugger)(dbgContext, vmError, size_t, void*, size_t, size_t) = rt->dbg->debug;
	clock_t now = clock();
	vmProcessor pu = rt->vm.cell;
	if (rt->dbg == NULL) {
		return noError;
	}
	// trace statement
	if (callee == 0) {
		fatal(ERR_INTERNAL_ERROR);
	}
	else if ((ssize_t)callee < 0) {
		// trace leave
		trcptr tp = pu->tp;
		int recursive = 0;

		if (tp - (trcptr)pu->bp < 1) {
			trace("stack underflow(tp: %d, sp: %d)", tp - (trcptr)pu->bp, pu->sp - (stkptr)pu->bp);
			return illegalState;
		}

		pu->tp -= 1;
		for (tp = (trcptr)pu->bp; tp < pu->tp; tp++) {
			if (tp->callee == pu->tp->callee) {
				recursive = 1;
				break;
			}
		}

		tp = pu->tp;
		clock_t ticks = now - tp->func;
		dbgn calleeFunc = mapDbgFunction(rt, tp->callee);
		dbgn callerFunc = mapDbgFunction(rt, tp->caller);
		//dbgn callerStmt = mapDbgStatement(rt, tp->caller);
		if (calleeFunc != NULL) {
			calleeFunc->exec += callee == (size_t) -1;
			if (!recursive) {
				calleeFunc->total += ticks;
			}
			calleeFunc->self += ticks;
		}
		if (callerFunc != NULL) {
			callerFunc->self -= ticks;
		}
		/* TODO: update caller statement times
		if (callerStmt != NULL) {
			if (!recursive) {
				callerStmt->self -= ticks;
			}
			else {
				callerStmt->total -= ticks;
			}
		}*/
		if (debugger != NULL) {
			debugger(rt->dbg, noError, pu->tp - (trcptr)pu->bp, NULL, caller, callee);
		}
	}
	else {
		// trace enter
		trcptr tp = pu->tp;
		if ((trcptr)pu->sp - tp < 1) {
			trace("stack overflow(tp: %d, sp: %d)", tp - (trcptr)pu->bp, pu->sp - (stkptr)pu->bp);
			return stackOverflow;
		}
		tp->caller = caller;
		tp->callee = callee;
		tp->func = now;
		tp->sp = sp;
		dbgn calleeFunc = mapDbgFunction(rt, tp->callee);
		if (calleeFunc != NULL) {
			calleeFunc->hits += 1;
		}

		if (debugger != NULL) {
			debugger(rt->dbg, noError, pu->tp - (trcptr)pu->bp, NULL, caller, callee);
		}
		pu->tp += 1;
	}
	return noError;
}

/// Private dummy debug function.
static dbgn dbgDummy(dbgContext ctx, vmError err, size_t ss, void *stack, size_t caller, size_t callee) {
	if (err != noError) {
		char *errMsg = vmErrorMessage(err);
		symn fun = rtLookupSym(ctx->rt, caller, 1);
		dbgn dbg = mapDbgStatement(ctx->rt, caller);
		size_t offs = caller;
		char *file = NULL;
		int line = 0;
		if (fun != NULL) {
			offs -= fun->offs;
		}
		if (dbg != NULL) {
			file = dbg->file;
			line = dbg->line;
		}
		error(ctx->rt, file, line, ERR_EXEC_INSTRUCTION, errMsg, caller, fun, offs, vmPointer(ctx->rt, caller));
		return ctx->abort;
	}
	(void) callee;
	(void) stack;
	(void) ss;
	return NULL;
}

/**
 * Execute bytecode.
 * 
 * @param rt Runtime context.
 * @param pu Cell to execute on.
 * @param fun Executing function.
 * @param extra Extra data for native calls.
 * @return Error code of execution, noError on success.
 */
static vmError exec(rtContext rt, vmProcessor pu, symn fun, const void *extra) {
	libc *nativeCalls = rt->vm.nfc;

	vmError execError = noError;
	const int cc = 1;						// cell count
	const size_t ms = rt->_size;			// memory size
	const size_t ro = rt->vm.ro;			// read only region
	const memptr mp = (void*)rt->_mem;
	const stkptr st = (void*)(pu->bp + pu->ss);

	if (fun == NULL || fun->offs == 0) {
		error(rt, NULL, 0, ERR_EXEC_FUNCTION, fun);
		return illegalState;
	}

	// run in debug or profile mode
	if (rt->dbg != NULL) {
		const void *oldTP = pu->tp;
		const stkptr spMin = (stkptr)(pu->bp);
		const stkptr spMax = (stkptr)(pu->bp + pu->ss);
		const vmInstruction ipMin = (vmInstruction)(rt->_mem + rt->vm.ro);
		const vmInstruction ipMax = (vmInstruction)(rt->_mem + rt->vm.px + px_size);

		dbgn (*debugger)(dbgContext, vmError, size_t, void*, size_t, size_t) = rt->dbg->debug;

		if (debugger == NULL) {
			debugger = dbgDummy;
		}
		// invoked function(from external code) will return with a ret instruction, removing trace info
		execError = traceCall(rt, pu->sp, 0, fun->offs);
		if (execError != noError) {
			debugger(rt->dbg, execError, 0, pu->sp, vmOffset(rt, pu->ip), 0);
			return execError;
		}

		for ( ; ; ) {
			register const vmInstruction ip = (vmInstruction)pu->ip;
			register const stkptr sp = pu->sp;

			const trcptr tp = pu->tp - 1;
			const size_t pc = vmOffset(rt, ip);

			if (ip >= ipMax || ip < ipMin) {
				debugger(rt->dbg, illegalState, st - sp, sp, pc, 0);
				return illegalState;
			}
			if (sp > spMax || sp < spMin) {
				debugger(rt->dbg, illegalState, st - sp, sp, pc, 0);
				return illegalState;
			}

			dbgn dbg = debugger(rt->dbg, noError, st - sp, sp, pc, 0);
			if (dbg == rt->dbg->abort) {
				// abort execution from debugger
				return nativeCallError;
			}
			if (dbg != NULL) {
				if (pc == dbg->start) {
					dbg->hits += 1;
					tp->stmt = clock();
				}
			}
			switch (ip->opc) {
				dbg_stop_vm:	// halt virtual machine
					if (execError != noError) {
						debugger(rt->dbg, execError, st - sp, sp, pc, 0);
						while (pu->tp != oldTP) {
							traceCall(rt, NULL, 0, (size_t) -2);
						}
					}
					while (pu->tp != oldTP) {
						traceCall(rt, NULL, 0, (size_t) -1);
					}
					return execError;

				dbg_error_opc:
					execError = illegalInstruction;
					goto dbg_stop_vm;

				dbg_error_ovf:
					execError = stackOverflow;
					goto dbg_stop_vm;

				dbg_error_mem:
					execError = illegalMemoryAccess;
					goto dbg_stop_vm;

				dbg_error_div_flt:
					debugger(rt->dbg, divisionByZero, st - sp, sp, pc, 0);
					// continue execution on floating point division by zero.
					break;

				dbg_error_div:
					execError = divisionByZero;
					goto dbg_stop_vm;

				dbg_error_libc:
					execError = nativeCallError;
					goto dbg_stop_vm;

				#define NEXT(__IP, __SP, __CHK) pu->sp -= (__SP); pu->ip += (__IP);
				#define STOP(__ERR, __CHK) do {if (__CHK) {goto dbg_##__ERR;}} while(0)
				#define EXEC
				#define TRACE(__IP) do { if ((execError = traceCall(rt, sp, pc, __IP)) != noError) goto dbg_stop_vm; } while(0)
				#include "code.inl"
			}
			if (dbg != NULL) {
				const trcptr tp2 = pu->tp - 1;
				if (pc != tp2->caller) {
					size_t pc2 = vmOffset(rt, pu->ip);
					if (pc2 < dbg->start || pc2 >= dbg->end) {
						clock_t now = clock();
						dbg->exec += 1;
						dbg->total += now - tp->stmt;
					}
				}
			}
		}
		return execError;
	}

	// code for maximum execution speed
	for ( ; ; ) {
		register const vmInstruction ip = (vmInstruction)pu->ip;
		register const stkptr sp = pu->sp;
		switch (ip->opc) {
			stop_vm:	// halt virtual machine
				if (execError != noError && fun == rt->main) {
					struct dbgContextRec dbg;
					dbg.rt = rt;
					dbgDummy(&dbg, execError, st - sp, sp, vmOffset(rt, ip), 0);
				}
				return execError;

			error_opc:
				execError = illegalInstruction;
				goto stop_vm;

			error_ovf:
				execError = stackOverflow;
				goto stop_vm;

			error_mem:
				execError = illegalMemoryAccess;
				goto stop_vm;

			error_div_flt:
				// continue execution on floating point division by zero.
				break;

			error_div:
				execError = divisionByZero;
				goto stop_vm;

			error_libc:
				execError = nativeCallError;
				goto stop_vm;

			#define NEXT(__IP, __SP, __CHK) { pu->sp -= (__SP); pu->ip += (__IP); }
			#define STOP(__ERR, __CHK) do {if (__CHK) {goto __ERR;}} while(0)
			#define TRACE(__IP)
			#define EXEC
			#include "code.inl"
		}
	}
	return execError;
}

vmError invoke(rtContext rt, symn fun, void *res, void *args, const void *extra) {
	const vmProcessor pu = rt->vm.cell;

	if (pu == NULL) {
		fatal(ERR_INTERNAL_ERROR": can not invoke %?T without execute", fun);
		return illegalState;
	}

	// invoked symbol must be a static function
	if (fun->params == NULL || fun->offs == 0 || !isStatic(fun)) {
		dieif(fun->params == NULL, ERR_INTERNAL_ERROR);
		dieif(fun->offs == 0, ERR_INTERNAL_ERROR);
		dieif(!isStatic(fun), ERR_INTERNAL_ERROR);
		return illegalState;
	}

	// result is the last argument.
	size_t argSize = argsSize(fun);
	size_t resSize = fun->params->size;
	void *ip = pu->ip;
	void *sp = pu->sp;
	void *tp = pu->tp;

	// make space for result and arguments// result is the first param
	pu->sp -= argSize / vm_size;

	if (args != NULL) {
		memcpy((char *)pu->sp, args, argSize - resSize);
	}

	// return here: vm->px: program halt
	*(pu->sp -= 1) = rt->vm.px;

	pu->ip = vmPointer(rt, fun->offs);
	vmError result = exec(rt, pu, fun, extra);
	if (result == noError && res != NULL) {
		memcpy(res, (memptr) sp - resSize, resSize);
	}

	pu->ip = ip;
	pu->sp = sp;
	pu->tp = tp;	// abort during exec

	return result;
}
vmError execute(rtContext rt, int argc, char *argv[], void *extra) {
	// TODO: cells should be in runtime read only memory?
	vmProcessor pu;

	// invalidate compiler
	rt->cc = NULL;

	// invalidate memory manager
	rt->vm.heap = NULL;

	// allocate processor(s)
	rt->_end = rt->_mem + rt->_size;
	rt->_end -= sizeof(struct vmProcessor);
	rt->vm.cell = pu = (void*)rt->_end;

	if (rt->vm.ss == 0) {
		rt->vm.ss = rt->_size / 4;
	}
	rt->_end -= rt->vm.ss;

	pu->cp = 0;
	pu->pp = 0;
	pu->ss = rt->vm.ss;
	pu->bp = rt->_end;
	pu->tp = (trcptr)rt->_end;
	pu->sp = (stkptr)(rt->_end + rt->vm.ss);
	pu->ip = rt->_mem + rt->vm.pc;

	if (rt->_beg > rt->_end) {
		fatal(ERR_INTERNAL_ERROR": memory overrun");
		return illegalState;
	}
	if (pu->bp > (memptr)pu->sp) {
		fatal(ERR_INTERNAL_ERROR": invalid stack size");
		return illegalState;
	}
	if (pu->sp != padPointer(pu->sp, vm_size)) {
		fatal(ERR_INTERNAL_ERROR": invalid stack alignment");
		return illegalState;
	}

	(void)argc;
	(void)argv;
	return exec(rt, pu, rt->main, extra);
}

int isChecked(dbgContext ctx) {
	rtContext rt = ctx->rt;
	vmProcessor pu = rt->vm.cell;
	trcptr trcBase = (trcptr)pu->bp;
	size_t maxTrace = pu->tp - (trcptr)pu->bp;
	for (int i = 0; i < maxTrace; ++i) {
		trcptr trace = &trcBase[maxTrace - i - 1];
		symn fun = rtLookupSym(rt, trace->callee, 1);
		if (fun == ctx->tryExec) {
			return 1;
		}
	}
	return 0;
}

void printOpc(FILE *out, const char **esc, vmOpcode opc, int64_t args) {
	struct vmInstruction instruction;
	instruction.opc = opc;
	instruction.arg.i64 = args;
	printAsm(out, esc, NULL, &instruction, prName);
}

void printAsm(FILE *out, const char **esc, rtContext rt, void *ptr, dmpMode mode) {
	vmInstruction ip = (vmInstruction)ptr;
	size_t i, len = (size_t) mode & prAsmCode;
	size_t offs = (size_t)ptr;
	char *fmt_addr = " .%06x";
	char *fmt_offs = " <%?.T%?+d>";
	symn sym = NULL;

	if (rt != NULL) {
		offs = vmOffset(rt, ptr);
		if (mode & prAsmName) {
			sym = rtLookupSym(rt, offs, 0);
		}
	}

	if (mode & prNoOffs) {
		fmt_addr = " .??????";
		fmt_offs = " <%?.T+?>";
	}
	if (mode & prAsmAddr) {
		printFmt(out, esc, fmt_addr + 1, offs);
		printFmt(out, esc, ": ");
	}

	if (mode & prAsmName && sym != NULL) {
		size_t symOffs = offs - sym->offs;
		int trailLen = 1, padLen = 4 + !symOffs;
		printFmt(out, esc, fmt_offs + 1, sym, symOffs);

		while (symOffs > 0) {
			symOffs /= 10;
			padLen -= 1;
		}
		if (padLen < 0) {
			padLen = 0;
		}
		if (mode & prNoOffs) {
			padLen = 0;
			trailLen = 4;
		}
		printFmt(out, esc, "% I:% I", padLen, trailLen);
	}

	if (ip == NULL) {
		printFmt(out, esc, "%s", "null");
		return;
	}

	//~ write code bytes
	if (len > 1 && len < opcode_tbl[ip->opc].size) {
		for (i = 0; i < len - 2; i++) {
			if (i < opcode_tbl[ip->opc].size) {
				printFmt(out, esc, "%02x ", ((unsigned char *) ptr)[i]);
			}
			else {
				printFmt(out, esc, "   ");
			}
		}
		if (i < opcode_tbl[ip->opc].size) {
			printFmt(out, esc, "%02x... ", ((unsigned char *) ptr)[i]);
		}
	}
	else {
		for (i = 0; i < len; i++) {
			if (i < opcode_tbl[ip->opc].size) {
				printFmt(out, esc, "%02x ", ((unsigned char *) ptr)[i]);
			}
			else {
				printFmt(out, esc, "   ");
			}
		}
	}

	if (opcode_tbl[ip->opc].name != NULL) {
		printFmt(out, esc, "%s", opcode_tbl[ip->opc].name);
	}
	else {
		printFmt(out, esc, "opc.x%02x", ip->opc);
	}

	switch (ip->opc) {
		default:
			break;

		case opc_dup1:
		case opc_dup2:
		case opc_dup4:
		case opc_set1:
		case opc_set2:
		case opc_set4:
			printFmt(out, esc, " sp(%d)", ip->idx);
			break;

		case opc_lc32:
			printFmt(out, esc, " %d", ip->arg.i32);
			break;

		case opc_lc64:
			printFmt(out, esc, " %D", ip->arg.i64);
			break;

		case opc_lf32:
			printFmt(out, esc, " %f", ip->arg.f32);
			break;

		case opc_lf64:
			printFmt(out, esc, " %F", ip->arg.f64);
			break;

		case opc_inc:
		case opc_spc:
		case opc_ldsp:
			printFmt(out, esc, "(%+d)", ip->rel);
			break;
		case opc_move:
		case opc_mad:
			printFmt(out, esc, " %d", ip->rel);
			break;

		case opc_jmp:
		case opc_jnz:
		case opc_jz:
		case opc_task:
			if (ip->opc == opc_task) {
				printFmt(out, esc, " %d,", ip->dl);
				i = ip->cl;
			}
			else {
				i = (size_t) ip->rel;
			}
			if (mode & prAsmName && sym != NULL) {
				printFmt(out, esc, fmt_offs, sym, offs + i - sym->offs);
			}
			else if (mode & prAsmAddr) {
				printFmt(out, esc, fmt_addr, offs + i);
			}
			else {
				printFmt(out, esc, " %+d", i);
			}
			break;

		case opc_sync:
			printFmt(out, esc, " %d", ip->idx);
			break;

		case b32_bit: switch (ip->idx & 0xc0) {
			default:
				fatal(ERR_INTERNAL_ERROR);
				return;

			case b32_bit_and:
				printFmt(out, esc, "%s 0x%03x", "and", (1 << (ip->idx & 0x3f)) - 1);
				break;

			case b32_bit_shl:
				printFmt(out, esc, "%s 0x%03x", "shl", ip->idx & 0x3f);
				break;

			case b32_bit_shr:
				printFmt(out, esc, "%s 0x%03x", "shr", ip->idx & 0x3f);
				break;

			case b32_bit_sar:
				printFmt(out, esc, "%s 0x%03x", "sar", ip->idx & 0x3f);
				break;
			}
			break;

		case opc_ld32:
		case opc_st32:
		case opc_ld64:
		case opc_st64:
		case opc_lref:
			if (ip->opc == opc_lref) {
				offs = ip->arg.u32;
			}
			else {
				offs = (size_t) ip->rel;
			}

			printFmt(out, esc, fmt_addr, offs);
			if (rt != NULL) {
				sym = rtLookupSym(rt, offs, 0);
				if (sym != NULL) {
					printFmt(out, esc, " ;%?T%?+d", sym, offs - sym->offs);
				}
				else if (rt->cc != NULL) {
					char *str = vmPointer(rt, offs);
					for (i = 0; i < hashTableSize; i += 1) {
						list lst;
						for (lst = rt->cc->strt[i]; lst; lst = lst->next) {
							if (str == (char*)lst->data) {
								printFmt(out, esc, " ;%c", type_fmt_string_chr);
								printFmt(out, esc ? esc : escapeStr(), "%s", str);
								printFmt(out, esc, "%c", type_fmt_string_chr);
								i = hashTableSize;
								break;
							}
						}
					}
				}
			}
			break;

		case opc_nfc:
			offs = (size_t) ip->rel;
			printFmt(out, esc, "(%d)", offs);

			if (rt != NULL) {
				sym = NULL;
				if (rt->vm.nfc != NULL) {
					sym = ((libc*)rt->vm.nfc)[offs]->sym;
				}
				if (sym != NULL) {
					printFmt(out, esc, " ;%T", sym);
				}
			}
			break;

		case p4x_swz: {
			char c1 = "xyzw"[ip->idx >> 0 & 3];
			char c2 = "xyzw"[ip->idx >> 2 & 3];
			char c3 = "xyzw"[ip->idx >> 4 & 3];
			char c4 = "xyzw"[ip->idx >> 6 & 3];
			printFmt(out, esc, " %c%c%c%c(%02x)", c1, c2, c3, c4, ip->idx);
			break;
		}
	}
}

static void printRef(FILE *out, const char **esc, rtContext rt, void* data) {
	if (!isValidOffset(rt, data)) {
		// we should never get here, but ...
		printFmt(out, esc, "BadRef@%06x", data);
		return;
	}

	size_t offs = vmOffset(rt, data);
	symn sym = rtLookupSym(rt, offs, 0);
	if (sym != NULL) {
		size_t rel = offs - sym->offs;
		printFmt(out, esc, "<%?.*T%?+d @%06x>", prSymQual, sym, rel, offs);
	}
	else {
		printFmt(out, esc, "<@%06x>", offs);
	}
}

void printVal(FILE *out, const char **esc, rtContext rt, symn var, vmValue *val, dmpMode mode, int indent) {
	ccKind varCast = castOf(var);
	const char *format = var->format;
	memptr data = (memptr) val;
	symn typ = var;

	if (indent > 0) {
		printFmt(out, esc, "%I", indent);
	}
	else {
		indent = -indent;
	}

	if (!isTypename(var)) {
		typ = var->type;
		if (format == NULL) {
			format = typ->format;
		}
		if (varCast == CAST_ref) {
			data = vmPointer(rt, (size_t) val->ref);
		}
	}
	else if (val == vmPointer(rt, var->offs)) {
		if (mode & prSymType) {
			printFmt(out, esc, "%.*T: ", mode & ~prSymType, var);
		}
		typ = var = rt->main->fields;
		format = type_fmt_typename;
	}

	ccKind typCast = castOf(typ);
	if (var != typ) {
		printFmt(out, esc, "%.*T: ", mode & ~prSymType, var);
	}

	if (mode & prSymType) {
		printFmt(out, esc, "%.T(", typ);
	}

	// null reference.
	if (data == NULL) {
		printFmt(out, esc, "null");
	}
	// invalid offset.
	else if (!isValidOffset(rt, data)) {
		printFmt(out, esc, "BadRef@%06x", var->offs);
	}
	// builtin type.
	else if (format != NULL) {
		vmValue *ref = (vmValue *) data;
		if (format == type_fmt_typename) {
			printFmt(out, esc, type_fmt_typename, ref);
		}
		else if (format == type_fmt_string) {
			printFmt(out, esc, "%c", type_fmt_string_chr);
			printFmt(out, esc ? esc : escapeStr(), type_fmt_string, ref);
			printFmt(out, esc, "%c", type_fmt_string_chr);
		}
		else if (format == type_fmt_character) {
			printFmt(out, esc, "%c", type_fmt_character_chr);
			printFmt(out, esc ? esc : escapeStr(), type_fmt_character, ref->u08);
			printFmt(out, esc, "%c", type_fmt_character_chr);
		}
		else switch (typ->size) {
			default:
				// there should be no formatted(<=> builtin) type matching none of this size.
				printFmt(out, esc, "%s @%06x, size: %d", format, vmOffset(rt, ref), var->size);
				break;

			case 1:
				if (typCast == CAST_u32) {
					printFmt(out, esc, format, ref->u08);
				}
				else {
					printFmt(out, esc, format, ref->i08);
				}
				break;

			case 2:
				if (typCast == CAST_u32) {
					printFmt(out, esc, format, ref->u16);
				}
				else {
					printFmt(out, esc, format, ref->i16);
				}
				break;

			case 4:
				if (typCast == CAST_f32) {
					printFmt(out, esc, format, ref->f32);
				}
				else if (typCast == CAST_u32) {
					// force zero extend (may sign extend to int64 ?).
					printFmt(out, esc, format, ref->u32);
				}
				else {
					printFmt(out, esc, format, ref->i32);
				}
				break;

			case 8:
				if (typCast == CAST_f64) {
					printFmt(out, esc, format, ref->f64);
				}
				else {
					printFmt(out, esc, format, ref->i64);
				}
				break;
		}
	}
	else if (indent > maxLogItems) {
		printFmt(out, esc, "...");
	}
	else if (typCast == CAST_var) {
		memptr varData = vmPointer(rt, (size_t) val->ref);
		symn varType = vmPointer(rt, (size_t) val->type);

		if (varType == NULL || varData == NULL) {
			printFmt(out, esc, "null");
		}
		else if (var != typ) {
			printFmt(out, esc, "{%.T: ", varType);
			printVal(out, esc, rt, varType, (vmValue *) varData, mode & ~(prSymQual | prSymType), -indent);
			printFmt(out, esc, "}");
		}
		else {
			printRef(out, esc, rt, data);
		}
	}
	else if (typCast == CAST_arr) {
		if (typ == var || varCast == CAST_val) {
			// fixed size array or direct reference
			data = (memptr) val;
		}
		else if (varCast == CAST_arr || varCast == CAST_ref) {
			// follow indirection
			data = vmPointer(rt, (size_t) val->ref);
		}
		else {
			printFmt(out, esc, "@BadArray");
			if (mode & prSymType) {
				printFmt(out, esc, ")");
			}
			return;
		}

		if (data == NULL) {
			printFmt(out, esc, "null");
		}
		else if (typ->type->format == type_fmt_character) {
			// interpret `char[]`, `char[*]` and `char[n]` as string
			printFmt(out, esc, "%c", type_fmt_string_chr);
			printFmt(out, esc ? esc : escapeStr(), type_fmt_string, data);
			printFmt(out, esc, "%c", type_fmt_string_chr);
		}
		else {
			size_t length;
			size_t inc = typ->type->size;
			symn lenField = typ->fields;
			if (lenField == NULL) {
				// pointer
				length = 0;
			}
			else if (isStatic(lenField)) {
				// fixed size array
				length = typ->size / inc;
			}
			else {
				// dinamic size array
				length = val->length;
			}
			printRef(out, esc, rt, data);
			printFmt(out, esc, "[%d] {", length);
			for (size_t idx = 0; idx < length; idx += 1) {
				if (idx > 0) {
					printFmt(out, esc, ", ");
				}
				if (idx >= maxLogItems) {
					printFmt(out, esc, "...");
					break;
				}
				printVal(out, esc, rt, typ->type, (vmValue *) (data + idx * inc), mode & ~(prSymQual | prSymType), -indent);
			}
			printFmt(out, esc, "}");
		}
	}
	else {
		// typename, function, pointer, etc (without format option)
		int fields = 0;
		printRef(out, esc, rt, data);
		if (typ->fields != NULL) {
			for (symn sym = typ->fields; sym; sym = sym->next) {
				if (isStatic(sym)) {
					// skip static fields
					continue;
				}

				if (!isVariable(sym)) {
					// skip types, functions and aliases
					continue;
				}

				if (fields > 0) {
					printFmt(out, esc, ",");
				}
				else {
					printFmt(out, esc, "{");
				}

				printFmt(out, esc, "\n");
				printVal(out, esc, rt, sym, (void *) (data + sym->offs), mode & ~prSymQual, indent + 1);
				fields += 1;
			}
			if (fields > 0) {
				printFmt(out, esc, "\n%I}", indent);
			}
		}
	}

	if (mode & prSymType) {
		printFmt(out, esc, ")");
	}
}

static void traceArgs(rtContext rt, FILE *out, symn fun, char *file, int line, void *sp, int indent) {
	symn sym;
	int printFileLine = 0;

	if (out == NULL) {
		out = rt->logFile;
	}
	if (file == NULL) {
		file = "native.code";
	}
	printFmt(out, NULL, "%I%?s:%?u: %?.T", indent, file, line, fun);
	if (indent < 0) {
		printFileLine = 1;
		indent = -indent;
	}

	dieif(sp == NULL, ERR_INTERNAL_ERROR);
	dieif(fun == NULL, ERR_INTERNAL_ERROR);
	if (fun->params != NULL) {
		int firstArg = 1;
		if (indent > 0) {
			printFmt(out, NULL, "(");
		}
		else {
			printFmt(out, NULL, "\n");
		}
		size_t args = 0;
		for (sym = fun->params->next; sym; sym = sym->next) {
			if (args < sym->offs) {
				args = sym->offs;
			}
		}
		for (sym = fun->params->next; sym; sym = sym->next) {
			size_t offs = args - sym->offs;

			if (firstArg == 0) {
				printFmt(out, NULL, ", ");
			}
			else {
				firstArg = 0;
			}

			if (printFileLine) {
				if (sym->file != NULL && sym->line != 0) {
					printFmt(out, NULL, "%I%s:%u: ", indent, sym->file, sym->line);
				}
				else {
					printFmt(out, NULL, "%I", indent);
				}
			}

			dieif(isStatic(sym), ERR_INTERNAL_ERROR);
			if (isFunction(fun)) {
				// at vm_size is the return value of the function.
				offs += vm_size;
			}
			printVal(out, NULL, rt, sym, (vmValue *)((char*)sp + offs), prValue, -indent);
		}
		if (indent > 0) {
			printFmt(out, NULL, ")");
		}
		else {
			printFmt(out, NULL, "\n");
		}
	}
}

void traceCalls(dbgContext dbg, FILE *out, int indent, size_t maxCalls) {
	rtContext rt = dbg->rt;
	vmProcessor pu = rt->vm.cell;
	trcptr trcBase = (trcptr)pu->bp;
	size_t maxTrace = pu->tp - (trcptr)pu->bp;
	size_t i, hasOutput = 0;

	if (out == NULL) {
		out = rt->logFile;
	}

	if (maxCalls > maxTrace) {
		maxCalls = maxTrace;
	}

	for (i = 0; i < maxCalls; ++i) {
		trcptr trace = &trcBase[maxTrace - i - 1];
		dbgn trInfo = mapDbgStatement(rt, trace->caller);
		symn fun = rtLookupSym(rt, trace->callee, 1);
		stkptr sp = trace->sp;
		char *file = NULL;
		int line = 0;

		dieif(fun == NULL, ERR_INTERNAL_ERROR);

		if (trInfo != NULL) {
			file = trInfo->file;
			line = trInfo->line;
		}
		if (hasOutput > 0) {
			printFmt(out, NULL, "\n");
		}
		traceArgs(rt, out, fun, file, line, sp, indent);
		hasOutput += 1;
	}
	if (i < maxTrace) {
		if (hasOutput > 0) {
			printFmt(out, NULL, "\n");
		}
		printFmt(out, NULL, "%I... %d more", indent, maxTrace - i);
		hasOutput += 1;
	}

	if (hasOutput > 0) {
		printFmt(out, NULL, "\n");;
	}
}

int vmSelfTest(void cb(const char *, const struct opcodeRec *)) {
	int errors = 0;
	struct vmInstruction ip[1];
	struct libc *nativeCalls[1];
	struct libc nfc0[1];

	nativeCalls[0] = nfc0;
	memset(nfc0, 0, sizeof(nfc0));

	for (int i = 0; i < opc_last; i++) {
		const struct opcodeRec *info = &opcode_tbl[i];
		unsigned int IS, CHK, DIFF;
		char *error = NULL;

		if (info->name == NULL && info->size == 0 && info->stack_in == 0 && info->stack_out == 0) {
			// skip unimplemented instruction.
			continue;
		}

		memset(ip, 0, sizeof(ip));
		// take care of some special opcodes
		switch (i) {
			default:
				break;

			// set.sp(0) will pop no elements from the stack,
			// actually it will do nothing, by setting itself to the same value ...
			case opc_set1:
				ip->idx = 1;
				break;

			case opc_set2:
				ip->idx = 2;
				break;

			case opc_set4:
				ip->idx = 4;
				break;
		}

		switch (ip->opc = (uint8_t) i) {
			error_opc:
				error = "Invalid instruction";
				errors += 1;
				break;
			error_len:
				error = "Invalid instruction length";
				errors += 1;
				break;

			error_chk:
				error = "Invalid stack arguments";
				errors += 1;
				break;

			error_diff:
				error = "Invalid stack size";
				errors += 1;
				break;

			#define NEXT(__IP, __SP, __CHK) {\
				IS = __IP;\
				CHK = __CHK;\
				DIFF = __SP;\
				if (IS && IS != info->size) {\
					goto error_len;\
				}\
				if (CHK != info->stack_in) {\
					goto error_chk;\
				}\
				if (DIFF != (info->stack_out - info->stack_in)) {\
					goto error_diff;\
				}\
			}
			#define STOP(__ERR, __CHK) if (__CHK) goto __ERR
			#include "code.inl"
		}
		if (cb != NULL) {
			cb(error, info);
		}
	}
	return errors;
}

/*******************************************************************************
 *   File: code.c
 *   Date: 2011/06/23
 *   Description: bytecode related stuff
 *******************************************************************************
code emmiting, executing and formatting
*******************************************************************************/
#include <string.h>
#include <math.h>
#include <time.h>
#include <stddef.h>
#include "internal.h"
#include "vmCore.h"

#ifdef __WATCOMC__
#pragma disable_message(124);
// needed for opcode f32_mod
static inline double fmodf(float x, float y) {
	return fmod(x, y);
}
#endif

const struct opcodeRec opcode_tbl[256] = {
	#define OPCODE_DEF(Name, Code, Size, In, Out, Text) {Code, Size, In, Out, Text},
	#include "defs.inl"
};


/// Bit scan forward. (get the index of the highest bit seted)
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

typedef uint8_t *memptr;
typedef uint32_t *stkptr;
typedef struct traceRec *trcptr;

/// bytecode instruction
typedef struct bcde *bcde;

/// bytecode processor
typedef struct cell *cell;
typedef struct traceRec *tracePtr;

#pragma pack(push, 1)
struct bcde {
	uint8_t opc;
	union {
		stkval arg;			// argument (load const)
		uint8_t idx;		// usually index for stack
		int32_t rel:24;		// relative offset (ip, sp) +- 7MB
		struct {
			// used when starting a new parallel task
			uint8_t  dl;	// data to to be copied to stack
			uint16_t cl;	// code to to be executed parallel: 0 means fork
		};
	};
};
#pragma pack(pop)

struct cell {
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
};

struct traceRec {
	memptr caller;      // Instruction pointer of caller
	memptr callee;      // Instruction pointer of callee
	clock_t func;       // time when the function was invoked
	clock_t stmt;       // time when the statement execution started
	stkptr sp;          // Stack pointer
};

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

static inline void rollbackPc(rtContext rt) {
	bcde ip = getip(rt, rt->vm.pc);
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

/**
 * @brief Check for an instruction at the given offset.
 * @param offs Offset of the opcode.
 * @param opc Opcode to check.
 * @param arg Copy the argument of the opcode.
 * @return non zero if at the given location the opc was found.
 * @note Aborts if opc is not valid.
 */
static int checkOcp(rtContext rt, size_t offs, vmOpcode opc, stkval *arg) {
	bcde ip = getip(rt, offs);
	if (opc >= opc_last) {
		trace("invalid opc_x%x", opc);
		return 0;
	}
	if (ip->opc == opc) {
		if (arg != NULL) {
			*arg = ip->arg;
		}
		return 1;
	}
	return 0;
}

static int removeOpc(rtContext rt, size_t offs) {
	if (offs >= rt->vm.ro && offs <= rt->vm.pc) {
		bcde ip = getip(rt, offs);
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
		bcde ip = getip(rt, offs);
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
	stkval arg;
	if (checkOcp(rt, offsBegin, opc_dup1, &arg)) {
		// duplicate top of stack then set top of stack
		if (arg.u1 != 0) {
			return 0;
		}
		if (!checkOcp(rt, offsEnd, opc_set1, &arg)) {
			return 0;
		}
		if (arg.u1 != 1) {
			return 0;
		}

		if (!removeOpc(rt, offsEnd)) {
			fatal(ERR_INTERNAL_ERROR": %.*A", offsEnd, getip(rt, offsEnd));
			return 0;
		}
		if (!removeOpc(rt, offsBegin)) {
			fatal(ERR_INTERNAL_ERROR": %.*A", offsEnd, getip(rt, offsEnd));
			return 0;
		}
		if (!decrementStackAccess(rt, offsBegin, offsEnd, 1)) {
			fatal(ERR_INTERNAL_ERROR": %.*A", offsBegin, getip(rt, offsBegin));
			return 0;
		}
		return 1;
	}
	if (checkOcp(rt, offsBegin, opc_dup2, &arg)) {
		// duplicate top of stack then set top of stack
		if (arg.u1 != 0) {
			return 0;
		}
		if (!checkOcp(rt, offsEnd, opc_set2, &arg)) {
			return 0;
		}
		if (arg.u1 != 2) {
			return 0;
		}
		if (!removeOpc(rt, offsEnd)) {
			fatal(ERR_INTERNAL_ERROR": %.*A", offsEnd, getip(rt, offsEnd));
			return 0;
		}
		if (!removeOpc(rt, offsBegin)) {
			fatal(ERR_INTERNAL_ERROR": %.*A", offsEnd, getip(rt, offsEnd));
			return 0;
		}
		if (!decrementStackAccess(rt, offsBegin, offsEnd, 2)) {
			fatal(ERR_INTERNAL_ERROR": %.*A", offsBegin, getip(rt, offsBegin));
			return 0;
		}
		return 1;
	}
	return 0;
}

size_t emitarg(rtContext rt, vmOpcode opc, stkval arg) {
	libc libcvec = rt->vm.nfc;
	bcde ip = (bcde)rt->_beg;

	dieif((unsigned char*)ip + 16 >= rt->_end, ERR_MEMORY_OVERRUN);

	/* TODO: set max stack used.
	switch (opc) {
		case opc_dup1:
		case opc_set1:
			if (rt->vm.su < arg.i4 + 1)
				rt->vm.su = arg.i4 + 1;
			break;
		case opc_dup2:
		case opc_set2:
			if (rt->vm.su < arg.i4 + 2)
				rt->vm.su = arg.i4 + 2;
			break;
		case opc_dup4:
		case opc_set4:
			if (rt->vm.su < arg.i4 + 4)
				rt->vm.su = arg.i4 + 4;
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
		case opc_stiq: {
			ip = getip(rt, rt->vm.pc);
			if (ip->opc == opc_ldsp) {
				if (rt->vm.su < arg.i4 / 4)
					rt->vm.su = arg.i4 / 4;
			}
		} break;
	}
	// */

	if (opc == markIP) {
		return rt->_beg - rt->_mem;
	}

	// load / store
	else if (opc == opc_ldi) {
		ip = getip(rt, rt->vm.pc);
		if (ip->opc == opc_ldsp) {
			size_t vardist = (ip->rel + arg.i4) / 4;
			if (rt->vm.su < vardist)
				rt->vm.su = vardist;
		}

		dieif (arg.i4 != arg.i8, ERR_INTERNAL_ERROR);
		switch (arg.i4) {
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
				if (!emitInt(rt, opc_ldsp, 4 - arg.i4)) {
					trace(ERR_INTERNAL_ERROR);
					return 0;
				}

				// copy
				if (!emitInt(rt, opc_move, -arg.i4)) {
					trace(ERR_INTERNAL_ERROR);
					return 0;
				}

				// create n bytes on stack
				opc = opc_spc;
				break;
		}
	}
	else if (opc == opc_sti) {
		ip = getip(rt, rt->vm.pc);
		if (ip->opc == opc_ldsp) {
			size_t vardist = (ip->rel + arg.i4) / 4;
			if (rt->vm.su < vardist)
				rt->vm.su = vardist;
		}

		dieif (arg.i4 != arg.i8, ERR_INTERNAL_ERROR);
		switch (arg.i4) {
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
				if (!emitInt(rt, opc_ldsp, vm_size)) {
					trace(ERR_INTERNAL_ERROR);
					return 0;
				}

				// copy
				if (!emitInt(rt, opc_move, arg.i4)) {
					trace(ERR_INTERNAL_ERROR);
					return 0;
				}

				if (ip->opc == opc_ldsp) {
					//~ if we copy from the stack to the stack
					//~ do not remove more elements than we copy
					if (arg.i8 > ip->rel) {
						arg.i8 = ip->rel;
					}
				}
				// remove n bytes from stack
				arg.i8 = -arg.i8;
				opc = opc_spc;
				break;
		}
	}
	else if (opc == opc_drop) {
		opc = opc_spc;
		arg.i8 = -arg.i8;
	}

	if (opc >= opc_last) {
		trace("invalid opc_x%x", opc);
		return 0;
	}

	// Optimize
	if (rt->foldInstr) {
		if (!rt->foldInstr) {}

		/* TODO: ldzs are not optimized, uncomment when vm does constant evaluations.
		else if (opc == opc_lc32) {
			if (arg.i8 == 0) {
				opc = opc_ldz1;
				ip = getip(rt, rt->vm.pc);
				if (ip->opc == opc_ldz1) {
					opc = opc_ldz2;
					rollbackPc(rt);
				}
			}
		}
		else if (opc == opc_lc64) {
			if (arg.i8 == 0) {
				opc = opc_ldz2;
				ip = getip(rt, rt->vm.pc);
				if (ip->opc == opc_ldz2) {
					opc = opc_ldz4;
					rollbackPc(rt);
				}
			}
		}

		/ *else if (opc == opc_ldi1) {
			ip = getip(rt, rt->vm.pc);
			if (ip->opc == opc_ldsp && ((ip->rel & (vm_size-1)) == 0) && ((ip->rel / vm_size) < vm_regs)) {
				arg.i4 = ip->rel / vm_size;
				opc = opc_dup1;
				rt->_beg = (memptr)ip;
				rt->vm.ss -= 1;
			}
		}
		else if (opc == opc_ldi2) {
			ip = getip(rt, rt->vm.pc);
			if (ip->opc == opc_ldsp && ((ip->rel & (vm_size-1)) == 0) && ((ip->rel / vm_size) < vm_regs)) {
				arg.i4 = ip->rel / vm_size;
				opc = opc_dup1;
				rt->_beg = (memptr)ip;
				rt->vm.ss -= 1;
			}
		}
		// */

		else if (opc == opc_ldi4) {
			ip = getip(rt, rt->vm.pc);
			if (ip->opc == opc_ldsp && ((ip->rel & (vm_size-1)) == 0) && ((ip->rel / vm_size) < vm_regs)) {
				arg.i4 = ip->rel / vm_size;
				opc = opc_dup1;
				rollbackPc(rt);
			}
			else if (ip->opc == opc_lref) {
				arg.i8 = ip->arg.u4;
				if (arg.i8 == arg.rel) {
					opc = opc_ld32;
					rollbackPc(rt);
				}
			}
		}
		else if (opc == opc_sti4) {
			ip = getip(rt, rt->vm.pc);
			if (ip->opc == opc_ldsp && ((ip->rel & (vm_size-1)) == 0) && ((ip->rel / vm_size) < vm_regs)) {
				arg.i4 = ip->rel / vm_size;
				opc = opc_set1;
				rollbackPc(rt);
			}
			else if (ip->opc == opc_lref) {
				arg.i8 = ip->arg.u4;
				if (arg.i8 == arg.rel) {
					opc = opc_st32;
					rollbackPc(rt);
				}
			}
		}

		else if (opc == opc_ldi8) {
			ip = getip(rt, rt->vm.pc);
			if (ip->opc == opc_ldsp && ((ip->rel & (vm_size-1)) == 0) && ((ip->rel / vm_size) < vm_regs)) {
				arg.i4 = ip->rel / vm_size;
				opc = opc_dup2;
				rollbackPc(rt);
			}
			else if (ip->opc == opc_lref) {
				arg.i8 = ip->arg.u4;
				if (arg.i8 == arg.rel) {
					opc = opc_ld64;
					rollbackPc(rt);
				}
			}
		}
		else if (opc == opc_sti8) {
			ip = getip(rt, rt->vm.pc);
			if (ip->opc == opc_ldsp && ((ip->rel & (vm_size-1)) == 0) && ((ip->rel / vm_size) < vm_regs)) {
				arg.i4 = ip->rel / vm_size;
				opc = opc_set2;
				rollbackPc(rt);
			}
			else if (ip->opc == opc_lref) {
				arg.i8 = ip->arg.u4;
				if (arg.i8 == arg.rel) {
					opc = opc_st64;
					rollbackPc(rt);
				}
			}
		}

		else if (opc == opc_ldiq) {
			ip = getip(rt, rt->vm.pc);
			if (ip->opc == opc_ldsp && ((ip->rel & (vm_size-1)) == 0) && ((ip->rel / vm_size) <= vm_regs)) {
				arg.i4 = ip->rel / vm_size;
				opc = opc_dup4;
				rollbackPc(rt);
			}
		}
		else if (opc == opc_stiq) {
			ip = getip(rt, rt->vm.pc);
			if (ip->opc == opc_ldsp && ((ip->rel & (vm_size-1)) == 0) && ((ip->rel / vm_size) <= vm_regs)) {
				arg.i4 = ip->rel / vm_size;
				opc = opc_set4;
				rollbackPc(rt);
			}
		}

		// shl, shr, sar, and with immediate value.
		else if (opc == b32_shl) {
			ip = getip(rt, rt->vm.pc);
			if (ip->opc == opc_lc32) {
				arg.i4 = b32_bit_shl | (ip->arg.i4 & 0x3f);
				opc = b32_bit;
				rollbackPc(rt);
			}
		}
		else if (opc == b32_shr) {
			ip = getip(rt, rt->vm.pc);
			if (ip->opc == opc_lc32) {
				arg.i4 = b32_bit_shr | (ip->arg.i4 & 0x3f);
				opc = b32_bit;
				rollbackPc(rt);
			}
		}
		else if (opc == b32_sar) {
			ip = getip(rt, rt->vm.pc);
			if (ip->opc == opc_lc32) {
				arg.i4 = b32_bit_sar | (ip->arg.i4 & 0x3f);
				opc = b32_bit;
				rollbackPc(rt);
			}
		}
		else if (opc == b32_and) {
			ip = getip(rt, rt->vm.pc);
			if (ip->opc == opc_lc32) {
				if ((ip->arg.i4 & (ip->arg.i4 + 1)) == 0) {
					arg.i4 = b32_bit_and | (bitsf(ip->arg.u4 + 1) & 0x3f);
					opc = b32_bit;
					rollbackPc(rt);
				}
			}
		}

		//* TODO: eval Arithmetic using vm.
		else if (opc == opc_not) {
			ip = getip(rt, rt->vm.pc);
			if (ip->opc == opc_not) {
				rt->_beg = (memptr)ip;
				ip->opc = opc_nop;
				return rt->vm.pc;
			}
		}
		else if (opc == i32_neg) {
			ip = getip(rt, rt->vm.pc);
			if (ip->opc == i32_neg) {
				rt->_beg = (memptr)ip;
				ip->opc = opc_nop;
				return rt->vm.pc;
			}
			if (ip->opc == opc_lc32) {
				ip->arg.i4 = -ip->arg.i4;
				return rt->vm.pc;
			}
		}
		else if (opc == i64_neg) {
			ip = getip(rt, rt->vm.pc);
			if (ip->opc == i64_neg) {
				rt->_beg = (memptr)ip;
				ip->opc = opc_nop;
				return rt->vm.pc;
			}
			if (ip->opc == opc_lc64) {
				ip->arg.i8 = -ip->arg.i8;
				return rt->vm.pc;
			}
		}
		else if (opc == f32_neg) {
			ip = getip(rt, rt->vm.pc);
			if (ip->opc == f32_neg) {
				rt->_beg = (memptr)ip;
				ip->opc = opc_nop;
				return rt->vm.pc;
			}
			if (ip->opc == opc_lc32) {
				ip->arg.f4 = -ip->arg.f4;
				return rt->vm.pc;
			}
		}
		else if (opc == f64_neg) {
			ip = getip(rt, rt->vm.pc);
			if (ip->opc == f64_neg) {
				rt->_beg = (memptr)ip;
				ip->opc = opc_nop;
				return rt->vm.pc;
			}
			if (ip->opc == opc_lc64) {
				ip->arg.f8 = -ip->arg.f8;
				return rt->vm.pc;
			}
		}
		else if (opc == i32_add) {
			ip = getip(rt, rt->vm.pc);
			if (ip->opc == opc_lc32) {
				arg.i8 = ip->arg.i4;
				if (arg.rel == arg.i8) {
					opc = opc_inc;
					rollbackPc(rt);
				}
			}
		}
		else if (opc == i32_sub) {
			ip = getip(rt, rt->vm.pc);
			if (ip->opc == opc_lc32) {
				arg.i8 = -ip->arg.i4;
				if (arg.rel == arg.i8) {
					opc = opc_inc;
					rollbackPc(rt);
				}
			}
		}
		else if (opc == opc_inc) {
			if (arg.i8 == 0) {
				return rt->vm.pc;
			}

			ip = getip(rt, rt->vm.pc);
			if (ip->opc == opc_inc) {
				stkval tmp;
				tmp.i8 = arg.i8 + ip->rel;
				if (tmp.rel == tmp.i8) {
					ip->rel = tmp.rel;
					return rt->vm.pc;
				}
			}
			else if (ip->opc == opc_ldsp) {
				stkval tmp;
				if (arg.i8 < 0) {
					// local variable bound error.
					trace(ERR_INTERNAL_ERROR);
					return 0;
				}
				tmp.i8 = arg.i8 + ip->rel;
				if (tmp.rel == tmp.i8) {
					ip->rel = tmp.rel;
					return rt->vm.pc;
				}
			}
			else if (ip->opc == opc_lref) {
				stkval tmp;
				if (arg.i8 < 0) {
					// global variable bound error.
					trace(ERR_INTERNAL_ERROR);
					return 0;
				}
				tmp.i8 = arg.i8 + ip->rel;
				if (tmp.rel == tmp.i8) {
					ip->rel = tmp.rel;
					return rt->vm.pc;
				}
			}
		}

		//* TODO: eval Conversions using vm.
		else if (opc == u32_i64) {
			ip = getip(rt, rt->vm.pc);
			//~ if (ip->opc == opc_ldz2) { ... }
			if (ip->opc == opc_lc32) {
				arg.i8 = ip->arg.u4;
				opc = opc_lc64;
				rollbackPc(rt);
			}
		}
		else if (opc == i32_bol) {
			ip = getip(rt, rt->vm.pc);
			if (ip->opc == opc_lc32) {
				arg.i8 = ip->arg.i4 != 0;
				opc = opc_lc32;
				rollbackPc(rt);
			}
		}
		else if (opc == i32_f32) {
			ip = getip(rt, rt->vm.pc);
			if (ip->opc == opc_lc32) {
				arg.f4 = (float32_t) ip->arg.i4;
				opc = opc_lf32;
				rollbackPc(rt);
				logif(ip->arg.i4 != arg.f4, "inexact cast: %d => %f", ip->arg.i4, arg.f4);
			}
		}
		else if (opc == i32_i64) {
			ip = getip(rt, rt->vm.pc);
			if (ip->opc == opc_lc32) {
				arg.i8 = ip->arg.i4;
				opc = opc_lc64;
				rollbackPc(rt);
			}
		}
		else if (opc == i32_f64) {
			ip = getip(rt, rt->vm.pc);
			if (ip->opc == opc_lc32) {
				arg.f8 = ip->arg.i4;
				opc = opc_lf64;
				rollbackPc(rt);
				logif(ip->arg.i4 != arg.f8, "inexact cast: %d => %F", ip->arg.i4, arg.f8);
			}
		}
		else if (opc == i64_i32) {
			ip = getip(rt, rt->vm.pc);
			if (ip->opc == opc_lc64) {
				arg.i8 = ip->arg.i8;
				opc = opc_lc32;
				rollbackPc(rt);
				logif(ip->arg.i8 != arg.i4, "inexact cast: %D => %d", ip->arg.i8, arg.i4);
				logif(ip->arg.i8 != arg.u4, "inexact cast: %D => %u", ip->arg.i8, arg.u4);
			}
		}
		else if (opc == i64_f32) {
			ip = getip(rt, rt->vm.pc);
			if (ip->opc == opc_lc64) {
				arg.f4 = (float32_t) ip->arg.i8;
				opc = opc_lf32;
				rollbackPc(rt);
				logif(ip->arg.i8 != arg.f4, "inexact cast: %D => %f", ip->arg.i8, arg.f4);
			}
		}
		else if (opc == i64_bol) {
			ip = getip(rt, rt->vm.pc);
			if (ip->opc == opc_lc64) {
				arg.i8 = ip->arg.i8 != 0;
				opc = opc_lc32;
				rollbackPc(rt);
			}
		}
		else if (opc == i64_f64) {
			ip = getip(rt, rt->vm.pc);
			if (ip->opc == opc_lc64) {
				arg.f8 = (float64_t) ip->arg.i8;
				opc = opc_lf64;
				rollbackPc(rt);
				logif(ip->arg.i8 != arg.f8, "inexact cast: %D => %F", ip->arg.i8, arg.f8);
			}
		}
		else if (opc == f32_i32) {
			ip = getip(rt, rt->vm.pc);
			if (ip->opc == opc_lf32) {
				arg.i8 = (int64_t) ip->arg.f4;
				opc = opc_lc32;
				rollbackPc(rt);
				logif(ip->arg.f4 != arg.i4, "inexact cast: %f => %d", ip->arg.f4, arg.i4);
			}
		}
		else if (opc == f32_bol) {
			ip = getip(rt, rt->vm.pc);
			if (ip->opc == opc_lf32) {
				arg.i8 = ip->arg.f4 != 0;
				opc = opc_lc32;
				rollbackPc(rt);
			}
		}
		else if (opc == f32_i64) {
			ip = getip(rt, rt->vm.pc);
			if (ip->opc == opc_lf32) {
				arg.i8 = (int64_t) ip->arg.f4;
				opc = opc_lc64;
				rollbackPc(rt);
				logif(ip->arg.f4 != arg.i8, "inexact cast: %f => %D", ip->arg.f4, arg.i8);
			}
		}
		else if (opc == f32_f64) {
			ip = getip(rt, rt->vm.pc);
			if (ip->opc == opc_lf32) {
				arg.f8 = ip->arg.f4;
				opc = opc_lf64;
				rollbackPc(rt);
				logif(ip->arg.f4 != arg.f8, "inexact cast: %f => %F", ip->arg.f4, arg.f8);
			}
		}
		else if (opc == f64_i32) {
			ip = getip(rt, rt->vm.pc);
			if (ip->opc == opc_lf64) {
				arg.i8 = (int64_t) ip->arg.f8;
				opc = opc_lc32;
				rollbackPc(rt);
				logif(ip->arg.f8 != arg.i4, "inexact cast: %F => %d", ip->arg.f8, arg.i4);
			}
		}
		else if (opc == f64_f32) {
			ip = getip(rt, rt->vm.pc);
			if (ip->opc == opc_lf64) {
				arg.f4 = (float32_t) ip->arg.f8;
				opc = opc_lf32;
				rollbackPc(rt);
				logif(ip->arg.f8 != arg.f4, "inexact cast: %F => %f", ip->arg.f8, arg.f4);
			}
		}
		else if (opc == f64_i64) {
			ip = getip(rt, rt->vm.pc);
			if (ip->opc == opc_lf64) {
				arg.i8 = (int64_t) ip->arg.f8;
				opc = opc_lc64;
				rollbackPc(rt);
				logif(ip->arg.f8 != arg.i8, "inexact cast: %F => %D", ip->arg.f8, arg.i8);
			}
		}
		else if (opc == f64_bol) {
			ip = getip(rt, rt->vm.pc);
			if (ip->opc == opc_lf64) {
				arg.i8 = ip->arg.f8 != 0;
				opc = opc_lc32;
				rollbackPc(rt);
			}
		}
		// */

		/* mul, div, mod
		else if (opc == u32_mod) {
			ip = getip(s, s->vm.pc);
			if (ip->opc == opc_lc32) {
				int x = ip->arg.i4;
				// if constant contains only one bit
				if (x > 0 && lobit(x) == x) {
					ip->arg.i4 -= 1;
					opc = b32_and;
				}
			}
		}
		else if (opc == u32_div) {
			ip = getip(s, s->vm.pc);
			if (ip->opc == opc_lc32) {
				int x = ip->arg.i4;
				// if constant contains only one bit
				if (x > 0 && lobit(x) == x) {
					ip->arg.i4 ?= 1;
					opc = b32_shr;
				}
			}
		}*/

		// conditional jumps
		else if (opc == opc_jnz) {
			ip = getip(rt, rt->vm.pc);
			if (ip->opc == opc_not) {
				rt->_beg = (memptr)ip;
				opc = opc_jz;
			}
			// i32 to boolean can be skipped
			else if (ip->opc == i32_bol) {
				rt->_beg = (memptr)ip;
			}
		}
		else if (opc == opc_jz) {
			ip = getip(rt, rt->vm.pc);
			if (ip->opc == opc_not) {
				rt->_beg = (memptr)ip;
				opc = opc_jnz;
			}
			// i32 to boolean can be skipped
			else if (ip->opc == i32_bol) {
				rt->_beg = (memptr)ip;
			}
		}

		else if (opc == opc_spc) {
			if (arg.i8 == 0) {
				return rt->_beg - rt->_mem;
			}
			/* TODO: merge stack allocations
			ip = getip(rt, rt->vm.pc);
			if (ip->opc == opc_spc) {
				stkval tmp;
				tmp.i8 = arg.i8 + ip->rel;
				if (tmp.rel == tmp.i8) {
					rt->_beg = (memptr)ip;
					rt->vm.ss -= ip->rel / vm_size;
					arg.i8 += ip->rel;
				}
			}// */
		}

		/* TODO: only if we have no jump here
		else if (opc == opc_jmpi) {
			ip = getip(rt, rt->vm.pc);
			if (ip->opc == opc_jmpi) {
				//~ rt->_beg = (memptr)ip;
				return rt->vm.pc;
			}
		}*/

	}

	ip = (bcde)rt->_beg;

	ip->opc = opc;
	ip->arg = arg;
	rt->vm.pc = rt->_beg - rt->_mem;	// previous program pointer is here

	// post checks
	if ((opc == opc_jmp || opc == opc_jnz || opc == opc_jz) && arg.i8 != 0) {
		ip->rel -= rt->vm.pc;
		dieif((ip->rel + rt->vm.pc) != arg.u8, ERR_INTERNAL_ERROR);
	}
	else switch (opc) {
		case opc_spc:
			if (arg.i8 > 0x7fffff) {
				trace("local variable is too large: %D", arg.i8);
				return 0;
			}
			dieif(ip->rel != arg.i8, ERR_INTERNAL_ERROR);
			break;

		case opc_st64:
		case opc_ld64:
			dieif((ip->rel & 7) != 0, ERR_INTERNAL_ERROR);
			dieif(ip->rel != arg.i8, ERR_INTERNAL_ERROR);
			break;

		case opc_st32:
		case opc_ld32:
			dieif((ip->rel & 3) != 0, ERR_INTERNAL_ERROR);
			dieif(ip->rel != arg.i8, ERR_INTERNAL_ERROR);
			break;

		case opc_ldsp:
		case opc_move:
		case opc_inc:
		case opc_mad:
			dieif(ip->rel != arg.i8, ERR_INTERNAL_ERROR);
			break;

		default:
			break;
	}

	switch (opc) {
		error_opc:
			error(rt, NULL, 0, ERR_INVALID_INSTRUCTION, ip, rt->vm.pc);
			return 0;

		error_stc:
			// stack underflow
			error(rt, NULL, 0, ERR_INVALID_INSTRUCTION, ip, rt->vm.pc);
			return 0;

		#define STOP(__ERR, __CHK, __ERC) if (__CHK) goto __ERR
		#define NEXT(__IP, __SP, __CHK)\
			STOP(error_stc, (ptrdiff_t)rt->vm.ss < (ptrdiff_t)(__CHK), -1);\
			rt->vm.ss += (__SP);\
			rt->_beg += (__IP);
		#include "code.inl"
	}

	if (opc == opc_call) {
		//~ call ends with a return,
		//~ wich drops ip from the stack.
		rt->vm.ss -= 1;
	}

	// set max stack used.
	if (rt->vm.sm < rt->vm.ss) {
		rt->vm.sm = rt->vm.ss;
	}

//	trace("%A", ip);
	return rt->vm.pc;
}
int fixjump(rtContext rt, int src, int dst, int stc) {
	dieif(stc > 0 && stc & 3, ERR_INTERNAL_ERROR);
	if (src >= 0) {
		bcde ip = getip(rt, src);
		if (src) switch (ip->opc) {
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
				ip->rel = dst - src;
				dieif(ip->rel != dst - src, ERR_INTERNAL_ERROR);
				break;
		}
		if (stc >= 0) {
			rt->vm.ss = stc / vm_size;
		}
		return 1;
	}

	fatal(ERR_INTERNAL_ERROR);
	return 0;
}

// TODO: to be removed.
static inline void logProc(cell pu, int cp, char *msg) {
	trace("%s: {pu:%d, ip:%x, bp:%x, sp:%x, stacksize:%d, parent:%d, childs:%d}", msg, cp, pu[cp].ip, pu[cp].bp, pu[cp].sp, pu[cp].ss, pu[cp].pp, pu[cp].cp);
	(void)pu;
	(void)cp;
	(void)msg;
}

/// Try to start a new child cell for task.
static inline int task(cell pu, int n, int master, int cl) {
	// find an empty cell
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

		logProc(pu, master, "master");
		logProc(pu, slave, "slave");

		return slave;
	}
	return 0;
}

/// Wait for child cells to halt.
static inline int sync(cell pu, int cp, int wait) {
	int pp = pu[cp].pp;
	logProc(pu, cp, "join");

	// slave proc
	if (pp != cp) {
		if (pu[cp].cp == 0) {
			pu[cp].ip = NULL;
			pu[pp].cp -= 1;
			return 1;
		}
		return 0;
	}

	if (pu[cp].cp > 0) {
		return !wait;
	}

	// cell can continue to work
	return 1;
}

/// Check for stack overflow.
static inline int ovf(cell pu) {
	return (memptr)pu->sp < (memptr)pu->tp;
}

/** manage function call stack traces
 * @param rt runtime context
 * @param sp tack pointer (for tracing arguments
 * @param caller the address of the caller, NULL on start and end.
 * @param callee the called function adress, -1 on leave.
 * @param extra TO BE REMOVED
 */
static inline vmError traceCall(rtContext rt, void *sp, void *caller, void *callee) {
	dbgn (*debugger)(dbgContext, vmError, size_t, void*, void*, void*) = rt->dbg->debug;
	clock_t now = clock();
	cell pu = rt->vm.cell;
	if (rt->dbg == NULL) {
		return noError;
	}
	// trace statement
	if (callee == NULL) {
		fatal(ERR_INTERNAL_ERROR);
	}
	else if ((ptrdiff_t)callee < 0) {
		// trace leave
		tracePtr tp = pu->tp;
		int recursive = 0;

		if (tp - (trcptr)pu->bp < 1) {
			trace("stack underflow(tp: %d, sp: %d)", tp - (trcptr)pu->bp, pu->sp - (stkptr)pu->bp);
			return illegalInstruction;
		}

		pu->tp -= 1;
		for (tp = (tracePtr)pu->bp; tp < pu->tp; tp++) {
			if (tp->callee == pu->tp->callee) {
				recursive = 1;
				break;
			}
		}

		tp = pu->tp;
		clock_t ticks = now - tp->func;
		size_t callerOffs = vmOffset(rt, tp->caller);
		dbgn calleeFunc = mapDbgFunction(rt, vmOffset(rt, tp->callee));
		dbgn callerFunc = mapDbgFunction(rt, callerOffs);
		//dbgn callerStmt = mapDbgStatement(rt, callerOffs);
		if (calleeFunc != NULL) {
			calleeFunc->exec += callee == (void *) -1;
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
		tracePtr tp = pu->tp;
		if ((trcptr)pu->sp - tp < 1) {
			trace("stack overflow(tp: %d, sp: %d)", tp - (trcptr)pu->bp, pu->sp - (stkptr)pu->bp);
			return stackOverflow;
		}
		tp->caller = caller;
		tp->callee = callee;
		tp->func = now;
		tp->sp = sp;
		dbgn calleeFunc = mapDbgFunction(rt, vmOffset(rt, tp->callee));
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
static dbgn dbgDummy(dbgContext ctx, vmError err, size_t ss, void *stack, void *caller, void *callee) {
	char *errorStr = NULL;
	switch (err) {
		case noError:
			break;

		case invalidIP:
			errorStr = "Invalid instruction pointer";
			break;

		case invalidSP:
			errorStr = "Invalid stack pointer";
			break;

		case illegalInstruction:
			errorStr = "Illegal instruction";
			break;

		case stackOverflow:
			errorStr = "Stack overflow";
			break;

		case divisionByZero:
			errorStr = "Division by zero";
			break;

		case libCallAbort:
		case executionAborted:
			errorStr = "Native call aborted";
			break;

		case memReadError:
		case memWriteError:
			errorStr = "Invalid memory acces";
			break;

		default:
			errorStr = "Unknown error";
			break;
	}

	if (errorStr != NULL) {
		size_t offs = vmOffset(ctx->rt, caller);
		dbgn info = mapDbgStatement(ctx->rt, offs);
		char *file = NULL;
		int line = 0;
		if (info != NULL) {
			file = info->file;
			line = info->line;
		}
		error(ctx->rt, file, line, ERR_EXEC_INSTRUCTION, errorStr, caller, offs);
		return ctx->abort;
	}
	(void) callee;
	(void) stack;
	(void) ss;
	return NULL;
}

/**
 * @brief Execute bytecode.
 * @param rt Runtime context.
 * @param pu Cell to execute on.
 * @param fun Executing function.
 * @param extra Extra data for libcalls.
 * @param dbg function which is executed after each instruction or on error.
 * @return Error code of execution, 0 on success.
 */
static vmError exec(rtContext rt, cell pu, symn fun, const void *extra) {
	const int cc = 1;
	const libc libcvec = rt->vm.nfc;

	vmError execError = noError;
	const size_t ms = rt->_size;			// memory size
	const size_t ro = rt->vm.ro;			// read only region
	const memptr mp = (void*)rt->_mem;
	const stkptr st = (void*)(pu->bp + pu->ss);

	// run in debug or profile mode
	if (rt->dbg != NULL) {
		const void *oldTP = pu->tp;
		const stkptr spMin = (stkptr)(pu->bp);
		const stkptr spMax = (stkptr)(pu->bp + pu->ss);
		const bcde ipMin = (bcde)(rt->_mem + rt->vm.ro);
		const bcde ipMax = (bcde)(rt->_mem + rt->vm.px + px_size);

		dbgn (*debugger)(dbgContext, vmError, size_t, void*, void*, void*) = rt->dbg->debug;

		if (debugger == NULL) {
			debugger = dbgDummy;
		}
		// invoked function(from external code) will return with a ret instruction, removing trace info
		execError = traceCall(rt, pu->sp, NULL, getip(rt, fun->offs));
		if (execError != noError) {
			debugger(rt->dbg, execError, 0, pu->sp, pu->ip, NULL);
			return execError;
		}

		for ( ; ; ) {
			register const bcde ip = (bcde)pu->ip;
			register const stkptr sp = pu->sp;

			const tracePtr tp = pu->tp - 1;
			const size_t pc = vmOffset(rt, ip);
			dbgn dbg;

			if (ip >= ipMax || ip < ipMin) {
				debugger(rt->dbg, invalidIP, st - sp, sp, ip, NULL);
				return invalidIP;
			}
			if (sp > spMax || sp < spMin) {
				debugger(rt->dbg, invalidSP, st - sp, sp, ip, NULL);
				return invalidSP;
			}

			dbg = debugger(rt->dbg, noError, st - sp, sp, ip, NULL);
			if (dbg == rt->dbg->abort) {
				// abort execution from debugger
				return executionAborted;
			}
			if (dbg != NULL) {
				if (pc == dbg->start) {
					dbg->hits += 1;
					tp->stmt = clock();
					/* TODO: remove: print start executing statement
					symn sym = rtFindSym(rt, dbg->start, 1);
					size_t symOffs = 0;
					if (sym != NULL) {
						symOffs = dbg->start - sym->offs;
					}
					printFmt(stdout, NULL, "%?s:%?u:enter(%d) @%D <%?.T%+d> %.A\n", dbg->file, dbg->line, dbg->hits, (int64_t) tp->stmt, sym, symOffs, ip);
					//~ */
				}
			}
			switch (ip->opc) {
				dbg_stop_vm:	// halt virtual machine
					if (execError != noError) {
						debugger(rt->dbg, execError, st - sp, sp, ip, NULL);
						while (pu->tp != oldTP) {
							traceCall(rt, NULL, NULL, (void *) -2);
						}
					}
					while (pu->tp != oldTP) {
						traceCall(rt, NULL, NULL, (void *) -1);
					}
					return execError;

				dbg_error_opc:
					execError = illegalInstruction;
					goto dbg_stop_vm;

				dbg_error_ovf:
					execError = stackOverflow;
					goto dbg_stop_vm;

				dbg_error_mem_read:
					execError = memReadError;
					goto dbg_stop_vm;

				dbg_error_mem_write:
					execError = memWriteError;
					goto dbg_stop_vm;

				dbg_error_div_flt:
					debugger(rt->dbg, divisionByZero, st - sp, sp, ip, NULL);
					// continue execution on floating point division by zero.
					break;

				dbg_error_div:
					execError = divisionByZero;
					goto dbg_stop_vm;

				dbg_error_libc:
					execError = libCallAbort;
					goto dbg_stop_vm;

				#define NEXT(__IP, __SP, __CHK) pu->sp -= (__SP); pu->ip += (__IP);
				#define STOP(__ERR, __CHK, __ERC) do {if (__CHK) {goto dbg_##__ERR;}} while(0)
				#define EXEC
				#define TRACE(__SP, __CALLER, __CALLEE) do { if ((execError = traceCall(rt, __SP, __CALLER, __CALLEE)) != noError) goto dbg_stop_vm; } while(0)
				#include "code.inl"
			}
			if (dbg != NULL) {
				const tracePtr tp2 = pu->tp - 1;
				if ((memptr)ip != tp2->caller) {
					size_t pc2 = vmOffset(rt, pu->ip);
					if (pc2 < dbg->start || pc2 >= dbg->end) {
						clock_t now = clock();
						dbg->exec += 1;
						dbg->total += now - tp->stmt;
						/* TODO: remove: print end executing statement
						symn sym = rtFindSym(rt, dbg->start, 1);
						size_t symOffs = 0;
						if (sym != NULL) {
							symOffs = dbg->start - sym->offs;
						}
						printFmt(stdout, NULL, "%?s:%?u:leave(%d) @%D: %D <%?.T%+d> %.A\n", dbg->file, dbg->line, dbg->exec, (int64_t)now, (int64_t)(now - tp->stmt), sym, symOffs, ip);
						// */
					}
				}
			}
		}
		return execError;
	}

	// code for maximum execution speed
	for ( ; ; ) {
		register const bcde ip = (bcde)pu->ip;
		register const stkptr sp = pu->sp;
		switch (ip->opc) {
			stop_vm:	// halt virtual machine
				if (execError != noError) {
					struct dbgContextRec dbg = { .rt = rt };
					dbgDummy(&dbg, execError, st - sp, sp, ip, NULL);
				}
				return execError;

			error_opc:
				execError = illegalInstruction;
				goto stop_vm;

			error_ovf:
				execError = stackOverflow;
				goto stop_vm;

			error_mem_read:
				execError = memReadError;
				goto stop_vm;

			error_mem_write:
				execError = memWriteError;
				goto stop_vm;

			error_div_flt: {
					struct dbgContextRec dbg = { .rt = rt };
					dbgDummy(&dbg, divisionByZero, st - sp, sp, ip, NULL);
				}
				// continue execution on floating point division by zero.
				break;

			error_div:
				execError = divisionByZero;
				goto stop_vm;

			error_libc:
				execError = libCallAbort;
				goto stop_vm;

			#define NEXT(__IP, __SP, __CHK) { pu->sp -= (__SP); pu->ip += (__IP); }
			#define STOP(__ERR, __CHK, __ERC) do {if (__CHK) {goto __ERR;}} while(0)
			#define TRACE(__SP, __CALLER, __CALLEE)
			#define EXEC
			#include "code.inl"
		}
	}
	return execError;
}
vmError invoke(rtContext rt, symn fun, void *res, void *args, const void *extra) {
	const cell pu = rt->vm.cell;
	void *ip, *sp, *tp;
	vmError result;
	size_t resSize;

	if (pu == NULL) {
		fatal(ERR_INTERNAL_ERROR": can not invoke %?T without execute", fun);
		return illegalInstruction;
	}

	// invoked symbol must be a static function
	if (fun->params == NULL || fun->offs == 0 || !(fun->kind & ATTR_stat)) {
		dieif(fun->params == NULL, ERR_INTERNAL_ERROR);
		dieif(fun->offs == 0, ERR_INTERNAL_ERROR);
		dieif(!(fun->kind & ATTR_stat), ERR_INTERNAL_ERROR);
		return illegalInstruction;
	}

	// result is the last argument.
	resSize = fun->params->size;
	ip = pu->ip;
	sp = pu->sp;
	tp = pu->tp;

	// make space for result and arguments// result is the first param
	pu->sp -= fun->params->offs;

	if (args != NULL) {
		memcpy(pu->sp, args, fun->params->offs - resSize);
	}

	// return here: vm->px: program halt
	*(pu->sp -= 1) = rt->vm.px;

	pu->ip = getip(rt, fun->offs);
	result = exec(rt, pu, fun, extra);
	if (result == noError && res != NULL) {
		memcpy(res, sp - resSize, resSize);
	}

	pu->ip = ip;
	pu->sp = sp;
	pu->tp = tp;	// abort during exec

	return result;
}
vmError execute(rtContext rt, int argc, char *argv[], void *extra) {
	// TODO: cells should be in runtime read only memory?
	cell pu;

	// invalidate compiler
	rt->cc = NULL;

	// invalidate memory manager
	rt->vm.heap = NULL;

	// allocate cell(s)
	rt->_end = rt->_mem + rt->_size;
	rt->_end -= sizeof(struct cell);
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

	if ((ptrdiff_t)pu->sp & (vm_size - 1)) {
		fatal(ERR_INTERNAL_ERROR": invalid stack alignment");
		return invalidSP;
	}
	if ((stkptr)pu->bp > pu->sp) {
		fatal(ERR_INTERNAL_ERROR": invalid stack size");
		return stackOverflow;
	}

	return exec(rt, pu, rt->main, extra);
}

void printAsm(FILE *out, const char **esc, rtContext rt, void *ptr, int mode) {
	bcde ip = (bcde)ptr;
	size_t i, len = (size_t) mode & prAsmCode;
	size_t offs = (size_t)ptr;
	char *fmt_addr = " .%06x";
	char *fmt_offs = " <%?.T%?+d>";
	symn sym = NULL;

	if (rt != NULL) {
		offs = vmOffset(rt, ptr);
		if (mode & prAsmName) {
			sym = rtFindSym(rt, offs, 0);
		}
	}

	if (mode & prAsmAddr) {
		printFmt(out, esc, fmt_addr + 1, offs);
		printFmt(out, esc, ": ");
	}

	if (mode & prAsmName && sym != NULL) {
		size_t symOffs = offs - sym->offs;
		printFmt(out, esc, fmt_offs + 1, sym, symOffs);

		int paddLen = 4 + !symOffs;
		while (symOffs > 0) {
			symOffs /= 10;
			paddLen -= 1;
		}
		if (paddLen < 0) {
			paddLen = 0;
		}
		printFmt(out, esc, "% I: ", paddLen);
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

		case opc_inc:
		case opc_spc:
		case opc_ldsp:
		case opc_move:
		case opc_mad:
			printFmt(out, esc, " %d", ip->rel);
			break;

		case opc_jmp:
		case opc_jnz:
		case opc_jz:
		case opc_task: {
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
		}

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
		} break;

		case opc_dup1:
		case opc_dup2:
		case opc_dup4:
		case opc_set1:
		case opc_set2:
		case opc_set4:
			printFmt(out, esc, " sp(%d)", ip->idx);
			break;

		case opc_lc32:
			printFmt(out, esc, " %d", ip->arg.i4);
			break;

		case opc_lc64:
			printFmt(out, esc, " %D", ip->arg.i8);
			break;

		case opc_lf32:
			printFmt(out, esc, " %f", ip->arg.f4);
			break;

		case opc_lf64:
			printFmt(out, esc, " %F", ip->arg.f8);
			break;

		case opc_ld32:
		case opc_st32:
		case opc_ld64:
		case opc_st64:
		case opc_lref:
			if (ip->opc == opc_lref) {
				offs = ip->arg.u4;
			}
			else {
				offs = (size_t) ip->rel;
			}

			printFmt(out, esc, fmt_addr, offs);
			if (rt != NULL) {
				sym = rtFindSym(rt, offs, 0);
				if (sym != NULL) {
					printFmt(out, esc, " ;%?T%?+d", sym, offs - sym->offs);
				}
				else if (rt->cc != NULL) {
					char *str = getip(rt, offs);
					for (i = 0; i < TBL_SIZE; i += 1) {
						list lst;
						for (lst = rt->cc->strt[i]; lst; lst = lst->next) {
							if (str == (char*)lst->data) {
								printFmt(out, esc, " ;\"%s\"", str);
								i = TBL_SIZE;
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
				libc lc = NULL;
				if (rt->vm.nfc) {
					lc = &((libc)rt->vm.nfc)[offs];
				}
				else if (rt->cc != NULL) {
					for (lc = rt->cc->libc; lc; lc = lc->next) {
						if (lc->sym->offs == offs) {
							break;
						}
					}
				}
				if (lc && lc->sym) {
					printFmt(out, esc, " ;%T", lc->sym);
				}
			}
			break;

		case p4x_swz: {
			char c1 = "xyzw"[ip->idx >> 0 & 3];
			char c2 = "xyzw"[ip->idx >> 2 & 3];
			char c3 = "xyzw"[ip->idx >> 4 & 3];
			char c4 = "xyzw"[ip->idx >> 6 & 3];
			printFmt(out, esc, " %c%c%c%c(%02x)", c1, c2, c3, c4, ip->idx);
		} break;
	}
}

void printVal(FILE *out, const char **esc, rtContext rt, symn var, stkval *ref, int mode, int indent) {
	ccKind typCast, varCast = var->kind & MASK_cast;
	const char *fmt = var->pfmt;
	symn typ = var;

	if (indent > 0) {
		printFmt(out, esc, "%I", indent);
	}
	else {
		indent = -indent;
	}

	if (isVariable(var)) {
		typ = var->type;
		fmt = var->pfmt != NULL ? var->pfmt : typ->pfmt;

		if (ref != NULL && varCast == CAST_ref) {
			// resolve indirect references
			ref = getip(rt, ref->u4);
		}
	}
	else if (var->kind == KIND_def) {
		fmt = NULL;
		mode = 0;
	}
	else if ((void*)var == (void*)ref) {
		typ = var->type;
		fmt = NULL;
	}

	typCast = typ->kind & MASK_cast;
	if (var != typ) {
		int byref = 0;
		if (varCast == CAST_ref) {
			if (typCast != CAST_ref) {
				byref = '&';
			}
		}
		printFmt(out, esc, "%.*T%?c: ", mode & ~prSymType, var, byref);
	}

	if (mode & prSymType) {
		printFmt(out, esc, "%.T(", typ);
	}

	// null reference.
	if (ref == NULL) {
		printFmt(out, esc, "null");
	}
	// invalid offset.
	else if (!isValidOffset(rt, ref)) {
		printFmt(out, esc, "BadRef@%06x", var->offs);
	}
	// builtin type.
	else if (fmt != NULL) {
		if (fmt == type_fmt_typename) {
			printFmt(out, esc, fmt, ref);
		}
		else if (fmt == type_fmt_string) {
			printFmt(out, esc, fmt, ref);
		}
		else if (fmt == type_fmt_variant) {
			typ = getip(rt, (size_t) ref->var.type);
			ref = getip(rt, (size_t) ref->var.value);
			if (typ == NULL) {
				printFmt(out, esc, "null");
			}
			else {
				printFmt(out, esc, fmt, typ);
				printVal(out, esc, rt, typ, ref, mode & ~(prSymQual | prSymType), indent);
			}
		}
		else switch (typ->size) {
			default:
				// there should be no formated(<=> builtin) type matching none of this size.
				printFmt(out, esc, "%s @%06x, size: %d", fmt, vmOffset(rt, ref), var->size);
				break;

			case 1:
				if (typCast == CAST_u32) {
					printFmt(out, esc, fmt, ref->u1);
				}
				else {
					printFmt(out, esc, fmt, ref->i1);
				}
				break;

			case 2:
				if (typCast == CAST_u32) {
					printFmt(out, esc, fmt, ref->u2);
				}
				else {
					printFmt(out, esc, fmt, ref->i2);
				}
				break;

			case 4:
				if (typCast == CAST_f32) {
					printFmt(out, esc, fmt, ref->f4);
				}
				else if (typCast == CAST_u32) {
					// force zero extend (may sign extend to int64 ?).
					printFmt(out, esc, fmt, ref->u4);
				}
				else {
					printFmt(out, esc, fmt, ref->i4);
				}
				break;

			case 8:
				if (typCast == CAST_f64) {
					printFmt(out, esc, fmt, ref->f8);
				}
				else {
					printFmt(out, esc, fmt, ref->i8);
				}
				break;
		}
	}
	else if (indent > LOG_MAX_ITEMS) {
		printFmt(out, esc, "???");
	}
	else if (typ->fields != NULL) {
		int n = 0;
		symn fld;
		printFmt(out, esc, "{");
		for (fld = typ->fields; fld; fld = fld->next) {
			if (fld->kind & ATTR_stat) {
				// skip static fields
				continue;
			}

			if (!isVariable(fld)) {
				// skip inlines, types and functions
				continue;
			}

			if (n > 0) {
				printFmt(out, esc, ",");
			}

			printFmt(out, esc, "\n");
			printVal(out, esc, rt, fld, (void *) ((char *) ref + fld->offs), mode & ~prSymQual, indent + 1);
			n += 1;
		}
		if (n > 0) {
			printFmt(out, esc, "\n%I}", indent);
		}
		else {
			printFmt(out, esc, "}");
		}
	}
	else {
		// empty struct, typename, function, pointer, array
		size_t offs = vmOffset(rt, ref);
		symn sym = rtFindSym(rt, offs, 0);
		if (sym != NULL) {
			offs = offs - sym->offs;
		}
		printFmt(out, esc, "<%?.T%?+d@%06x>", sym, offs, vmOffset(rt, ref));
	}

	if (mode & prSymType) {
		printFmt(out, esc, ")");
	}
}

static void traceArgs(rtContext rt, FILE *out, symn fun, char *file, int line, void *sp, int ident) {
	symn sym;
	int printFileLine = 0;

	if (out == NULL) {
		out = rt->logFile;
	}
	if (file == NULL) {
		file = "native.code";
	}
	printFmt(out, NULL, "%I%s:%u: %?T", ident, file, line, fun);
	if (ident < 0) {
		printFileLine = 1;
		ident = -ident;
	}

	dieif(sp == NULL, ERR_INTERNAL_ERROR);
	dieif(fun == NULL, ERR_INTERNAL_ERROR);
	if (fun->params != NULL/* && fun->prms != rt->vars*/) {
		int firstArg = 1;
		if (ident > 0) {
			printFmt(out, NULL, "(");
		}
		else {
			printFmt(out, NULL, "\n");
		}
		for (sym = fun->params; sym; sym = sym->next) {
			char *offs = (char*)sp;

			if (firstArg == 0) {
				printFmt(out, NULL, ", ");
			}
			else {
				firstArg = 0;
			}

			if (printFileLine) {
				if (sym->file != NULL && sym->line != 0) {
					printFmt(out, NULL, "%I%s:%u: ", ident, sym->file, sym->line);
				}
				else {
					printFmt(out, NULL, "%I", ident);
				}
			}
			dieif(sym->kind & ATTR_stat, ERR_INTERNAL_ERROR);

			if (fun->kind == KIND_var) {
				// at vm_size is the return value of the function.
				offs += vm_size + fun->params->offs - sym->offs;
				printVal(out, NULL, rt, sym, (void *) offs, 0, -ident);
			}
			else {
				// TODO: find the offsets of arguments for libcalls.
				//offs += sym->offs;
				//printVal(rt, out, sym, (void*)offs, -ident, 0);
				printFmt(out, NULL, "%.T: %.T", sym, sym->type);
			}
		}
		if (ident > 0) {
			printFmt(out, NULL, ")");
		}
		else {
			printFmt(out, NULL, "\n");
		}
	}
}

void logTrace(dbgContext dbg, FILE *out, int indent, int startLevel, int traceLevel) {
	int i, hasOutput = 0;
	size_t pos;
	if (dbg == NULL) {
		return;
	}

	rtContext rt = dbg->rt;
	cell pu = rt->vm.cell;
	tracePtr tr = (tracePtr)pu->bp;

	if (out == NULL) {
		out = rt->logFile;
	}

	pos = pu->tp - tr - 1;
	if (pos < 1) {
		// TODO: check why we get 0
		pos = 1;
	}
	if (traceLevel > (int) pos) {
		traceLevel = (int) pos;
	}
	for (i = startLevel; i < traceLevel; ++i) {
		dbgn trInfo = mapDbgStatement(rt, vmOffset(rt, tr[pos - i].caller));
		symn fun = rtFindSym(rt, vmOffset(rt, tr[pos - i - 1].callee), 1);
		stkptr sp = tr[pos - i - 1].sp;
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
	if (i < (int) pos) {
		if (hasOutput > 0) {
			printFmt(out, NULL, "\n");
		}
		printFmt(out, NULL, "%I... %d more", indent, pos - i);
		hasOutput += 1;
	}

	if (hasOutput > 0) {
		printFmt(out, NULL, "\n");;
	}
}

int vmSelfTest() {
	int i, err = 0;
	int test = 0, skip = 0;
	FILE *out = stdout;
	struct bcde ip[1];
	struct libc libcvec[1];

	memset(ip, 0, sizeof(ip));
	memset(libcvec, 0, sizeof(libcvec));

	for (i = 0; i < opc_last; i++) {
		const struct opcodeRec *info = &opcode_tbl[i];
		unsigned int IS, CHK, DIFF;
		int NEXT = 0;

		if (info->name == NULL && info->size == 0 && info->stack_in == 0 && info->stack_out == 0) {
			// skip unimplemented instruction.
			skip += 1;
			continue;
		}

		switch (ip->opc = (uint8_t) i) {
			error_opc:
				printFmt(out, NULL, "Invalid instruction opc_x%02x: %s\n", i, info->name);
				err += 1;
				break;

			error_len:
				printFmt(out, NULL, "invalid opcode size 0x%02x: '%.A': expected: %d, found: %d\n", i, ip, info->size, IS);
				err += 1;
				break;

			error_chk:
				printFmt(out, NULL, "stack check 0x%02x: '%.A': expected: %d, found: %d\n", i, ip, info->stack_in, CHK);
				err += 1;
				break;

			error_diff:
				printFmt(out, NULL, "stack 0x%02x: '%.A': expected: %d, found: %d\n", i, ip,
					info->stack_out - info->stack_in, DIFF);
				err += 1;
				break;

			#define NEXT(__IP, __SP, __CHK) {\
				NEXT += 1;\
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
			#define STOP(__ERR, __CHK, __ERR1) if (__CHK) goto __ERR
			#include "code.inl"
		}

		test += 1;
		if (NEXT > 1 && i != opc_spc) {
			printFmt(out, NULL, "More than one NEXT: opcode 0x%02x: '%.A'\n", i, ip);
		}
	}
	printFmt(out, NULL, "vmSelfTest finished with %d errors from %d tested and %d skiped instructions\n", err, test, skip);
	return err;
}

/* when including this file define the following symbols:
 * NEXT(__IP, __SP, __CHK) advance ip, sp.
 *    __IP: instruction size.
 *    __SP: diff of stack after instruction executes.
 *    __CHK: number of elements needed on stack. (compile time only check.)
 *    #define NEXT(__IP, __SP, __CHK) if (checkStack(sp, __CHK)) { ip += __IP; sp += __SP; }
 * }
 * STOP(__ERR, __CHK, __ERC)
 * EXEC if execution is needed.
 * TRACE(__SP, __CALLER, __CALLEE) trace functions.
 *
 *
 *
*/

#define SP(__POS, __TYP) (((stkval*)((stkptr)sp + (__POS)))->__TYP)
#define MP(__POS, __TYP) (((stkval*)((memptr)mp + (__POS)))->__TYP)

//#{ switch (ip->opc) { ---------------------------------------------------------
//#{ 0x0?: SYS		// System
case opc_nop:  NEXT(1, 0, 0) {
} break;
case opc_nfc:  NEXT(4, libcvec[ip->rel].out - libcvec[ip->rel].in, libcvec[ip->rel].in) {
#ifdef EXEC
	vmError libcError;
	libc nfc = &libcvec[ip->rel];
	struct libcContextRec args = {
		.rt = rt,
		.extra = extra,
		.proto = nfc->proto,
		.args = sp,
		.argc = libcvec[ip->rel].in,
	};

	TRACE(sp, ip, nfc);
	libcError = nfc->call(&args);
	TRACE(sp, ip, (void*)-1);

	STOP(error_libc, libcError != 0, libcError);
	STOP(stop_vm, ip->rel == 0, 0);			// Halt();
#endif
} break;
case opc_call: NEXT(1, +1, 0) {
#ifdef EXEC
	size_t ret = pu->ip - mp;
	pu->ip = mp + SP(0, u4);
	SP(0, u4) = (uint32_t)ret;
	TRACE(sp, ip, pu->ip);
#endif
} break;
case opc_jmpi: NEXT(1, -1, 1) {
#ifdef EXEC
	pu->ip = mp + SP(0, u4);
	TRACE(sp, ip, (void*)-1);
#endif
} break;
case opc_jmp:  NEXT(4, -0, 0) {
#ifdef EXEC
	pu->ip += ip->rel - 4;
#endif
} break;
case opc_jnz:  NEXT(4, -1, 1) {
#ifdef EXEC
	if (SP(0, i4) != 0)
		pu->ip += ip->rel - 4;
#endif
} break;
case opc_jz:   NEXT(4, -1, 1) {
#ifdef EXEC
	if (SP(0, i4) == 0)
		pu->ip += ip->rel - 4;
#endif
} break;
case opc_task: NEXT(4, -0, 0) {
#ifdef EXEC
	if (task(pu, cc, -1, ip->cl * vm_size))
		pu->ip += ip->cl - 4;
#endif
} break;
case opc_sync: NEXT(2, -0, 0) {
#ifdef EXEC
	if (!sync(pu, -1, ip->idx)) {
		NEXT(-2, -0, 0);
	}
#endif
} break;
case opc_not:  NEXT(1, -0, 1) {
#if defined(EXEC)
	SP(0, u4) = !SP(0, u4);
#endif
} break;
case opc_inc:  NEXT(4, -0, 1) {
#ifdef EXEC
	SP(0, u4) += ip->rel;
#endif
} break;
case opc_mad:  NEXT(4, -1, 2) {
#if defined(EXEC)
	//~ SP(2, u4) += SP(1, u4) * SP(0, u4);
	SP(1, u4) += SP(0, u4) * ip->rel;
#endif
} break;
//#}
//#{ 0x1?: STK		// Stack
case opc_spc:  NEXT(4, 0, 0) {
	int sm = ip->rel / vm_size;
	STOP(error_opc, ip->rel & (vm_size - 1), vmOffset(rt, ip));
	if (sm > 0) {
		NEXT(0, sm, 0);
#ifdef EXEC
		STOP(error_ovf, ovf(pu), vmOffset(rt, ip));
#endif
	}
	else {
		NEXT(0, sm, -sm);
	}
} break;
case opc_ldsp: NEXT(4, +1, 0) {
#ifdef EXEC
	STOP(error_ovf, ovf(pu), vmOffset(rt, ip));
	SP(-1, i4) = (memptr)sp - mp + ip->rel;
#endif
} break;
case opc_dup1: NEXT(2, +1, ip->idx) {
#ifdef EXEC
	STOP(error_ovf, ovf(pu), vmOffset(rt, ip));
	SP(-1, u4) = SP(ip->idx, u4);
#endif
} break;
case opc_dup2: NEXT(2, +2, ip->idx) {
#ifdef EXEC
	STOP(error_ovf, ovf(pu), vmOffset(rt, ip));
	SP(-2, u8) = SP(ip->idx, u8);
#endif
} break;
case opc_dup4: NEXT(2, +4, ip->idx) {
#ifdef EXEC
	STOP(error_ovf, ovf(pu), vmOffset(rt, ip));
	SP(-4, u4) = SP(ip->idx + 0, u4);
	SP(-3, u4) = SP(ip->idx + 1, u4);
	SP(-2, u4) = SP(ip->idx + 2, u4);
	SP(-1, u4) = SP(ip->idx + 3, u4);
#endif
} break;
case opc_set1: NEXT(2, ip->idx <= 1 ? -ip->idx : -1, ip->idx) {
#ifdef EXEC
	SP(ip->idx, u4) = SP(0, u4);
#endif
} break;
case opc_set2: NEXT(2, ip->idx <= 2 ? -ip->idx : -2, ip->idx) {
#ifdef EXEC
	SP(ip->idx, i8) = SP(0, i8);
#endif
} break;
case opc_set4: NEXT(2, ip->idx <= 4 ? -ip->idx : -4, ip->idx) {
#ifdef EXEC
	SP(ip->idx + 3, u4) = SP(3, u4);
	SP(ip->idx + 2, u4) = SP(2, u4);
	SP(ip->idx + 1, u4) = SP(1, u4);
	SP(ip->idx + 0, u4) = SP(0, u4);
#endif
} break;
case opc_lzx1: NEXT(1, +1, 0) {
#ifdef EXEC
	STOP(error_ovf, ovf(pu), vmOffset(rt, ip));
	SP(-1, i4) = 0;
#endif
} break;
case opc_lzx2: NEXT(1, +2, 0) {
#ifdef EXEC
	STOP(error_ovf, ovf(pu), vmOffset(rt, ip));
	SP(-1, i4) = 0;
	SP(-2, i4) = 0;
#endif
} break;
case opc_lzx4: NEXT(1, +4, 0) {
#ifdef EXEC
	STOP(error_ovf, ovf(pu), vmOffset(rt, ip));
	SP(-1, i4) = 0;
	SP(-2, i4) = 0;
	SP(-3, i4) = 0;
	SP(-4, i4) = 0;
#endif
} break;
case opc_lf32: // temporary opc
case opc_lref: // temporary opc
case opc_lc32: NEXT(5, +1, 0) {
#ifdef EXEC
	STOP(error_ovf, ovf(pu), vmOffset(rt, ip));
	SP(-1, i4) = ip->arg.i4;
#endif
} break;
case opc_lf64: // temporary opc
case opc_lc64: NEXT(9, +2, 0) {
#ifdef EXEC
	STOP(error_ovf, ovf(pu), vmOffset(rt, ip));
	SP(-2, i8) = ip->arg.i8;
#endif
} break;
//#}
//#{ 0x2?: MEM		// Memory
case opc_ldi1: NEXT(1, -0, 1) {
#ifdef EXEC
	size_t mem = SP(0, i4);
	STOP(error_mem_read, mem <= 0, mem);
	STOP(error_mem_read, mem > ms - 1, mem);
	//~ STOP(error_mem_read, !aligned(mem, 1), mem);
	SP(0, i4) = MP(mem, i1);
#endif
} break;
case opc_ldi2: NEXT(1, -0, 1) {
#ifdef EXEC
	size_t mem = SP(0, i4);
	STOP(error_mem_read, mem <= 0, mem);
	STOP(error_mem_read, mem > ms - 2, mem);
	//~ STOP(error_mem_read, !aligned(mem, 2), mem);
	SP(0, i4) = MP(mem, i2);
#endif
} break;
case opc_ldi4: NEXT(1, -0, 1) {
#ifdef EXEC
	size_t mem = SP(0, i4);
	STOP(error_mem_read, mem <= 0, mem);
	STOP(error_mem_read, mem > ms - 4, mem);
	//~ STOP(error_mem_read, !aligned(mem, 4), mem);
	SP(0, i4) = MP(mem, i4);
#endif
} break;
case opc_ldi8: NEXT(1, +1, 1) {
#ifdef EXEC
	size_t mem = SP(0, i4);
	STOP(error_ovf, ovf(pu), vmOffset(rt, ip));
	STOP(error_mem_read, mem <= 0, mem);
	STOP(error_mem_read, mem > ms - 8, mem);
	//~ STOP(error_mem_read, !aligned(mem, 8), mem);
	SP(-1, i8) = MP(mem, i8);
#endif
} break;
case opc_ldiq: NEXT(1, +3, 1) {
#ifdef EXEC
	size_t mem = SP(0, i4);
	STOP(error_ovf, ovf(pu), vmOffset(rt, ip));
	STOP(error_mem_read, mem <= 0, mem);
	STOP(error_mem_read, mem > ms - 16, mem);
	//~ STOP(error_mem_read, !aligned(mem, 16), mem);
	memmove(&SP(-3, u4), &MP(mem, u4), 16);
#endif
} break;
case opc_sti1: NEXT(1, -2, 2) {
#ifdef EXEC
	size_t mem = SP(0, i4);
	STOP(error_mem_write, mem < ro, mem);
	STOP(error_mem_write, mem > ms - 1, mem);
	//~ STOP(error_mem_write, !aligned(mem, 1), mem);
	MP(mem, i1) = SP(1, i1);
#endif
} break;
case opc_sti2: NEXT(1, -2, 2) {
#ifdef EXEC
	size_t mem = SP(0, i4);
	STOP(error_mem_write, mem < ro, mem);
	STOP(error_mem_write, mem > ms - 2, mem);
	//~ STOP(error_mem_write, !aligned(mem, 2), mem);
	MP(mem, i2) = SP(1, i2);
#endif
} break;
case opc_sti4: NEXT(1, -2, 2) {
#ifdef EXEC
	size_t mem = SP(0, i4);
	STOP(error_mem_write, mem < ro, mem);
	STOP(error_mem_write, mem > ms - 4, mem);
	//~ STOP(error_mem_write, !aligned(mem, 4), mem);
	MP(mem, i4) = SP(1, i4);
#endif
} break;
case opc_sti8: NEXT(1, -3, 3) {
#ifdef EXEC
	size_t mem = SP(0, i4);
	STOP(error_mem_write, mem < ro, mem);
	STOP(error_mem_write, mem > ms - 8, mem);
	//~ STOP(error_mem_write, !aligned(mem, 8), mem);
	MP(mem, i8) = SP(1, i8);
#endif
} break;
case opc_stiq: NEXT(1, -5, 5) {
#ifdef EXEC
	size_t mem = SP(0, i4);
	STOP(error_mem_write, mem < ro, mem);
	STOP(error_mem_write, mem > ms - 16, mem);
	//~ STOP(error_mem_write, !aligned(mem, 16), mem);
	memmove(&MP(mem, u4), &SP(1, u4), 16);
#endif
} break;
case opc_ld32: NEXT(4, +1, 0) {
#ifdef EXEC
	size_t mem = ip->rel;
	STOP(error_mem_read, mem <= 0, mem);
	STOP(error_mem_read, mem > ms - 4, mem);
	//~ STOP(error_mem_read, !aligned(mem, 4), mem);
	SP(-1, i4) = MP(mem, i4);
#endif
} break;
case opc_ld64: NEXT(4, +2, 0) {
#ifdef EXEC
	size_t mem = ip->rel;
	STOP(error_mem_read, mem <= 0, mem);
	STOP(error_mem_read, mem > ms - 8, mem);
	//~ STOP(error_mem_read, !aligned(mem, 8), mem);
	SP(-2, i8) = MP(mem, i8);
#endif
} break;
case opc_st32: NEXT(4, -1, 1) {
#ifdef EXEC
	size_t mem = ip->rel;
	STOP(error_mem_write, mem < ro, mem);
	STOP(error_mem_write, mem > ms - 4, mem);
	//~ STOP(error_mem_write, !aligned(mem, 4), mem);
	MP(mem, i4) = SP(0, i4);
#endif
} break;
case opc_st64: NEXT(4, -2, 2) {
#ifdef EXEC
	size_t mem = ip->rel;
	STOP(error_mem_write, mem < ro, mem);
	STOP(error_mem_write, mem > ms - 8, mem);
	//~ STOP(error_mem_write, !aligned(mem, 8), mem);
	MP(mem, i8) = SP(0, i8);
#endif
} break;
case opc_move: NEXT(4, -2, 2) {
#ifdef EXEC
	size_t si, di;
	int cnt = ip->rel;

	if (cnt < 0) {
		cnt = -cnt;
		si = SP(1, i4);
		di = SP(0, i4);
	}
	else {
		si = SP(0, i4);
		di = SP(1, i4);
	}

	STOP(error_mem_read, si <= 0, si);
	STOP(error_mem_read, si > ms - cnt, si);
	STOP(error_mem_write, di < ro, di);
	STOP(error_mem_write, di > ms - cnt, di);

	memmove(mp + di, mp + si, cnt);

#endif
} break;
//#}
//#{ 0x3?: B32		// uint32
case b32_cmt: NEXT(1, -0, 1) {
#if defined(EXEC)
	SP(0, u4) = ~SP(0, u4);
#endif
} break;
case b32_and: NEXT(1, -1, 2) {
#if defined(EXEC)
	SP(1, u4) &= SP(0, u4);
#endif
} break;
case b32_ior: NEXT(1, -1, 2) {
#if defined(EXEC)
	SP(1, u4) |= SP(0, u4);
#endif
} break;
case u32_mul: NEXT(1, -1, 2) {
#if defined(EXEC)
	SP(1, u4) *= SP(0, u4);
#endif
} break;
case u32_div: NEXT(1, -1, 2) {
#if defined(EXEC)
	STOP(error_div, SP(0, u4) == 0, vmOffset(rt, ip));
	SP(1, u4) /= SP(0, u4);
#endif
} break;
case u32_mod: NEXT(1, -1, 2) {
#if defined(EXEC)
	STOP(error_div, SP(0, u4) == 0, vmOffset(rt, ip));
	SP(1, u4) %= SP(0, u4);
#endif
} break;
case b32_xor: NEXT(1, -1, 2) {
#if defined(EXEC)
	SP(1, u4) ^= SP(0, u4);
#endif
} break;
case u32_clt: NEXT(1, -1, 2) {
#if defined(EXEC)
	SP(1, u4) = SP(1, u4)  < SP(0, u4);
#endif
} break;
case u32_cgt: NEXT(1, -1, 2) {
#if defined(EXEC)
	SP(1, u4) = SP(1, u4)  > SP(0, u4);
#endif
} break;
case b32_shl: NEXT(1, -1, 2) {
#if defined(EXEC)
	SP(1, u4) <<= SP(0, u4);
#endif
} break;
case b32_shr: NEXT(1, -1, 2) {
#if defined(EXEC)
	SP(1, u4) >>= SP(0, u4);
#endif
} break;
case b32_sar: NEXT(1, -1, 2) {
#if defined(EXEC)
	SP(1, i4) >>= SP(0, i4);
#endif
} break;
case u32_i64: NEXT(1, +1, 1) {
#if defined(EXEC)
	SP(-1, i8) = SP(0, u4);
#endif
} break;
case b32_bit: NEXT(2, -0, 1) {
#if defined(EXEC)
	switch (ip->idx & 0xC0) {
		case b32_bit_and: SP(0, u4) &= (1 << (ip->idx & 0x3f)) - 1; break;
		case b32_bit_shl: SP(0, u4) <<= ip->idx & 0x3f; break;
		case b32_bit_shr: SP(0, u4) >>= ip->idx & 0x3f; break;
		case b32_bit_sar: SP(0, i4) >>= ip->idx & 0x3f; break;
		default: STOP(error_opc, 1, vmOffset(rt, ip));
	}
#endif
} break;
//#}
//#{ 0x3?: B32		// uint32
case b64_cmt: NEXT(1, -0, 2) {
#if defined(EXEC)
	SP(0, u8) = ~SP(0, u8);
#endif
} break;
case b64_and: NEXT(1, -2, 4) {
#if defined(EXEC)
	SP(2, u8) &= SP(0, u8);
#endif
} break;
case b64_ior: NEXT(1, -2, 4) {
#if defined(EXEC)
	SP(2, u8) |= SP(0, u8);
#endif
} break;
case u64_mul: NEXT(1, -2, 4) {
#if defined(EXEC)
	SP(2, u8) *= SP(0, u8);
#endif
} break;
case u64_div: NEXT(1, -2, 4) {
#if defined(EXEC)
	STOP(error_div, SP(0, u4) == 0, vmOffset(rt, ip));
	SP(2, u8) /= SP(0, u8);
#endif
} break;
case u64_mod: NEXT(1, -2, 4) {
#if defined(EXEC)
	STOP(error_div, SP(0, u4) == 0, vmOffset(rt, ip));
	SP(2, u8) %= SP(0, u8);
#endif
} break;
case b64_xor: NEXT(1, -2, 4) {
#if defined(EXEC)
	SP(2, u8) ^= SP(0, u8);
#endif
} break;
case u64_clt: NEXT(1, -3, 4) {
#if defined(EXEC)
	SP(3, u4) = SP(2, u8)  < SP(0, u8);
#endif
} break;
case u64_cgt: NEXT(1, -3, 4) {
#if defined(EXEC)
	SP(3, u4) = SP(2, u8)  > SP(0, u8);
#endif
} break;
case b64_shl: NEXT(1, -1, 3) {
#if defined(EXEC)
	SP(1, u8) <<= SP(0, u4);
#endif
} break;
case b64_shr: NEXT(1, -1, 3) {
#if defined(EXEC)
	SP(1, u8) >>= SP(0, u4);
#endif
} break;
case b64_sar: NEXT(1, -1, 3) {
#if defined(EXEC)
	SP(1, i8) >>= SP(0, i4);
#endif
} break;
//#}
//#{ 0x4?: I32		// int32
case i32_neg: NEXT(1, -0, 1) {
#if defined(EXEC)
	SP(0, i4) = -SP(0, i4);
#endif
} break;
case i32_add: NEXT(1, -1, 2) {
#if defined(EXEC)
	SP(1, i4) += SP(0, i4);
#endif
} break;
case i32_sub: NEXT(1, -1, 2) {
#if defined(EXEC)
	SP(1, i4) -= SP(0, i4);
#endif
} break;
case i32_mul: NEXT(1, -1, 2) {
#if defined(EXEC)
	SP(1, i4) *= SP(0, i4);
#endif
} break;
case i32_div: NEXT(1, -1, 2) {
#if defined(EXEC)
	STOP(error_div, SP(0, i4) == 0, vmOffset(rt, ip));
	SP(1, i4) /= SP(0, i4);
#endif
} break;
case i32_mod: NEXT(1, -1, 2) {
#if defined(EXEC)
	STOP(error_div, SP(0, i4) == 0, vmOffset(rt, ip));
	SP(1, i4) %= SP(0, i4);
#endif
} break;
case i32_ceq: NEXT(1, -1, 2) {
#if defined(EXEC)
	SP(1, i4) = SP(1, i4) == SP(0, i4);
#endif
} break;
case i32_clt: NEXT(1, -1, 2) {
#if defined(EXEC)
	SP(1, i4) = SP(1, i4)  < SP(0, i4);
#endif
} break;
case i32_cgt: NEXT(1, -1, 2) {
#if defined(EXEC)
	SP(1, i4) = SP(1, i4)  > SP(0, i4);
#endif
} break;
case i32_bol: NEXT(1, -0, 1) {
#if defined(EXEC)
	SP(0, i4) = 0 != SP(0, i4);
#endif
} break;
case i32_f32: NEXT(1, -0, 1) {
#if defined(EXEC)
	SP(0, f4) = (float32_t)SP(0, i4);
#endif
} break;
case i32_i64: NEXT(1, +1, 1) {
#if defined(EXEC)
	SP(-1, i8) = SP(0, i4);
#endif
} break;
case i32_f64: NEXT(1, +1, 1) {
#if defined(EXEC)
	SP(-1, f8) = SP(0, i4);
#endif
} break;
//#}
//#{ 0x6?: I64		// int64
case i64_neg: NEXT(1, -0, 2) {
#if defined(EXEC)
	SP(0, i8) = -SP(0, i8);
#endif
} break;
case i64_add: NEXT(1, -2, 4) {
#if defined(EXEC)
	SP(2, i8) += SP(0, i8);
#endif
} break;
case i64_sub: NEXT(1, -2, 4) {
#if defined(EXEC)
	SP(2, i8) -= SP(0, i8);
#endif
} break;
case i64_mul: NEXT(1, -2, 4) {
#if defined(EXEC)
	SP(2, i8) *= SP(0, i8);
#endif
} break;
case i64_div: NEXT(1, -2, 4) {
#if defined(EXEC)
	STOP(error_div, SP(0, i8) == 0, vmOffset(rt, ip));
	SP(2, i8) /= SP(0, i8);
#endif
} break;
case i64_mod: NEXT(1, -2, 4) {
#if defined(EXEC)
	STOP(error_div, SP(0, i8) == 0, vmOffset(rt, ip));
	SP(2, i8) %= SP(0, i8);
#endif
} break;
case i64_ceq: NEXT(1, -3, 4) {
#if defined(EXEC)
	SP(3, i4) = SP(2, i8) == SP(0, i8);
#endif
} break;
case i64_clt: NEXT(1, -3, 4) {
#if defined(EXEC)
	SP(3, i4) = SP(2, i8)  < SP(0, i8);
#endif
} break;
case i64_cgt: NEXT(1, -3, 4) {
#if defined(EXEC)
	SP(3, i4) = SP(2, i8)  > SP(0, i8);
#endif
} break;
case i64_i32: NEXT(1, -1, 2) {
#if defined(EXEC)
	SP(1, i4) = (int32_t)SP(0, i8);
#endif
} break;
case i64_f32: NEXT(1, -1, 2) {
#if defined(EXEC)
	SP(1, f4) = (float32_t)SP(0, i8);
#endif
} break;
case i64_bol: NEXT(1, -1, 2) {
#if defined(EXEC)
	SP(1, i4) = 0 != SP(0, i8);
#endif
} break;
case i64_f64: NEXT(1, -0, 2) {
#if defined(EXEC)
	SP(0, f8) = (float64_t)SP(0, i8);
#endif
} break;
//#}
//#{ 0x5?: F32		// float32
case f32_neg: NEXT(1, -0, 1) {
#if defined(EXEC)
	SP(0, f4) = -SP(0, f4);
#endif
} break;
case f32_add: NEXT(1, -1, 2) {
#if defined(EXEC)
	SP(1, f4) += SP(0, f4);
#endif
} break;
case f32_sub: NEXT(1, -1, 2) {
#if defined(EXEC)
	SP(1, f4) -= SP(0, f4);
#endif
} break;
case f32_mul: NEXT(1, -1, 2) {
#if defined(EXEC)
	SP(1, f4) *= SP(0, f4);
#endif
} break;
case f32_div: NEXT(1, -1, 2) {
#if defined(EXEC)
	SP(1, f4) /= SP(0, f4);
	STOP(error_div_flt, SP(0, f4) == 0., vmOffset(rt, ip));
#endif
} break;
case f32_mod: NEXT(1, -1, 2) {
#if defined(EXEC)
	SP(1, f4) = fmodf(SP(1, f4), SP(0, f4));
	STOP(error_div_flt, SP(0, f4) == 0, vmOffset(rt, ip));
#endif
} break;
case f32_ceq: NEXT(1, -1, 2) {
#if defined(EXEC)
	SP(1, i4) = SP(1, f4) == SP(0, f4);
#endif
} break;
case f32_clt: NEXT(1, -1, 2) {
#if defined(EXEC)
	SP(1, i4) = SP(1, f4)  < SP(0, f4);
#endif
} break;
case f32_cgt: NEXT(1, -1, 2) {
#if defined(EXEC)
	SP(1, i4) = SP(1, f4)  > SP(0, f4);
#endif
} break;
case f32_i32: NEXT(1, -0, 1) {
#if defined(EXEC)
	SP(0, i4) = (int32_t)SP(0, f4);
#endif
} break;
case f32_bol: NEXT(1, -0, 1) {
#if defined(EXEC)
	SP(0, i4) = 0 != SP(0, f4);
#endif
} break;
case f32_i64: NEXT(1, +1, 1) {
#if defined(EXEC)
	STOP(error_ovf, ovf(pu), vmOffset(rt, ip));
	SP(-1, i8) = (int64_t)SP(0, f4);
#endif
} break;
case f32_f64: NEXT(1, +1, 1) {
#if defined(EXEC)
	STOP(error_ovf, ovf(pu), vmOffset(rt, ip));
	SP(-1, f8) = SP(0, f4);
#endif
} break;
//#}
//#{ 0x7?: F64		// float64
case f64_neg: NEXT(1, -0, 2) {
#if defined(EXEC)
	SP(0, f8) = -SP(0, f8);
#endif
} break;
case f64_add: NEXT(1, -2, 4) {
#if defined(EXEC)
	SP(2, f8) += SP(0, f8);
#endif
} break;
case f64_sub: NEXT(1, -2, 4) {
#if defined(EXEC)
	SP(2, f8) -= SP(0, f8);
#endif
} break;
case f64_mul: NEXT(1, -2, 4) {
#if defined(EXEC)
	SP(2, f8) *= SP(0, f8);
#endif
} break;
case f64_div: NEXT(1, -2, 4) {
#if defined(EXEC)
	SP(2, f8) /= SP(0, f8);
	STOP(error_div_flt, SP(0, f8) == 0., vmOffset(rt, ip));
#endif
} break;
case f64_mod: NEXT(1, -2, 4) {
#if defined(EXEC)
	SP(2, f8) = fmod(SP(2, f8), SP(0, f8));
	STOP(error_div_flt, SP(0, f8) == 0, vmOffset(rt, ip));
#endif
} break;
case f64_ceq: NEXT(1, -3, 4) {
#if defined(EXEC)
	SP(3, i4) = SP(2, f8) == SP(0, f8);
#endif
} break;
case f64_clt: NEXT(1, -3, 4) {
#if defined(EXEC)
	SP(3, i4) = SP(2, f8)  < SP(0, f8);
#endif
} break;
case f64_cgt: NEXT(1, -3, 4) {
#if defined(EXEC)
	SP(3, i4) = SP(2, f8)  > SP(0, f8);
#endif
} break;
case f64_i32: NEXT(1, -1, 2) {
#if defined(EXEC)
	SP(1, i4) = (int32_t)SP(0, f8);
#endif
} break;
case f64_f32: NEXT(1, -1, 2) {
#if defined(EXEC)
	SP(1, f4) = (float32_t)SP(0, f8);
#endif
} break;
case f64_i64: NEXT(1, -0, 2) {
#if defined(EXEC)
	SP(0, i8) = (int64_t)SP(0, f8);
#endif
} break;
case f64_bol: NEXT(1, -1, 2) {
#if defined(EXEC)
	SP(1, i4) = 0 != SP(0, f8);
#endif
} break;
//#}
//#{ 0x8?: P4F		// packed 4 floats
case v4f_neg: NEXT(1, -0, 4) {
#if defined(EXEC)
	SP(0, f4) = -SP(0, f4);
	SP(1, f4) = -SP(1, f4);
	SP(2, f4) = -SP(2, f4);
	SP(3, f4) = -SP(3, f4);
#endif
} break;
case v4f_add: NEXT(1, -4, 8) {
#if defined(EXEC)
	SP(4, f4) += SP(0, f4);
	SP(5, f4) += SP(1, f4);
	SP(6, f4) += SP(2, f4);
	SP(7, f4) += SP(3, f4);
#endif
} break;
case v4f_sub: NEXT(1, -4, 8) {
#if defined(EXEC)
	SP(4, f4) -= SP(0, f4);
	SP(5, f4) -= SP(1, f4);
	SP(6, f4) -= SP(2, f4);
	SP(7, f4) -= SP(3, f4);
#endif
} break;
case v4f_mul: NEXT(1, -4, 8) {
#if defined(EXEC)
	SP(4, f4) *= SP(0, f4);
	SP(5, f4) *= SP(1, f4);
	SP(6, f4) *= SP(2, f4);
	SP(7, f4) *= SP(3, f4);
#endif
} break;
case v4f_div: NEXT(1, -4, 8) {
#if defined(EXEC)
	//~ STOP(error_div, !SP(0, f4) || !SP(1, f4) || !SP(2, f4) || !SP(3, f4));
	SP(4, f4) /= SP(0, f4);
	SP(5, f4) /= SP(1, f4);
	SP(6, f4) /= SP(2, f4);
	SP(7, f4) /= SP(3, f4);
#endif
} break;
case v4f_ceq: NEXT(1, -7, 8) {
#if defined(EXEC)
	SP(7, i4) = (SP(4, f4) == SP(0, f4))
			 && (SP(5, f4) == SP(1, f4))
			 && (SP(6, f4) == SP(2, f4))
			 && (SP(7, f4) == SP(3, f4));
#endif
} break;
case v4f_min: NEXT(1, -4, 8) {
#if defined(EXEC)
	if (SP(4, f4) > SP(0, f4))
		SP(4, f4) = SP(0, f4);
	if (SP(5, f4) > SP(1, f4))
		SP(5, f4) = SP(1, f4);
	if (SP(6, f4) > SP(2, f4))
		SP(6, f4) = SP(2, f4);
	if (SP(7, f4) > SP(3, f4))
		SP(7, f4) = SP(3, f4);
#endif
} break;
case v4f_max: NEXT(1, -4, 8) {
#if defined(EXEC)
	if (SP(4, f4) < SP(0, f4))
		SP(4, f4) = SP(0, f4);
	if (SP(5, f4) < SP(1, f4))
		SP(5, f4) = SP(1, f4);
	if (SP(6, f4) < SP(2, f4))
		SP(6, f4) = SP(2, f4);
	if (SP(7, f4) < SP(3, f4))
		SP(7, f4) = SP(3, f4);
#endif
} break;
case v4f_dp3: NEXT(1, -7, 8) {
#if defined(EXEC)
	SP(7, f4) =
	SP(4, f4) * SP(0, f4) +
	SP(5, f4) * SP(1, f4) +
	SP(6, f4) * SP(2, f4);
#endif
} break;
case v4f_dph: NEXT(1, -7, 8) {
#if defined(EXEC)
	SP(7, f4) +=
	SP(4, f4) * SP(0, f4) +
	SP(5, f4) * SP(1, f4) +
	SP(6, f4) * SP(2, f4);
#endif
} break;
case v4f_dp4: NEXT(1, -7, 8) {
#if defined(EXEC)
	SP(7, f4) =
	SP(4, f4) * SP(0, f4) +
	SP(5, f4) * SP(1, f4) +
	SP(6, f4) * SP(2, f4) +
	SP(7, f4) * SP(3, f4);
#endif
} break;
//#}
//#{ 0x9?: PD2		// packed 2 doubles
case v2d_neg: NEXT(1, -0, 4) {
#if defined(EXEC)
	SP(0, f8) = -SP(0, f8);
	SP(2, f8) = -SP(2, f8);
#endif
} break;
case v2d_add: NEXT(1, -4, 8) {
#if defined(EXEC)
	SP(4, f8) += SP(0, f8);
	SP(6, f8) += SP(2, f8);
#endif
} break;
case v2d_sub: NEXT(1, -4, 8) {
#if defined(EXEC)
	SP(4, f8) -= SP(0, f8);
	SP(6, f8) -= SP(2, f8);
#endif
} break;
case v2d_mul: NEXT(1, -4, 8) {
#if defined(EXEC)
	SP(4, f8) *= SP(0, f8);
	SP(6, f8) *= SP(2, f8);
#endif
} break;
case v2d_div: NEXT(1, -4, 8) {
#if defined(EXEC)
	//~ STOP(error_div, SP(0, f8) || SP(2, f8));
	SP(4, f8) /= SP(0, f8);
	SP(6, f8) /= SP(2, f8);
#endif
} break;
case p4x_swz: NEXT(2, -0, 4) {
#if defined(EXEC)
	uint32_t swz = ip->idx;
	uint32_t d0 = SP((swz >> 0) & 3, i4);
	uint32_t d1 = SP((swz >> 2) & 3, i4);
	uint32_t d2 = SP((swz >> 4) & 3, i4);
	uint32_t d3 = SP((swz >> 6) & 3, i4);
	SP(0, i4) = d0;
	SP(1, i4) = d1;
	SP(2, i4) = d2;
	SP(3, i4) = d3;
#endif
} break;
case v2d_ceq: NEXT(1, -7, 8) {
#if defined(EXEC)
	SP(7, i4) = (SP(4, f8) == SP(0, f8)) && (SP(6, f8) == SP(2, f8));
#endif
} break;
case v2d_min: NEXT(1, -4, 8) {
#if defined(EXEC)
	if (SP(4, f8) > SP(0, f8))
		SP(4, f8) = SP(0, f8);
	if (SP(6, f8) > SP(2, f8))
		SP(6, f8) = SP(2, f8);
#endif
} break;
case v2d_max: NEXT(1, -4, 8) {
#if defined(EXEC)
	if (SP(4, f8) < SP(0, f8))
		SP(4, f8) = SP(0, f8);
	if (SP(6, f8) < SP(2, f8))
		SP(6, f8) = SP(2, f8);
#endif
} break;
//#}
//~ 0xa?: ???		//
//~ 0xb?: ???		//
//~ 0xc?: ???		//
//~ 0xd?: ???		// ext i32+i64
//~ 0xe?: ???		// ext f32+f64
//~ 0xf?: ???		// ext p4x
default:
	STOP(error_opc, 1, vmOffset(rt, ip));
	break;
//#}-----------------------------------------------------------------------------

#undef SP
#undef MP

#undef EXEC
#undef STOP
#undef NEXT
#undef TRACE

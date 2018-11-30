/* when including this file define the following symbols:
 * NEXT(__IP, __SP, __CHK) advance ip, sp.
 *    __IP: instruction size.
 *    __SP: diff of stack after instruction executes.
 *    __CHK: number of elements needed on stack. (compile time only check.)
 *    #define NEXT(__IP, __SP, __CHK) if (checkStack(sp, __CHK)) { ip += __IP; sp += __SP; }
 * }
 * STOP(__ERR, __CHK)
 * EXEC if execution is needed.
 * TRACE(__CALLEE) trace functions.
 *
 *
 *
*/

#define SP(__POS, __TYP) (((vmValue *)((stkptr)sp + (__POS)))->__TYP)
#define MP(__POS, __TYP) (((vmValue *)((memptr)mp + (__POS)))->__TYP)

//#{ switch (ip->opc) { ---------------------------------------------------------
//#{ 0x0?: SYS		// System
case opc_nop:  NEXT(1, 0, 0) {
	break;
}
case opc_nfc:  NEXT(4, 0, 0) {
	libc nfc = nativeCalls[ip->rel];
#ifdef EXEC
	struct nfcContextRec args;
	*((void const **) &args.rt) = rt;
	*((void const **) &args.sym) = nfc->sym;
	*((void const **) &args.extra) = extra;
	*((char const **) &args.proto) = nfc->proto;
	*((void const **) &args.args) = sp;
	*((size_t*) &args.argc) = vm_stk_align * nfc->in;
	args.param = (void *) -1;

	TRACE(nfc->sym->offs);
	vmError nfcError = nfc->call(&args);
	TRACE((size_t)-1);

	STOP(error_libc, nfcError != noError);
	STOP(stop_vm, ip->rel == 0);			// Halt();
#endif
	NEXT(0, nfc->out - nfc->in, nfc->in);
	break;
}
case opc_call: NEXT(1, +0, 1) {
#ifdef EXEC
	size_t jmp = SP(0, u32);
	size_t ret = pu->ip - mp;
	STOP(error_opc, jmp <= 0);
	STOP(error_opc, jmp > ms);
	pu->ip = mp + jmp;
	SP(0, u32) = ret;
	TRACE(jmp);
#endif
	break;
}
case opc_jmpi: NEXT(1, -1, 1) {
#ifdef EXEC
	size_t jmp = SP(0, u32);
	STOP(error_opc, jmp <= 0);
	STOP(error_opc, jmp > ms);
	pu->ip = mp + jmp;
	TRACE((size_t)-1);
#endif
	break;
}
case opc_jmp:  NEXT(4, -0, 0) {
#ifdef EXEC
	pu->ip += ip->rel - 4;
#endif
	break;
}
case opc_jnz:  NEXT(4, -1, 1) {
#ifdef EXEC
	if (SP(0, i32) != 0)
		pu->ip += ip->rel - 4;
#endif
	break;
}
case opc_jz:   NEXT(4, -1, 1) {
#ifdef EXEC
	if (SP(0, i32) == 0)
		pu->ip += ip->rel - 4;
#endif
	break;
}
case opc_task: NEXT(4, -0, 0) {
#ifdef EXEC
	if (vmFork(pu, cc, -1, ip->cl * vm_stk_align))
		pu->ip += ip->cl - 4;
#endif
	break;
}
case opc_sync: NEXT(2, -0, 0) {
#ifdef EXEC
	if (!vmJoin(pu, -1, ip->idx)) {
		NEXT(-2, -0, 0);
	}
#endif
	break;
}
case opc_not:  NEXT(1, -0, 1) {
#if defined(EXEC)
	SP(0, u32) = !SP(0, u32);
#endif
	break;
}
case opc_inc:  NEXT(4, -0, 1) {
#ifdef EXEC
	SP(0, u32) += ip->rel;
#endif
	break;
}
case opc_mad:  NEXT(4, -1, 2) {
#if defined(EXEC)
	//~ SP(2, u32) += SP(1, u32) * SP(0, u32);
	SP(1, u32) += SP(0, u32) * ip->rel;
#endif
	break;
}
//#}
//#{ 0x1?: STK		// Stack
case opc_spc:  NEXT(4, 0, 0) {
	int sm = ip->rel / vm_stk_align;
	STOP(error_opc, ip->rel & (vm_stk_align - 1));
	if (sm > 0) {
		NEXT(0, sm, 0);
#ifdef EXEC
		STOP(error_ovf, ovf(pu));
#endif
	}
	else {
		NEXT(0, sm, -sm);
	}
	break;
}
case opc_ldsp: NEXT(4, +1, 0) {
#ifdef EXEC
	STOP(error_ovf, ovf(pu));
	SP(-1, i32) = (memptr)sp - mp + ip->rel;
#endif
	break;
}
case opc_dup1: NEXT(2, +1, ip->idx) {
#ifdef EXEC
	STOP(error_ovf, ovf(pu));
	SP(-1, u32) = SP(ip->idx, u32);
#endif
	break;
}
case opc_dup2: NEXT(2, +2, ip->idx) {
#ifdef EXEC
	STOP(error_ovf, ovf(pu));
	SP(-2, u64) = SP(ip->idx, u64);
#endif
	break;
}
case opc_dup4: NEXT(2, +4, ip->idx) {
#ifdef EXEC
	STOP(error_ovf, ovf(pu));
	SP(-4, u32) = SP(ip->idx + 0, u32);
	SP(-3, u32) = SP(ip->idx + 1, u32);
	SP(-2, u32) = SP(ip->idx + 2, u32);
	SP(-1, u32) = SP(ip->idx + 3, u32);
#endif
	break;
}
case opc_set1: NEXT(2, ip->idx <= 1 ? -ip->idx : -1, ip->idx) {
#ifdef EXEC
	SP(ip->idx, u32) = SP(0, u32);
#endif
	break;
}
case opc_set2: NEXT(2, ip->idx <= 2 ? -ip->idx : -2, ip->idx) {
#ifdef EXEC
	SP(ip->idx, i64) = SP(0, i64);
#endif
	break;
}
case opc_set4: NEXT(2, ip->idx <= 4 ? -ip->idx : -4, ip->idx) {
#ifdef EXEC
	SP(ip->idx + 3, u32) = SP(3, u32);
	SP(ip->idx + 2, u32) = SP(2, u32);
	SP(ip->idx + 1, u32) = SP(1, u32);
	SP(ip->idx + 0, u32) = SP(0, u32);
#endif
	break;
}
case opc_lzx1: NEXT(1, +1, 0) {
#ifdef EXEC
	STOP(error_ovf, ovf(pu));
	SP(-1, i32) = 0;
#endif
	break;
}
case opc_lzx2: NEXT(1, +2, 0) {
#ifdef EXEC
	STOP(error_ovf, ovf(pu));
	SP(-1, i32) = 0;
	SP(-2, i32) = 0;
#endif
	break;
}
case opc_lzx4: NEXT(1, +4, 0) {
#ifdef EXEC
	STOP(error_ovf, ovf(pu));
	SP(-1, i32) = 0;
	SP(-2, i32) = 0;
	SP(-3, i32) = 0;
	SP(-4, i32) = 0;
#endif
	break;
}
case opc_lf32: // temporary opc
case opc_lref: // temporary opc
case opc_lc32: NEXT(5, +1, 0) {
#ifdef EXEC
	STOP(error_ovf, ovf(pu));
	SP(-1, i32) = ip->arg.i32;
#endif
	break;
}
case opc_lf64: // temporary opc
case opc_lc64: NEXT(9, +2, 0) {
#ifdef EXEC
	STOP(error_ovf, ovf(pu));
	SP(-2, i64) = ip->arg.i64;
#endif
	break;
}
//#}
//#{ 0x2?: MEM		// Memory
case opc_ldi1: NEXT(1, -0, 1) {
#ifdef EXEC
	size_t mem = SP(0, i32);
	STOP(error_mem, mem <= 0);
	STOP(error_mem, mem > ms - 1);
	//~ STOP(error_mem, !aligned(mem, 1));
	SP(0, i32) = MP(mem, i08);
#endif
	break;
}
case opc_ldi2: NEXT(1, -0, 1) {
#ifdef EXEC
	size_t mem = SP(0, i32);
	STOP(error_mem, mem <= 0);
	STOP(error_mem, mem > ms - 2);
	//~ STOP(error_mem, !aligned(mem, 2));
	SP(0, i32) = MP(mem, i16);
#endif
	break;
}
case opc_ldi4: NEXT(1, -0, 1) {
#ifdef EXEC
	size_t mem = SP(0, i32);
	STOP(error_mem, mem <= 0);
	STOP(error_mem, mem > ms - 4);
	//~ STOP(error_mem, !aligned(mem, 4));
	SP(0, i32) = MP(mem, i32);
#endif
	break;
}
case opc_ldi8: NEXT(1, +1, 1) {
#ifdef EXEC
	size_t mem = SP(0, i32);
	STOP(error_ovf, ovf(pu));
	STOP(error_mem, mem <= 0);
	STOP(error_mem, mem > ms - 8);
	//~ STOP(error_mem, !aligned(mem, 8));
	SP(-1, i64) = MP(mem, i64);
#endif
	break;
}
case opc_ldiq: NEXT(1, +3, 1) {
#ifdef EXEC
	size_t mem = SP(0, i32);
	STOP(error_ovf, ovf(pu));
	STOP(error_mem, mem <= 0);
	STOP(error_mem, mem > ms - 16);
	//~ STOP(error_mem, !aligned(mem, 16));
	memmove(&SP(-3, u32), &MP(mem, u32), 16);
#endif
	break;
}
case opc_sti1: NEXT(1, -2, 2) {
#ifdef EXEC
	size_t mem = SP(0, i32);
	STOP(error_mem, mem < ro);
	STOP(error_mem, mem > ms - 1);
	//~ STOP(error_mem, !aligned(mem, 1));
	MP(mem, i08) = SP(1, i08);
#endif
	break;
}
case opc_sti2: NEXT(1, -2, 2) {
#ifdef EXEC
	size_t mem = SP(0, i32);
	STOP(error_mem, mem < ro);
	STOP(error_mem, mem > ms - 2);
	//~ STOP(error_mem, !aligned(mem, 2));
	MP(mem, i16) = SP(1, i16);
#endif
	break;
}
case opc_sti4: NEXT(1, -2, 2) {
#ifdef EXEC
	size_t mem = SP(0, i32);
	STOP(error_mem, mem < ro);
	STOP(error_mem, mem > ms - 4);
	//~ STOP(error_mem, !aligned(mem, 4));
	MP(mem, i32) = SP(1, i32);
#endif
	break;
}
case opc_sti8: NEXT(1, -3, 3) {
#ifdef EXEC
	size_t mem = SP(0, i32);
	STOP(error_mem, mem < ro);
	STOP(error_mem, mem > ms - 8);
	//~ STOP(error_mem, !aligned(mem, 8));
	MP(mem, i64) = SP(1, i64);
#endif
	break;
}
case opc_stiq: NEXT(1, -5, 5) {
#ifdef EXEC
	size_t mem = SP(0, i32);
	STOP(error_mem, mem < ro);
	STOP(error_mem, mem > ms - 16);
	//~ STOP(error_mem, !aligned(mem, 16));
	memmove(&MP(mem, u32), &SP(1, u32), 16);
#endif
	break;
}
case opc_ld32: NEXT(4, +1, 0) {
#ifdef EXEC
	size_t mem = ip->rel;
	STOP(error_mem, mem <= 0);
	STOP(error_mem, mem > ms - 4);
	//~ STOP(error_mem, !aligned(mem, 4));
	SP(-1, i32) = MP(mem, i32);
#endif
	break;
}
case opc_ld64: NEXT(4, +2, 0) {
#ifdef EXEC
	size_t mem = ip->rel;
	STOP(error_mem, mem <= 0);
	STOP(error_mem, mem > ms - 8);
	//~ STOP(error_mem, !aligned(mem, 8));
	SP(-2, i64) = MP(mem, i64);
#endif
	break;
}
case opc_st32: NEXT(4, -1, 1) {
#ifdef EXEC
	size_t mem = ip->rel;
	STOP(error_mem, mem < ro);
	STOP(error_mem, mem > ms - 4);
	//~ STOP(error_mem, !aligned(mem, 4));
	MP(mem, i32) = SP(0, i32);
#endif
	break;
}
case opc_st64: NEXT(4, -2, 2) {
#ifdef EXEC
	size_t mem = ip->rel;
	STOP(error_mem, mem < ro);
	STOP(error_mem, mem > ms - 8);
	//~ STOP(error_mem, !aligned(mem, 8));
	MP(mem, i64) = SP(0, i64);
#endif
	break;
}
case opc_move: NEXT(4, -2, 2) {
#ifdef EXEC
	size_t si, di;
	int cnt = ip->rel;

	if (cnt < 0) {
		cnt = -cnt;
		si = SP(1, i32);
		di = SP(0, i32);
	}
	else {
		si = SP(0, i32);
		di = SP(1, i32);
	}

	STOP(error_mem, si <= 0);
	STOP(error_mem, si > ms - cnt);
	STOP(error_mem, di < ro);
	STOP(error_mem, di > ms - cnt);

	memmove(mp + di, mp + si, cnt);

#endif
	break;
}
//#}
//#{ 0x3?: B32		// uint32
case b32_cmt: NEXT(1, -0, 1) {
#if defined(EXEC)
	SP(0, u32) = ~SP(0, u32);
#endif
	break;
}
case b32_and: NEXT(1, -1, 2) {
#if defined(EXEC)
	SP(1, u32) &= SP(0, u32);
#endif
	break;
}
case b32_ior: NEXT(1, -1, 2) {
#if defined(EXEC)
	SP(1, u32) |= SP(0, u32);
#endif
	break;
}
case u32_mul: NEXT(1, -1, 2) {
#if defined(EXEC)
	SP(1, u32) *= SP(0, u32);
#endif
	break;
}
case u32_div: NEXT(1, -1, 2) {
#if defined(EXEC)
	STOP(error_div, SP(0, u32) == 0);
	SP(1, u32) /= SP(0, u32);
#endif
	break;
}
case u32_mod: NEXT(1, -1, 2) {
#if defined(EXEC)
	STOP(error_div, SP(0, u32) == 0);
	SP(1, u32) %= SP(0, u32);
#endif
	break;
}
case b32_xor: NEXT(1, -1, 2) {
#if defined(EXEC)
	SP(1, u32) ^= SP(0, u32);
#endif
	break;
}
case u32_clt: NEXT(1, -1, 2) {
#if defined(EXEC)
	SP(1, u32) = SP(1, u32)  < SP(0, u32);
#endif
	break;
}
case u32_cgt: NEXT(1, -1, 2) {
#if defined(EXEC)
	SP(1, u32) = SP(1, u32)  > SP(0, u32);
#endif
	break;
}
case b32_shl: NEXT(1, -1, 2) {
#if defined(EXEC)
	SP(1, u32) <<= SP(0, u32);
#endif
	break;
}
case b32_shr: NEXT(1, -1, 2) {
#if defined(EXEC)
	SP(1, u32) >>= SP(0, u32);
#endif
	break;
}
case b32_sar: NEXT(1, -1, 2) {
#if defined(EXEC)
	SP(1, i32) >>= SP(0, i32);
#endif
	break;
}
case u32_i64: NEXT(1, +1, 1) {
#if defined(EXEC)
	SP(-1, i64) = SP(0, u32);
#endif
	break;
}
case b32_bit: NEXT(2, -0, 1) {
#if defined(EXEC)
	switch (ip->idx & 0xC0) {
		case b32_bit_and: SP(0, u32) &= (1 << (ip->idx & 0x3f)) - 1; break;
		case b32_bit_shl: SP(0, u32) <<= ip->idx & 0x3f; break;
		case b32_bit_shr: SP(0, u32) >>= ip->idx & 0x3f; break;
		case b32_bit_sar: SP(0, i32) >>= ip->idx & 0x3f; break;
		default: STOP(error_opc, 1);
	}
#endif
	break;
}
//#}
//#{ 0x3?: B32		// uint32
case b64_cmt: NEXT(1, -0, 2) {
#if defined(EXEC)
	SP(0, u64) = ~SP(0, u64);
#endif
	break;
}
case b64_and: NEXT(1, -2, 4) {
#if defined(EXEC)
	SP(2, u64) &= SP(0, u64);
#endif
	break;
}
case b64_ior: NEXT(1, -2, 4) {
#if defined(EXEC)
	SP(2, u64) |= SP(0, u64);
#endif
	break;
}
case u64_mul: NEXT(1, -2, 4) {
#if defined(EXEC)
	SP(2, u64) *= SP(0, u64);
#endif
	break;
}
case u64_div: NEXT(1, -2, 4) {
#if defined(EXEC)
	STOP(error_div, SP(0, u32) == 0);
	SP(2, u64) /= SP(0, u64);
#endif
	break;
}
case u64_mod: NEXT(1, -2, 4) {
#if defined(EXEC)
	STOP(error_div, SP(0, u32) == 0);
	SP(2, u64) %= SP(0, u64);
#endif
	break;
}
case b64_xor: NEXT(1, -2, 4) {
#if defined(EXEC)
	SP(2, u64) ^= SP(0, u64);
#endif
	break;
}
case u64_clt: NEXT(1, -3, 4) {
#if defined(EXEC)
	SP(3, u32) = SP(2, u64)  < SP(0, u64);
#endif
	break;
}
case u64_cgt: NEXT(1, -3, 4) {
#if defined(EXEC)
	SP(3, u32) = SP(2, u64)  > SP(0, u64);
#endif
	break;
}
case b64_shl: NEXT(1, -1, 3) {
#if defined(EXEC)
	SP(1, u64) <<= SP(0, u32);
#endif
	break;
}
case b64_shr: NEXT(1, -1, 3) {
#if defined(EXEC)
	SP(1, u64) >>= SP(0, u32);
#endif
	break;
}
case b64_sar: NEXT(1, -1, 3) {
#if defined(EXEC)
	SP(1, i64) >>= SP(0, i32);
#endif
	break;
}
//#}
//#{ 0x4?: I32		// int32
case i32_neg: NEXT(1, -0, 1) {
#if defined(EXEC)
	SP(0, i32) = -SP(0, i32);
#endif
	break;
}
case i32_add: NEXT(1, -1, 2) {
#if defined(EXEC)
	SP(1, i32) += SP(0, i32);
#endif
	break;
}
case i32_sub: NEXT(1, -1, 2) {
#if defined(EXEC)
	SP(1, i32) -= SP(0, i32);
#endif
	break;
}
case i32_mul: NEXT(1, -1, 2) {
#if defined(EXEC)
	SP(1, i32) *= SP(0, i32);
#endif
	break;
}
case i32_div: NEXT(1, -1, 2) {
#if defined(EXEC)
	STOP(error_div, SP(0, i32) == 0);
	SP(1, i32) /= SP(0, i32);
#endif
	break;
}
case i32_mod: NEXT(1, -1, 2) {
#if defined(EXEC)
	STOP(error_div, SP(0, i32) == 0);
	SP(1, i32) %= SP(0, i32);
#endif
	break;
}
case i32_ceq: NEXT(1, -1, 2) {
#if defined(EXEC)
	SP(1, i32) = SP(1, i32) == SP(0, i32);
#endif
	break;
}
case i32_clt: NEXT(1, -1, 2) {
#if defined(EXEC)
	SP(1, i32) = SP(1, i32)  < SP(0, i32);
#endif
	break;
}
case i32_cgt: NEXT(1, -1, 2) {
#if defined(EXEC)
	SP(1, i32) = SP(1, i32)  > SP(0, i32);
#endif
	break;
}
case i32_bol: NEXT(1, -0, 1) {
#if defined(EXEC)
	SP(0, i32) = 0 != SP(0, i32);
#endif
	break;
}
case i32_f32: NEXT(1, -0, 1) {
#if defined(EXEC)
	SP(0, f32) = (float32_t)SP(0, i32);
#endif
	break;
}
case i32_i64: NEXT(1, +1, 1) {
#if defined(EXEC)
	SP(-1, i64) = SP(0, i32);
#endif
	break;
}
case i32_f64: NEXT(1, +1, 1) {
#if defined(EXEC)
	SP(-1, f64) = SP(0, i32);
#endif
	break;
}
//#}
//#{ 0x6?: I64		// int64
case i64_neg: NEXT(1, -0, 2) {
#if defined(EXEC)
	SP(0, i64) = -SP(0, i64);
#endif
	break;
}
case i64_add: NEXT(1, -2, 4) {
#if defined(EXEC)
	SP(2, i64) += SP(0, i64);
#endif
	break;
}
case i64_sub: NEXT(1, -2, 4) {
#if defined(EXEC)
	SP(2, i64) -= SP(0, i64);
#endif
	break;
}
case i64_mul: NEXT(1, -2, 4) {
#if defined(EXEC)
	SP(2, i64) *= SP(0, i64);
#endif
	break;
}
case i64_div: NEXT(1, -2, 4) {
#if defined(EXEC)
	STOP(error_div, SP(0, i64) == 0);
	SP(2, i64) /= SP(0, i64);
#endif
	break;
}
case i64_mod: NEXT(1, -2, 4) {
#if defined(EXEC)
	STOP(error_div, SP(0, i64) == 0);
	SP(2, i64) %= SP(0, i64);
#endif
	break;
}
case i64_ceq: NEXT(1, -3, 4) {
#if defined(EXEC)
	SP(3, i32) = SP(2, i64) == SP(0, i64);
#endif
	break;
}
case i64_clt: NEXT(1, -3, 4) {
#if defined(EXEC)
	SP(3, i32) = SP(2, i64)  < SP(0, i64);
#endif
	break;
}
case i64_cgt: NEXT(1, -3, 4) {
#if defined(EXEC)
	SP(3, i32) = SP(2, i64)  > SP(0, i64);
#endif
	break;
}
case i64_i32: NEXT(1, -1, 2) {
#if defined(EXEC)
	SP(1, i32) = (int32_t)SP(0, i64);
#endif
	break;
}
case i64_f32: NEXT(1, -1, 2) {
#if defined(EXEC)
	SP(1, f32) = (float32_t)SP(0, i64);
#endif
	break;
}
case i64_bol: NEXT(1, -1, 2) {
#if defined(EXEC)
	SP(1, i32) = 0 != SP(0, i64);
#endif
	break;
}
case i64_f64: NEXT(1, -0, 2) {
#if defined(EXEC)
	SP(0, f64) = (float64_t)SP(0, i64);
#endif
	break;
}
//#}
//#{ 0x5?: F32		// float32
case f32_neg: NEXT(1, -0, 1) {
#if defined(EXEC)
	SP(0, f32) = -SP(0, f32);
#endif
	break;
}
case f32_add: NEXT(1, -1, 2) {
#if defined(EXEC)
	SP(1, f32) += SP(0, f32);
#endif
	break;
}
case f32_sub: NEXT(1, -1, 2) {
#if defined(EXEC)
	SP(1, f32) -= SP(0, f32);
#endif
	break;
}
case f32_mul: NEXT(1, -1, 2) {
#if defined(EXEC)
	SP(1, f32) *= SP(0, f32);
#endif
	break;
}
case f32_div: NEXT(1, -1, 2) {
#if defined(EXEC)
	SP(1, f32) /= SP(0, f32);
	STOP(error_div_flt, SP(0, f32) == 0.);
#endif
	break;
}
case f32_mod: NEXT(1, -1, 2) {
#if defined(EXEC)
	SP(1, f32) = fmodf(SP(1, f32), SP(0, f32));
	STOP(error_div_flt, SP(0, f32) == 0);
#endif
	break;
}
case f32_ceq: NEXT(1, -1, 2) {
#if defined(EXEC)
	SP(1, i32) = SP(1, f32) == SP(0, f32);
#endif
	break;
}
case f32_clt: NEXT(1, -1, 2) {
#if defined(EXEC)
	SP(1, i32) = SP(1, f32)  < SP(0, f32);
#endif
	break;
}
case f32_cgt: NEXT(1, -1, 2) {
#if defined(EXEC)
	SP(1, i32) = SP(1, f32)  > SP(0, f32);
#endif
	break;
}
case f32_i32: NEXT(1, -0, 1) {
#if defined(EXEC)
	SP(0, i32) = (int32_t)SP(0, f32);
#endif
	break;
}
case f32_bol: NEXT(1, -0, 1) {
#if defined(EXEC)
	SP(0, i32) = 0 != SP(0, f32);
#endif
	break;
}
case f32_i64: NEXT(1, +1, 1) {
#if defined(EXEC)
	STOP(error_ovf, ovf(pu));
	SP(-1, i64) = (int64_t)SP(0, f32);
#endif
	break;
}
case f32_f64: NEXT(1, +1, 1) {
#if defined(EXEC)
	STOP(error_ovf, ovf(pu));
	SP(-1, f64) = SP(0, f32);
#endif
	break;
}
//#}
//#{ 0x7?: F64		// float64
case f64_neg: NEXT(1, -0, 2) {
#if defined(EXEC)
	SP(0, f64) = -SP(0, f64);
#endif
	break;
}
case f64_add: NEXT(1, -2, 4) {
#if defined(EXEC)
	SP(2, f64) += SP(0, f64);
#endif
	break;
}
case f64_sub: NEXT(1, -2, 4) {
#if defined(EXEC)
	SP(2, f64) -= SP(0, f64);
#endif
	break;
}
case f64_mul: NEXT(1, -2, 4) {
#if defined(EXEC)
	SP(2, f64) *= SP(0, f64);
#endif
	break;
}
case f64_div: NEXT(1, -2, 4) {
#if defined(EXEC)
	SP(2, f64) /= SP(0, f64);
	STOP(error_div_flt, SP(0, f64) == 0.);
#endif
	break;
}
case f64_mod: NEXT(1, -2, 4) {
#if defined(EXEC)
	SP(2, f64) = fmod(SP(2, f64), SP(0, f64));
	STOP(error_div_flt, SP(0, f64) == 0);
#endif
	break;
}
case f64_ceq: NEXT(1, -3, 4) {
#if defined(EXEC)
	SP(3, i32) = SP(2, f64) == SP(0, f64);
#endif
	break;
}
case f64_clt: NEXT(1, -3, 4) {
#if defined(EXEC)
	SP(3, i32) = SP(2, f64)  < SP(0, f64);
#endif
	break;
}
case f64_cgt: NEXT(1, -3, 4) {
#if defined(EXEC)
	SP(3, i32) = SP(2, f64)  > SP(0, f64);
#endif
	break;
}
case f64_i32: NEXT(1, -1, 2) {
#if defined(EXEC)
	SP(1, i32) = (int32_t)SP(0, f64);
#endif
	break;
}
case f64_f32: NEXT(1, -1, 2) {
#if defined(EXEC)
	SP(1, f32) = (float32_t)SP(0, f64);
#endif
	break;
}
case f64_i64: NEXT(1, -0, 2) {
#if defined(EXEC)
	SP(0, i64) = (int64_t)SP(0, f64);
#endif
	break;
}
case f64_bol: NEXT(1, -1, 2) {
#if defined(EXEC)
	SP(1, i32) = 0 != SP(0, f64);
#endif
	break;
}
//#}
//#{ 0x8?: P4F		// packed 4 floats
case v4f_neg: NEXT(1, -0, 4) {
#if defined(EXEC)
	SP(0, f32) = -SP(0, f32);
	SP(1, f32) = -SP(1, f32);
	SP(2, f32) = -SP(2, f32);
	SP(3, f32) = -SP(3, f32);
#endif
	break;
}
case v4f_add: NEXT(1, -4, 8) {
#if defined(EXEC)
	SP(4, f32) += SP(0, f32);
	SP(5, f32) += SP(1, f32);
	SP(6, f32) += SP(2, f32);
	SP(7, f32) += SP(3, f32);
#endif
	break;
}
case v4f_sub: NEXT(1, -4, 8) {
#if defined(EXEC)
	SP(4, f32) -= SP(0, f32);
	SP(5, f32) -= SP(1, f32);
	SP(6, f32) -= SP(2, f32);
	SP(7, f32) -= SP(3, f32);
#endif
	break;
}
case v4f_mul: NEXT(1, -4, 8) {
#if defined(EXEC)
	SP(4, f32) *= SP(0, f32);
	SP(5, f32) *= SP(1, f32);
	SP(6, f32) *= SP(2, f32);
	SP(7, f32) *= SP(3, f32);
#endif
	break;
}
case v4f_div: NEXT(1, -4, 8) {
#if defined(EXEC)
	//~ STOP(error_div, !SP(0, f32) || !SP(1, f32) || !SP(2, f32) || !SP(3, f32));
	SP(4, f32) /= SP(0, f32);
	SP(5, f32) /= SP(1, f32);
	SP(6, f32) /= SP(2, f32);
	SP(7, f32) /= SP(3, f32);
#endif
	break;
}
case v4f_ceq: NEXT(1, -7, 8) {
#if defined(EXEC)
	SP(7, i32) = (SP(4, f32) == SP(0, f32))
			 && (SP(5, f32) == SP(1, f32))
			 && (SP(6, f32) == SP(2, f32))
			 && (SP(7, f32) == SP(3, f32));
#endif
	break;
}
case v4f_min: NEXT(1, -4, 8) {
#if defined(EXEC)
	if (SP(4, f32) > SP(0, f32))
		SP(4, f32) = SP(0, f32);
	if (SP(5, f32) > SP(1, f32))
		SP(5, f32) = SP(1, f32);
	if (SP(6, f32) > SP(2, f32))
		SP(6, f32) = SP(2, f32);
	if (SP(7, f32) > SP(3, f32))
		SP(7, f32) = SP(3, f32);
#endif
	break;
}
case v4f_max: NEXT(1, -4, 8) {
#if defined(EXEC)
	if (SP(4, f32) < SP(0, f32))
		SP(4, f32) = SP(0, f32);
	if (SP(5, f32) < SP(1, f32))
		SP(5, f32) = SP(1, f32);
	if (SP(6, f32) < SP(2, f32))
		SP(6, f32) = SP(2, f32);
	if (SP(7, f32) < SP(3, f32))
		SP(7, f32) = SP(3, f32);
#endif
	break;
}
case v4f_dp3: NEXT(1, -7, 8) {
#if defined(EXEC)
	SP(7, f32) =
	SP(4, f32) * SP(0, f32) +
	SP(5, f32) * SP(1, f32) +
	SP(6, f32) * SP(2, f32);
#endif
	break;
}
case v4f_dph: NEXT(1, -7, 8) {
#if defined(EXEC)
	SP(7, f32) +=
	SP(4, f32) * SP(0, f32) +
	SP(5, f32) * SP(1, f32) +
	SP(6, f32) * SP(2, f32);
#endif
	break;
}
case v4f_dp4: NEXT(1, -7, 8) {
#if defined(EXEC)
	SP(7, f32) =
	SP(4, f32) * SP(0, f32) +
	SP(5, f32) * SP(1, f32) +
	SP(6, f32) * SP(2, f32) +
	SP(7, f32) * SP(3, f32);
#endif
	break;
}
//#}
//#{ 0x9?: PD2		// packed 2 doubles
case v2d_neg: NEXT(1, -0, 4) {
#if defined(EXEC)
	SP(0, f64) = -SP(0, f64);
	SP(2, f64) = -SP(2, f64);
#endif
	break;
}
case v2d_add: NEXT(1, -4, 8) {
#if defined(EXEC)
	SP(4, f64) += SP(0, f64);
	SP(6, f64) += SP(2, f64);
#endif
	break;
}
case v2d_sub: NEXT(1, -4, 8) {
#if defined(EXEC)
	SP(4, f64) -= SP(0, f64);
	SP(6, f64) -= SP(2, f64);
#endif
	break;
}
case v2d_mul: NEXT(1, -4, 8) {
#if defined(EXEC)
	SP(4, f64) *= SP(0, f64);
	SP(6, f64) *= SP(2, f64);
#endif
	break;
}
case v2d_div: NEXT(1, -4, 8) {
#if defined(EXEC)
	//~ STOP(error_div, SP(0, f64) || SP(2, f64));
	SP(4, f64) /= SP(0, f64);
	SP(6, f64) /= SP(2, f64);
#endif
	break;
}
case p4x_swz: NEXT(2, -0, 4) {
#if defined(EXEC)
	uint32_t swz = ip->idx;
	uint32_t d0 = SP((swz >> 0) & 3, i32);
	uint32_t d1 = SP((swz >> 2) & 3, i32);
	uint32_t d2 = SP((swz >> 4) & 3, i32);
	uint32_t d3 = SP((swz >> 6) & 3, i32);
	SP(0, i32) = d0;
	SP(1, i32) = d1;
	SP(2, i32) = d2;
	SP(3, i32) = d3;
#endif
	break;
}
case v2d_ceq: NEXT(1, -7, 8) {
#if defined(EXEC)
	SP(7, i32) = (SP(4, f64) == SP(0, f64)) && (SP(6, f64) == SP(2, f64));
#endif
	break;
}
case v2d_min: NEXT(1, -4, 8) {
#if defined(EXEC)
	if (SP(4, f64) > SP(0, f64))
		SP(4, f64) = SP(0, f64);
	if (SP(6, f64) > SP(2, f64))
		SP(6, f64) = SP(2, f64);
#endif
	break;
}
case v2d_max: NEXT(1, -4, 8) {
#if defined(EXEC)
	if (SP(4, f64) < SP(0, f64))
		SP(4, f64) = SP(0, f64);
	if (SP(6, f64) < SP(2, f64))
		SP(6, f64) = SP(2, f64);
#endif
	break;
}
//#}
//~ 0xa?: ???		//
//~ 0xb?: ???		//
//~ 0xc?: ???		//
//~ 0xd?: ???		// ext i32+i64
//~ 0xe?: ???		// ext f32+f64
//~ 0xf?: ???		// ext p4x
default:
	STOP(error_opc, 1);
	break;
//#}-----------------------------------------------------------------------------

#undef SP
#undef MP

#undef EXEC
#undef STOP
#undef NEXT
#undef TRACE

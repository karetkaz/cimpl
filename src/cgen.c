/*******************************************************************************
 *   File: cgen.c
 *   Date: 2011/06/23
 *   Desc: code generation
 *******************************************************************************
description:
*******************************************************************************/
#include <string.h>
#include "internal.h"

// TODO: remove utility function for debugging.
static void dmpDbg(rtContext rt, astn ast, size_t offsStart, size_t offsEnd) {
#if DEBUGGING >= 3	// print generated code.
	size_t pc;
	prerr("ast", "%?s:%?u: %+t", ast->file, ast->line, ast);
	for (pc = offsStart; pc < offsEnd; ) {
		unsigned char *ip = getip(rt, pc);
		prerr("asm", "%.6x: %9A", pc, ip);
		pc += opcode_tbl[*ip].size;
	}
#else
	(void)rt;
	(void)offsStart;
	(void)offsEnd;
#endif
	(void)ast;
}

/**
 * @brief get absolute position on stack, of relative offset
 * @param rt Runtime context.
 * @param size size of variable.
 * @return the position of variable on stack.
 */
static inline size_t stkOffs(rtContext rt, size_t size) {
	dieif(size > rt->_size, "Error(expected: %d, actual: %d)", rt->_size, size);
	return padded(size, vm_size) + rt->vm.ss * vm_size;
}

// utility function swap memory
static inline void memSwap(void *_a, void *_b, size_t size) {
	register char *a = _a;
	register char *b = _b;
	register char *end = a + size;
	while (a < end) {
		char c = *a;
		*a = *b;
		*b = c;
		a += 1;
		b += 1;
	}
}

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wmany-braces-around-scalar-init"

// emit constant values.
static inline size_t emitI64(rtContext rt, int64_t value) {
	stkval arg = { .i8 = value };
	return emitarg(rt, opc_lc64, arg);
}
static inline size_t emitF64(rtContext rt, float64_t value) {
	stkval arg = { .f8 = value };
	return emitarg(rt, opc_lf64, arg);
}
static inline size_t emitRef(rtContext rt, void *value) {
	stkval arg = { .i8 = vmOffset(rt, value) };
	return emitarg(rt, opc_lref, arg);
}

// emit an offset: address or index (32 or 64 bit based on vm size)
static inline size_t emitOffs(rtContext rt, size_t value) {
	stkval arg = { .i8 = value };
	return emitarg(rt, vm_size == (4) ? opc_lc32 : opc_lc64, arg);
}

/// Emit an instruction with no argument.
static inline size_t emitOpc(rtContext rt, vmOpcode opc) {
	stkval arg = { .i8 = 0 };
	return emitarg(rt, opc, arg);
}

/// Emit an instruction with integer argument.
size_t emitInt(rtContext rt, vmOpcode opc, int64_t value) {
	stkval arg = { .i8 = value };
	return emitarg(rt, opc, arg);
}

/// Emit an instruction indexing nth element on stack.
static inline size_t emitStack(rtContext rt, vmOpcode opc, size_t arg) {
	stkval tmp = { .u8 = rt->vm.ss * vm_size - arg };

	switch (opc) {
		default:
			fatal(ERR_INTERNAL_ERROR);
			return 0;

		case opc_drop:
		case opc_ldsp:
			break;

		case opc_dup1:
		case opc_dup2:
		case opc_dup4:
		case opc_set1:
		case opc_set2:
		case opc_set4:
			tmp.u8 /= vm_size;
			break;
	}

	if (tmp.u8 > vm_regs) {
		trace(ERR_INTERNAL_ERROR);
		return 0;
	}
	if (tmp.u8 > rt->vm.ss * vm_size) {
		trace(ERR_INTERNAL_ERROR);
		return 0;
	}
	return emitarg(rt, opc, tmp);
}

#pragma clang diagnostic pop

/// Increment the variable of type int32 on top of stack.
static inline size_t emitIncrement(rtContext rt, size_t value) {
	return emitInt(rt, opc_inc, value);
}

// emit operator(add, sub, mul, ...), based on type
static size_t emitOperator(rtContext rt, ccToken token, ccKind cast) {
	vmOpcode opc = opc_last;
	switch (token) {
		case OPER_adr:
		case OPER_pls:
			opc = opc_nop;
			break;

		case OPER_not:
			switch (cast) {
				default:
					break;

				case CAST_bit:
					opc = opc_not;
					break;
			}
			break;

		case OPER_ceq:
			switch (cast) {
				default:
					break;

				case CAST_bit:
				case CAST_i32:
				case CAST_u32:
					opc = i32_ceq;
					break;

				case CAST_i64:
				case CAST_u64:
					opc = i64_ceq;
					break;

				case CAST_f32:
					opc = f32_ceq;
					break;

				case CAST_f64:
					opc = f64_ceq;
					break;
			}
			break;

		case OPER_cne:
			if (!emitOperator(rt, OPER_ceq, cast)) {
				trace(ERR_INTERNAL_ERROR);
				return 0;
			}
			opc = opc_not;
			break;

		case OPER_clt:
			switch (cast) {
				default:
					break;

				case CAST_i32:
					opc = i32_clt;
					break;

				case CAST_i64:
					opc = i64_clt;
					break;

				case CAST_u32:
					opc = u32_clt;
					break;

				case CAST_u64:
					opc = u64_clt;
					break;

				case CAST_f32:
					opc = f32_clt;
					break;

				case CAST_f64:
					opc = f64_clt;
					break;
			}
			break;

		case OPER_cle:
			if (!emitOperator(rt, OPER_cgt, cast)) {
				trace(ERR_INTERNAL_ERROR);
				return 0;
			}
			opc = opc_not;
			break;

		case OPER_cgt:
			switch (cast) {
				default:
					break;

				case CAST_i32:
					opc = i32_cgt;
					break;

				case CAST_i64:
					opc = i64_cgt;
					break;

				case CAST_u32:
					opc = u32_cgt;
					break;

				case CAST_u64:
					opc = u64_cgt;
					break;

				case CAST_f32:
					opc = f32_cgt;
					break;

				case CAST_f64:
					opc = f64_cgt;
					break;
			}
			break;

		case OPER_cge:
			if (!emitOperator(rt, OPER_clt, cast)) {
				trace(ERR_INTERNAL_ERROR);
				return 0;
			}
			opc = opc_not;
			break;

		// arithmetic
		case OPER_mns:
			switch (cast) {
				default:
					break;

				case CAST_i32:
				case CAST_u32:
					opc = i32_neg;
					break;

				case CAST_i64:
				case CAST_u64:
					opc = i64_neg;
					break;

				case CAST_f32:
					opc = f32_neg;
					break;

				case CAST_f64:
					opc = f64_neg;
					break;
			}
			break;

		case OPER_add:
			switch (cast) {
				default:
					break;

				case CAST_i32:
				case CAST_u32:
					opc = i32_add;
					break;

				case CAST_i64:
				case CAST_u64:
					opc = i64_add;
					break;

				case CAST_f32:
					opc = f32_add;
					break;

				case CAST_f64:
					opc = f64_add;
					break;
			}
			break;

		case OPER_sub:
			switch (cast) {
				default:
					break;

				case CAST_i32:
				case CAST_u32:
					opc = i32_sub;
					break;

				case CAST_i64:
				case CAST_u64:
					opc = i64_sub;
					break;

				case CAST_f32:
					opc = f32_sub;
					break;

				case CAST_f64:
					opc = f64_sub;
					break;
			}
			break;

		case OPER_mul:
			switch (cast) {
				default:
					break;

				case CAST_i32:
					opc = i32_mul;
					break;

				case CAST_i64:
					opc = i64_mul;
					break;

				case CAST_u32:
					opc = u32_mul;
					break;

				case CAST_u64:
					opc = u64_mul;
					break;

				case CAST_f32:
					opc = f32_mul;
					break;

				case CAST_f64:
					opc = f64_mul;
					break;
			}
			break;

		case OPER_div:
			switch (cast) {
				default:
					break;

				case CAST_i32:
					opc = i32_div;
					break;

				case CAST_i64:
					opc = i64_div;
					break;

				case CAST_u32:
					opc = u32_div;
					break;

				case CAST_u64:
					opc = u64_div;
					break;

				case CAST_f32:
					opc = f32_div;
					break;

				case CAST_f64:
					opc = f64_div;
					break;
			}
			break;

		case OPER_mod:
			switch (cast) {
				default:
					break;

				case CAST_i32:
					opc = i32_mod;
					break;

				case CAST_i64:
					opc = i64_mod;
					break;

				case CAST_u32:
					opc = u32_mod;
					break;

				case CAST_u64:
					opc = u64_mod;
					break;

				case CAST_f32:
					opc = f32_mod;
					break;

				case CAST_f64:
					opc = f64_mod;
					break;
			}
			break;

		// bit operations
		case OPER_cmt:
			switch (cast) {
				default:
					break;

				case CAST_i32:
				case CAST_u32:
					opc = b32_cmt;
					break;

				case CAST_i64:
				case CAST_u64:
					opc = b64_cmt;
					break;
			}
			break;

		case OPER_shl:
			switch (cast) {
				default:
					break;

				case CAST_i32:
				case CAST_u32:
					opc = b32_shl;
					break;

				case CAST_i64:
				case CAST_u64:
					opc = b64_shl;
					break;
			}
			break;

		case OPER_shr:
			switch (cast) {
				default:
					break;

				case CAST_i32:
					opc = b32_sar;
					break;

				case CAST_i64:
					opc = b64_sar;
					break;

				case CAST_u32:
					opc = b32_shr;
					break;

				case CAST_u64:
					opc = b64_shr;
					break;
			}
			break;

		case OPER_and:
			switch (cast) {
				default:
					break;

				case CAST_i32:
				case CAST_u32:
					opc = b32_and;
					break;

				case CAST_i64:
				case CAST_u64:
					opc = b64_and;
					break;
			}
			break;

		case OPER_ior:
			switch (cast) {
				default:
					break;

				case CAST_i32:
				case CAST_u32:
					opc = b32_ior;
					break;

				case CAST_i64:
				case CAST_u64:
					opc = b64_ior;
					break;
			}
			break;

		case OPER_xor:
			switch (cast) {
				default:
					break;

				case CAST_u32:
				case CAST_i32:
					opc = b32_xor;
					break;

				case CAST_i64:
				case CAST_u64:
					opc = b64_xor;
					break;
			}
			break;
	}

	return emitOpc(rt, opc);
}

/**
 * @brief emit the address of the variable.
 * @param rt Runtime context.
 * @param var Variable to be used.
 * @return Program counter.
 */
static inline size_t genOffset(rtContext rt, symn var) {
	if (!(var->kind & ATTR_stat)) {
		return emitStack(rt, opc_ldsp, var->offs);
	}
	return emitInt(rt, opc_lref, var->offs);
}

static ccKind genAst(rtContext rt, astn ast, ccKind get);

static inline ccKind genLoop(rtContext rt, astn ast) {
	ccContext cc = rt->cc;
	astn jl = cc->jmps;
	size_t lincr;
	size_t jstep, lcont, lbody, lbreak;
	size_t stbreak;

	if (ast->stmt.init && !genAst(rt, ast->stmt.init, CAST_vid)) {
		traceAst(ast);
		return TYPE_any;
	}

	if (!(jstep = emitOpc(rt, opc_jmp))) {		// continue;
		traceAst(ast);
		return TYPE_any;
	}

	lbody = emitOpc(rt, markIP);
	if (ast->stmt.stmt && !genAst(rt, ast->stmt.stmt, CAST_vid)) {
		traceAst(ast);
		return TYPE_any;
	}

	lcont = emitOpc(rt, markIP);
	if (ast->stmt.step && !genAst(rt, ast->stmt.step, CAST_vid)) {
		traceAst(ast);
		return TYPE_any;
	}

	lincr = emitOpc(rt, markIP);
	fixjump(rt, jstep, emitOpc(rt, markIP), -1);
	if (ast->stmt.test) {
		if (!genAst(rt, ast->stmt.test, CAST_bit)) {
			traceAst(ast);
			return TYPE_any;
		}
		if (!emitInt(rt, opc_jnz, lbody)) {		// continue;
			traceAst(ast);
			return TYPE_any;
		}
	}
	else {
		if (!emitInt(rt, opc_jmp, lbody)) {		// continue;
			traceAst(ast);
			return TYPE_any;
		}
	}

	lbreak = emitOpc(rt, markIP);
	stbreak = stkOffs(rt, 0);

	addDbgStatement(rt, lcont, lincr, ast->stmt.step);
	addDbgStatement(rt, lincr, lbreak, ast->stmt.test);

	while (cc->jmps != jl) {
		astn jmp = cc->jmps;
		cc->jmps = jmp->next;

		if (jmp->go2.stks != stbreak) {
			error(rt, jmp->file, jmp->line, ERR_INVALID_JUMP, jmp);
			return TYPE_any;
		}
		switch (jmp->kind) {
			default :
				fatal(ERR_INTERNAL_ERROR);
				return TYPE_any;

			case STMT_brk:
				fixjump(rt, jmp->go2.offs, lbreak, jmp->go2.stks);
				break;

			case STMT_con:
				fixjump(rt, jmp->go2.offs, lcont, jmp->go2.stks);
				break;
		}
	}
	return CAST_vid;
}
static inline ccKind genBranch(rtContext rt, astn ast) {
	size_t jmpt = 0, jmpf = 0;
	ccContext cc = rt->cc;
	struct astNode tmp;
	ccKind tt = eval(&tmp, ast->stmt.test);

	if (ast->kind == STMT_sif) {
		if (tt == TYPE_any) {
			error(rt, ast->file, ast->line, ERR_INVALID_CONST_EXPR, ast->stmt.test);
		}
		if (ast->stmt.step != NULL && ast->stmt.step->kind != STMT_beg) {
			error(rt, ast->file, ast->line, ERR_EMIT_STATEMENT, ast);
			return TYPE_any;
		}
	}

	if (tt && ast->kind == STMT_sif) {	// static if
		astn gen = bolValue(&tmp) ? ast->stmt.stmt : ast->stmt.step;
		if (gen != NULL && !genAst(rt, gen, TYPE_any)) {	// leave the stack
			traceAst(gen);
			return TYPE_any;
		}
	}
	else {
		// hack: `if (true) sin(pi/4);` leaves the result of sin on stack
		// so, code will be generated as: if (true) { sin(pi/4); }
		struct astNode block;

		memset(&block, 0, sizeof(block));
		block.type = cc->type_vid;
		block.kind = STMT_beg;

		if (tt != TYPE_any && rt->foldConst) {	// if (const)
			block.stmt.stmt = bolValue(&tmp) ? ast->stmt.stmt : ast->stmt.step;
			if (block.stmt.stmt != NULL) {
				if (!genAst(rt, &block, CAST_vid)) {
					traceAst(ast);
					return TYPE_any;
				}
			}
			else {
				warn(rt, 2, ast->file, ast->line, WARN_NO_CODE_GENERATED, ast);
			}
		}
		else if (ast->stmt.stmt && ast->stmt.step) {
			if (!genAst(rt, ast->stmt.test, CAST_bit)) {
				traceAst(ast);
				return TYPE_any;
			}
			if (!(jmpt = emitOpc(rt, opc_jz))) {
				traceAst(ast);
				return TYPE_any;
			}

			block.stmt.stmt = ast->stmt.stmt;
			if (!genAst(rt, &block, CAST_vid)) {
				traceAst(ast);
				return TYPE_any;
			}
			if (!(jmpf = emitOpc(rt, opc_jmp))) {
				traceAst(ast);
				return TYPE_any;
			}
			fixjump(rt, jmpt, emitOpc(rt, markIP), -1);

			block.stmt.stmt = ast->stmt.step;
			if (!genAst(rt, &block, CAST_vid)) {
				traceAst(ast);
				return TYPE_any;
			}
			fixjump(rt, jmpf, emitOpc(rt, markIP), -1);
		}
		else if (ast->stmt.stmt) {
			if (!genAst(rt, ast->stmt.test, CAST_bit)) {
				traceAst(ast);
				return TYPE_any;
			}
			//~ if false skip THEN block
			//~ TODO: warn: never executed code
			if (!(jmpt = emitOpc(rt, opc_jz))) {
				traceAst(ast);
				return TYPE_any;
			}

			block.stmt.stmt = ast->stmt.stmt;
			if (!genAst(rt, &block, CAST_vid)) {
				traceAst(ast);
				return TYPE_any;
			}
			fixjump(rt, jmpt, emitOpc(rt, markIP), -1);
		}
		else if (ast->stmt.step) {
			if (!genAst(rt, ast->stmt.test, CAST_bit)) {
				traceAst(ast);
				return TYPE_any;
			}
			//~ if true skip ELSE block
			//~ TODO: warn: never executed code
			if (!(jmpt = emitOpc(rt, opc_jnz))) {
				traceAst(ast);
				return TYPE_any;
			}

			block.stmt.stmt = ast->stmt.step;
			if (!genAst(rt, &block, CAST_vid)) {
				traceAst(ast);
				return TYPE_any;
			}
			fixjump(rt, jmpt, emitOpc(rt, markIP), -1);
		}
	}
	return CAST_vid;
}

static inline ccKind genDeclaration(rtContext rt, symn variable) {
	ccKind varCast = castOf(variable);

	if (!isVariable(variable)) {
		debug("%?s:%?u: definition: %T", variable->file, variable->line, variable);
		return CAST_vid;
	}

	debug("%?s:%?u: %-T", variable->file, variable->line, variable);
	if (variable->init != NULL) {
		debug("%?s:%?u: initialize variable: %T := %t", variable->file, variable->line, variable, variable->init);
		size_t initSize = stkOffs(rt, variable->size);
		if (!(varCast = genAst(rt, variable->init, varCast))) {
			return TYPE_any;
		}
		if (initSize != stkOffs(rt, 0)) {
			trace(ERR_INTERNAL_ERROR);
			return TYPE_any;
		}
	}
	if (variable->offs == 0) {
		if (variable->init == NULL) {
			if (!emitInt(rt, opc_spc, padded(variable->size, vm_size))) {
				return TYPE_any;
			}
		}
		variable->offs = stkOffs(rt, 0);
		debug("%?s:%?u: local variable @%06x: %T", variable->file, variable->line, variable->offs, variable);
		if (variable->offs < variable->size) {
			//error(rt, ast->file, ast->line, "stack underflow", variable->offs, variable->size);
			return TYPE_any;
		}
	}
	else {
		// global variable or function argument
		if (variable->init != NULL) {
			if (!genOffset(rt, variable)) {
				return TYPE_any;
			}
			if (!emitInt(rt, opc_sti, variable->size)) {
				return TYPE_any;
			}
		}
	}
	return varCast;
}
static inline ccKind genVariable(rtContext rt, symn variable, ccKind get) {
	ccKind typCast = castOf(variable->type);
	ccKind varCast = castOf(variable);

	debug("%T", variable);
	// use variable
	if (variable->kind == EMIT_opc) {
		if (!emitInt(rt, (vmOpcode)variable->offs, variable->init ? intValue(variable->init) : 0)) {
			return TYPE_any;
		}
		return typCast;
	}
	if (isInline(variable)) {
		return genAst(rt, variable->init, get);
	}

	// typename is by reference
	if (variable->type == rt->cc->type_rec) {
		varCast = typCast = CAST_ref;
	}
	// array: push length of variable first.
	if (get == CAST_arr && varCast != CAST_arr) {
		if (!emitOffs(rt, variable->type->offs)) {
			return TYPE_any;
		}
		// TODO: varCast = CAST_arr;
	}

	// variant: push type of variable first.
	if (get == CAST_var && varCast != CAST_var) {
		if (!genOffset(rt, variable->type)) {
			return TYPE_any;
		}
		varCast = CAST_var;
		typCast = CAST_ref;
	}

	if (!genOffset(rt, variable)) {
		return TYPE_any;
	}

	if (get == CAST_ref && variable == rt->cc->null_ref) {
		// `int &a = null;`
		return CAST_ref;
	}

	if (get == CAST_ref && variable->type == rt->cc->type_var) {
		// convert a variant to a reference: `int &a = var;`
		// TODO: un-box variant with type checking.
		varCast = CAST_ref;
		typCast = CAST_val;
	}

	if (get == CAST_ref && variable->type == rt->cc->type_ptr) {
		// convert a pointer to a reference: `int &a = ptr;`
		varCast = CAST_ref;
		typCast = CAST_val;
	}

	// load reference indirection (variable is a reference to a value)
	if (varCast == CAST_ref && typCast != CAST_ref) {
		dieif(variable->size != vm_size, ERR_INTERNAL_ERROR);
		if (!emitInt(rt, opc_ldi, vm_size)) {
			return TYPE_any;
		}
	}

	// load the value of the variable
	if (get != CAST_ref && typCast != CAST_ref) {
		if (!emitInt(rt, opc_ldi, variable->type->size)) {
			return TYPE_any;
		}
		return typCast;
	}
	return get;
}
static inline ccKind genValue(rtContext rt, astn ast) {
	ccKind cast = castOf(ast->type);
	debug("%t", ast);

	switch (cast) {
		default:
			break;

		case CAST_i32:
		case CAST_i64:
			if (!emitI64(rt, ast->cint)) {
				traceAst(ast);
				return TYPE_any;
			}
			return CAST_i64;

		case CAST_f32:
		case CAST_f64:
			if (!emitF64(rt, ast->cflt)) {
				traceAst(ast);
				return TYPE_any;
			}
			return CAST_f64;

		case CAST_arr: // push size and push reference
			if (!emitOffs(rt, strlen(ast->ref.name))) {
				traceAst(ast);
				return TYPE_any;
			}
			// continue
		case CAST_ref: // push reference
			if (!emitRef(rt, ast->ref.name)) {
				traceAst(ast);
				return TYPE_any;
			}
			return cast;
	}

	fatal(ERR_INTERNAL_ERROR);
	return TYPE_any;
}

static inline ccKind genCall(rtContext rt, astn ast) {
	ccContext cc = rt->cc;
	astn args = ast->op.rhso;
	const symn function = linkOf(ast->op.lhso);
	const size_t stkret = stkOffs(rt, ast->type->size);
	ccKind result = castOf(ast->type);

	debug("%t", ast);
	if (function == NULL) {
		fatal(ERR_INTERNAL_ERROR);
		traceAst(ast);
		return TYPE_any;
	}

	/*/ pointer(&variable); variant(&variable);
	if (var == cc->type_ptr || var == cc->type_var) {
		symn object = linkOf(args);
		if (object != NULL) {
			int loadIndirect = 0;
			if (var == cc->type_var) {
				// push type
				if (!emitVariable(rt, object->type)) {
					traceAst(ast);
					return TYPE_any;
				}
			}
			// push ref
			if (!genOffset(rt, object)) {
				traceAst(ast);
				return TYPE_any;
			}

			switch (object->kind & MASK_cast) {
				default:
					if (args->kind != OPER_adr && (object->type->kind & MASK_cast) != CAST_ref) {
						warn(rt, 2, args->file, args->line, WARN_PASS_ARG_BY_REF, args);
					}
					break;

				case CAST_arr:    // from slice
					warn(rt, 2, args->file, args->line, WARN_DISCARD_DATA, args, ast->type);
					loadIndirect = 1;
					break;

				case CAST_var:    // from variant
					warn(rt, 2, args->file, args->line, WARN_DISCARD_DATA, args, ast->type);
					loadIndirect = 1;
					break;

				case CAST_ref:    // from reference
					loadIndirect = 1;
					break;
			}
			if (loadIndirect != 0) {
				if (!emitInt(rt, opc_ldi, vm_size)) {
					traceAst(ast);
					return TYPE_any;
				}
			}
			return CAST_var;
		}
	}*/

	//emit intrinsic
	if (function == cc->emit_opc) {
		// TODO: args are emited in reverse order
		while (args != NULL) {
			astn arg = args;
			if (arg->kind == OPER_com) {
				args = args->op.lhso;
				arg = arg->op.rhso;
			}
			else {
				args = NULL;
			}
			if (!genAst(rt, arg, TYPE_any)) {
				traceAst(arg);
				return TYPE_any;
			}
		}
		return result;
	}

	if (function->params != NULL && args != NULL) {
		symn prm = function->params->next;
		astn arg = chainArgs(args);

		// alloc space for result and arguments ?
		if (!emitInt(rt, opc_spc, function->params->offs)) {
			traceAst(ast);
			return TYPE_any;
		}

		while (prm != NULL && arg != NULL) {
			prm->init = arg;
			debug("param: %-T", prm);
			if (!isInline(prm)) {
				if (!genAst(rt, prm->tag, TYPE_any)) {
					traceAst(ast);
					return TYPE_any;
				}
			}
			prm = prm->next;
			arg = arg->next;
		}
		// more or less args than params
		if (prm != NULL || arg != NULL) {
			fatal(ERR_INTERNAL_ERROR);
			traceAst(ast);
			return TYPE_any;
		}
	}

	if (isInline(function)) {
		if (function == cc->libc_dbg) {
			dieif(!emitInt(rt, opc_lc32, ast->line), "__FILE__");
			dieif(!emitRef(rt, ast->file), "__LINE__");
		}

		// generate inline expression
		if (!genAst(rt, function->init, TYPE_any)) {
			traceAst(function->init);
			return TYPE_any;
		}

		// copy result value
		if (function->params->size > 0) {
			if (!emitStack(rt, opc_ldsp, stkret)) {
				trace(ERR_INTERNAL_ERROR);
				return TYPE_any;
			}
			if (!emitInt(rt, opc_sti, function->params->size)) {
				trace(ERR_INTERNAL_ERROR);
				//error(rt, ast->file, ast->line, "store indirect: %T: %d of `%+t`", ast->type, sizeOf(ast->type, 1), ast);
				return TYPE_any;
			}
		}

		// drop arguments
		if (!emitStack(rt, opc_drop, stkret)) {
			fatal(ERR_INTERNAL_ERROR);
			return TYPE_any;
		}
	}

	// reset parameters
	if (function->params != NULL) {
		symn prm = function->params;
		for (; prm != NULL; prm = prm->next) {
			prm->init = NULL;
		}
	}

	return result;
}
static inline ccKind genIndex(rtContext rt, astn ast, ccKind get) {
	struct astNode tmp;
	size_t esize;
	ccKind r;

	debug("%t", ast);
	if (!(r = genAst(rt, ast->op.lhso, KIND_var))) {
		traceAst(ast);
		return TYPE_any;
	}
	if (r == CAST_arr) {
		if (!emitInt(rt, opc_ldi, vm_size)) {
			traceAst(ast);
			return TYPE_any;
		}
	}

	esize = ast->type->size;	// size of array element
	if (rt->foldConst && eval(&tmp, ast->op.rhso) == CAST_i64) {
		size_t offs = esize * intValue(&tmp);
		if (!emitIncrement(rt, offs)) {
			traceAst(ast);
			return TYPE_any;
		}
	}
	else {
		if (!genAst(rt, ast->op.rhso, CAST_u32)) {
			traceAst(ast);
			return TYPE_any;
		}
		if (esize > 1) {
			if (!emitInt(rt, opc_mad, esize)) {
				traceAst(ast);
				return TYPE_any;
			}
		}
		else if (esize == 1) {
			if (!emitOpc(rt, i32_add)) {
				traceAst(ast);
				return TYPE_any;
			}
		}
		else {
			fatal(ERR_INTERNAL_ERROR": invalid element size: %d", esize);
			return TYPE_any;
		}
	}

	if (get == KIND_var)
		return KIND_var;

	// we need the value on that position (this can be a ref).
	if (!emitInt(rt, opc_ldi, esize)) {
		traceAst(ast);
		return TYPE_any;
	}
	return castOf(ast->type);
}
static inline ccKind genMember(rtContext rt, astn ast, ccKind get) {
	// TODO: this should work as indexing
	symn object = linkOf(ast->op.lhso);
	symn member = linkOf(ast->op.rhso);
	int lhsStat = isTypeExpr(ast->op.lhso);

	debug("%t", ast);
	if (member == NULL && ast->op.rhso != NULL) {
		astn call = ast->op.rhso;
		if (call->kind == OPER_fnc) {
			member = linkOf(call->op.lhso);
		}
	}

	if (!object || !member) {
		traceAst(ast);
		return TYPE_any;
	}

	if (!(member->kind & ATTR_stat) && lhsStat) {
		error(rt, ast->file, ast->line, ERR_INVALID_FIELD_ACCESS, member);
		return TYPE_any;
	}
	if (member->kind & ATTR_stat) {
		if (!lhsStat && object->kind != CAST_arr) {
			warn(rt, 5, ast->file, ast->line, WARN_STATIC_FIELD_ACCESS, member, object->type);
		}
		return genAst(rt, ast->op.rhso, get);
	}

	if (member->kind & ATTR_memb) {
		if (!genAst(rt, ast->op.lhso, KIND_var)) {
			traceAst(ast);
			return TYPE_any;
		}
		return genAst(rt, ast->op.rhso, get);
	}
	if (member->kind == KIND_def) {
		// static array length is of this type
		debug("accessing inline field %T: %t", member, ast);
		return genAst(rt, ast->op.rhso, get);
	}

	if (!genAst(rt, ast->op.lhso, KIND_var)) {
		traceAst(ast);
		return TYPE_any;
	}

	if (!emitIncrement(rt, member->offs)) {
		traceAst(ast);
		return TYPE_any;
	}

	if (castOf(member) == CAST_ref) {
		if (!emitInt(rt, opc_ldi, vm_size)) {
			traceAst(ast);
			return TYPE_any;
		}
	}

	if (get == KIND_var) {
		return KIND_var;
	}

	if (!emitInt(rt, opc_ldi, ast->type->size)) {
		traceAst(ast);
		return TYPE_any;
	}
	return castOf(ast->type);
}

static inline ccKind genLogical(rtContext rt, astn ast) {
	struct astNode tmp;
	size_t jmpt, jmpf;

	if (rt->foldConst && eval(&tmp, ast->op.test)) {
		if (!genAst(rt, bolValue(&tmp) ? ast->op.lhso : ast->op.rhso, TYPE_any)) {
			traceAst(ast);
			return TYPE_any;
		}
	}
	else {
		size_t bppos = stkOffs(rt, 0);

		if (!genAst(rt, ast->op.test, CAST_bit)) {
			traceAst(ast);
			return TYPE_any;
		}

		if (!(jmpt = emitOpc(rt, opc_jz))) {
			traceAst(ast);
			return TYPE_any;
		}

		if (!genAst(rt, ast->op.lhso, TYPE_any)) {
			traceAst(ast);
			return TYPE_any;
		}

		if (!(jmpf = emitOpc(rt, opc_jmp))) {
			traceAst(ast);
			return TYPE_any;
		}

		fixjump(rt, jmpt, emitOpc(rt, markIP), bppos);

		if (!genAst(rt, ast->op.rhso, TYPE_any)) {
			traceAst(ast);
			return TYPE_any;
		}

		fixjump(rt, jmpf, emitOpc(rt, markIP), -1);
	}
	return castOf(ast->type);
}

/**
 * @brief Generate bytecode from abstract syntax tree.
 * @param rt Runtime context.
 * @param ast Abstract syntax tree.
 * @param get Override node cast.
 * @return Should be get || cast of ast node.
 * TODO: simplify
 */
static ccKind genAst(rtContext rt, astn ast, ccKind get) {
	const size_t ipBegin = emitOpc(rt, markIP);
	const size_t spBegin = stkOffs(rt, 0);
	const ccContext cc = rt->cc;
	ccKind got, op;

	if (ast == NULL || ast->type == NULL) {
		fatal(ERR_INTERNAL_ERROR);
		return TYPE_any;
	}

	got = castOf(ast->type);

	if (got == CAST_arr && ast->type->size == vm_size) {
		// fixed size arrays are refences.
		got = CAST_ref;
	}

	if (get == TYPE_any) {
		get = got;
	}

	// generate instructions
	switch (ast->kind) {

		default:
			fatal(ERR_INTERNAL_ERROR);
			return TYPE_any;

		//#{ STATEMENTS
		case STMT_beg: {	// statement block or function body
			astn ptr;
			for (ptr = ast->stmt.stmt; ptr != NULL; ptr = ptr->next) {
				int errors = rt->errors;
				int nested = ptr->kind == STMT_beg;
				size_t ipStart2 = emitOpc(rt, markIP);
//				trace("%?s:%?u: %t", ptr->file, ptr->line, ptr);
				if (!genAst(rt, ptr, nested ? get : TYPE_any)) {
					if (errors == rt->errors) {
						// report unreported error
						error(rt, ptr->file, ptr->line, ERR_EMIT_STATEMENT, ptr);
					}
					dmpDbg(rt, ptr, ipStart2, emitOpc(rt, markIP));
					return TYPE_any;
				}
				addDbgStatement(rt, ipStart2, emitOpc(rt, markIP), ptr);
			}
			if (get == CAST_vid) {
				// clean the stack: remove local variables
				got = CAST_val;
			}
			if (get == CAST_val) {
				// {...}: can produce local variables
				got = CAST_val;
			}
			break;
		}
		case STMT_end:      // expr statement
			if (!genAst(rt, ast->stmt.stmt, CAST_vid)) {
				traceAst(ast);
				return TYPE_any;
			}
			dieif(spBegin > stkOffs(rt, 0), ERR_INTERNAL_ERROR);
			break;

		case STMT_for:
			if (!(got = genLoop(rt, ast))) {
				traceAst(ast);
				return TYPE_any;
			}
			dieif(get != CAST_vid, ERR_INTERNAL_ERROR);
			dieif(spBegin != stkOffs(rt, 0), ERR_INTERNAL_ERROR);
			break;

		case STMT_if:
		case STMT_sif:
			if (!(got = genBranch(rt, ast))) {
				traceAst(ast);
				return TYPE_any;
			}
			dieif(get != CAST_vid, ERR_INTERNAL_ERROR);
			dieif(spBegin != stkOffs(rt, 0), ERR_INTERNAL_ERROR);
			break;

		case STMT_con:
		case STMT_brk: {
			size_t offs;
			dieif(get != CAST_vid, ERR_INTERNAL_ERROR": get: %K", get);
			if (!(offs = emitOpc(rt, opc_jmp))) {
				traceAst(ast);
				return TYPE_any;
			}

			ast->go2.stks = spBegin;
			ast->go2.offs = offs;

			ast->next = cc->jmps;
			cc->jmps = ast;
		} break;
		case STMT_ret:
			//~ TODO: declared reference variables should be freed.
			if (ast->stmt.stmt && !genAst(rt, ast->stmt.stmt, CAST_vid)) {
				traceAst(ast);
				return TYPE_any;
			}
			dieif(get != CAST_vid, ERR_INTERNAL_ERROR": get: %K", get);
			if (get == CAST_vid) {
				// TODO: drop local variables.
				if (!emitStack(rt, opc_drop, -1)) {
					trace(ERR_INTERNAL_ERROR);
					return TYPE_any;
				}
			}
			if (!emitOpc(rt, opc_jmpi)) {
				traceAst(ast);
				return TYPE_any;
			}
			fixjump(rt, 0, 0, spBegin);
			break;

		//#}
		//#{ OPERATORS
		case OPER_fnc:		// '()'
			if (ast->op.lhso == NULL) {
				if (!(got = genAst(rt, ast->op.rhso, TYPE_any))) {
					traceAst(ast);
					return TYPE_any;
				}
				break;
			}

			if (!(got = genCall(rt, ast))) {
				traceAst(ast);
				return TYPE_any;
			}
			break;

		case OPER_idx:      // '[]'
			if (!(got = genIndex(rt, ast, get))) {
				traceAst(ast);
				return TYPE_any;
			}
			break;

		case OPER_dot:      // '.'
			if (!(got = genMember(rt, ast, get))) {
				traceAst(ast);
				return TYPE_any;
			}
			break;

		case OPER_not:		// '!'
		case OPER_adr:		// '&'
		case OPER_pls:		// '+'
		case OPER_mns:		// '-'
		case OPER_cmt:		// '~'
			if (!genAst(rt, ast->op.rhso, got)) {
				traceAst(ast);
				return TYPE_any;
			}
			if (!emitOperator(rt, ast->kind, got)) {
				traceAst(ast);
				return TYPE_any;
			}
			break;

		case OPER_shl:		// '>>'
		case OPER_shr:		// '<<'
		case OPER_and:		// '&'
		case OPER_ior:		// '|'
		case OPER_xor:		// '^'

		case OPER_ceq:		// '=='
		case OPER_cne:		// '!='
		case OPER_clt:		// '<'
		case OPER_cle:		// '<='
		case OPER_cgt:		// '>'
		case OPER_cge:		// '>='

		case OPER_add:		// '+'
		case OPER_sub:		// '-'
		case OPER_mul:		// '*'
		case OPER_div:		// '/'
		case OPER_mod:		// '%'
			if (!(op = genAst(rt, ast->op.lhso, TYPE_any))) {
				traceAst(ast);
				return TYPE_any;
			}
			if (!genAst(rt, ast->op.rhso, TYPE_any)) {
				traceAst(ast);
				return TYPE_any;
			}
			if (!emitOperator(rt, ast->kind, op)) {	// uint % int => u32.mod
				traceAst(ast);
				return TYPE_any;
			}

			#ifdef DEBUGGING	// extra check: validate some conditions.
			switch (ast->kind) {
				default:
					if (ast->op.lhso->type != ast->op.rhso->type) {
						fatal(ERR_INTERNAL_ERROR);
						dmpDbg(rt, ast, ipBegin, emitOpc(rt, markIP));
						return TYPE_any;
					}
					break;
				case OPER_shl:
				case OPER_shr:
					dieif(ast->type != ast->op.lhso->type, ERR_INTERNAL_ERROR);
					dieif(ast->op.rhso->type != cc->type_i32, ERR_INTERNAL_ERROR);
					break;
			}
			switch (ast->kind) {
				default:
					break;

				case OPER_ceq:
				case OPER_cne:
				case OPER_clt:
				case OPER_cle:
				case OPER_cgt:
				case OPER_cge:
					dieif(got != CAST_bit, ERR_INTERNAL_ERROR": (%t) -> %K", ast, got);
			}
			#endif
			break;

		case OPER_all:		// '&&'
		case OPER_any: {	// '||'
			ccToken operator = TOKEN_any;
			switch (ast->kind) {
				default:
					break;
				case OPER_all:
					operator = OPER_and;// TODO: short circuit
					break;
				case OPER_any:
					operator = OPER_ior;// TODO: short circuit
					break;
			}

			if (!genAst(rt, ast->op.lhso, CAST_bit)) {
				traceAst(ast);
				return TYPE_any;
			}

			if (!genAst(rt, ast->op.rhso, CAST_bit)) {
				traceAst(ast);
				return TYPE_any;
			}

			if (!emitOperator(rt, operator, CAST_u32)) {
				traceAst(ast);
				return TYPE_any;
			}

			#ifdef DEBUGGING	// extra check: validate some conditions.
			dieif(ast->op.lhso->type != cc->type_bol, ERR_INTERNAL_ERROR);
			dieif(ast->op.rhso->type != cc->type_bol, ERR_INTERNAL_ERROR);
			dieif(got != CAST_bit, ERR_INTERNAL_ERROR": (%t) -> %K", ast, got);
			#endif
			if (cc->warn > 0) {
				static int firstTimeShowOnly = 1;
				if (firstTimeShowOnly) {
					warn(rt, 1, ast->file, ast->line, WARN_SHORT_CIRCUIT, ast);
					firstTimeShowOnly = 0;
				}
			}
		} break;
		case OPER_sel:      // '?:'
			if (!genLogical(rt, ast)) {
				traceAst(ast);
				return TYPE_any;
			}
			break;

		case ASGN_set: {	// '='
			ccKind cast = castOf(ast->op.lhso->type);
			size_t size = ast->op.lhso->type->size;
			size_t codeEnd;

			dieif(size == 0, ERR_INTERNAL_ERROR);

//			debug("%?s:%?u: assignment: %t", ast->file, ast->line, ast);
			if (!genAst(rt, ast->op.rhso, cast)) {
				traceAst(ast);
				return TYPE_any;
			}

			if (get != CAST_vid) {
				// in case a = b = sum(2, 700);
				// duplicate the result
				if (!emitInt(rt, opc_ldsp, 0)) {
					traceAst(ast);
					return TYPE_any;
				}
				if (!emitInt(rt, opc_ldi, size)) {
					traceAst(ast);
					return TYPE_any;
				}
			}

			codeEnd = emitOpc(rt, markIP);
			if (!genAst(rt, ast->op.lhso, CAST_ref)) {
				traceAst(ast);
				return TYPE_any;
			}
			if (!emitInt(rt, opc_sti, size)) {
				traceAst(ast);
				return TYPE_any;
			}

			// optimize assignments.
			if (ast->op.rhso->kind >= OPER_beg && ast->op.rhso->kind <= OPER_end) {
				if (ast->op.lhso == ast->op.rhso->op.lhso && rt->fastAssign) {
					// HACK: speed up for (int i = 0; i < 10, i += 1) ...
					if (optimizeAssign(rt, ipBegin, codeEnd)) {
						debug("assignment optimized: %t", ast);
					}
				}
			}
		} break;
		//#}
		//#{ VALUES
		case TOKEN_val:
			if (get == CAST_vid) { // void(0);
				debug("no code is generated for: %t", ast);
				return CAST_vid;
			}
			if (!(got = genValue(rt, ast))) {
				traceAst(ast);
				return TYPE_any;
			}
			break;

		case TOKEN_var: {                // var, func, type, enum, alias, inline
			symn var = ast->ref.link;
			if (var->tag == ast) {
				if (!(got = genDeclaration(rt, var))) {
					traceAst(ast);
					return TYPE_any;
				}
				// TODO: type definition returns CAST_vid
				get = got;
			}
			else {
				if (!(got = genVariable(rt, var, get))) {
					error(rt, ast->file, ast->line, ERR_EMIT_VARIABLE, ast);
					return TYPE_any;
				}
			}
			break;
		}
		//#}
	}

	// generate cast
	if (get != got) {
		vmOpcode cast = opc_last;
		switch (get) {
			default:
				break;

			case CAST_vid:
				// TODO: call destructor for variables
				if (!emitStack(rt, opc_drop, spBegin)) {
					traceAst(ast);
					return TYPE_any;
				}
				cast = opc_nop;
				break;

			case CAST_bit:
				switch (got) {
					default:
						break;

					case CAST_u32:
					case CAST_i32:
						cast = i32_bol;
						break;

					case CAST_i64:
					case CAST_u64:
						cast = i64_bol;
						break;

					case CAST_f32:
						cast = f32_bol;
						break;

					case CAST_f64:
						cast = f64_bol;
						break;
				}
				break;

			case CAST_i32:
			case CAST_u32:	// TODO: not all conversions are ok
				switch (got) {
					default:
						break;

					case CAST_bit:
					case CAST_i32:
					case CAST_u32:
						cast = opc_nop;
						break;

					case CAST_i64:
					case CAST_u64:
						cast = i64_i32;
						break;

					case CAST_f32:
						cast = f32_i32;
						break;

					case CAST_f64:
						cast = f64_i32;
						break;
				}
				break;

			case CAST_i64:
			case CAST_u64:	// TODO: not all conversions are ok
				switch (got) {
					default:
						break;

					case CAST_bit:
					case CAST_i32:
						cast = i32_i64;
						break;

					case CAST_u32:
						cast = u32_i64;
						break;

					case CAST_i64:
					case CAST_u64:
						cast = opc_nop;
						break;

					case CAST_f32:
						cast = f32_i64;
						break;

					case CAST_f64:
						cast = f64_i64;
						break;
				}
				break;

			case CAST_f32:
				switch (got) {
					default:
						break;

					case CAST_bit:
					case CAST_i32:
						cast = i32_f32;
						break;

					case CAST_i64:
						cast = i64_f32;
						break;

					case CAST_f64:
						cast = f64_f32;
						break;
				}
				break;

			case CAST_f64:
				switch (got) {
					default:
						break;

					case CAST_bit:
					case CAST_i32:
						cast = i32_f64;
						break;

					case CAST_i64:
						cast = i64_f64;
						break;

					case CAST_f32:
						cast = f32_f64;
						break;
				}
				break;
		}

		if (cast != opc_nop && !emitOpc(rt, cast)) {
			error(rt, ast->file, ast->line, ERR_INVALID_CAST, got, get, ast);
			return TYPE_any;
		}

		got = get;
	}

	/* TODO: zero extend should be treated somewhere else
	if (get == CAST_u32) {

		debug("%?s:%?u: zero extend[%K->%K / %T]: %t", ast->file, ast->line, got, get, ast->type, ast);
		switch (ast->type->size) {
			default:
				trace(ERR_INTERNAL_ERROR": size: %d, cast [%K->%K]: %t", ast->type->size, got, get, ast);
				return TYPE_any;

			case 4:
				break;

			case 2:
				if (!emitInt(rt, b32_bit, b32_bit_and | 16)) {
					traceAst(ast);
					return TYPE_any;
				}
				break;

			case 1:
				if (!emitInt(rt, b32_bit, b32_bit_and | 8)) {
					traceAst(ast);
					return TYPE_any;
				}
				break;
		}
	}*/

	return got;
}

int gencode(rtContext rt, int mode) {
	ccContext cc = rt->cc;
	size_t lMeta;	// read only section emitted by the compiler
	//TODO: size_t lCode;	// read only section: function bodies
	//TODO: size_t lData;	// writeable section: static variables
	//TODO: size_t lHeap;	// heap and stacks
	size_t lMain;

	// make global variables static
	int gStatic = rt->genGlobals;
	symn params;

	if (rt->errors != 0) {
		trace("can not generate code with errors");
		return 0;
	}

	dieif(cc == NULL, ERR_INTERNAL_ERROR);

	enter(cc);
	install(cc, ".result", KIND_var, 0, cc->type_vid, NULL);
	params = leave(cc, rt->main, KIND_def, 0, NULL);

	// leave the global scope.
	rt->main = install(cc, ".main", ATTR_stat | KIND_fun, vm_size, cc->type_fun, cc->root);
	cc->scope = leave(cc, NULL, gStatic ? ATTR_stat | KIND_def : KIND_def, 0, NULL);

	dieif(cc->scope != cc->global, ERR_INTERNAL_ERROR);

	/* reorder the initialization of static variables and functions.
	 * TODO: optimize code and rename variables.
	 *
	 *	ex: g must be generated before f
	 *	int f() {
	 *		static int g = 9;
	 *		// ...
	 *	}
	 */
	if (cc->global != NULL) {
		symn ng, pg = NULL;

		for (ng = cc->global; ng; ng = ng->global) {
			symn Ng, Pg = NULL;

			if (!isFunction(ng)) {
				// skip non functions
				continue;
			}

			for (Ng = ng; Ng != NULL; Ng = Ng->global) {
				if (Ng->owner == ng) {
					break;
				}
				Pg = Ng;
			}

			//~ this must be generated before sym;
			if (Ng != NULL /*&& Pg != NULL*/) {
				debug("global `%T` must be generated before `%T`", Ng, ng);
				Pg->global = Ng->global;	// remove
				Ng->global = ng;
				if (pg) {
					pg->global = Ng;
				}
				else {
					cc->global = Ng;
					break;
				}
				ng = pg;
			}
			pg = ng;
		}
	}

	// set used memory by metadata (string constants and type information)
	lMeta = rt->_beg - rt->_mem;

	// debug info
	if (mode != 0) {
		rt->dbg = (dbgContext)(rt->_beg = paddptr(rt->_beg, rt_size));
		rt->_beg += sizeof(struct dbgContextRec);

		dieif(rt->_beg >= rt->_end, ERR_MEMORY_OVERRUN);
		memset(rt->dbg, 0, sizeof(struct dbgContextRec));

		rt->dbg->rt = rt;
		rt->dbg->abort = (dbgn)-1;
		initBuff(&rt->dbg->functions, 128, sizeof(struct dbgNode));
		initBuff(&rt->dbg->statements, 128, sizeof(struct dbgNode));
	}

	// native calls
	if (cc->libc != NULL) {
		libc lc, calls;

		calls = (libc)(rt->_beg = paddptr(rt->_beg, rt_size));
		rt->_beg += sizeof(struct libc) * (cc->libc->sym->offs + 1);
		dieif(rt->_beg >= rt->_end, ERR_MEMORY_OVERRUN);

		rt->vm.nfc = calls;
		for (lc = cc->libc; lc; lc = lc->next) {
			size_t nfcId = lc->sym->offs;
			// relocate native call offsets to be debuggable.
			lc->sym->offs = vmOffset(rt, &calls[nfcId]);
			lc->sym->size = sizeof(struct libc);
			addDbgFunction(rt, lc->sym);
			calls[nfcId] = *lc;
		}
	}

	//~ read only memory ends here.
	//~ strings, types, add(constants, functions, enums, ...)
	rt->vm.ro = rt->_beg - rt->_mem;

	// TODO: generate functions before variables
	// static variables & functions
	if (cc->global != NULL) {
		symn var;

		// we will append the list of declarations here.
		astn staticInitList = newNode(cc, STMT_beg);

		// generate static variables & functions
		for (var = cc->global; var; var = var->global) {

			// exclude non static variables
			if (!(var->kind & ATTR_stat)) {
				continue;
			}

			// exclude typename
			if (isTypename(var)) {
				continue;
			}

			// exclude aliases
			if (isInline(var)) {
				continue;
			}

			// exclude builtin null reference
			if (var == cc->null_ref) {
				continue;
			}

			// exclude main
			if (var == rt->main) {
				continue;
			}

			dieif(var->offs != 0, "Error `%T` offs: %d", var, var->offs);	// already generated ?

			if (var->kind & ATTR_cnst && var->init == NULL) {
				error(cc->rt, var->file, var->line, ERR_UNINITIALIZED_CONSTANT, var);
			}

			// uninitialized function
			if (isInvokable(var) && var->init == NULL) {
				error(cc->rt, var->file, var->line, ERR_UNIMPLEMENTED_FUNCTION, var);
			}

			if (isFunction(var)) {
				size_t seg = emitOpc(rt, markIP);

				rt->vm.sm = 0;
				fixjump(rt, 0, 0, vm_size + var->size);
				rt->vm.sm = rt->vm.ss;		// leave return address on stack

				var->offs = emitOpc(rt, markIP);

				if (!genAst(rt, var->init, CAST_vid)) {
					continue;
				}
				if (!emitOpc(rt, opc_jmpi)) {
					error(rt, var->file, var->line, ERR_EMIT_FUNCTION, var);
					continue;
				}
				while (cc->jmps) {
					fatal(ERR_INTERNAL_ERROR": invalid jump: `%t`", cc->jmps);
					cc->jmps = cc->jmps->next;
				}
				var->size = emitOpc(rt, markIP) - seg;
				var->kind |= ATTR_stat;

				addDbgFunction(rt, var);
			}
			else {
				unsigned align;
				dieif(var->size <= 0, "Error `%T` size: %d", var, var->size);	// instance of void ?
				dieif(var->offs != 0, "Error `%T` offs: %d", var, var->offs);	// already generated ?

				// align the memory of the variable. speeding up the read and write of it.
				if (var->size >= 16) {
					align = 16;
				}
				else if (var->size >= 8) {
					align = 8;
				}
				else if (var->size >= 4) {
					align = 4;
				}
				else if (var->size >= 2) {
					align = 2;
				}
				else if (var->size >= 1) {
					align = 1;
				}
				else {
					fatal(ERR_INTERNAL_ERROR);
					return 0;
				}

				rt->_beg = paddptr(rt->_beg, align);
				var->offs = vmOffset(rt, rt->_beg);
				rt->_beg += var->size;

				dieif(rt->_beg >= rt->_end, ERR_MEMORY_OVERRUN);

				// TODO: recheck double initialization fix(var->nest > 0)
				if (var->init != NULL && var->nest > 0) {
					astn init = newNode(cc, TOKEN_var);

					//~ make initialization from initializer
					init->type = var->type;
					init->file = var->file;
					init->line = var->line;
					init->ref.link = var;

					if (staticInitList->lst.head == NULL) {
						staticInitList->lst.head = init;
					}
					else {
						staticInitList->lst.tail->next = init;
					}
					staticInitList->lst.tail = init;
				}
			}
//			debug("%?s:%?u: static variable @%06x: %T", var->file, var->line, var->offs, var);
		}

		// initialize static non global variables
		if (staticInitList && staticInitList->lst.tail) {
			dieif(cc->root == NULL || cc->root->kind != STMT_beg, ERR_INTERNAL_ERROR);
			staticInitList->lst.tail->next = cc->root->lst.head;
			cc->root->lst.head = staticInitList->lst.head;
		}
	}

	lMain = emitOpc(rt, markIP);
	if (cc->root != NULL) {
		rt->vm.sm = rt->vm.ss = 0;

		dieif(cc->root->kind != STMT_beg, ERR_INTERNAL_ERROR);

		// CAST_vid clears the stack
		if (!genAst(rt, cc->root, gStatic ? CAST_vid : CAST_val)) {
			traceAst(cc->root);
			return 0;
		}

		while (cc->jmps) {
			fatal(ERR_INTERNAL_ERROR": invalid jump: `%t`", cc->jmps);
			cc->jmps = cc->jmps->next;
			return 0;
		}
	}

	// execute and invoke exit point: halt()
	rt->vm.px = emitInt(rt, opc_nfc, 0);

	// program entry point
	rt->vm.pc = lMain;
	rt->vm.ss = 0;

	// build the main initializer function.
	rt->main->size = emitOpc(rt, markIP) - lMain;
	rt->main->offs = lMain;
	rt->main->params = params;
	rt->main->fields = cc->scope;
	rt->main->pfmt = rt->main->name;

	rt->_end = rt->_mem + rt->_size;
	if (rt->dbg != NULL) {
		int i, j;
		struct arrBuffer *codeMap = &rt->dbg->statements;
		for (i = 0; i < codeMap->cnt; ++i) {
			for (j = i; j < codeMap->cnt; ++j) {
				dbgn a = getBuff(codeMap, i);
				dbgn b = getBuff(codeMap, j);
				if (a->end > b->end) {
					memSwap(a, b, sizeof(struct dbgNode));
				}
				else if (a->end == b->end) {
					if (a->start < b->start) {
						memSwap(a, b, sizeof(struct dbgNode));
					}
				}
			}
		}
		addDbgFunction(rt, rt->main);
	}

	(void)lMeta;
	return rt->errors == 0;
}

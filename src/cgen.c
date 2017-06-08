/*******************************************************************************
 *   File: cgen.c
 *   Date: 2011/06/23
 *   Desc: code generation
 *******************************************************************************
description:
*******************************************************************************/
#include <string.h>
#include "internal.h"

/**
 * @brief get absolute position on stack, of relative offset
 * @param rt Runtime context.
 * @param size size of variable.
 * @return the position of variable on stack.
 */
static inline size_t stkOffset(rtContext rt, size_t size) {
	dieif(size > rt->_size, "Error(expected: %d, actual: %d)", rt->_size, size);
	return padOffset(size, vm_size) + rt->vm.ss * vm_size;
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

static inline vmOpcode emit32or64(vmOpcode opc32, vmOpcode opc64) {
	return sizeof(vmOffs) > vm_size ? opc64 : opc32;
}

// emit constant values.
static inline size_t emitI64(rtContext rt, int64_t value) {
	vmValue arg;
	arg.i64 = value;
	return emitarg(rt, opc_lc64, arg);
}
static inline size_t emitF64(rtContext rt, float64_t value) {
	vmValue arg;
	arg.f64 = value;
	return emitarg(rt, opc_lf64, arg);
}
static inline size_t emitRef(rtContext rt, void *value) {
	vmValue arg;
	arg.i64 = vmOffset(rt, value);
	return emitarg(rt, opc_lref, arg);
}

// emit an offset: address or index (32 or 64 bit based on vm size)
static inline size_t emitOffs(rtContext rt, size_t value) {
	vmValue arg;
	arg.i64 = value;
	return emitarg(rt, emit32or64(opc_lc32, opc_lc64), arg);
}

/// Emit an instruction with no argument.
static inline size_t emitOpc(rtContext rt, vmOpcode opc) {
	vmValue arg;
	arg.i64 = 0;
	return emitarg(rt, opc, arg);
}

/// Emit an instruction with integer argument.
size_t emitInt(rtContext rt, vmOpcode opc, int64_t value) {
	vmValue arg;
	arg.i64 = value;
	return emitarg(rt, opc, arg);
}

/// Emit an instruction indexing nth element on stack.
static inline size_t emitStack(rtContext rt, vmOpcode opc, ssize_t arg) {
	vmValue tmp;
	tmp.u64 = rt->vm.ss * vm_size - arg;

	switch (opc) {
		default:
			fatal(ERR_INTERNAL_ERROR);
			return 0;

		case opc_drop:
		case opc_ldsp:
			if (tmp.u64 > rt->vm.ss * vm_size) {
				trace(ERR_INTERNAL_ERROR);
				return 0;
			}
			break;

		case opc_dup1:
		case opc_dup2:
		case opc_dup4:
		case opc_set1:
		case opc_set2:
		case opc_set4:
			tmp.u64 /= vm_size;
			if (tmp.u64 > vm_regs) {
				trace(ERR_INTERNAL_ERROR);
				return 0;
			}
			break;
	}

	return emitarg(rt, opc, tmp);
}

/// Increment the variable of type int32 on top of stack.
static inline size_t emitIncrement(rtContext rt, size_t value) {
	return emitInt(rt, opc_inc, value);
}

// emit operator(add, sub, mul, ...), based on type
static size_t emitOperator(rtContext rt, ccToken token, ccKind cast) {
	vmOpcode opc = opc_last;
	switch (token) {
		default:
			// opc = opc_last;
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

				case CAST_ref:
					opc = emit32or64(i32_ceq, i64_ceq);
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

				case CAST_bit:
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

				case CAST_bit:
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
		case OPER_pls:
			switch (cast) {
				default:
					break;

				case CAST_i32:
				case CAST_u32:
				case CAST_i64:
				case CAST_u64:
				case CAST_f32:
				case CAST_f64:
					opc = opc_nop;
					break;
			}
			break;

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

				case CAST_bit:
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

				case CAST_bit:
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

				case CAST_bit:
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
	if (!isStatic(var)) {
		return emitStack(rt, opc_ldsp, var->offs);
	}
	return emitInt(rt, opc_lref, var->offs);
}

static ccKind genAst(ccContext cc, astn ast, ccKind get);

static inline ccKind genLoop(ccContext cc, astn ast) {
	const rtContext rt = cc->rt;
	astn jl = cc->jumps;

	if (ast->stmt.init && !genAst(cc, ast->stmt.init, CAST_vid)) {
		traceAst(ast);
		return CAST_any;
	}

	size_t jStep = emitOpc(rt, opc_jmp);
	if (jStep <= 0) {		// continue;
		traceAst(ast);
		return CAST_any;
	}

	size_t lBody = emitOpc(rt, markIP);
	if (ast->stmt.stmt && !genAst(cc, ast->stmt.stmt, CAST_vid)) {
		traceAst(ast);
		return CAST_any;
	}

	size_t lCont = emitOpc(rt, markIP);
	if (ast->stmt.step && !genAst(cc, ast->stmt.step, CAST_vid)) {
		traceAst(ast);
		return CAST_any;
	}

	size_t lInc = emitOpc(rt, markIP);
	fixJump(rt, jStep, emitOpc(rt, markIP), -1);
	if (ast->stmt.test) {
		if (!genAst(cc, ast->stmt.test, CAST_bit)) {
			traceAst(ast);
			return CAST_any;
		}
		if (!emitInt(rt, opc_jnz, lBody)) {		// continue;
			traceAst(ast);
			return CAST_any;
		}
	}
	else {
		if (!emitInt(rt, opc_jmp, lBody)) {		// continue;
			traceAst(ast);
			return CAST_any;
		}
	}

	size_t lBreak = emitOpc(rt, markIP);
	size_t stBreak = stkOffset(rt, 0);
	while (cc->jumps != jl) {
		astn jmp = cc->jumps;
		cc->jumps = jmp->next;

		if (jmp->go2.stks != stBreak) {
			error(rt, jmp->file, jmp->line, ERR_INVALID_JUMP, jmp);
			return CAST_any;
		}
		switch (jmp->kind) {
			default :
				fatal(ERR_INTERNAL_ERROR);
				return CAST_any;

			case STMT_brk:
				fixJump(rt, jmp->go2.offs, lBreak, jmp->go2.stks);
				break;

			case STMT_con:
				fixJump(rt, jmp->go2.offs, lCont, jmp->go2.stks);
				break;
		}
	}
	addDbgStatement(rt, lCont, lInc, ast->stmt.step);
	addDbgStatement(rt, lInc, lBreak, ast->stmt.test);

	return CAST_vid;
}
static inline ccKind genBranch(ccContext cc, astn ast) {
	const rtContext rt = cc->rt;
	struct astNode testValue;
	astn *genOnly = NULL;
	if (eval(cc, &testValue, ast->stmt.test) != CAST_any) {
		if (bolValue(&testValue)) {
			// generate only then
			genOnly = &ast->stmt.stmt;
		}
		else {
			// generate only else
			genOnly = &ast->stmt.step;
		}
	}

	if (ast->kind == STMT_sif) {
		if (genOnly == NULL) {
			error(rt, ast->file, ast->line, ERR_INVALID_CONST_EXPR, ast->stmt.test);
			return CAST_any;
		}
		if (*genOnly && !genAst(cc, *genOnly, CAST_any)) {	// leave the stack
			traceAst(ast);
			return CAST_any;
		}
	}
	else if (genOnly && rt->foldConst) {    // if (const) ...
		astn notGen = genOnly != &ast->stmt.stmt ? ast->stmt.stmt : ast->stmt.step;
		if (*genOnly && !genAst(cc, *genOnly, CAST_vid)) {    // clear the stack
			traceAst(ast);
			return CAST_any;
		}
		if (notGen != NULL) {
			warn(cc->rt, 8, notGen->file, notGen->line, WARN_NO_CODE_GENERATED, notGen);
		}
	}
	else if (ast->stmt.stmt && ast->stmt.step) {
		if (!genAst(cc, ast->stmt.test, CAST_bit)) {
			traceAst(ast);
			return CAST_any;
		}
		size_t jTrue = emitOpc(rt, opc_jz);
		if (jTrue <= 0) {
			traceAst(ast);
			return CAST_any;
		}

		if (!genAst(cc, ast->stmt.stmt, CAST_vid)) {
			traceAst(ast);
			return CAST_any;
		}
		size_t jFalse = emitOpc(rt, opc_jmp);
		if (jFalse <= 0) {
			traceAst(ast);
			return CAST_any;
		}
		fixJump(rt, jTrue, emitOpc(rt, markIP), -1);

		if (!genAst(cc, ast->stmt.step, CAST_vid)) {
			traceAst(ast);
			return CAST_any;
		}
		fixJump(rt, jFalse, emitOpc(rt, markIP), -1);
	}
	else if (ast->stmt.stmt) {
		if (!genAst(cc, ast->stmt.test, CAST_bit)) {
			traceAst(ast);
			return CAST_any;
		}
		size_t jTrue = emitOpc(rt, opc_jz);
		if (jTrue <= 0) {
			traceAst(ast);
			return CAST_any;
		}

		if (!genAst(cc, ast->stmt.stmt, CAST_vid)) {
			traceAst(ast);
			return CAST_any;
		}
		fixJump(rt, jTrue, emitOpc(rt, markIP), -1);
	}
	else if (ast->stmt.step) {
		if (!genAst(cc, ast->stmt.test, CAST_bit)) {
			traceAst(ast);
			return CAST_any;
		}
		size_t jFalse = emitOpc(rt, opc_jnz);
		if (jFalse <= 0) {
			traceAst(ast);
			return CAST_any;
		}

		if (!genAst(cc, ast->stmt.step, CAST_vid)) {
			traceAst(ast);
			return CAST_any;
		}
		fixJump(rt, jFalse, emitOpc(rt, markIP), -1);
	}
	return CAST_vid;
}

static inline ccKind genDeclaration(ccContext cc, symn variable, ccKind get) {
	const rtContext rt = cc->rt;
	ccKind varCast = castOf(variable);
	size_t localSize = stkOffset(rt, variable->size);

	logif(varCast != get, "%?s:%?u: %T(%K->%K)", variable->file, variable->line, variable, varCast, get);
	if (!isVariable(variable)) {
		return CAST_vid;
	}

	if (variable->init != NULL) {
		dbgCgen("%?s:%?u: %.T := %t", variable->file, variable->line, variable, variable->init);
		if (varCast != genAst(cc, variable->init, varCast)) {
			return CAST_any;
		}
		if (localSize != stkOffset(rt, 0)) {
			trace(ERR_INTERNAL_ERROR);
			return CAST_any;
		}
	}
	if (variable->offs == 0) {
		if (variable->init == NULL) {
			if (!emitInt(rt, opc_spc, padOffset(variable->size, vm_size))) {
				return CAST_any;
			}
		}
		variable->offs = stkOffset(rt, 0);
		dbgCgen("%?s:%?u: %.T is local(@%06x)", variable->file, variable->line, variable, variable->offs);
		if (localSize != stkOffset(rt, 0)) {
			trace(ERR_INTERNAL_ERROR);
			return CAST_any;
		}
	}
	else if (!isInline(variable)) {
		// global variable or function argument
		if (variable->init != NULL) {
			if (!genOffset(rt, variable)) {
				return CAST_any;
			}
			if (!emitInt(rt, opc_sti, variable->size)) {
				return CAST_any;
			}
		}
		if (isStatic(variable)) {
			dbgCgen("%?s:%?u: %.T is global(@%06x)", variable->file, variable->line, variable, variable->offs);
		}
		else {
			dbgCgen("%?s:%?u: %.T is param(@%06x)", variable->file, variable->line, variable, variable->offs);
		}
	}
	return varCast;
}
static inline ccKind genVariable(ccContext cc, symn variable, ccKind get) {
	const rtContext rt = cc->rt;
	symn type = variable->type;
	ccKind typCast = castOf(type);
	ccKind varCast = castOf(variable);

	dbgCgen("%?s:%?u: %T / (%K->%K)", variable->file, variable->line, variable, variable->kind, get);

	if (variable == cc->null_ref) {
		switch (get) {
			default:
				break;

			case CAST_var:		// variant a = null;
				// push type pointer
				if (!genOffset(rt, type)) {
					return CAST_any;
				}
				goto push_null;

			case CAST_arr:		// int a[] = null;
				// push length 0
				if (!emitOffs(rt, 0)) {
					return CAST_any;
				}
				goto push_null;

			push_null:
			case CAST_ref:		// pointer a = null;
				if (!emitInt(rt, opc_lref, 0)) {	//push reference
					return CAST_any;
				}
				return get;
		}
	}

	switch (variable->kind & MASK_kind) {
		default:
			fatal(ERR_INTERNAL_ERROR);
			return CAST_any;

		case KIND_def:
			// generate inline
			return genAst(cc, variable->init, get);

		case KIND_typ:
		case KIND_fun:
			// typename is by reference, ex: pointer b = int32;
			varCast = typCast = CAST_ref;
			break;

		case KIND_var:
			if (get == CAST_ref && castOf(type) == CAST_ref) {
				// copy references and pointers
				// ex: `int &a = ptr;`
				varCast = CAST_ref;
				typCast = CAST_val;
			}
			break;
	}

	// array: push length of variable first.
	if (get == CAST_arr && typCast == CAST_arr && varCast != CAST_arr) {
		symn length = type->fields;
		if (length != NULL && isStatic(length)) {
			if (!genVariable(cc, length, castOf(length))) {
				return CAST_any;
			}
			varCast = CAST_arr;
			typCast = CAST_ref;
		}
	}

	// variant: push type of variable first.
	if (get == CAST_var && varCast != CAST_var) {
		if (!genOffset(rt, type)) {
			return CAST_any;
		}
		varCast = CAST_var;
		typCast = CAST_ref;
	}

	// generate the address of the variable
	if (!genOffset(rt, variable)) {
		return CAST_any;
	}

	// TODO: raise a warning on data loss: ex: when assigning a variant to a pointer
	// warn(rt, 2, variable->file, variable->line, WARN_DISCARD_DATA, variable->tag, type);

	// convert a variant to a reference: `int &a = var;`
	if (get == CAST_ref && type == cc->type_var) {
		// TODO: un-box variant with type checking.
		varCast = CAST_ref;
		typCast = CAST_val;
	}

	// convert a pointer to a variant: `variant a = ptr;`
	if (get == CAST_var && type == cc->type_ptr) {
		if (!emitInt(rt, opc_ldi, sizeof(vmOffs))) {
			return CAST_any;
		}
	}

	// load reference indirection (variable is a reference to a value)
	if (varCast == CAST_ref && typCast != CAST_ref) {
		logif(variable->size != sizeof(vmOffs), ERR_INTERNAL_ERROR"(%T)", variable);
		if (!emitInt(rt, opc_ldi, sizeof(vmOffs))) {
			return CAST_any;
		}
	}

	// load the value of the variable
	if (get != CAST_ref && typCast != CAST_ref) {
		if (!emitInt(rt, opc_ldi, type->size)) {
			return CAST_any;
		}
		return typCast;
	}
	return get;
}

static inline ccKind genCall(ccContext cc, astn ast, ccKind get) {
	const rtContext rt = cc->rt;
	astn args = ast->op.rhso;
	const symn function = linkOf(ast->op.lhso);

	const size_t localSize = stkOffset(rt, ast->type->size);
	ccKind result = castOf(ast->type);

	dbgCgen("%?s:%?u: %t", ast->file, ast->line, ast);
	if (function == NULL) {
		fatal(ERR_INTERNAL_ERROR);
		return CAST_any;
	}

	// emit intrinsic
	if (function == cc->emit_opc) {
		dbgCgen("%?s:%?u: emit: %t", ast->file, ast->line, ast);
		// TODO: args are emitted in reverse order
		while (args != NULL) {
			astn arg = args;
			if (arg->kind == OPER_com) {
				args = args->op.lhso;
				arg = arg->op.rhso;
			}
			else {
				args = NULL;
			}
			dbgCgen("%?s:%?u: emit.arg: %t", arg->file, arg->line, arg);
			if (!genAst(cc, arg, CAST_any)) {
				traceAst(arg);
				return CAST_any;
			}
		}
		return get;
	}

	// generate arguments (push or cache)
	if (isInvokable(function) && args != NULL) {
		symn prm = function->params->next;
		astn arg = chainArgs(args);
		//const int cacheAllArgs = 0;

		// alloc space for result and arguments
		if (!isInline(function->params)) {
			if (!emitInt(rt, opc_spc, function->params->offs)) {
				traceAst(ast);
				return CAST_any;
			}
			while (prm != NULL && arg != NULL) {
				prm->init = arg;
				dbgCgen("param: %T", prm);
				if (!genDeclaration(cc, prm, get)) {
					traceAst(ast);
					return CAST_any;
				}
				prm = prm->next;
				arg = arg->next;
			}
		}
		else {
			// if the first argument is inline
			while (prm != NULL && arg != NULL) {
				prm->init = arg;
				dbgCgen("param: %+T", prm);
				dieif(!isInline(prm), ERR_INTERNAL_ERROR);
				prm = prm->next;
				arg = arg->next;
			}
		}
		// more or less args than params is a fatal error
		if (prm != NULL || arg != NULL) {
			fatal(ERR_INTERNAL_ERROR);
			return CAST_any;
		}
	}

	if (isInline(function)) {
		if (function == cc->libc_dbg) {
			// add hidden arguments to raise: file, line
			dieif(!emitInt(rt, opc_lc32, ast->line), "__FILE__");
			dieif(!emitRef(rt, ast->file), "__LINE__");
		}

		// generate inline expression
		if (!genAst(cc, function->init, result)) {
			traceAst(function->init);
			return CAST_any;
		}

		// drop cached arguments
		if (localSize < stkOffset(rt, 0)) {
			// copy result value
			if (function->params->size > 0) {
				if (!emitStack(rt, opc_ldsp, localSize)) {
					trace(ERR_INTERNAL_ERROR);
					return CAST_any;
				}
				if (!emitInt(rt, opc_sti, function->params->size)) {
					trace(ERR_INTERNAL_ERROR);
					return CAST_any;
				}
			}

			// drop cached arguments
			if (!emitStack(rt, opc_drop, localSize)) {
				fatal(ERR_INTERNAL_ERROR);
				return CAST_any;
			}
		}
	}
	else if (isTypename(function)) {
		// void("message") || void(0)
		if (function == cc->type_vid && args != NULL && args->kind == TOKEN_val) {
			warn(rt, 8, args->file, args->line, WARN_NO_CODE_GENERATED, ast);
			return castOf(function);
		}
		// variant(data) || typename(data) || pointer(data)
		if (function == cc->type_var || function == cc->type_rec || function == cc->type_ptr) {
			symn variable = linkOf(args);
			if (variable == NULL) {
				traceAst(ast);
				return CAST_any;
			}
			if (function == cc->type_var || function == cc->type_rec) {
				// TODO: if variable is typename, extract type
				if (!genOffset(rt, variable->type)) {
					return CAST_any;
				}
			}
			if (function == cc->type_var || function == cc->type_ptr) {
				if (!genVariable(cc, variable, CAST_ref)) {
					return CAST_any;
				}
				// TODO: warn[1]: value escapes local scope
				if (!isStatic(variable) && castOf(variable->type) != CAST_ref) {
					warn(rt, 6, args->file, args->line, WARN_PASS_ARG_BY_REF, args);
				}
			}
			return castOf(function);
		}

		// float64(3)
		switch (result = castOf(function)) {
			default:
			case CAST_vid:
			case CAST_val:
			case CAST_arr:
			case CAST_ref:
			case CAST_var:
				result = CAST_any;
				break;

			// cast to basic type
			case CAST_bit:
			case CAST_i32:
			case CAST_u32:
			case CAST_i64:
			case CAST_u64:
			case CAST_f32:
			case CAST_f64:
				if (!genAst(cc, args, result)) {
					traceAst(ast);
					return CAST_any;
				}
				break;
		}
	}
	else {
		// no code generated for call expression !
		error(rt, ast->file, ast->line, ERR_EMIT_EXPRESSION, ast);
		fatal(ERR_INTERNAL_ERROR);
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
static inline ccKind genIndex(ccContext cc, astn ast, ccKind get) {
	const rtContext rt = cc->rt;
	struct astNode tmp;
	ccKind r;

	dbgCgen("%?s:%?u: %t", ast->file, ast->line, ast);
	if (!(r = genAst(cc, ast->op.lhso, KIND_var))) {
		traceAst(ast);
		return CAST_any;
	}
	if (r == CAST_arr) {
		if (!emitInt(rt, opc_ldi, sizeof(vmOffs))) {
			traceAst(ast);
			return CAST_any;
		}
	}

	size_t elementSize = ast->type->size;	// size of array element
	if (rt->foldConst && eval(cc, &tmp, ast->op.rhso) == CAST_i64) {
		size_t offs = elementSize * intValue(&tmp);
		if (!emitIncrement(rt, offs)) {
			traceAst(ast);
			return CAST_any;
		}
	}
	else {
		if (!genAst(cc, ast->op.rhso, CAST_u32)) {
			traceAst(ast);
			return CAST_any;
		}
		if (!emitInt(rt, opc_mad, elementSize)) {
			traceAst(ast);
			return CAST_any;
		}
		else {
			fatal(ERR_INTERNAL_ERROR": invalid element size: %d", elementSize);
			return CAST_any;
		}
	}

	if (get == KIND_var)
		return KIND_var;

	// we need the value on that position (this can be a ref).
	if (!emitInt(rt, opc_ldi, elementSize)) {
		traceAst(ast);
		return CAST_any;
	}
	return castOf(ast->type);
}
static inline ccKind genMember(ccContext cc, astn ast, ccKind get) {
	const rtContext rt = cc->rt;
	// TODO: this should work as indexing
	symn object = linkOf(ast->op.lhso);
	symn member = linkOf(ast->op.rhso);
	int lhsStat = isTypeExpr(ast->op.lhso);

	dbgCgen("%?s:%?u: %t", ast->file, ast->line, ast);
	if (member == NULL && ast->op.rhso != NULL) {
		astn call = ast->op.rhso;
		if (call->kind == OPER_fnc) {
			member = linkOf(call->op.lhso);
		}
	}

	if (!object || !member) {
		traceAst(ast);
		return CAST_any;
	}

	if (!isStatic(member) && lhsStat) {
		error(rt, ast->file, ast->line, ERR_INVALID_FIELD_ACCESS, member);
		return CAST_any;
	}
	if (isStatic(member)) {
		if (!lhsStat && object->kind != CAST_arr) {
			warn(rt, 1, ast->file, ast->line, WARN_STATIC_FIELD_ACCESS, member, object->type);
		}
		return genAst(cc, ast->op.rhso, get);
	}

	if (member->kind == KIND_def) {
		// static array length is of this type
		dbgCgen("accessing inline field %T: %t", member, ast);
		return genAst(cc, ast->op.rhso, get);
	}

	if (!genAst(cc, ast->op.lhso, CAST_ref)) {
		traceAst(ast);
		return CAST_any;
	}

	// TODO: invoke genVariable() with member
	if (!emitIncrement(rt, member->offs)) {
		traceAst(ast);
		return CAST_any;
	}

	if (castOf(member) == CAST_ref) {
		if (!emitInt(rt, opc_ldi, sizeof(vmOffs))) {
			traceAst(ast);
			return CAST_any;
		}
	}

	if (get == KIND_var) {
		return KIND_var;
	}

	if (!emitInt(rt, opc_ldi, ast->type->size)) {
		traceAst(ast);
		return CAST_any;
	}
	return castOf(ast->type);
}

static inline ccKind genLogical(ccContext cc, astn ast) {
	const rtContext rt = cc->rt;
	struct astNode tmp;

	if (rt->foldConst && eval(cc, &tmp, ast->op.test) == CAST_bit) {
		if (!genAst(cc, bolValue(&tmp) ? ast->op.lhso : ast->op.rhso, CAST_any)) {
			traceAst(ast);
			return CAST_any;
		}
	}
	else {
		size_t spBegin = stkOffset(rt, 0);

		if (!genAst(cc, ast->op.test, CAST_bit)) {
			traceAst(ast);
			return CAST_any;
		}

		size_t jmpTrue = emitOpc(rt, opc_jz);
		if (jmpTrue == 0) {
			traceAst(ast);
			return CAST_any;
		}

		if (!genAst(cc, ast->op.lhso, CAST_any)) {
			traceAst(ast);
			return CAST_any;
		}

		size_t jmpFalse = emitOpc(rt, opc_jmp);
		if (jmpFalse == 0) {
			traceAst(ast);
			return CAST_any;
		}

		fixJump(rt, jmpTrue, emitOpc(rt, markIP), spBegin);

		if (!genAst(cc, ast->op.rhso, CAST_any)) {
			traceAst(ast);
			return CAST_any;
		}

		fixJump(rt, jmpFalse, emitOpc(rt, markIP), -1);
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
static ccKind genAst(ccContext cc, astn ast, ccKind get) {
	const rtContext rt = cc->rt;
	const size_t ipBegin = emitOpc(rt, markIP);
	const size_t spBegin = stkOffset(rt, 0);
	ccKind got, op;

	if (ast == NULL || ast->type == NULL) {
		fatal(ERR_INTERNAL_ERROR);
		return CAST_any;
	}

	got = castOf(ast->type);

	if (got == CAST_arr) {
		symn len = ast->type->fields;
		if (len == NULL || isStatic(len)) {
			// pointer or fixed size array
			got = CAST_ref;
		}
	}

	if (get == CAST_any) {
		get = got;
	}

	dbgCgen("%?s:%?u: %T(%K): %t", ast->file, ast->line, ast->type, get, ast);
	// generate instructions
	switch (ast->kind) {
		default:
			fatal(ERR_INTERNAL_ERROR);
			return CAST_any;

		//#{ STATEMENTS
		case STMT_beg: {	// statement block or function body
			astn ptr;
			for (ptr = ast->stmt.stmt; ptr != NULL; ptr = ptr->next) {
				int errors = rt->errors;
				int nested = ptr->kind == STMT_beg;
				size_t ipStart2 = emitOpc(rt, markIP);
//				trace("%?s:%?u: %t", ptr->file, ptr->line, ptr);
				if (!genAst(cc, ptr, nested ? get : CAST_any)) {
					if (errors == rt->errors) {
						// report unreported error
						error(rt, ptr->file, ptr->line, ERR_EMIT_STATEMENT, ptr);
					}
					return CAST_any;
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
			if (!genAst(cc, ast->stmt.stmt, CAST_vid)) {
				traceAst(ast);
				return CAST_any;
			}
			dieif(spBegin != stkOffset(rt, 0), ERR_INTERNAL_ERROR);
			break;

		case STMT_for:
			if (!(got = genLoop(cc, ast))) {
				traceAst(ast);
				return CAST_any;
			}
			dieif(get != CAST_vid, ERR_INTERNAL_ERROR);
			dieif(spBegin != stkOffset(rt, 0), ERR_INTERNAL_ERROR);
			break;

		case STMT_if:
		case STMT_sif:
			if (!(got = genBranch(cc, ast))) {
				traceAst(ast);
				return CAST_any;
			}
			dieif(get != CAST_vid, ERR_INTERNAL_ERROR);
			dieif(spBegin != stkOffset(rt, 0), ERR_INTERNAL_ERROR);
			break;

		case STMT_con:
		case STMT_brk: {
			size_t offs;
			dieif(get != CAST_vid, ERR_INTERNAL_ERROR": get: %K", get);
			if (!(offs = emitOpc(rt, opc_jmp))) {
				traceAst(ast);
				return CAST_any;
			}

			ast->go2.stks = spBegin;
			ast->go2.offs = offs;

			ast->next = cc->jumps;
			cc->jumps = ast;
		} break;
		case STMT_ret:
			//~ TODO: declared reference variables should be freed.
			if (ast->stmt.stmt != NULL) {
				// `return 3;` should be modified to `return (result := 3);`
				dieif(ast->stmt.stmt->kind != ASGN_set, ERR_INTERNAL_ERROR);

				if (!genAst(cc, ast->stmt.stmt, CAST_vid)) {
					traceAst(ast);
					return CAST_any;
				}
			}
			dieif(get != CAST_vid, ERR_INTERNAL_ERROR": get: %K", get);
			if (get == CAST_vid) {
				// TODO: drop local variables.
				if (!emitStack(rt, opc_drop, -1)) {
					trace(ERR_INTERNAL_ERROR);
					return CAST_any;
				}
			}
			if (!emitOpc(rt, opc_jmpi)) {
				traceAst(ast);
				return CAST_any;
			}
			fixJump(rt, 0, 0, spBegin);
			break;

		//#}
		//#{ OPERATORS
		case OPER_fnc:		// '()'
			if (ast->op.lhso == NULL) {
				if (!(got = genAst(cc, ast->op.rhso, CAST_any))) {
					traceAst(ast);
					return CAST_any;
				}
				break;
			}

			if (!(got = genCall(cc, ast, get))) {
				traceAst(ast);
				return CAST_any;
			}
			break;

		case OPER_idx:      // '[]'
			if (!(got = genIndex(cc, ast, get))) {
				traceAst(ast);
				return CAST_any;
			}
			break;

		case OPER_dot:      // '.'
			if (!(got = genMember(cc, ast, get))) {
				traceAst(ast);
				return CAST_any;
			}
			break;

		case OPER_not:		// '!'
		case OPER_pls:		// '+'
		case OPER_mns:		// '-'
		case OPER_cmt:		// '~'
			if (!genAst(cc, ast->op.rhso, got)) {
				traceAst(ast);
				return CAST_any;
			}
			if (!emitOperator(rt, ast->kind, got)) {
				traceAst(ast);
				return CAST_any;
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
			if (!(op = genAst(cc, ast->op.lhso, CAST_any))) {
				traceAst(ast);
				return CAST_any;
			}
			if (!genAst(cc, ast->op.rhso, CAST_any)) {
				traceAst(ast);
				return CAST_any;
			}
			if (!emitOperator(rt, ast->kind, op)) {	// uint % int => u32.mod
				traceAst(ast);
				return CAST_any;
			}

			#ifdef DEBUGGING	// extra check: validate some conditions.
			switch (ast->kind) {
				default:
					if (ast->op.lhso->type != ast->op.rhso->type) {
						fatal(ERR_INTERNAL_ERROR);
						return CAST_any;
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

			if (!genAst(cc, ast->op.lhso, CAST_bit)) {
				traceAst(ast);
				return CAST_any;
			}

			if (!genAst(cc, ast->op.rhso, CAST_bit)) {
				traceAst(ast);
				return CAST_any;
			}

			if (!emitOperator(rt, operator, CAST_u32)) {
				traceAst(ast);
				return CAST_any;
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
			if (!genLogical(cc, ast)) {
				traceAst(ast);
				return CAST_any;
			}
			break;

		case ASGN_set: {	// '='
			ccKind cast = castOf(ast->op.lhso->type);
			size_t size = ast->op.lhso->type->size;
			size_t codeEnd;

			dieif(size == 0, ERR_INTERNAL_ERROR);

			if (!genAst(cc, ast->op.rhso, cast)) {
				traceAst(ast);
				return CAST_any;
			}

			if (get != CAST_vid) {
				// in case a = b = sum(2, 700);
				// duplicate the result
				if (!emitInt(rt, opc_ldsp, 0)) {
					traceAst(ast);
					return CAST_any;
				}
				if (!emitInt(rt, opc_ldi, size)) {
					traceAst(ast);
					return CAST_any;
				}
			}

			codeEnd = emitOpc(rt, markIP);
			if (!genAst(cc, ast->op.lhso, CAST_ref)) {
				traceAst(ast);
				return CAST_any;
			}
			if (!emitInt(rt, opc_sti, size)) {
				traceAst(ast);
				return CAST_any;
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
		case TOKEN_opc:
			if (!emitInt(rt, ast->opc.code, ast->opc.args)) {
				return CAST_any;
			}
			got = castOf(ast->type);
			break;

		case TOKEN_val:
			switch (got) {
				default:
					fatal(ERR_INTERNAL_ERROR);
					return CAST_any;

				case CAST_bit:
				case CAST_i32:
				case CAST_u32:
				case CAST_i64:
				case CAST_u64:
					if (!emitI64(rt, ast->cInt)) {
						traceAst(ast);
						return CAST_any;
					}
					got = CAST_i64;
					break;

				case CAST_f32:
				case CAST_f64:
					if (!emitF64(rt, ast->cFlt)) {
						traceAst(ast);
						return CAST_any;
					}
					got = CAST_f64;
					break;

				case CAST_ref: // push reference
					// TODO: allow string constant to be converted to slice and/or variant (see: genVariable)
					if (get == CAST_arr && !emitOffs(rt, strlen(ast->ref.name))) {
						traceAst(ast);
						return CAST_any;
					}
					if (!emitRef(rt, ast->ref.name)) {
						traceAst(ast);
						return CAST_any;
					}
					if (get == CAST_arr) {
						got = CAST_arr;
					}
					else {
						got = CAST_ref;
					}
					break;
			}
			break;

		case TOKEN_var: {                // var, func, type, enum, alias, inline
			symn var = ast->ref.link;
			if (var->tag == ast) {
				if (!(got = genDeclaration(cc, var, get))) {
					traceAst(ast);
					return CAST_any;
				}
				// TODO: type definition returns CAST_vid
				get = got;
			}
			else {
				if (!(got = genVariable(cc, var, get))) {
					traceAst(ast);
					return CAST_any;
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
					return CAST_any;
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
			error(rt, ast->file, ast->line, ERR_CAST_EXPRESSION, ast, got, get);
			return CAST_any;
		}

		got = get;
	}

	/* TODO: zero extend should be treated somewhere else
	if (get == CAST_u32) {

		dbgCgen("%?s:%?u: zero extend[%K->%K / %T]: %t", ast->file, ast->line, got, get, ast->type, ast);
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

int gencode(rtContext rt, int debug) {
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
	rt->main = install(cc, ".main", ATTR_stat | KIND_fun, cc->type_fun->size, cc->type_fun, cc->root);
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
	if (debug != 0) {
		rt->dbg = (dbgContext)(rt->_beg = padPointer(rt->_beg, pad_size));
		rt->_beg += sizeof(struct dbgContextRec);

		dieif(rt->_beg >= rt->_end, ERR_MEMORY_OVERRUN);
		memset(rt->dbg, 0, sizeof(struct dbgContextRec));

		rt->dbg->rt = rt;
		rt->dbg->abort = (dbgn)-1;
		initBuff(&rt->dbg->functions, 128, sizeof(struct dbgNode));
		initBuff(&rt->dbg->statements, 128, sizeof(struct dbgNode));
	}

	// native calls
	if (cc->native != NULL) {
		list lst = cc->native;
		libc last = (libc) lst->data;
		libc *calls = (libc*)(rt->_beg = padPointer(rt->_beg, pad_size));

		rt->_beg += (last->offs + 1) * sizeof(libc);
		if(rt->_beg >= rt->_end) {
			fatal(ERR_MEMORY_OVERRUN);
			return 0;
		}

		rt->vm.nfc = calls;
		for (; lst != NULL; lst = lst->next) {
			libc nfc = (libc) lst->data;
			calls[nfc->offs] = nfc;

			// relocate native call offsets to be debuggable and traceable.
			nfc->sym->offs = vmOffset(rt, nfc);
			nfc->sym->size = 0;
			addDbgFunction(rt, nfc->sym);
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
			if (!isStatic(var)) {
				continue;
			}

			// exclude aliases
			if (isInline(var)) {
				continue;
			}

			// exclude typename
			if (isTypename(var)) {
				continue;
			}

			// exclude main
			if (var == rt->main) {
				continue;
			}

			dieif(var->offs != 0, "Error `%T` offs: %d", var, var->offs);	// already generated ?

			// uninitialized constant
			if (isConst(var) && var->init == NULL) {
				error(cc->rt, var->file, var->line, ERR_UNINITIALIZED_CONSTANT, var);
			}

			// unimplemented function
			if (isInvokable(var) && var->init == NULL) {
				error(cc->rt, var->file, var->line, ERR_UNIMPLEMENTED_FUNCTION, var);
			}

			if (isFunction(var)) {

				rt->vm.sm = 0;
				fixJump(rt, 0, 0, sizeof(vmOffs) + var->size);
				rt->vm.sm = rt->vm.ss;		// leave return address on stack

				var->offs = emitOpc(rt, markIP);

				if (!genAst(cc, var->init, CAST_vid)) {
					error(rt, var->file, var->line, ERR_EMIT_FUNCTION, var);
					continue;
				}
				if (!emitOpc(rt, opc_jmpi)) {
					error(rt, var->file, var->line, ERR_EMIT_FUNCTION, var);
					continue;
				}
				while (cc->jumps) {
					fatal(ERR_INTERNAL_ERROR": invalid jump: `%t`", cc->jumps);
					cc->jumps = cc->jumps->next;
				}
				var->size = emitOpc(rt, markIP) - var->offs;
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

				rt->_beg = padPointer(rt->_beg, align);
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
		if (!genAst(cc, cc->root, gStatic ? CAST_vid : CAST_val)) {
			traceAst(cc->root);
			return 0;
		}

		while (cc->jumps) {
			fatal(ERR_INTERNAL_ERROR": invalid jump: `%t`", cc->jumps);
			cc->jumps = cc->jumps->next;
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
	rt->main->format = rt->main->name;

	rt->_end = rt->_mem + rt->_size;
	if (rt->dbg != NULL) {
		// TODO: remove bubble sort, replace with qsort
		struct arrBuffer *codeMap = &rt->dbg->statements;
		for (int i = 0; i < codeMap->cnt; ++i) {
			for (int j = i; j < codeMap->cnt; ++j) {
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

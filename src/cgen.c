/*******************************************************************************
 *   File: cgen.c
 *   Date: 2011/06/23
 *   Desc: code generation
 *******************************************************************************
 * convert ast to bytecode
 */

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

// emit an offset: address or index (32 or 64 bit based on vm size)
static inline size_t emitOffs(rtContext rt, size_t value) {
	vmValue arg;
	arg.i64 = value;
	return emitOpc(rt, emit32or64(opc_lc32, opc_lc64), arg);
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

	return emitOpc(rt, opc, tmp);
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

	return emit(rt, opc);
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

/**
 * Generate bytecode from abstract syntax tree.
 * 
 * @param rt Runtime context.
 * @param ast Abstract syntax tree.
 * @param get Override node cast.
 * @return Should be get || cast of ast node.
 * TODO: simplify
 */
static ccKind genAst(ccContext cc, astn ast, ccKind get);

static inline ccKind genLoop(ccContext cc, astn ast) {
	rtContext rt = cc->rt;
	astn jl = cc->jumps;

	if (ast->stmt.init && !genAst(cc, ast->stmt.init, CAST_vid)) {
		traceAst(ast);
		return CAST_any;
	}

	size_t jStep = emit(rt, opc_jmp);
	if (jStep <= 0) {		// continue;
		traceAst(ast);
		return CAST_any;
	}

	size_t lBody = emit(rt, markIP);
	if (ast->stmt.stmt && !genAst(cc, ast->stmt.stmt, CAST_vid)) {
		traceAst(ast);
		return CAST_any;
	}

	size_t lCont = emit(rt, markIP);
	if (ast->stmt.step && !genAst(cc, ast->stmt.step, CAST_vid)) {
		traceAst(ast);
		return CAST_any;
	}

	size_t lInc = emit(rt, markIP);
	fixJump(rt, jStep, emit(rt, markIP), -1);
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

	size_t lBreak = emit(rt, markIP);
	size_t stBreak = stkOffset(rt, 0);
	while (cc->jumps != jl) {
		astn jmp = cc->jumps;
		cc->jumps = jmp->next;

		if (jmp->jmp.stks != stBreak) {
			error(rt, jmp->file, jmp->line, ERR_INVALID_JUMP, jmp);
			return CAST_any;
		}
		switch (jmp->kind) {
			default :
				fatal(ERR_INTERNAL_ERROR);
				return CAST_any;

			case STMT_brk:
				fixJump(rt, jmp->jmp.offs, lBreak, jmp->jmp.stks);
				break;

			case STMT_con:
				fixJump(rt, jmp->jmp.offs, lCont, jmp->jmp.stks);
				break;
		}
	}
	addDbgStatement(rt, lCont, lInc, ast->stmt.step);
	addDbgStatement(rt, lInc, lBreak, ast->stmt.test);

	return CAST_vid;
}
static inline ccKind genBranch(ccContext cc, astn ast) {
	rtContext rt = cc->rt;
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
		size_t jTrue = emit(rt, opc_jz);
		if (jTrue <= 0) {
			traceAst(ast);
			return CAST_any;
		}

		if (!genAst(cc, ast->stmt.stmt, CAST_vid)) {
			traceAst(ast);
			return CAST_any;
		}
		size_t jFalse = emit(rt, opc_jmp);
		if (jFalse <= 0) {
			traceAst(ast);
			return CAST_any;
		}
		fixJump(rt, jTrue, emit(rt, markIP), -1);

		if (!genAst(cc, ast->stmt.step, CAST_vid)) {
			traceAst(ast);
			return CAST_any;
		}
		fixJump(rt, jFalse, emit(rt, markIP), -1);
	}
	else if (ast->stmt.stmt) {
		if (!genAst(cc, ast->stmt.test, CAST_bit)) {
			traceAst(ast);
			return CAST_any;
		}
		size_t jTrue = emit(rt, opc_jz);
		if (jTrue <= 0) {
			traceAst(ast);
			return CAST_any;
		}

		if (!genAst(cc, ast->stmt.stmt, CAST_vid)) {
			traceAst(ast);
			return CAST_any;
		}
		fixJump(rt, jTrue, emit(rt, markIP), -1);
	}
	else if (ast->stmt.step) {
		if (!genAst(cc, ast->stmt.test, CAST_bit)) {
			traceAst(ast);
			return CAST_any;
		}
		size_t jFalse = emit(rt, opc_jnz);
		if (jFalse <= 0) {
			traceAst(ast);
			return CAST_any;
		}

		if (!genAst(cc, ast->stmt.step, CAST_vid)) {
			traceAst(ast);
			return CAST_any;
		}
		fixJump(rt, jFalse, emit(rt, markIP), -1);
	}
	return CAST_vid;
}

static inline ccKind genDeclaration(ccContext cc, symn variable, ccKind get) {
	rtContext rt = cc->rt;
	astn varInit = variable->init;
	ccKind varCast = castOf(variable);
	size_t varOffset = stkOffset(rt, variable->size);

	if (!isVariable(variable)) {
		// function, struct, alias or enum declaration.
		fatal(ERR_INTERNAL_ERROR);
		return CAST_vid;
	}
	logif(varCast != get && get != CAST_vid, "%?s:%?u: %T(%K->%K)", variable->file, variable->line, variable, varCast, get);

	if (varInit == NULL && variable->type->init != NULL) {
		varInit = variable->type->init;
		warn(cc->rt, 6, variable->file, variable->line, WARN_USING_DEFAULT_INITIALIZER, variable, varInit);
	}

	if (varInit != NULL) {
		dbgCgen("%?s:%?u: %.T := %t", variable->file, variable->line, variable, varInit);
		if (varCast == CAST_val && (varInit->kind == OPER_com || varInit->kind == INIT_set)) {
			// TODO: implement code generation for object literals
			error(rt, variable->file, variable->line, ERR_EXPR_TOO_COMPLEX);
			return CAST_any;
		}
		if (varCast != genAst(cc, varInit, varCast)) {
			traceAst(varInit);
			return CAST_any;
		}
		if (varOffset != stkOffset(rt, 0)) {
			traceAst(varInit);
			return CAST_any;
		}
	}
	else if (isConst(variable)) {
		error(rt, variable->file, variable->line, ERR_UNINITIALIZED_CONSTANT, variable);
	}
	else if (isInvokable(variable)) {
		error(rt, variable->file, variable->line, ERR_UNIMPLEMENTED_FUNCTION, variable);
	}
	else {
		warn(rt, 1, variable->file, variable->line, ERR_UNINITIALIZED_VARIABLE, variable);
	}

	if (variable->offs == 0) {
		// local variable => on stack
		if (varInit == NULL) {
			if (!emitInt(rt, opc_spc, padOffset(variable->size, vm_size))) {
				return CAST_any;
			}
		}
		variable->offs = stkOffset(rt, 0);
		debug("%?s:%?u: %.T is local(@%06x)", variable->file, variable->line, variable, variable->offs);
		if (varOffset != variable->offs) {
			trace(ERR_INTERNAL_ERROR);
			return CAST_any;
		}
	}
	else if (!isInline(variable)) {
		// global variable or function argument
		if (varInit != NULL) {
			if (!genOffset(rt, variable)) {
				return CAST_any;
			}
			if (!emitInt(rt, opc_sti, variable->size)) {
				return CAST_any;
			}
		}
		if (isStatic(variable)) {
			debug("%?s:%?u: %.T is global(@%06x)", variable->file, variable->line, variable, variable->offs);
		}
		else {
			debug("%?s:%?u: %.T is param(@%06x)", variable->file, variable->line, variable, variable->offs);
		}
	}
	return varCast;
}
static inline ccKind genVariable(ccContext cc, symn variable, ccKind get, astn ast) {
	rtContext rt = cc->rt;
	symn type = variable->type;
	ccKind typCast = castOf(type);
	ccKind varCast = castOf(variable);

	dbgCgen("%?s:%?u: %T / (%K->%K)", ast->file, ast->line, variable, variable->kind, get);
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

		case KIND_typ:
		case KIND_fun:
			// typename is by reference, ex: pointer b = int32;
			varCast = typCast = CAST_ref;
			break;

		case KIND_def:
			// generate inline
			return genAst(cc, variable->init, get);

		case KIND_var:
			if (get == CAST_ref && castOf(type) == CAST_ref) {
				// copy references and pointers (ex: `int &a = ptr;`)
				varCast = CAST_ref;
				typCast = CAST_val;
			}
			break;
	}

	if (variable->offs == 0) {
		error(rt, ast->file, ast->line, ERR_EMIT_VARIABLE, variable);
	}

	// array: push length of variable first.
	if (get == CAST_arr && typCast == CAST_arr && varCast != CAST_arr) {
		symn length = type->fields;
		if (length != NULL && isStatic(length)) {
			if (!genVariable(cc, length, castOf(length), ast)) {
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

	// convert a variant to a reference (ex: `int &a = var;`)
	if (get == CAST_ref && type == cc->type_var) {
		// TODO: add runtime assertion: `variant.type == int`.
		warn(rt, 2, ast->file, ast->line, WARN_VARIANT_TO_REF, type, ast->type);
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
		logif(variable->size != sizeof(vmOffs), "%?s:%?u: "ERR_INTERNAL_ERROR"(%T)", variable->file, variable->line, variable);
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
	rtContext rt = cc->rt;
	astn args = chainArgs(ast->op.rhso);
	symn function = linkOf(ast->op.lhso, 0);

	dbgCgen("%?s:%?u: %t", ast->file, ast->line, ast);
	if (function == NULL) {
		traceAst(ast);
		return CAST_any;
	}

	ccKind result = castOf(ast->type);
	const size_t locals = stkOffset(rt, 0);
	const size_t localSize = stkOffset(rt, ast->type->size);

	// emit intrinsic
	if (function == cc->emit_opc) {
		dbgCgen("%?s:%?u: emit: %t", ast->file, ast->line, ast);
		// Reverse Polish notation: int32 a = emit(int32(a), int32(b), i32.add);
		while (args != NULL) {
			dbgCgen("%?s:%?u: emit.arg: %t", args->file, args->line, args);
			if (!genAst(cc, args, CAST_any)) {
				traceAst(args);
				return CAST_any;
			}
			// extra check to not underflow the stack
			if (stkOffset(rt, 0) < locals) {
				error(rt, ast->file, ast->line, ERR_EMIT_STATEMENT, ast);
			}

			// warn if values are passed without cast
			if (!(args->kind == OPER_fnc && isTypeExpr(args->op.lhso))) {
				// argument is not a cast, check if it is an instruction
				symn lnk = linkOf(args, 1);
				if (lnk == NULL || lnk->init == NULL || lnk->init->kind != EMIT_kwd) {
					warn(rt, 1, ast->file, ast->line, WARN_PASS_ARG_NO_CAST, args, args->type);
				}
			}
			args = args->next;
		}
		return get;
	}

	// generate arguments (push or cache)
	if (isInvokable(function) && args != NULL) {
		size_t offs = 0;
		symn prm = function->params;
		struct astNode argFileLine[2];

		logif(prm->size != ast->type->size, ERR_INTERNAL_ERROR": %T", function);
		if (preAllocateArgs) {
			size_t preAlloc = argsSize(function);
			// alloc space for result and arguments
			if (preAlloc > 0 && !emitInt(rt, opc_spc, preAlloc)) {
				traceAst(ast);
				return CAST_any;
			}
		}
		if (isInline(prm) || prm->size == 0) {
			// skip generating void or inline result
		}
		else if (prm->init != NULL) {
			offs += padOffset(prm->size, vm_size);
			// result has a default value
			size_t resOffs = stkOffset(rt, prm->size);
			if (!genAst(cc, prm->init, castOf(prm))) {
				traceAst(ast);
				return CAST_any;
			}

			if (resOffs != stkOffset(rt, 0)) {
				fatal(ERR_INTERNAL_ERROR": argument size does not math parameter size");
				traceAst(ast);
				return CAST_any;
			}

			if (resOffs - locals != prm->offs) {
				if (!emitStack(rt, opc_ldsp, locals + prm->offs)) {
					trace(ERR_INTERNAL_ERROR);
					return CAST_any;
				}
				if (!emitInt(rt, opc_sti, prm->size)) {
					trace(ERR_INTERNAL_ERROR);
					return CAST_any;
				}
			}
		}
		else {
			warn(rt, 1, ast->file, ast->line, ERR_UNINITIALIZED_VARIABLE, prm);
			if (!emitInt(rt, opc_spc, prm->size)) {
				traceAst(ast);
				return CAST_any;
			}
		}

		if (isInline(function)) {
			prm->offs = locals + offs;
		}
		prm = prm->next;	// skip from result to the first parameter
		if (function == cc->libc_dbg) {
			char *file = ast->file;
			int line = ast->line;

			// use the location of the expression statement, not the expansion
			if (cc->file != NULL && cc->file != file) {
				if (cc->line > 0 && cc->line != line) {
					file = cc->file;
					line = cc->line;
					dbgCgen("%?s:%?u: Using the location of the last expression statement", file, line);
				}
			}

			// add 2 extra computed param to the raise function(file and line)
			memset(argFileLine, 0, sizeof(argFileLine));
			argFileLine[0].kind = TOKEN_val;
			argFileLine[1].kind = TOKEN_val;
			argFileLine[0].type = prm->type;
			argFileLine[1].type = prm->next->type;
			argFileLine[0].ref.name = file;
			argFileLine[1].cInt = line;

			// chain the new arguments
			argFileLine[0].next = &argFileLine[1];
			argFileLine[1].next = args;
			args = argFileLine;
		}
		while (prm != NULL && args != NULL) {
			if (isInline(prm) || prm->size == 0) {
				// skip generating void or inline parameter
				prm->init = args;
			}
			else {
				offs += padOffset(prm->size, vm_size);

				// generate the argument value
				size_t argOffs = stkOffset(rt, prm->size);
				if (!genAst(cc, args, castOf(prm))) {
					traceAst(ast);
					return CAST_any;
				}

				if (argOffs != stkOffset(rt, 0)) {
					fatal(ERR_INTERNAL_ERROR": argument size does not match parameter size");
					traceAst(ast);
					return CAST_any;
				}

				if (argOffs - locals != prm->offs) {
					if (!emitStack(rt, opc_ldsp, locals + prm->offs)) {
						trace(ERR_INTERNAL_ERROR);
						return CAST_any;
					}
					if (!emitInt(rt, opc_sti, prm->size)) {
						trace(ERR_INTERNAL_ERROR);
						return CAST_any;
					}
				}
			}

			if (isInline(function)) {
				prm->offs = locals + offs;
			}
			prm = prm->next;
			args = args->next;
		}
		// more or less args than params is a fatal error
		if (prm != NULL || args != NULL) {
			fatal(ERR_INTERNAL_ERROR);
			return CAST_any;
		}
	}

	if (isInline(function)) {
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

		size_t offs = 0;
		// restore parameter offsets and reset init value
		for (symn prm = function->params; prm != NULL; prm = prm->next) {
			if (!isInline(prm)) {
				offs += padOffset(prm->size, vm_size);
			}
			prm->init = NULL;
			prm->offs = offs;
		}
	}
	else if (isTypename(function)) {
		// casts may have a single argument
		if (args == NULL || args->next != NULL) {
			error(rt, ast->file, ast->line, ERR_INVALID_CAST, ast);
			return CAST_any;
		}

		// variant(data) || typename(data) || pointer(data)
		if (function == cc->type_var || function == cc->type_rec || function == cc->type_ptr) {
			symn variable = linkOf(args, 1);
			// accept an identifier as parameter
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
				if (!genVariable(cc, variable, CAST_ref, ast)) {
					return CAST_any;
				}
				// TODO: warn[1]: value escapes local scope
				if (!isStatic(variable) && castOf(variable->type) != CAST_ref) {
					warn(rt, 6, args->file, args->line, WARN_LOCAL_MIGHT_ESCAPE, args);
				}
			}
			return castOf(function);
		}

		// float64(3)
		switch (result = castOf(function)) {
			default:
			case CAST_val:
				// cast a variable to the same value-type,
				// or cast the result of an emit to a value-type
				if (args->type == function || args->type == cc->emit_opc) {
					if (!genAst(cc, args, result)) {
						traceAst(ast);
						return CAST_any;
					}
					break;
				}
				// fall trough
			case CAST_arr:
			case CAST_ref:
			case CAST_var:
				traceAst(ast);
				result = CAST_any;
				break;

			// cast to basic type
			case CAST_vid:
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
		if (!genAst(cc, ast->op.lhso, CAST_ref)) {
			traceAst(ast);
			return CAST_any;
		}
		if (!emit(rt, opc_call)) {
			traceAst(ast);
			return CAST_any;
		}
		// drop arguments, except result.
		if (!emitStack(rt, opc_drop, localSize)) {
			fatal(ERR_INTERNAL_ERROR);
			traceAst(ast);
			return CAST_any;
		}
	}
	return result;
}
static inline ccKind genIndex(ccContext cc, astn ast, ccKind get) {
	rtContext rt = cc->rt;
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
	rtContext rt = cc->rt;
	// TODO: this should work as indexing
	symn object = linkOf(ast->op.lhso, 1);
	symn member = linkOf(ast->op.rhso, 1);
	int lhsStat = isTypeExpr(ast->op.lhso);

	dbgCgen("%?s:%?u: %t", ast->file, ast->line, ast);
	if (member == NULL && ast->op.rhso != NULL) {
		astn call = ast->op.rhso;
		if (call->kind == OPER_fnc) {
			member = linkOf(call->op.lhso, 1);
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
		if (!lhsStat && isVariable(object) && castOf(object->type) != CAST_arr) {
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
	rtContext rt = cc->rt;
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

		size_t jmpTrue = emit(rt, opc_jz);
		if (jmpTrue == 0) {
			traceAst(ast);
			return CAST_any;
		}

		if (!genAst(cc, ast->op.lhso, CAST_any)) {
			traceAst(ast);
			return CAST_any;
		}

		size_t jmpFalse = emit(rt, opc_jmp);
		if (jmpFalse == 0) {
			traceAst(ast);
			return CAST_any;
		}

		fixJump(rt, jmpTrue, emit(rt, markIP), spBegin);

		if (!genAst(cc, ast->op.rhso, CAST_any)) {
			traceAst(ast);
			return CAST_any;
		}

		fixJump(rt, jmpFalse, emit(rt, markIP), -1);
	}
	return castOf(ast->type);
}

static ccKind genAst(ccContext cc, astn ast, ccKind get) {
	rtContext rt = cc->rt;
	const size_t ipBegin = emit(rt, markIP);
	const size_t spBegin = stkOffset(rt, 0);

	if (ast == NULL || ast->type == NULL) {
		fatal(ERR_INTERNAL_ERROR);
		return CAST_any;
	}

	ccKind op, got = castOf(ast->type);

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
				size_t ipStart2 = emit(rt, markIP);
//				trace("%?s:%?u: %t", ptr->file, ptr->line, ptr);
				if (!genAst(cc, ptr, nested ? get : CAST_any)) {
					if (errors == rt->errors) {
						// report unreported error
						error(rt, ptr->file, ptr->line, ERR_EMIT_STATEMENT, ptr);
					}
					return CAST_any;
				}
				addDbgStatement(rt, ipStart2, emit(rt, markIP), ptr);
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
		case STMT_end:      // expression statement
			// save the position for inline calls
			cc->file = ast->file;
			cc->line = ast->line;
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
			dieif(get != CAST_vid, ERR_INTERNAL_ERROR": get: %K", get);
			size_t offs = emit(rt, opc_jmp);
			if (offs == 0) {
				traceAst(ast);
				return CAST_any;
			}

			ast->jmp.stks = spBegin;
			ast->jmp.offs = offs;

			ast->next = cc->jumps;
			cc->jumps = ast;
			break;
		}
		case STMT_ret:
			dieif(get != CAST_vid, ERR_INTERNAL_ERROR": get: %K", get);
			if (ast->jmp.value != NULL) {
				astn res = ast->jmp.value;
				// `return 3;` must be modified to `return (result := 3);`
				dieif(res->kind != ASGN_set, ERR_INTERNAL_ERROR);

				if (res->type == cc->type_vid && res->kind == ASGN_set) {
					// do not generate assignment to void(ie: `void f1() { return f2(); }`)
					res = res->op.rhso;
				}
				if (!genAst(cc, res, CAST_vid)) {
					traceAst(ast);
					return CAST_any;
				}
			}
			// TODO: destroy local reference variables.
			if (!emitStack(rt, opc_drop, sizeof(vmOffs) + argsSize(ast->jmp.func))) {
				trace(ERR_INTERNAL_ERROR);
				return CAST_any;
			}
			if (!emit(rt, opc_jmpi)) {
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
			if (!(op = genAst(cc, ast->op.lhso, CAST_any))) {    // (int == int): bool
				traceAst(ast);
				return CAST_any;
			}
			if (!genAst(cc, ast->op.rhso, CAST_any)) {
				traceAst(ast);
				return CAST_any;
			}
			if (!emitOperator(rt, ast->kind, op)) {    // uint % int => u32.mod
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
			if (cc->rt->warnLevel > 0) {
				static int firstTimeShowOnly = 1;
				if (firstTimeShowOnly) {
					warn(rt, 1, ast->file, ast->line, WARN_SHORT_CIRCUIT, ast);
					firstTimeShowOnly = 0;
				}
			}
			break;
		}
		case OPER_sel:      // '?:'
			if (!genLogical(cc, ast)) {
				traceAst(ast);
				return CAST_any;
			}
			break;

		case INIT_set:  	// '='
		case ASGN_set: {	// '='
			ccKind cast = castOf(ast->op.lhso->type);

			if (!genAst(cc, ast->op.rhso, cast)) {
				traceAst(ast);
				return CAST_any;
			}

			size_t size = ast->op.lhso->type->size;
			dieif(size == 0, ERR_INTERNAL_ERROR);

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

			size_t codeEnd = emit(rt, markIP);
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
			break;
		}
		//#}
		//#{ VALUES
		case TOKEN_opc:
			if (!emitInt(rt, ast->opc.code, ast->opc.args)) {
				return CAST_any;
			}
			got = castOf(ast->type);
			break;

		case TOKEN_val:
			if (get == CAST_vid) {
				// void("message") || void(0)
				warn(rt, 8, ast->file, ast->line, WARN_NO_CODE_GENERATED, ast);
				return CAST_vid;
			}
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
					if (!emitRef(rt, vmOffset(rt, ast->ref.name))) {
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

		case TOKEN_var: {                // var, func, type, enum, alias
			symn var = ast->ref.link;
			if (var->tag == ast) {
				if (isTypename(var) || isFunction(var) || isInline(var) || isStatic(var)) {
					dieif(!isInline(var) && !isStatic(var), ERR_INTERNAL_ERROR);
					// types, functions, static variables
					// static variables are generated 
				}
				else if (!(got = genDeclaration(cc, var, get))) {
					traceAst(ast);
					return CAST_any;
				}
				// TODO: type definition returns CAST_vid
				get = got;
			}
			else {
				if (!(got = genVariable(cc, var, get, ast))) {
					traceAst(ast);
					return CAST_any;
				}
			}
			break;
		}
		//#}
	}

	if (ast->kind >= STMT_beg && ast->kind <= STMT_end) {
		ssize_t locals = stkOffset(rt, 0) - spBegin;
		logif(get == CAST_vid && locals != 0, "%s:%u: locals left on stack(get: %K, size: %D): `%t`", ast->file, ast->line, get, locals, ast);
		if (get == CAST_vid && locals != 0) {
			if (!emitStack(rt, opc_drop, spBegin)) {
				traceAst(ast);
				return CAST_any;
			}
		}
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

		if (cast != opc_nop && !emit(rt, cast)) {
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

int ccGenCode(rtContext rt, int debug) {
	ccContext cc = rt->cc;
	//TODO: size_t lMeta;	// read only section: emitted by the compiler
	//TODO: size_t lCode;	// read only section: function bodies
	//TODO: size_t lData;	// writeable section: static variables
	size_t lMain;

	// make global variables static
	int gStatic = rt->genGlobals;
	symn params;

	if (cc == NULL || rt->errors != 0) {
		dieif(cc == NULL, ERR_INTERNAL_ERROR);
		trace("can not generate code with errors");
		return 0;
	}

	enter(cc, rt->main);
	install(cc, ".result", KIND_var, 0, cc->type_vid, NULL);
	params = leave(cc, KIND_def, 0, 0, NULL);

	// leave the global scope.
	rt->main = install(cc, ".main", ATTR_stat | KIND_fun, cc->type_fun->size, cc->type_fun, cc->root);
	cc->scope = leave(cc, gStatic ? ATTR_stat | KIND_def : KIND_def, 0, 0, NULL);

	dieif(cc->scope != cc->global, ERR_INTERNAL_ERROR);
	/* reorder the initialization of static variables and functions.
	 *	int f() {
	 *		static int g = 9;
	 *		// ...
	 *	}
	 *	// variable `g` must be generated before function `f` is generated
	 */
	if (cc->global != NULL) {
		symn prevGlobal = NULL;

		for (symn global = cc->global; global; global = global->global) {
			symn prevInner = NULL;

			for (symn inner = global; inner != NULL; inner = inner->global) {
				if (inner->owner == global && !isInline(inner)) {
					// logif(">", "global `%T` must be generated before `%T`", inner, global);
					if (prevGlobal == NULL || prevInner == NULL) {
						// the first globals are the builtin types, so we should never get here
						fatal("global `%T` cant be generated before `%T`", inner, global);
						return 0;
					}

					// remove inner from list
					prevInner->global = inner->global;

					// place it before its owner
					inner->global = prevGlobal->global;
					prevGlobal->global = inner;

					// continue with the next symbol
					global = prevGlobal;
					inner = prevInner;
				}
				prevInner = inner;
			}
			prevGlobal = global;
		}
	}

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

	vmInit(rt, NULL);

	// TODO: generate functions before variables
	// static variables & functions
	if (cc->global != NULL) {
		// generate static variables & functions
		for (symn var = cc->global; var; var = var->global) {

			if (var == rt->main || isInline(var)) {
				// exclude main and inline symbols
				continue;
			}
			if (isTypename(var)) {
				// types are "generated" when they are declared
				dieif(var->offs == 0, ERR_INTERNAL_ERROR);
				dieif(!isStatic(var), ERR_INTERNAL_ERROR);
				continue;
			}
			if (!isStatic(var)) {
				// exclude non static variables
				continue;
			}
			dieif(var->offs != 0, "Error `%T` offs: %d", var, var->offs);	// already generated ?

			// align memory. Speed up read, write and execution.
			rt->_beg = padPointer(rt->_beg, pad_size);

			if (isFunction(var)) {
				// reset the stack size
				fixJump(rt, 0, 0, sizeof(vmOffs) + argsSize(var));

				var->offs = emit(rt, markIP);
				if (!genAst(cc, var->init, CAST_vid)) {
					error(rt, var->file, var->line, ERR_EMIT_FUNCTION, var);
					continue;
				}
				size_t end = emit(rt, markIP);
				if (end == var->offs || !testOcp(rt, rt->vm.pc, opc_jmpi, NULL)) {
					if (!emit(rt, opc_jmpi)) {
						error(rt, var->file, var->line, ERR_EMIT_FUNCTION, var);
						continue;
					}
				}
				while (cc->jumps) {
					fatal(ERR_INTERNAL_ERROR": invalid jump: `%t`", cc->jumps);
					cc->jumps = cc->jumps->next;
				}
				var->size = emit(rt, markIP) - var->offs;
				var->kind |= ATTR_stat;

				addDbgFunction(rt, var);
			}
			else if (isVariable(var)) {
				dieif(var->size <= 0, "Error `%T` size: %d", var, var->size);	// instance of void ?

				// allocate the memory for the global variable
				var->offs = vmOffset(rt, rt->_beg);
				rt->_beg += var->size;

				if (rt->_beg >= rt->_end) {
					error(rt, var->file, var->line, ERR_DECLARATION_COMPLEX, var);
					return 0;
				}
			}
			else {
				fatal(ERR_INTERNAL_ERROR);
			}
		}
	}

	lMain = emit(rt, markIP);
	if (cc->root != NULL) {
		// reset the stack size
		fixJump(rt, 0, 0, 0);

		dieif(cc->root->kind != STMT_beg, ERR_INTERNAL_ERROR);

		// generate static variable initializations
		for (symn var = cc->global; var; var = var->global) {
			if (!isVariable(var)) {
				continue;
			}
			size_t begin = emit(rt, markIP);
			if (!genDeclaration(cc, var, CAST_vid)) {
				traceAst(var->tag);
				return 0;
			}
			addDbgStatement(rt, begin, emit(rt, markIP), var->tag);
		}

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

	// program entry(main()) and exit(halt())
	rt->vm.px = emitInt(rt, opc_nfc, 0);
	rt->vm.pc = lMain;
	rt->vm.ss = 0;

	// build the main initializer function.
	rt->main->offs = lMain;
	rt->main->size = emit(rt, markIP) - lMain;
	rt->main->params = params;
	rt->main->fields = cc->scope;
	rt->main->format = rt->main->name;
	addDbgFunction(rt, rt->main);

	rt->_end = rt->_mem + rt->_size;
	if (rt->dbg != NULL) {
		// TODO: replace bubble sort with qsort
		struct arrBuffer *codeMap = &rt->dbg->statements;
		for (size_t i = 0; i < codeMap->cnt; ++i) {
			for (size_t j = i; j < codeMap->cnt; ++j) {
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
	}

	return rt->errors == 0;
}

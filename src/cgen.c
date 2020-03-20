/*******************************************************************************
 *   File: cgen.c
 *   Date: 2011/06/23
 *   Desc: code generation
 *******************************************************************************
 * convert ast to bytecode
 */

#include "internal.h"

// forward declarations
static ccKind genAst(ccContext cc, astn ast, ccKind get);

/// Utility function to use the correct opcode in 32/64 bit
static inline vmOpcode vmSelect(vmOpcode opc32, vmOpcode opc64) {
	return vm_ref_size == 8 ? opc64 : opc32;
}

/// Utility function to get the absolute position on stack, of a relative offset
static inline size_t stkOffset(rtContext rt, size_t size) {
	logif(size > rt->_size, "Error(expected: %d, actual: %d)", rt->_size, size);
	return padOffset(size, vm_stk_align) + rt->vm.ss * vm_stk_align;
}

/// Emit an offset: address or index (32 or 64 bit based on vm size)
static inline size_t emitOffs(rtContext rt, size_t value) {
	vmValue arg;
	arg.i64 = value;
	return emitOpc(rt, vmSelect(opc_lc32, opc_lc64), arg);
}

/// Emit an instruction indexing nth element on stack.
static inline size_t emitStack(rtContext rt, vmOpcode opc, ssize_t arg) {
	vmValue tmp;
	tmp.u64 = rt->vm.ss * vm_stk_align - arg;

	switch (opc) {
		default:
			fatal(ERR_INTERNAL_ERROR);
			return 0;

		case opc_drop:
		case opc_ldsp:
			if (tmp.u64 > rt->vm.ss * vm_stk_align) {
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
			tmp.u64 /= vm_stk_align;
			if (tmp.u64 > vm_regs) {
				trace(ERR_INTERNAL_ERROR);
				return 0;
			}
			break;
	}

	return emitOpc(rt, opc, tmp);
}

/// Emit the address of the variable.
static inline size_t emitVarOffs(rtContext rt, symn var) {
	if (!isStatic(var)) {
		return emitStack(rt, opc_ldsp, var->offs);
	}
	return emitInt(rt, opc_lref, var->offs);
}

/// Generate byte-code from STMT_for.
static ccKind genLoop(ccContext cc, astn ast) {
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
/// Generate byte-code from STMT_if.
static ccKind genBranch(ccContext cc, astn ast) {
	rtContext rt = cc->rt;
	struct astNode testValue;
	if (ast->stmt.init && !genAst(cc, ast->stmt.init, CAST_vid)) {
		traceAst(ast);
		return CAST_any;
	}
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
		if (*genOnly && !genAst(cc, *genOnly, CAST_any)) {    // leave the stack
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
			warn(cc->rt, raise_warn_gen8, notGen->file, notGen->line, WARN_NO_CODE_GENERATED, notGen);
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

/// Generate byte-code for variable declaration with initialization.
static ccKind genDeclaration(ccContext cc, symn variable, ccKind get) {
	rtContext rt = cc->rt;
	astn varInit = variable->init;
	ccKind varCast = refCast(variable);
	size_t varOffset = stkOffset(rt, variable->size);

	if (!isVariable(variable)) {
		// function, struct, alias or enum declaration.
		fatal(ERR_INTERNAL_ERROR);
		return CAST_vid;
	}
	if (variable->size == 0) {
		// function, struct, alias or enum declaration.
		error(cc->rt, variable->file, variable->line, ERR_EMIT_VARIABLE, variable);
		return CAST_vid;
	}
	if (get != CAST_vid) {
		logif(get != varCast, "%?s:%?u: %T(%K->%K)", variable->file, variable->line, variable, varCast, get);
	}

	if (varInit == NULL) {
		varInit = variable->type->init;
		if (varInit != NULL) {
			warn(cc->rt, raise_warn_typ6, variable->file, variable->line, WARN_USING_DEF_TYPE_INITIALIZER, variable, varInit);
		}
		else if (isConst(variable)) {
			error(rt, variable->file, variable->line, ERR_UNINITIALIZED_CONSTANT, variable);
		}
		else if (isInvokable(variable)) {
			error(rt, variable->file, variable->line, ERR_UNIMPLEMENTED_FUNCTION, variable);
		}
		else {
			raiseLevel level = cc->errUninitialized ? raiseWarn : raiseError;
			warn(rt, level, variable->file, variable->line, ERR_UNINITIALIZED_VARIABLE, variable);
		}
	}

	if (variable->offs == 0) {
		// local variable => on stack
		if (varInit == NULL || varInit->kind == STMT_beg) {
			if (!emitInt(rt, opc_spc, padOffset(variable->size, vm_stk_align))) {
				return CAST_any;
			}
			variable->offs = stkOffset(rt, 0);
		}
		if (varInit != NULL) {
			if (!(varCast = genAst(cc, varInit, varCast))) {
				traceAst(varInit);
				return CAST_any;
			}
			variable->offs = stkOffset(rt, 0);
		}
		debug("%?s:%?u: %.T is local(@%06x)", variable->file, variable->line, variable, variable->offs);
		if (varOffset != variable->offs) {
			trace(ERR_INTERNAL_ERROR);
			return CAST_any;
		}
	}
	else if (!isInline(variable)) {
		// global variable or function argument
		if (varInit != NULL) {
			if (varCast != genAst(cc, varInit, varCast)) {
				traceAst(varInit);
				return CAST_any;
			}
			if (varInit->kind != STMT_beg) {
				if (!emitVarOffs(rt, variable)) {
					return CAST_any;
				}
				if (!emitInt(rt, opc_sti, variable->size)) {
					return CAST_any;
				}
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

/// Generate byte-code for variable indirect load.
static ccKind genIndirection(ccContext cc, symn variable, ccKind get, int isIndex) {
	if (get == KIND_var) {
		// load only the offset of the variable, no matter if its a reference or not
		// HACK: currently used to get the length of a slice
		return CAST_ref;
	}

	rtContext rt = cc->rt;
	symn type = variable->type;
	ccKind cast = refCast(variable);

	switch (variable->kind & MASK_kind) {
		default:
			fatal(ERR_INTERNAL_ERROR);
			return CAST_any;

		case KIND_typ:
		case KIND_fun:
			// typename is by reference, ex: pointer b = int32;
			if (!isIndex) {
				cast = CAST_any;
			}
			type = variable;
			break;

		case KIND_var:
			break;
	}

	if (get == CAST_ref || get == CAST_var || get == CAST_arr) {
		if (cast == get) {
			// make a copy of a reference, variant or slice
			if (!emitInt(rt, opc_ldi, refSize(variable))) {
				return CAST_any;
			}
			return get;
		}

		if (cast == CAST_ref || cast == CAST_var || cast == CAST_arr) {
			// convert a variant or an array to a reference (ex: `int &a = var;`)
			if (!emitInt(rt, opc_ldi, vm_ref_size)) {
				return CAST_any;
			}
			return get;
		}

		// use the offset of the variable.
		return get;
	}

	if (cast == CAST_ref) {
		// load reference indirection (variable is a reference to a value)
		if (!emitInt(rt, opc_ldi, vm_ref_size)) {
			return CAST_any;
		}
	}

	// load the value of the variable
	if (!emitInt(rt, opc_ldi, refSize(type))) {
		return CAST_any;
	}

	return castOf(type);
}
static ccKind genVariable(ccContext cc, symn variable, ccKind get, astn ast);
static ccKind genPreVarLen(ccContext cc, astn ast, symn type, ccKind get, ccKind got) {
	rtContext rt = cc->rt;

	// array: push length of variable first or copy.
	if (get == CAST_arr && got != CAST_arr) {
		symn length = type->fields;
		if (length == NULL || !isStatic(length)) {
			error(rt, ast->file, ast->line, ERR_EMIT_LENGTH, NULL);
			return CAST_any;
		}
		if (!genVariable(cc, length, refCast(length), ast)) {
			return CAST_any;
		}
	}

	// variant: push type of variable first or copy.
	if (get == CAST_var && got != CAST_var) {
		if (!emitVarOffs(rt, type)) {
			return CAST_any;
		}
	}
	return get;
}

/// Generate byte-code for variable usage.
static ccKind genVariable(ccContext cc, symn variable, ccKind get, astn ast) {
	rtContext rt = cc->rt;
	symn type = variable->type;
	ccKind got = castOf(variable);

	dbgCgen("%?s:%?u: %T / (%K->%K)", ast->file, ast->line, variable, variable->kind, get);
	if (variable == cc->null_ref) {
		switch (get) {
			default:
				traceAst(ast);
				return CAST_any;

			case CAST_var:
				// variant a = null;
				if (!emitVarOffs(rt, type)) {
					return CAST_any;
				}
				break;

			case CAST_arr:
				// int a[] = null;
				if (!emitOffs(rt, 0)) {
					return CAST_any;
				}
				break;

			case CAST_ref:
				// pointer a = null;
				break;
		}
		if (!emitInt(rt, opc_lref, 0)) {	//push reference
			return CAST_any;
		}
		return get;
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
			// typename is by reference
			got = CAST_ref;
			break;

		case KIND_var:
			break;
	}

	if (variable->offs == 0) {
		error(rt, ast->file, ast->line, ERR_EMIT_VARIABLE, variable);
	}

	if (!genPreVarLen(cc, ast, type, get, got)) {
		return CAST_any;
	}

	// generate the address of the variable
	if (!emitVarOffs(rt, variable)) {
		return CAST_any;
	}

	return genIndirection(cc, variable, get, 0);
}
/// Generate byte-code for OPER_dot `a.b`.
static ccKind genMember(ccContext cc, astn ast, ccKind get) {
	rtContext rt = cc->rt;
	symn object = linkOf(ast->op.lhso, 1);
	symn member = linkOf(ast->op.rhso, 1);

	dbgCgen("%?s:%?u: %t", ast->file, ast->line, ast);
	if (member == NULL && ast->op.rhso != NULL) {
		astn call = ast->op.rhso;
		if (call->kind == OPER_fnc) {
			member = linkOf(call->op.lhso, 1);
		}
	}
	if (object == NULL && ast->op.lhso != NULL) {
		// we may have an expression that is not a variable: a[0].x;
		object = ast->op.lhso->type;
	}

	if (!object || !member) {
		traceAst(ast);
		return CAST_any;
	}

	if (isStatic(member)) {
		if (isVariable(object) && !isArrayType(object->type)) {
			warn(rt, raise_warn_typ2, ast->file, ast->line, WARN_STATIC_FIELD_ACCESS, member, object->type);
		}
		return genAst(cc, ast->op.rhso, get);
	}

	if (isTypeExpr(ast->op.lhso)) {
		// check what is on the left side
		error(rt, ast->file, ast->line, ERR_INVALID_FIELD_ACCESS, member);
		return CAST_any;
	}

	if (!genPreVarLen(cc, ast, member->type, get, castOf(member))) {
		return CAST_any;
	}

	ccKind lhsCast = CAST_ref;
	if (member == cc->length_ref) {
		// HACK: dynamic array length: do not load indirect the address of the first element
		lhsCast = KIND_var;
	}

	if (!genAst(cc, ast->op.lhso, lhsCast)) {
		traceAst(ast);
		return CAST_any;
	}

	if (!emitInt(rt, opc_inc, member->offs)) {
		traceAst(ast);
		return CAST_any;
	}

	return genIndirection(cc, member, get, 0);
}
/// Generate byte-code for OPER_idx `a[b]`.
static ccKind genIndex(ccContext cc, astn ast, ccKind get) {
	rtContext rt = cc->rt;
	struct astNode tmp;

	if (!genPreVarLen(cc, ast, ast->type, get, refCast(ast->type))) {
		return CAST_any;
	}

	dbgCgen("%?s:%?u: %t", ast->file, ast->line, ast);
	if (!genAst(cc, ast->op.lhso, CAST_ref)) {
		traceAst(ast);
		return CAST_any;
	}

	size_t elementSize = refSize(ast->type);	// size of array element
	if (rt->foldConst && eval(cc, &tmp, ast->op.rhso) == CAST_i64) {
		size_t offs = elementSize * intValue(&tmp);
		if (!emitInt(rt, opc_inc, offs)) {
			traceAst(ast);
			return CAST_any;
		}
	}
	else {
		if (!genAst(cc, ast->op.rhso, refCast(cc->type_idx))) {
			traceAst(ast);
			return CAST_any;
		}
		if (!emitInt(rt, opc_mad, elementSize)) {
			traceAst(ast);
			return CAST_any;
		}
	}

	return genIndirection(cc, ast->type, get, 1);
}

/// Generate byte-code for OPER_fnc `a(b)`.
static ccKind genCast(ccContext cc, astn ast, ccKind get) {
	rtContext rt = cc->rt;

	symn function = linkOf(ast->op.lhso, 0);
	if (function == NULL || !isTypename(function)) {
		error(rt, ast->file, ast->line, ERR_INVALID_CAST, ast);
		traceAst(ast);
		return CAST_any;
	}

	// emit intrinsic
	// FIXME: don't know why emit is a type ...
	if (function == cc->emit_opc) {
		astn args = chainArgs(ast->op.rhso);
		const size_t locals = stkOffset(rt, 0);
		dbgCgen("%?s:%?u: emit: %t", ast->file, ast->line, ast);
		// Reverse Polish notation: int32 a = emit(int32(a), int32(b), i32.add);
		for (; args != NULL; args = args->next) {
			dbgCgen("%?s:%?u: emit.arg: %t", args->file, args->line, args);
			if (!genAst(cc, args, CAST_any)) {
				traceAst(args);
				return CAST_any;
			}
			// extra check to not underflow the stack
			if (stkOffset(rt, 0) < locals) {
				error(rt, ast->file, ast->line, ERR_EMIT_STATEMENT, ast);
			}

			symn lnk = linkOf(args, 1);
			if (lnk && lnk->init && lnk->init->kind == TOKEN_opc) {
				// instruction: add.i32
				continue;
			}

			if (args->kind == OPER_fnc && args->op.lhso) {
				if (args->op.lhso->kind == RECORD_kwd) {
					// value cast: struct(value)
					continue;
				}
				lnk = linkOf(args->op.lhso, 1);
				if (lnk && lnk == args->type) {
					// type cast: float32(value)
					continue;
				}
			}
			warn(rt, raiseWarn, ast->file, ast->line, WARN_PASS_ARG_NO_CAST, args, args->type);
		}
		return get;
	}

	astn arg = ast->op.rhso;
	// casts may have a single argument
	if (arg == NULL || arg->kind == OPER_com) {
		error(rt, ast->file, ast->line, ERR_INVALID_CAST, ast);
		return CAST_any;
	}

	ccKind cast = refCast(function);
	// variant(...) || typename(...) || pointer(...)
	if (function == cc->type_var || function == cc->type_rec || function == cc->type_ptr) {
		if (function == cc->type_rec) {
			// push type
			if (!emitVarOffs(rt, arg->type)) {
				traceAst(ast);
				return CAST_any;
			}
			return cast;
		}
		return genAst(cc, arg, cast);
	}

	// float64(3)
	switch (cast) {
		case CAST_val:
			// cast the result of an emit to a value-type
			if (arg->type == cc->emit_opc) {
				if (!genAst(cc, arg, cast)) {
					traceAst(ast);
					return CAST_any;
				}
				return cast;
			}
			// fall through
		default:
			return CAST_any;

			// cast to basic type
		case CAST_vid:
		case CAST_bit:
		case CAST_i32:
		case CAST_u32:
		case CAST_i64:
		case CAST_u64:
		case CAST_f32:
		case CAST_f64:
			return genAst(cc, arg, cast);
	}

	error(cc->rt, ast->file, ast->line, ERR_INVALID_CAST, ast);
	return CAST_any;
}
// TODO: simplify
static ccKind genCall(ccContext cc, astn ast) {
	rtContext rt = cc->rt;
	symn function = linkOf(ast->op.lhso, 0);
	if (function == NULL || function->params == NULL) {
		error(cc->rt, ast->file, ast->line, ERR_EMIT_STATEMENT, ast);
		traceAst(ast);
		return CAST_any;
	}

	struct astNode extraArguments[3];
	size_t offsets[maxTokenCount];
	astn values[maxTokenCount];
	symn vaElemType = NULL;
	astn vaList = NULL;

	// save parameter offsets and init value
	if (function->params != NULL) {
		// add extra (file, line, ...) arguments to the raise builtin function
		astn arg = chainArgs(ast->op.rhso);
		symn prm = function->params;
		int argc = 0;

		memset(extraArguments, 0, sizeof(extraArguments));

		if (function == cc->libc_dbg) {
			char *file = ast->file;
			int line = ast->line;

			// use the location of the expression statement, not the expansion
			if (cc->file != NULL && cc->line > 0) {
				file = cc->file;
				line = cc->line;
				dbgCgen("%?s:%?u: Using the location of the last expression statement", file, line);
			}

			// add 2 extra computed argument to the raise function(file and line)
			extraArguments[1].kind = TOKEN_val;
			extraArguments[2].kind = TOKEN_val;
			extraArguments[1].type = function->params->next->type;
			extraArguments[2].type = function->params->next->next->type;
			extraArguments[1].ref.name = file;
			extraArguments[2].cInt = line;

			// chain the new arguments
			extraArguments[1].next = &extraArguments[2];
			extraArguments[2].next = arg;
			arg = &extraArguments[1];
		}

		if (function->params->init != NULL) {
			*extraArguments = *function->params->init;
		}

		extraArguments->type = function->params->type;
		extraArguments->next = arg;
		arg = extraArguments;

		while (prm != NULL) {
			if (argc >= maxTokenCount) {
				error(cc->rt, ast->file, ast->line, ERR_EXPR_TOO_COMPLEX, ast);
				traceAst(ast);
				return CAST_any;
			}
			offsets[argc] = prm->offs;
			values[argc] = prm->init;
			prm->init = arg;
			argc += 1;

			// advance
			if ((prm->kind & ATTR_varg) != 0 && (arg == NULL || arg->kind != PNCT_dot3)) {
				vaElemType = prm->type->type;
				vaList = arg;
				arg = NULL;
			}
			prm = prm->next;
			if (arg == NULL) {
				break;
			}
			arg = arg->next;
		}

		// more or less args than params is a fatal error
		// should have been raised by the type checker
		if (prm != NULL || arg != NULL) {
			fatal(ERR_INTERNAL_ERROR);
			return CAST_any;
		}
	}

	const ccKind resultCast = refCast(function->params->type);
	const size_t resultOffs = stkOffset(rt, function->params->size);
	struct {
		struct symNode var;
		struct astNode ast;
	} tempArguments[8], *temp = tempArguments;

	// first make an array of varargs
	ssize_t vaLength = 0;
	if (vaElemType != NULL) {
		size_t vaStore = 0;
		if (vaElemType->size % vm_stk_align != 0) {
			vaStore = vaElemType->size;

			size_t vaAlloc = 0;
			for (astn arg = vaList; arg; arg = arg->next) {
				vaAlloc += vaStore;
			}

			vaAlloc = padOffset(vaAlloc, vm_stk_align);
			if (!emitInt(rt, opc_spc, vaAlloc)) {
				traceAst(ast);
				return CAST_any;
			}
		}
		else {
			// reverse the list of arguments: stack works in reverse order
			astn reverse = NULL;
			for (astn arg = vaList; arg; ) {
				astn next = arg->next;
				arg->next = reverse;
				reverse = arg;
				arg = next;
			}
			vaList = reverse;
		}

		// generate the arguments
		size_t vaOffset = stkOffset(rt, 0);
		for (astn arg = vaList; arg; arg = arg->next) {
			if (!genAst(cc, arg, refCast(vaElemType))) {
				traceAst(ast);
				return CAST_any;
			}
			if (vaStore != 0) {
				if (!emitStack(rt, opc_ldsp, vaOffset)) {
					traceAst(ast);
					return CAST_any;
				}
				if (!emitInt(rt, opc_sti, vaStore)) {
					traceAst(ast);
					return CAST_any;
				}
				vaOffset -= vaStore;
			}
			vaLength += 1;
		}
	}

	// convert const ref parameter arguments to local variables
	for (symn prm = function->params; prm; prm = prm->next) {
		astn arg = prm->init;
		ccKind typCast = refCast(prm->type);

		if ((prm->kind & ATTR_varg) != 0 && (arg == NULL || arg->kind != PNCT_dot3)) {
			size_t vaOffset = stkOffset(rt, 0);
			if (!emitOffs(rt, vaLength)) {
				return CAST_any;
			}
			if (!emitStack(rt, opc_ldsp, vaOffset)) {
				return CAST_any;
			}

			if (temp > tempArguments + lengthOf(tempArguments)) {
				fatal(ERR_INTERNAL_ERROR);
				return CAST_any;
			}
			memset(temp, 0, sizeof(*temp));
			temp->var.name = "<.va_arg>";
			temp->var.kind = ATTR_cnst | KIND_var | CAST_arr;
			temp->var.type = prm->type;
			temp->var.init = prm->init;
			temp->var.offs = stkOffset(rt, 0);
			temp->var.size = prm->size;
			temp->var.unit = prm->unit;
			temp->var.file = prm->file;
			temp->var.line = prm->line;

			temp->ast.kind = TOKEN_var;
			temp->ast.type = prm->type;
			temp->ast.ref.link = &temp->var;
			temp->ast.ref.name = prm->name;
			temp->ast.ref.hash = 0;
			temp->ast.ref.used = NULL;
			prm->init = &temp->ast;
			temp++;
			break;
		}

		if (refCast(prm) != CAST_ref) {
			continue;
		}
		if (typCast == CAST_ref) {
			continue;
		}

		switch (arg->kind) {

			default:
				break;

			case TOKEN_var:
			case OPER_dot:
			case OPER_idx:
			case OPER_adr:
			case PNCT_dot3:
				// if argument is already a variable, do not create a new one
				continue;

			case TOKEN_any:
			case TOKEN_val:
				// no need to extract "string constants" to variable
				if (refCast(arg->type) == CAST_ref) {
					continue;
				}
				break;
		}

		warn(rt, raiseDebug, ast->file, ast->line, WARN_ADDING_TEMPORARY_VAR, prm, arg);
		if (temp > tempArguments + lengthOf(tempArguments)) {
			fatal(ERR_INTERNAL_ERROR);
			return CAST_any;
		}

		memset(temp, 0, sizeof(*temp));
		temp->var.kind = ATTR_cnst | KIND_var | typCast;
		temp->var.type = prm->type;
		temp->var.init = prm->init;
		temp->var.offs = 0;
		temp->var.size = prm->type->size;
		temp->var.unit = prm->unit;
		temp->var.file = prm->file;
		temp->var.line = prm->line;

		temp->ast.kind = TOKEN_var;
		temp->ast.type = prm->type;
		temp->ast.ref.link = &temp->var;
		temp->ast.ref.name = prm->name;
		temp->ast.ref.hash = 0;
		temp->ast.ref.used = NULL;

		ccKind got = genDeclaration(cc, &temp->var, CAST_vid);
		if (got == CAST_any) {
			fatal(ERR_INTERNAL_ERROR);
			return CAST_any;
		}

		prm->init = &temp->ast;
		temp++;
	}

	// generate arguments (push or cache)
	const size_t locals = stkOffset(rt, 0);
	size_t resultPos = stkOffset(rt, function->params->size);
	if (preAllocateArgs && isFunction(function)) {
		size_t preAlloc = argsSize(function);
		// alloc space for result and arguments
		if (preAlloc > 0 && !emitInt(rt, opc_spc, preAlloc)) {
			traceAst(ast);
			return CAST_any;
		}
	}

	// generate arguments
	size_t offs = 0;
	for (symn prm = function->params; prm; prm = prm->next) {

		// skip generating void parameter
		if (prm->size == 0) {
			continue;
		}

		// update offset of inline variable
		if (isInline(prm)) {
			prm->offs = locals + offs;
			continue;
		}

		astn arg = prm->init;
		if ((prm->kind & ATTR_varg) != 0 && arg && arg->kind == PNCT_dot3) {
			// drop `...`
			arg = arg->op.rhso;
		}
		if (castOf(prm) == CAST_ref && arg && arg->kind == OPER_adr) {
			// drop `&`
			arg = arg->op.rhso;
		}

		// allocate space for uninitialized arguments
		if (arg == NULL || arg->kind == TOKEN_any) {
			if (prm->name && *prm->name == '.') {
				warn(rt, raise_warn_var8, ast->file, ast->line, ERR_UNINITIALIZED_VARIABLE, prm);
			}
			else {
				raiseLevel level = cc->errUninitialized ? raiseWarn : raiseError;
				warn(rt, level, ast->file, ast->line, ERR_UNINITIALIZED_VARIABLE, prm);
			}
			if (!emitInt(rt, opc_spc, prm->size)) {
				traceAst(ast);
				return CAST_any;
			}
			continue;
		}

		// generate value
		offs += padOffset(prm->size, vm_stk_align);

		// generate the argument value
		size_t argOffs = stkOffset(rt, prm->size);
		if (!genAst(cc, arg, refCast(prm))) {
			traceAst(ast);
			return CAST_any;
		}

		if (argOffs != stkOffset(rt, 0)) {
			fatal(ERR_INTERNAL_ERROR": argument size does not match parameter size");
			traceAst(ast);
			return CAST_any;
		}

		if (argOffs - locals > prm->offs) {
			if (!emitStack(rt, opc_ldsp, locals + prm->offs)) {
				trace(ERR_INTERNAL_ERROR);
				return CAST_any;
			}
			if (!emitInt(rt, opc_sti, prm->size)) {
				trace(ERR_INTERNAL_ERROR);
				return CAST_any;
			}
		}

		if (isInline(function)) {
			prm->offs = locals + offs;
		}
	}

	if (isInline(function)) {
		// generate inline expression
		if (!genAst(cc, function->init, resultCast)) {
			traceAst(function->init);
			return CAST_any;
		}
		resultPos = stkOffset(rt, 0);
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
	}

	// restore parameter offsets and reset init value
	int argc = 0;
	for (symn prm = function->params; prm != NULL; prm = prm->next) {
		prm->offs = offsets[argc];
		prm->init = values[argc];
		argc += 1;
	}

	if (resultPos != resultOffs && function->params->size > 0) {
		// move result value and free some temporary locals

		size_t assignBegin = emit(rt, markIP);
		// move result value and free some temporary locals
		if (!emitStack(rt, opc_ldsp, resultPos)) {
			trace(ERR_INTERNAL_ERROR);
			return CAST_any;
		}
		if (!emitInt(rt, opc_ldi, function->params->size)) {
			trace(ERR_INTERNAL_ERROR);
			return CAST_any;
		}
		size_t assignEnd = emit(rt, markIP);
		if (!emitStack(rt, opc_ldsp, resultOffs)) {
			trace(ERR_INTERNAL_ERROR);
			return CAST_any;
		}
		if (!emitInt(rt, opc_sti, function->params->size)) {
			trace(ERR_INTERNAL_ERROR);
			return CAST_any;
		}

		// optimize generated assignment to a single `set` instruction
		if (foldAssignment(rt, 0, assignBegin, assignEnd)) {
			debug("assignment optimized: %t", ast);
		}
	}

	// drop cached arguments, except result.
	if (!emitStack(rt, opc_drop, resultOffs)) {
		fatal(ERR_INTERNAL_ERROR);
		return CAST_any;
	}

	return resultCast;
}

/// Emit operator based on type: (add, sub, mul, ...)
// TODO: this should be implemented using operator overloading in `cmpl.lang.ci`
static ccKind genOperator(rtContext rt, ccToken token, ccKind cast) {
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
					opc = vmSelect(i32_ceq, i64_ceq);
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
			if (!genOperator(rt, OPER_ceq, cast)) {
				trace(ERR_INTERNAL_ERROR);
				return CAST_any;
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
			if (!genOperator(rt, OPER_cgt, cast)) {
				trace(ERR_INTERNAL_ERROR);
				return CAST_any;
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
			if (!genOperator(rt, OPER_clt, cast)) {
				trace(ERR_INTERNAL_ERROR);
				return CAST_any;
			}
			opc = opc_not;
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

	if (opc != opc_nop && !emit(rt, opc)) {
		return CAST_any;
	}

	return cast;
}

/// Generate byte-code for OPER_sel `a ? b : c`.
static ccKind genLogical(ccContext cc, astn ast) {
	rtContext rt = cc->rt;
	struct astNode tmp;

	if (rt->foldConst && eval(cc, &tmp, ast->op.test) == CAST_bit) {
		if (!genAst(cc, bolValue(&tmp) ? ast->op.lhso : ast->op.rhso, CAST_any)) {
			traceAst(ast);
			return CAST_any;
		}
		return refCast(ast->type);
	}

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
	return refCast(ast->type);
}

/// Generate bytecode from abstract syntax tree.
static ccKind genAst(ccContext cc, astn ast, ccKind get) {
	rtContext rt = cc->rt;

	if (ast == NULL || ast->type == NULL) {
		fatal(ERR_INTERNAL_ERROR);
		return CAST_any;
	}

	const size_t ipBegin = emit(rt, markIP);
	const size_t spBegin = stkOffset(rt, 0);
	const size_t spEnd = stkOffset(rt, refCast(ast->type));
	ccKind op, got = refCast(ast->type);

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
		case STMT_beg:
			// statement block or function body
			for (astn ptr = ast->stmt.stmt; ptr != NULL; ptr = ptr->next) {
				int errors = rt->errors;
				int nested = ptr->kind == STMT_beg;
				size_t ipStart2 = emit(rt, markIP);
				cc->file = ptr->file;
				cc->line = ptr->line;
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
				// {...}: may produce local variables
				got = CAST_val;
			}
			break;

		case STMT_end:
			// expression statement
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
			break;

		case STMT_con:
		case STMT_brk:
			dieif(get != CAST_vid, ERR_INTERNAL_ERROR);

			ast->jmp.stks = spBegin;
			ast->jmp.offs = emit(rt, opc_jmp);
			if (ast->jmp.offs == 0) {
				traceAst(ast);
				return CAST_any;
			}

			ast->next = cc->jumps;
			cc->jumps = ast;
			break;

		case STMT_ret:
			dieif(get != CAST_vid, ERR_INTERNAL_ERROR);
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
			// TODO: destroy local variables.
			if (!emitStack(rt, opc_drop, vm_ref_size + argsSize(ast->jmp.func))) {
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
			// parenthesised expression: `(value)`
			if (ast->op.lhso == NULL) {
				if (!(got = genAst(cc, ast->op.rhso, get))) {
					traceAst(ast);
					return CAST_any;
				}
				break;
			}

			// emit by value: `struct(value)`
			if (ast->op.lhso->kind == RECORD_kwd) {
				ccKind cast = castOf(ast->type);
				if (cast == CAST_ref) {
					error(cc->rt, ast->file, ast->line, ERR_EMIT_STATEMENT, ast);
				}
				if (!(got = genAst(cc, ast->op.rhso, cast))) {
					traceAst(ast);
					return CAST_any;
				}
				break;
			}

			// type cast: `typename(value)`
			if (isTypeExpr(ast->op.lhso)) {
				if (!(got = genCast(cc, ast, get))) {
					traceAst(ast);
					return CAST_any;
				}
				break;
			}

			// function call: `function(arguments)`
			if (!(got = genCall(cc, ast))) {
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

		case OPER_adr:		// '&'
		case PNCT_dot3:		// '...'
			// `&` and `...` may be used only in arguments, raise error if used somewhere else
			error(rt, ast->file, ast->line, ERR_INVALID_OPERATOR, ast, cc->type_ptr, ast->type);
			// fall through

		case OPER_pls:		// '+'
			if (!(got = genAst(cc, ast->op.rhso, get))) {
				traceAst(ast);
				return CAST_any;
			}
			break;

		case OPER_not:		// '!'
		case OPER_mns:		// '-'
		case OPER_cmt:		// '~'
			if (!genAst(cc, ast->op.rhso, got)) {
				traceAst(ast);
				return CAST_any;
			}
			if (!genOperator(rt, ast->kind, got)) {
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
			if (!genOperator(rt, ast->kind, op)) {    // uint % int => u32.mod
				traceAst(ast);
				return CAST_any;
			}

			#ifdef DEBUGGING	// extra check: validate some conditions.
			switch (ast->kind) {
				default:
					if (ast->op.lhso->type != ast->op.rhso->type) {
						fatal(ERR_INTERNAL_ERROR);
						break;
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
		case OPER_any:		// '||'
			if (!genAst(cc, ast->op.lhso, CAST_bit)) {
				traceAst(ast);
				return CAST_any;
			}

			// duplicate lhs and check if it is true
			if (!emitInt(rt, opc_dup1, 0)) {
				traceAst(ast);
				return CAST_any;
			}
			vmOpcode opc = ast->kind != OPER_all ? opc_jnz : opc_jz;
			size_t jmp = emit(rt, opc);
			if (jmp == 0) {
				traceAst(ast);
				return CAST_any;
			}

			// discard lhs and replace it with rhs
			if (!emitInt(rt, opc_spc, -vm_stk_align)) {
				traceAst(ast);
				return CAST_any;
			}
			if (!genAst(cc, ast->op.rhso, CAST_bit)) {
				traceAst(ast);
				return CAST_any;
			}

			fixJump(rt, jmp, emit(rt, markIP), spEnd);

			#ifdef DEBUGGING	// extra check: validate some conditions.
			dieif(ast->op.lhso->type != cc->type_bol, ERR_INTERNAL_ERROR);
			dieif(ast->op.rhso->type != cc->type_bol, ERR_INTERNAL_ERROR);
			dieif(got != CAST_bit, ERR_INTERNAL_ERROR": (%t) -> %K", ast, got);
			#endif
			break;

		case OPER_sel:      // '?:'
			if (!genLogical(cc, ast)) {
				traceAst(ast);
				return CAST_any;
			}
			break;

		case INIT_set:  	// '='
		case ASGN_set: {	// '='
			ccKind cast = refCast(ast->op.lhso->type);
			size_t size = refSize(ast->op.lhso->type);
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

			switch (cast) {
				default:
					cast = CAST_ref;
					break;

				// assign arrays and variants
				case CAST_ref:
					size = cc->type_ptr->size;
					cast = KIND_var;
					break;
				case CAST_arr:
				case CAST_var:
					size = cc->type_var->size;
					cast = KIND_var;
					break;
			}

			if (!genAst(cc, ast->op.lhso, cast)) {
				traceAst(ast);
				return CAST_any;
			}
			size_t codeEnd = emitInt(rt, opc_sti, size);
			if (!codeEnd) {
				traceAst(ast);
				return CAST_any;
			}

			// optimize assignments.
			if (rt->foldAssign && foldAssignment(rt, spBegin, ipBegin, codeEnd)) {
				if (get == CAST_vid && stkOffset(rt, 0) < spBegin) {
					// we probably cleared the stack
					got = CAST_vid;
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
			got = refCast(ast->type);
			break;

		case TOKEN_val:
			if (get == CAST_vid) {
				// void("message") || void(0)
				warn(rt, raise_warn_gen8, ast->file, ast->line, WARN_NO_CODE_GENERATED, ast);
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

				case CAST_arr:
				case CAST_ref: // push reference
					if (get == CAST_var && !emitOffs(rt, ast->type->offs)) {
						traceAst(ast);
						return CAST_any;
					}
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
			dieif(isTypename(var) && !isStatic(var), ERR_INTERNAL_ERROR);	// types are static
			dieif(isFunction(var) && !isStatic(var), ERR_INTERNAL_ERROR);	// functions are static

			if (var->tag == ast) {
				if (isStatic(var) || isInline(var)) {
					// static variables are already generated.
					debug("%?s:%?u: not a variable declaration: %T", ast->file, ast->line, var);
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
		if (get == CAST_vid && spBegin != stkOffset(rt, 0)) {
			debug("%s:%u: locals left on stack(get: %K, size: %D): `%t`", ast->file, ast->line, get, stkOffset(rt, 0) - spBegin, ast);
			if (!emitStack(rt, opc_drop, spBegin)) {
				traceAst(ast);
				return CAST_any;
			}
		}
	}

	if (get == KIND_var && (got == CAST_ref || got == CAST_arr)) {
		get = got;
	}

	// generate cast
	if (get != got) {
		if ((get == CAST_u32 || get == CAST_u64) && (got == CAST_f32 || got == CAST_f64)) {
			warn(rt, raise_warn_typ4, ast->file, ast->line, WARN_USING_SIGNED_CAST, ast);
		}
		if ((get == CAST_f32 || get == CAST_f64) && (got == CAST_u32 || got == CAST_u64)) {
			warn(rt, raise_warn_typ4, ast->file, ast->line, WARN_USING_SIGNED_CAST, ast);
		}

		vmOpcode cast = opc_last;
		switch (get) {
			default:
				break;

			case CAST_vid:
				// TODO: destroy local variables.
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
			case CAST_u32:
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

			case CAST_u64:
			case CAST_i64:
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
					case CAST_u32:
						cast = i32_f32;
						break;

					case CAST_i64:
					case CAST_u64:
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
					case CAST_u32:
						cast = i32_f64;
						break;

					case CAST_i64:
					case CAST_u64:
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

	if (zeroExtendUnsigned && get == CAST_u32) {

		dbgCgen("%?s:%?u: zero extend[%K->%K / %T]: %t", ast->file, ast->line, got, get, ast->type, ast);
		switch (ast->type->size) {
			default:
				trace(ERR_INTERNAL_ERROR": size: %d, cast [%K->%K]: %t", ast->type->size, got, get, ast);
				return CAST_any;

			case 8:
			case 4:
				break;

			case 2:
				if (!testOcp(rt, rt->vm.pc, opc_ldi2, NULL)) {
					// zero extend only if previous instruction is an indirect load?
					break;
				}
				if (!emitInt(rt, b32_bit, b32_bit_and | 16)) {
					traceAst(ast);
					return CAST_any;
				}
				break;

			case 1:
				if (!testOcp(rt, rt->vm.pc, opc_ldi1, NULL)) {
					// zero extend only if previous instruction is an indirect load?
					break;
				}
				if (!emitInt(rt, b32_bit, b32_bit_and | 8)) {
					traceAst(ast);
					return CAST_any;
				}
				break;
		}
	}

	return got;
}

int ccGenCode(ccContext cc, int debug) {
	rtContext rt = cc->rt;

	if (rt->errors != 0) {
		dieif(cc == NULL, ERR_INTERNAL_ERROR);
		trace("can not generate code with errors");
		return rt->errors;
	}

	// leave the global scope.
	rt->main = install(cc, ".main", ATTR_stat | KIND_fun, cc->type_fun->size, cc->type_fun, cc->root);
	cc->scope = leave(cc, cc->genStaticGlobals ? ATTR_stat | KIND_def : KIND_def, 0, 0, NULL, NULL);
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
						return -1;
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

	// prepare to emit instructions
	if (!vmInit(rt, debug, NULL)) {
		return -2;
	}

	//~ read only memory ends here.
	//~ strings, types, add(constants, functions, enums, ...)
	rt->vm.ro = emit(rt, markIP);
	rt->vm.cs = rt->vm.ds = 0;

	// static variables & functions
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

		dieif(var->offs != 0, "Error `%T` offs: %d", var, var->offs);    // already generated ?

		// align memory. Speed up read, write and execution.
		rt->_beg = padPointer(rt->_beg, vm_mem_align);

		if (isFunction(var)) {
			size_t offs = 0;
			// recalculate the offsets of the parameters
			for (symn param = var->params; param != NULL; param = param->next) {
				if (isStatic(param)) {
					fatal(ERR_INTERNAL_ERROR);
					continue;
				}
				if (param->size != 0) {
					continue;
				}
				param->size = padOffset(param->type->size, vm_stk_align);
				param->offs = offs += param->size;
			}

			// function starts here
			var->offs = emit(rt, markIP);

			// reset the stack size and max jump
			fixJump(rt, 0, var->offs, vm_ref_size + argsSize(var));

			if (!genAst(cc, var->init, CAST_vid)) {
				error(rt, var->file, var->line, ERR_EMIT_FUNCTION, var);
				continue;
			}
			// add return instruction to functions with empty body
			if (var->offs == emit(rt, markIP) && !emit(rt, opc_jmpi)) {
				error(rt, var->file, var->line, ERR_EMIT_FUNCTION, var);
				continue;
			}
			// remove the last (drop; ret) instructions if possible
			if (testOcp(rt, rt->vm.pc, opc_spc, NULL)) {
				size_t pc = var->offs;
				size_t pc1 = pc;
				while (pc < rt->vm.pc) {
					pc1 = pc;
					if (!nextOpc(rt, &pc, NULL)) {
						fatal(ERR_INTERNAL_ERROR);
						return -1;
					}
				}
				if (rt->vm.px < pc1 && testOcp(rt, pc1, opc_jmpi, NULL)) {
					// it is safe to remove the last opcode (clear stack)
					if (rollbackPc(rt)) {
						rt->vm.pc = pc1;
					}
				}
			}
			// last instruction of a function must be ret
			if (!testOcp(rt, rt->vm.pc, opc_jmpi, NULL)) {
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

			rt->vm.cs += var->size;
			addDbgFunction(rt, var);
		}
		else if (isVariable(var)) {
			dieif(var->size <= 0, "Error `%T` size: %d", var, var->size);    // instance of void ?

			// allocate the memory for the global variable
			var->offs = vmOffset(rt, rt->_beg);
			rt->_beg += var->size;

			if (rt->_beg >= rt->_end) {
				error(rt, var->file, var->line, ERR_MEMORY_OVERRUN);
				return -3;
			}
			rt->vm.ds += var->size;
		}
		else {
			fatal(ERR_INTERNAL_ERROR);
			return -1;
		}

		debug("Global `%T` offs: %d, size: %d", var, var->offs, var->size);
	}

	rt->main->offs = emit(rt, markIP);
	if (cc->root != NULL) {
		// reset the stack
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
				return -4;
			}
			addDbgStatement(rt, begin, emit(rt, markIP), var->tag);
		}

		// CAST_vid clears the stack
		if (!genAst(cc, cc->root, cc->genStaticGlobals ? CAST_vid : CAST_val)) {
			traceAst(cc->root);
			return -5;
		}

		if (cc->jumps != NULL) {
			fatal(ERR_INTERNAL_ERROR": invalid jump: `%t`", cc->jumps);
			cc->jumps = cc->jumps->next;
			return -6;
		}
	}

	// program entry(main()) and exit(halt())
	rt->vm.px = emitInt(rt, opc_nfc, 0);
	rt->vm.pc = rt->main->offs;
	rt->vm.ss = 0;

	// build the main initializer function.
	rt->main->size = emit(rt, markIP) - rt->main->offs;
	rt->main->fields = cc->scope;
	rt->main->fmt = rt->main->name;
	rt->main->unit = cc->unit;

	rt->vm.cs += rt->main->size;
	addDbgFunction(rt, rt->main);

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

	return rt->errors;
}

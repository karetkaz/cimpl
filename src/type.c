/*******************************************************************************
 *   File: type.c
 *   Date: 2011/06/23
 *   Desc: type system
 *******************************************************************************

Types are special kind of variables.

Basic types
	void
	bool
	char
	int8
	int16
	int32
	int64
	uint8
	uint16
	uint32
	uint64
	float32
	float64

	pointer
	variant
	typename        // compilers internal type representation structure
	function        // base type of all functions.
	object          // base class of all reference types.

#typedefs
	@int: alias for int32 or int64
	@byte: alias for uint8
	@float: alias for float32
	@double: alias for float64

#constants
	@true := 0 == 0;
	@false := 0 != 0;
	@null: pointer(emit(struct, i32(0)));

Derived data types:
	slice: struct {const pointer array; const int length;}
	variant: struct variant {const pointer value; const typename type;}
	delegate: struct delegate {const pointer function; const pointer closure;}

User defined types:
	pointers arrays and slices:
		pointers have unknown-size:
			ex: int a[*];
			are passed by reference,
		arrays have fixed-size:
			ex: int a[2];
			are passed by reference,
		slices have dynamic-size:
			ex: int a[];
			is a combination of pointer and length.
			where type of data is known by the compiler, the length by runtime.

	record
	function

*******************************************************************************/

#include "internal.h"

static symn promote(symn lht, symn rht);

/// Enter a new scope.
void enter(ccContext cc, symn owner) {
	cc->nest += 1;
	if (owner != NULL) {
		owner->owner = cc->owner;
		cc->owner = owner;
	}
}

/// Leave current scope.
symn leave(ccContext cc, ccKind mode, size_t align, size_t baseSize, size_t *outSize) {
	symn sym, result = NULL;

	symn owner = cc->owner;

	cc->nest -= 1;
	if (owner && owner->nest >= cc->nest) {
		cc->owner = owner->owner;
	} else {
		owner = NULL;
	}

	// clear from symbol table
	for (int i = 0; i < TBL_SIZE; i++) {
		for (sym = cc->deft[i]; sym && sym->nest > cc->nest; sym = sym->next) {
		}
		cc->deft[i] = sym;
	}

	// clear from scope stack
	for (sym = cc->scope; sym && sym->nest > cc->nest; sym = sym->scope) {
		sym->kind |= mode & MASK_attr;
		sym->owner = owner;

		// add to the list of scope symbols
		sym->next = result;
		result = sym;

		// add to the list of global symbols
		if (isStatic(sym)) {
			if (!cc->siff && sym->global == NULL) {
				sym->global = cc->global;
				cc->global = sym;
			}
		}

		// TODO: this logic should be not here
		if (!isStatic(sym) && sym->params && sym->init && sym->init->kind == STMT_beg) {
			warn(cc->rt, 3, sym->file, sym->line, WARN_FUNCTION_MARKED_STATIC, sym);
			sym->kind |= ATTR_stat;
		}
	}
	cc->scope = sym;

	// fix padding
	size_t offs = baseSize;
	size_t size = baseSize;
	switch (mode & MASK_kind) {
		default:
			fatal(ERR_INTERNAL_ERROR);
			break;

		case KIND_typ:
			// update offsets
			for (sym = result; sym != NULL; sym = sym->next) {
				if (isStatic(sym)) {
					continue;
				}
				sym->offs = padOffset(offs, align < sym->size ? align : sym->size);
				if (offs < sym->offs) {
					warn(cc->rt, 6, sym->file, sym->line, WARN_PADDING_ALIGNMENT, sym, sym->offs - offs, offs, sym->offs);
				}
				offs = sym->offs + sym->size;
				if (size < offs) {
					size = offs;
				}
			}
			size_t padded = padOffset(size, align);
			if (align && size != padded) {
				char *file = owner ? owner->file : NULL;
				int line = owner ? owner->line : 0;
				warn(cc->rt, 6, file, line, WARN_PADDING_ALIGNMENT, owner, padded - size, size, padded);
				size = padded;
			}
			break;

		case KIND_fun:
			// update offsets
			for (sym = result; sym != NULL; sym = sym->next) {
				if (isStatic(sym)) {
					continue;
				}
				sym->size = padOffset(sym->size, align);
				sym->offs = offs += sym->size;
			}
			break;

		case KIND_def:
			//
			break;
	}

	if (outSize != NULL) {
		*outSize = size;
	}

	return result;
}

/// Install a symbol (typename, variable, function or alias)
symn install(ccContext cc, const char *name, ccKind kind, size_t size, symn type, astn init) {
	int isType = (kind & MASK_kind) == KIND_typ;

	if (cc == NULL || name == NULL) {
		fatal(ERR_INTERNAL_ERROR);
		return NULL;
	}

	// initializer must be type checked
	if (init && init->type == NULL) {
		fatal(ERR_INTERNAL_ERROR);
		return NULL;
	}

	if (isType) {
		ccKind kind2 = kind;
		kind = ATTR_stat | ATTR_cnst | kind;
		logif(kind != kind2, "symbol `%s` should be declared as static and constant", name);
	}

	if (size == 0 && (kind & MASK_kind) == KIND_var) {
		switch (castOf(type)) {
			default:
				size = type->size;
				break;

			case CAST_ref:
				size = sizeof(vmOffs);
				break;
		}
	}

	symn def = newDef(cc, kind);
	if (def != NULL) {
		size_t length = strlen(name) + 1;
		unsigned hash = rehash(name, length) % TBL_SIZE;
		def->name = mapstr(cc, (char*)name, length, hash);
		def->nest = cc->nest;
		def->type = type;
		def->init = init;
		def->size = size;

		if (isType) {
			def->offs = vmOffset(cc->rt, def);
			if (type != NULL && type != cc->type_rec) {
				int maxChain = 16;
				symn obj = type;
				while (obj != NULL) {
					if ((maxChain -= 1) < 0) {
						error(cc->rt, cc->file, cc->line, ERR_DECLARATION_COMPLEX, def);
						break;
					}

					// stop on typename (int32 base type is typename)
					if (obj == cc->type_rec) {
						break;
					}

					// stop on object (inheritance chain)
					if (obj == cc->type_obj) {
						def->kind = (def->kind & ~MASK_cast) | CAST_ref;
						break;
					}
					obj = obj->type;
				}
				if (obj == NULL) {
					error(cc->rt, cc->file, cc->line, ERR_DECLARATION_COMPLEX, def);
				}
			}
		}

		def->next = cc->deft[hash];
		cc->deft[hash] = def;
		def->scope = cc->scope;
		cc->scope = def;
	}
	return def;
}

symn lookup(ccContext cc, symn sym, astn ref, astn arguments, int raise) {
	symn byName = NULL;
	symn best = NULL;
	int found = 0;

	dieif(!ref || ref->kind != TOKEN_var, ERR_INTERNAL_ERROR);

	for (; sym; sym = sym->next) {
		symn parameter = sym->params;
		int hasCast = 0;

		if (sym->name == NULL) {
			// exclude anonymous symbols
			continue;
		}

		if (strcmp(sym->name, ref->ref.name) != 0) {
			// exclude non matching names
			continue;
		}

		// lookup function by name (without arguments)
		if (parameter != NULL && arguments == NULL) {
			// keep the first match.
			if (byName == NULL) {
				byName = sym;
			}
			found += 1;
			continue;
		}

		if (arguments != NULL) {
			astn argument = NULL;
			if (arguments != cc->void_tag) {
				argument = chainArgs(arguments);
			}

			if (parameter != NULL) {
				// first argument starts next to result.
				parameter = parameter->next;
			}

			if (sym == cc->libc_dbg) {
				// debug has 2 hidden params: file and line.
				parameter = parameter->next->next;
			}

			while (parameter != NULL && argument != NULL) {
				if (!canAssign(cc, parameter, argument, 0)) {
					break;
				}
				if (!canAssign(cc, parameter, argument, 1)) {
					hasCast += 1;
				}
				parameter = parameter->next;
				argument = argument->next;
			}

			if (sym->params != NULL && (argument || parameter)) {
				continue;
			}
		}

		// perfect match
		if (hasCast == 0) {
			break;
		}

		// keep first match
		if (best == NULL) {
			best = sym;
		}

		found += 1;
		// if we are here then sym is found, but it has implicit cast in it
		debug("%?s:%?u: %t(%t) is probably %T", ref->file, ref->line, ref, arguments, sym);
	}

	if (sym == NULL && best) {
		if (found > 1) {
			warn(cc->rt, 3, ref->file, ref->line, WARN_USING_BEST_OVERLOAD, best, found);
		}
		sym = best;
	}

	if (sym == NULL && byName) {
		if (found == 1 || cc->siff) {
			debug("as ref `%T`(%?t)", byName, arguments);
			sym = byName;
		}
		else if (raise) {
			error(cc->rt, ref->file, ref->line, ERR_MULTIPLE_OVERLOADS, found, byName);
		}
	}

	// skip over aliases
	while (sym != NULL) {
		if (!isInline(sym)) {
			break;
		}
		if (isInvokable(sym)) {
			break;
		}
		if (sym->init == NULL) {
			sym = sym->type;
		}
		else if (sym->init->kind == TOKEN_var) {
			sym = sym->init->ref.link;
		}
		else {
			break;
		}
	}

	return sym;
}

/// change the type of a tree node.
static symn convert(ccContext cc, astn ast, symn type) {
	if (type == NULL) {
		return NULL;
	}
	if (ast->type == NULL) {
		ast->type = type;
		return type;
	}
	if (ast->type == type) {
		// no update required.
		return type;
	}

	warn(cc->rt, 6, ast->file, ast->line, WARN_ADDING_IMPLICIT_CAST, type, ast, ast->type);
	astn value = newNode(cc, TOKEN_any);
	*value = *ast;
	ast->kind = OPER_fnc;
	ast->op.rhso = value;
	ast->op.lhso = NULL;
	ast->op.test = NULL;
	ast->type = type;
	return type;
}

static symn typeCheckRef(ccContext cc, symn loc, astn ref, astn args, int raise) {
	if (ref == NULL || ref->kind != TOKEN_var) {
		traceAst(ref);
		return NULL;
	}

	symn sym;
	if (loc != NULL) {
		sym = loc->fields;
	}
	else {
		sym = cc->deft[ref->ref.hash];
	}

	if ((sym = lookup(cc, sym, ref, args, 1)) != NULL) {
		symn type;
		if (isInline(sym) && sym->init != NULL) {
			// resulting type is the type of the inline expression
			type = sym->init->type;
		}
		else if (isInvokable(sym) && args != NULL) {
			// resulting type is the type of result parameter
			type = sym->params->type;
		}
		else if (isTypename(sym) && args != NULL) {
			// resulting type is the type of the cast
			type = sym;
		}
		else {
			// resulting type is the type of the variable
			type = sym->type;
		}

		dieif(ref->kind != TOKEN_var, ERR_INTERNAL_ERROR);
		ref->ref.link = sym;
		ref->type = sym->type;
		addUsage(sym, ref);
		return type;
	}

	if (raise) {
		error(cc->rt, ref->file, ref->line, ERR_UNDEFINED_REFERENCE, ref);
	}
	return NULL;
}

symn typeCheck(ccContext cc, symn loc, astn ast, int raise) {
	symn lType = NULL;
	symn rType = NULL;
	symn type = NULL;
	symn sym = NULL;

	if (ast == NULL) {
		fatal(ERR_INTERNAL_ERROR);
		return NULL;
	}

	// do not type-check again
	if (ast->type != NULL) {
		return ast->type;
	}

	switch (ast->kind) {
		default:
			fatal(ERR_INTERNAL_ERROR);
			return NULL;

		case OPER_fnc: {
			astn ref = ast->op.lhso;
			astn args = ast->op.rhso;

			if (ref == NULL && args == NULL) {
				// int a = ();
				traceAst(ast);
				return NULL;
			}
			if (ref == NULL) {
				// int a = (3 + 6);
				rType = typeCheck(cc, linkOf(ref, 1), args, raise);
				convert(cc, args, rType);
				convert(cc, ast, rType);
				ast->type = rType;
				return rType;
			}
			if (args == NULL) {
				// int a = func();
				args = cc->void_tag;
			}
			// int a = func(4, 2);

			// try to lookup arguments in the current scope
			rType = typeCheck(cc, loc, args, 0);

			if (ref->kind == OPER_dot) {    // float32.sin(args)
				if (!typeCheck(cc, loc, ref->op.lhso, raise)) {
					traceAst(ast);
					return NULL;
				}
				loc = linkOf(ref->op.lhso, 1);
				ref->type = cc->type_fun;
				ref = ref->op.rhso;
			}

			if (rType == NULL) {
				// if failed, try to lookup the function
				lType = typeCheck(cc, loc, ref, 0);

				// typename(identifier): returns null if identifier is not defined.
				if (lType == cc->type_rec && linkOf(ref, 1) == cc->type_rec) {
					if (args->kind == TOKEN_var) {
						char *name = cc->type_rec->name;
						size_t len = strlen(name);
						rType = cc->type_rec;
						ast->kind = TOKEN_var;
						ast->type = rType;
						ast->ref.link = cc->null_ref;
						ast->ref.hash = rehash(name, len + 1) % TBL_SIZE;
						ast->ref.name = mapstr(cc, name, len + 1, ast->ref.hash);
						ast->type = rType;
						return rType;
					}
				}
				// lookup arguments in the function's scope (emit, raise, ...)
				rType = typeCheck(cc, linkOf(ref, 1), args, raise);
			}
			if (rType == NULL) {
				// FIXME: error: could not lookup arguments ...
				traceAst(ast);
				return NULL;
			}
			convert(cc, args, rType);
			type = typeCheckRef(cc, loc, ref, args, raise);
			if (type == NULL) {
				traceAst(ast);
				return NULL;
			}
			ast->type = type;
			return type;
		}

		case OPER_dot:
			lType = typeCheck(cc, loc, ast->op.lhso, raise);
			loc = linkOf(ast->op.lhso, 1);
			if (loc == NULL) {
				traceAst(ast);
				return NULL;
			}
			if (isVariable(loc)) {
				loc = loc->type;
				dieif(loc != lType, ERR_INTERNAL_ERROR);
			}
			rType = typeCheck(cc, loc, ast->op.rhso, raise);

			if (!lType || !rType) {
				if (raise) {
					traceAst(ast);
				}
				return NULL;
			}
			convert(cc, ast->op.lhso, lType);
			convert(cc, ast->op.rhso, rType);
			ast->type = rType;
			return rType;

		case OPER_idx:
			if (ast->op.lhso != NULL) {
				lType = typeCheck(cc, loc, ast->op.lhso, raise);
			}
			if (ast->op.rhso != NULL) {
				rType = typeCheck(cc, loc, ast->op.rhso, raise);
			}

			if (!lType || !rType) {
				traceAst(ast);
				return NULL;
			}
			// base type of array;
			type = lType->type;
			ast->type = type;
			return type;

		case OPER_pls:		// '+'
		case OPER_mns:		// '-'
		case OPER_cmt:		// '~'
			rType = typeCheck(cc, loc, ast->op.rhso, raise);

			if (!rType) {
				traceAst(ast);
				return NULL;
			}
			type = promote(NULL, rType);
			convert(cc, ast->op.rhso, type);
			ast->type = type;
			return type;

		case OPER_not:		// '!'
			rType = typeCheck(cc, loc, ast->op.rhso, raise);

			if (!rType) {
				traceAst(ast);
				return NULL;
			}
			type = cc->type_bol;
			convert(cc, ast->op.rhso, type);
			ast->type = type;
			return type;

		case OPER_add:		// '+'
		case OPER_sub:		// '-'
		case OPER_mul:		// '*'
		case OPER_div:		// '/'
		case OPER_mod:		// '%'
			lType = typeCheck(cc, loc, ast->op.lhso, raise);
			rType = typeCheck(cc, loc, ast->op.rhso, raise);

			if (!lType || !rType) {
				traceAst(ast);
				return NULL;
			}
			type = promote(lType, rType);
			convert(cc, ast->op.lhso, type);
			convert(cc, ast->op.rhso, type);
			ast->type = type;
			return type;

		case OPER_shl:		// '>>'
		case OPER_shr:		// '<<'
			lType = typeCheck(cc, loc, ast->op.lhso, raise);
			rType = typeCheck(cc, loc, ast->op.rhso, raise);

			if (!lType || !rType) {
				traceAst(ast);
				return NULL;
			}
			type = promote(lType, rType);
			convert(cc, ast->op.lhso, type);
			convert(cc, ast->op.rhso, cc->type_i32);

			if (type != NULL) {
				switch (castOf(type)) {
					default:
						error(cc->rt, ast->file, ast->line, ERR_INVALID_OPERATOR, ast, lType, rType);
						break;

					case CAST_i32:
					case CAST_i64:

					case CAST_u32:
					case CAST_u64:
						break;
				}
			}
			ast->type = type;
			return type;

		case OPER_and:		// '&'
		case OPER_ior:		// '|'
		case OPER_xor:		// '^'
			lType = typeCheck(cc, loc, ast->op.lhso, raise);
			rType = typeCheck(cc, loc, ast->op.rhso, raise);

			if (!lType || !rType) {
				traceAst(ast);
				return NULL;
			}
			type = promote(lType, rType);
			convert(cc, ast->op.lhso, type);
			convert(cc, ast->op.rhso, type);

			if (type != NULL) {
				switch (castOf(type)) {
					default:
						error(cc->rt, ast->file, ast->line, ERR_INVALID_OPERATOR, ast, lType, rType);
						break;

					case CAST_bit:

					case CAST_i32:
					case CAST_i64:

					case CAST_u32:
					case CAST_u64:
						break;
				}
			}
			ast->type = type;
			return type;

		case OPER_ceq:		// '=='
		case OPER_cne:		// '!='
		case OPER_clt:		// '<'
		case OPER_cle:		// '<='
		case OPER_cgt:		// '>'
		case OPER_cge:		// '>='
			lType = typeCheck(cc, loc, ast->op.lhso, raise);
			rType = typeCheck(cc, loc, ast->op.rhso, raise);

			if (!lType || !rType) {
				traceAst(ast);
				return NULL;
			}
			type = promote(lType, rType);
			convert(cc, ast->op.lhso, type);
			convert(cc, ast->op.rhso, type);
			type = cc->type_bol;
			ast->type = type;
			return type;

		case OPER_all:		// '&&'
		case OPER_any:		// '||'
			lType = typeCheck(cc, loc, ast->op.lhso, raise);
			rType = typeCheck(cc, loc, ast->op.rhso, raise);

			if (!lType || !rType) {
				traceAst(ast);
				return NULL;
			}
			type = cc->type_bol;
			convert(cc, ast->op.lhso, type);
			convert(cc, ast->op.rhso, type);
			ast->type = type;
			return type;

		case OPER_sel:		// '?:'
			lType = typeCheck(cc, loc, ast->op.lhso, raise);
			rType = typeCheck(cc, loc, ast->op.rhso, raise);
			type = typeCheck(cc, loc, ast->op.test, raise);

			if (!lType || !rType || !type) {
				traceAst(ast);
				return NULL;
			}
			type = promote(lType, rType);
			convert(cc, ast->op.test, cc->type_bol);
			convert(cc, ast->op.lhso, type);
			convert(cc, ast->op.rhso, type);
			ast->type = type;
			return type;

		case OPER_com:	// ','
			lType = typeCheck(cc, loc, ast->op.lhso, raise);
			rType = typeCheck(cc, loc, ast->op.rhso, raise);

			if (!lType || !rType) {
				traceAst(ast);
				return NULL;
			}
			convert(cc, ast->op.lhso, lType);
			convert(cc, ast->op.rhso, rType);
			type = cc->type_vid;
			ast->type = type;
			return type;

		// operator set
		case INIT_set:		// ':='
		case ASGN_set:		// ':='
			lType = typeCheck(cc, loc, ast->op.lhso, raise);
			rType = typeCheck(cc, NULL, ast->op.rhso, raise);
			sym = linkOf(ast->op.lhso, 1);

			if (!lType || !rType || !sym) {
				traceAst(ast);
				return NULL;
			}

			if (isConst(sym) && ast->kind != INIT_set) {
				error(cc->rt, ast->file, ast->line, ERR_INVALID_CONST_ASSIGN, ast);
			}
			if (!canAssign(cc, lType, ast->op.rhso, 0)) {
				traceAst(ast);
				return NULL;
			}
			convert(cc, ast->op.rhso, lType);
			convert(cc, ast->op.lhso, lType);
			ast->type = lType;
			return lType;

		// variable
		case TOKEN_var:
			type = typeCheckRef(cc, loc, ast, NULL, 0);
			if (type == NULL) {
				type = typeCheckRef(cc, cc->owner, ast, NULL, raise);
			}
			if (type == NULL) {
				traceAst(ast);
				return NULL;
			}
			ast->type = type;
			return type;
	}

	fatal(ERR_INTERNAL_ERROR": unimplemented: %.t (%T, %T): %t", ast, lType, rType, ast);
	return NULL;
}

symn initCheck(ccContext cc, symn var, int raise) {
	symn type = isTypename(var) ? var : var->type;
	astn ast = var->init;
	if (ast == NULL) {
		// uninitialized variable
		if (raise) {
			warn(cc->rt, 1, var->file, var->line, ERR_UNINITIALIZED_VARIABLE, var);
		}
		return type;
	}

	if (ast->kind != STMT_beg) {
		// expression initialized variable
		type = typeCheck(cc, NULL, ast, raise);
		if (raise && !canAssign(cc, var, ast, 0)) {
			warn(cc->rt, 8, ast->file, ast->line, ERR_INVALID_VALUE_ASSIGN, var, ast);
		}
	}
	else if (isTypename(var)) {
		type = cc->type_fun;
		warn(cc->rt, 1, ast->file, ast->line, ERR_UNINITIALIZED_VARIABLE, var);
	}
	else if (isFunction(var)) {
		type = cc->type_fun;
		warn(cc->rt, 1, ast->file, ast->line, ERR_UNIMPLEMENTED_FUNCTION, var);
	}
	else {
		// literal initialized variable
		for (astn n = ast->stmt.stmt; n != NULL; n = n->next) {
			astn id = NULL, val = n;
			if (n->kind == INIT_set) {
				id = n->op.lhso;
				val = n->op.rhso;
			}
			if (id != NULL) {
				symn fieldRef = lookup(cc, type->fields, id, NULL, 0);
				if (raise && fieldRef == NULL) {
					error(cc->rt, ast->file, ast->line, ERR_INVALID_TYPE, ast);
					fieldRef = cc->type_int;
				}
				initCheck(cc, fieldRef, raise);
			}
			else {
				type = typeCheck(cc, NULL, val, raise);
				if (raise && !canAssign(cc, type, val, 0)) {
					warn(cc->rt, 1, val->file, val->line, ERR_INVALID_VALUE_ASSIGN, type, val);
				}
			}
		}
		// TODO: convert initializer literal to initializer statement list
		fatal("%?s:%?u: "ERR_UNIMPLEMENTED_FEATURE": `%T` := `%+t`", var->file, var->line, var, ast);
	}
	return ast->type = type;
}

ccKind canAssign(ccContext cc, symn var, astn val, int strict) {
	symn typ = isTypename(var) ? var : var->type;
	symn lnk = linkOf(val, 1);
	ccKind varCast;

	dieif(!var, ERR_INTERNAL_ERROR);
	dieif(!val, ERR_INTERNAL_ERROR);

	if (val->type == NULL) {
		return CAST_any;
	}

	varCast = castOf(var);

	// assign null or pass by reference
	if (lnk == cc->null_ref) {
		if (typ != NULL) {
			switch (castOf(typ)) {
				default:
					break;

				case CAST_ref:
					// assign null to a reference type
					return CAST_ref;

				case CAST_arr:
					// assign null to array
					return CAST_arr;

				case CAST_var:
					// assign null to variant
					return CAST_arr;
			}
		}
		switch (varCast) {
			default:
				break;

			case CAST_ref:
			case CAST_var:
			case CAST_arr:
				return varCast;
		}
	}

	// assigning a typename or pass by reference
	if (lnk != NULL && var->type == cc->type_rec) {
		switch (lnk->kind & MASK_kind) {
			default:
				break;

			case KIND_typ:
			// case KIND_fun:
				return CAST_ref;
		}
	}

	// assigning a variable to a pointer or variant
	if (lnk != NULL && (typ == cc->type_ptr || typ == cc->type_var)) {
		return castOf(typ);
	}

	if (typ != var) {
		// assigning a function
		if (var->params != NULL) {
			symn fun = linkOf(val, 1);
			symn arg1 = var->params;
			symn arg2 = NULL;
			struct astNode temp;

			temp.kind = TOKEN_var;
			temp.type = typ;
			temp.ref.link = var;

			if (fun && canAssign(cc, fun->type, &temp, 1)) {
				arg2 = fun->params;
				while (arg1 && arg2) {

					temp.type = arg2->type;
					temp.ref.link = arg2;

					if (!canAssign(cc, arg1, &temp, 1)) {
						trace("%T ~ %T", arg1, arg2);
						break;
					}

					arg1 = arg1->next;
					arg2 = arg2->next;
				}
			}
			else {
				trace("%T ~ %T", typ, fun);
				return CAST_any;
			}
			if (arg1 || arg2) {
				trace("%T ~ %T", arg1, arg2);
				return CAST_any;
			}
			// function is by ref
			return KIND_var;
		}
		if (!strict) {
			strict = varCast == CAST_ref;
		}
	}

	if (typ == val->type) {
		return varCast;
	}

	/* FIXME: assign enum.
	if (lnk && lnk->cast == ENUM_kwd) {
		if (typ == lnk->type->type) {
			return lnk->type->type->cast;
		}
	}
	else if (typ->cast == ENUM_kwd) {
		symn t;
		for (t = typ->fields; t != NULL; t = t->next) {
			if (t == lnk) {
				return lnk->cast;
			}
		}
	}*/

	// Assign array
	if (castOf(typ) == CAST_arr) {
		struct astNode temp;
		symn vty = val->type;

		memset(&temp, 0, sizeof(temp));
		temp.kind = TOKEN_var;
		temp.type = vty ? vty->type : NULL;
		temp.ref.link = NULL;
		temp.ref.name = ".generated token";

		//~ check if subtypes are assignable
		if (canAssign(cc, typ->type, &temp, strict)) {
			// assign to dynamic array
			if (typ->init == NULL) {
				return CAST_arr;
			}
			if (typ->size == val->type->size) {
				// TODO: return <?>
				return KIND_var;
			}
		}

		if (!strict) {
			return canAssign(cc, var->type, val, strict);
		}
	}

	if (!strict && (typ = promote(typ, val->type))) {
		// TODO: return <?> val->cast ?
		return castOf(typ);
	}

	debug("%?s:%?u: %T := %T(%t)", val->file, val->line, var, val->type, val);
	return CAST_any;
}

void addUsage(symn sym, astn tag) {
	if (sym == NULL || tag == NULL) {
		return;
	}
	if (tag->ref.used != NULL) {
#ifdef DEBUGGING	// extra check: if this node is linked (.used) it must be in the list
		astn usage;
		for (usage = sym->use; usage; usage = usage->ref.used) {
			if (usage == tag) {
				break;
			}
		}
		dieif(usage == NULL, ERR_INTERNAL_ERROR);
#endif
		return;
	}
	if (sym->use != tag) {
		tag->ref.used = sym->use;
		sym->use = tag;
	}
}

static inline ccKind castKind(symn typ) {
	if (typ != NULL) {
		switch (castOf(typ)) {
			default:
				break;

			case CAST_vid:
				return CAST_vid;

			case CAST_bit:
				return CAST_bit;

			case CAST_u32:
			case CAST_i32:
				return CAST_i32;

			case CAST_i64:
			case CAST_u64:
				return CAST_i64;

			case CAST_f32:
			case CAST_f64:
				return CAST_f64;

			case KIND_var:
				return KIND_var;
		}
	}
	return CAST_any;
}
// determine the resulting type of a OP b
static symn promote(symn lht, symn rht) {
	symn result = 0;
	if (lht && rht) {
		if (lht == rht) {
			result = lht;
		}
		else switch (castKind(rht)) {
				default:
					break;

				case CAST_i32:
				case CAST_i64:
				case CAST_u32:
				case CAST_u64:
					switch (castKind(lht)) {
						default:
							break;

						case CAST_i32:
						case CAST_i64:
						case CAST_u32:
						case CAST_u64:
							//~ TODO: bool + int is bool; if sizeof(bool) == 4
							if (castOf(lht) == CAST_bit && lht->size == rht->size) {
								result = rht;
							}
							else {
								result = lht->size >= rht->size ? lht : rht;
							}
							break;

						case CAST_f32:
						case CAST_f64:
							result = lht;
							break;

					}
					break;

				case CAST_f32:
				case CAST_f64:
					switch (castKind(lht)) {
						default:
							break;

						case CAST_i32:
						case CAST_i64:
						case CAST_u32:
						case CAST_u64:
							result = rht;
							break;

						case CAST_f32:
						case CAST_f64:
							result = lht->size >= rht->size ? lht : rht;
							break;
					}
					break;
			}
	}
	else if (rht) {
		result = rht;
	}

	return result;
}

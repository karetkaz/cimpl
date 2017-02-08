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

TODO's:
	struct initialization:
		struct Demo {
			// normal member variable
			int32 a;

			// constant member variable, compiler error if not initialized when a new instance is created.
			const int32 b;

			// global variable hidden in this class.
			static int32 c;

			// constant global variable, compiler error if not initialized when declared.
			static const int32 d = 0;
		}
		Demo a = {a: 12, b: 88};
		Demo b = Demo(12, 88);
	array initialization:
		Demo a1[] = {Demo(1,2), Demo(2,3), ...};

*******************************************************************************/

#include "internal.h"

static symn promote(symn lht, symn rht);

symn getResultType(symn sym, astn args);

/// Enter a new scope.
void enter(ccContext cc) {
	cc->nest += 1;
}

/// Leave current scope.
symn leave(ccContext cc, symn owner, ccKind mode, size_t align, size_t *outSize) {
	symn sym, result = NULL;

	cc->nest -= 1;

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
			warn(cc->rt, 1, sym->file, sym->line, WARN_FUNCTION_MARKED_STATIC, sym);
			sym->kind |= ATTR_stat;
		}
	}
	cc->scope = sym;

	// fix padding
	size_t offs = 0, size = 0;
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
					warn(cc->rt, 6, sym->file, sym->line, "padding %T (%d -> %d): %d", sym, offs, sym->offs, sym->offs - offs);
				}
				offs = sym->offs + sym->size;
				if (size < offs) {
					size = offs;
				}
			}
			size_t padded = padOffset(size, align);
			if (align && size != padded) {
				warn(cc->rt, 6, result->file, result->line, "padding %T (%d -> %d): %d", sym, size, padded, padded - size);
				size = padded;
			}
			break;

		case KIND_fun:
			// update offsets
			for (sym = result; sym != NULL; sym = sym->next) {
				if (isStatic(sym)) {
					continue;
				}
				sym->offs = offs;
				offs += padOffset(sym->size, align);
			}
			// arguments are evaluated right to left
			for (sym = result; sym != NULL; sym = sym->next) {
				if (isStatic(sym)) {
					continue;
				}
				sym->offs = offs - sym->offs;
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
	symn def;

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

	if ((def = newDef(cc, kind))) {
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
						error(cc->rt, cc->file, cc->line, ERR_TYPE_TOO_COMPLEX, def);
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
					error(cc->rt, cc->file, cc->line, ERR_TYPE_TOO_COMPLEX, def);
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
	symn asRef = NULL;
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

		// lookup function as reference (without arguments)
		if (parameter != NULL && arguments == NULL) {
			// keep the first match.
			if (sym->kind == KIND_var) {
				if (asRef == NULL) {
					asRef = sym;
				}
				found += 1;
			}
			continue;
		}

		if (arguments != NULL) {
			astn argument = chainArgs(arguments);

			if (parameter != NULL) {
				// first argument starts next to result.
				parameter = parameter->next;
			}

			while (parameter != NULL && argument != NULL) {

				if (!canAssign(cc, parameter, argument, 0)) {
					break;
				}

				else if (!canAssign(cc, parameter, argument, 1)) {
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
			warn(cc->rt, 2, ref->file, ref->line, WARN_BEST_OVERLOAD, best, found);
		}
		sym = best;
	}

	if (sym == NULL && asRef) {
		if (found == 1 || cc->siff) {
			debug("as ref `%T`(%?t)", asRef, arguments);
			sym = asRef;
		}
		else if (raise) {
			error(cc->rt, ref->file, ref->line, ERR_MULTIPLE_OVERLOADS, found, asRef);
		}
	}

	// skip over aliases
	while (sym != NULL) {
		if (!isInline(sym)) {
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
	astn value;
	if (ast->type == type) {
		return type;
	}

	if (ast->type == NULL) {
		ast->type = type;
		return type;
	}

	switch (ast->kind) {
		default:
			fatal(ERR_INTERNAL_ERROR);
			return NULL;

		case OPER_fnc:		// '()'
		case OPER_dot:		// '.'
		case OPER_idx:		// '[]'

		case OPER_adr:		// '&'
		case OPER_pls:		// '+'
		case OPER_mns:		// '-'
		case OPER_cmt:		// '~'
		case OPER_not:		// '!'

		case OPER_add:		// '+'
		case OPER_sub:		// '-'
		case OPER_mul:		// '*'
		case OPER_div:		// '/'
		case OPER_mod:		// '%'

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

		case OPER_all:		// '&&'
		case OPER_any:		// '||'
		case OPER_sel:		// '?:'

		case OPER_com:		// ','

		case ASGN_set:		// '='

		case TOKEN_opc:
		case TOKEN_var:
			ast->type = type;

		case TOKEN_val:
			// wrap the value into cast
			value = newNode(cc, TOKEN_any);
			*value = *ast;
			ast->kind = OPER_fnc;
			ast->op.rhso = value;
			ast->op.lhso = NULL;
			ast->op.test = NULL;
			ast->type = type;
			break;
	}

	(void)cc;
	return type;
}

symn typeCheck(ccContext cc, symn loc, astn ast, int raise) {
	astn ref = NULL, args = NULL;
	astn dot = NULL;
	symn lType, rType, type, sym = 0;

	if (ast == NULL) {
		fatal(ERR_INTERNAL_ERROR);
		return NULL;
	}

	// do not check again
	if (ast->type != NULL) {
		return ast->type;
	}

	switch (ast->kind) {
		default:
			fatal(ERR_INTERNAL_ERROR);
			return NULL;

		case OPER_fnc:
			ref = ast->op.lhso;
			args = ast->op.rhso;

			if (args != NULL) {
				rType = NULL;
				if (linkOf(ref) == cc->emit_opc) {
					rType = cc->emit_opc;
				}
				if (!(rType = typeCheck(cc, rType, args, 0))) {
					rType = typeCheck(cc, rType, args, raise);
				}
				convert(cc, args, rType);
			}
			else {
				rType = cc->type_vid;
			}

			if (!rType) {
				traceAst(ast);
				return NULL;
			}

			// TODO: try to avoid hacks
			if (ref != NULL) {
				switch (ref->kind) {
					default:
						fatal(ERR_INTERNAL_ERROR);
						return NULL;

					case OPER_dot:    // Math.isNan ???
						if (!(loc = typeCheck(cc, loc, ref->op.lhso, raise))) {
							traceAst(ast);
							return NULL;
						}
						dot = ref;
						ref = ref->op.rhso;
						break;

					case TOKEN_var:
						break;
				}
			}
			break;

		case OPER_dot:
			lType = typeCheck(cc, loc, ast->op.lhso, raise);
			rType = typeCheck(cc, lType, ast->op.rhso, raise);

			if (!lType || !rType) {
				traceAst(ast);
				return NULL;
			}
			convert(cc, ast->op.lhso, lType);
			convert(cc, ast->op.rhso, rType);
			return rType;

		case OPER_idx:
			lType = typeCheck(cc, loc, ast->op.lhso, raise);
			rType = typeCheck(cc, loc, ast->op.rhso, raise);

			if (!lType || !rType) {
				traceAst(ast);
				return NULL;
			}
			convert(cc, ast->op.lhso, lType);
			convert(cc, ast->op.rhso, rType);
			// base type of array;
			return lType->type;

		case OPER_adr:		// '&'
		case OPER_pls:		// '+'
		case OPER_mns:		// '-'
		case OPER_cmt:		// '~'
		case OPER_not:		// '!'
			rType = typeCheck(cc, loc, ast->op.rhso, raise);
			if (!rType) {
				traceAst(ast);
				return NULL;
			}

			type = promote(NULL, rType);
			if (type != NULL) {
				convert(cc, ast->op.rhso, type);
				if (ast->kind == OPER_not) {
					type = cc->type_bol;
				}
				return type;
			}
			fatal(FATAL_UNIMPLEMENTED_OPERATOR, ast, NULL, rType, ast);
			break;

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
			if (type != NULL) {
				convert(cc, ast->op.lhso, type);
				convert(cc, ast->op.rhso, type);
				return type;
			}
			fatal(FATAL_UNIMPLEMENTED_OPERATOR, ast, lType, rType, ast);
			break;

		case OPER_shl:		// '>>'
		case OPER_shr:		// '<<'
			lType = typeCheck(cc, loc, ast->op.lhso, raise);
			rType = typeCheck(cc, loc, ast->op.rhso, raise);
			if (!lType || !rType) {
				traceAst(ast);
				return NULL;
			}

			type = promote(lType, rType);
			if (type != NULL) {
				convert(cc, ast->op.lhso, type);
				convert(cc, ast->op.rhso, cc->type_i32);

				switch (castOf(type)) {
					default:
						break;

					case CAST_i32:
					case CAST_i64:

					case CAST_u32:
					case CAST_u64:
						return type;

					case CAST_f32:
					case CAST_f64:
						error(cc->rt, ast->file, ast->line, ERR_INVALID_TYPE, ast);
						return NULL;
				}
			}
			fatal(FATAL_UNIMPLEMENTED_OPERATOR, ast, lType, rType, ast);
			break;
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
			if (type != NULL) {
				convert(cc, ast->op.lhso, type);
				convert(cc, ast->op.rhso, type);

				switch (castOf(type)) {
					default:
						break;

					case CAST_bit:

					case CAST_i32:
					case CAST_i64:

					case CAST_u32:
					case CAST_u64:
						return type;

					case CAST_f32:
					case CAST_f64:
						error(cc->rt, ast->file, ast->line, ERR_INVALID_TYPE, ast);
						return NULL;
				}
			}
			fatal(FATAL_UNIMPLEMENTED_OPERATOR, ast, lType, rType, ast);
			break;

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
			if (ast->kind == OPER_ceq || ast->kind == OPER_cne) {
				symn lLink = linkOf(ast->op.lhso);
				symn rLink = linkOf(ast->op.rhso);

				// compare with null: bool isNull = variable == null;
				if (lLink == cc->null_ref || rLink == cc->null_ref) {
					type = cc->type_ptr;
				}
				// compare with typename: bool isint32 = type == int32;
				else if (rLink == rType && lType == cc->type_rec) {
					type = cc->type_ptr;
				}
				else if (lLink == lType && rType == cc->type_rec) {
					type = cc->type_ptr;
				}
			}
			if (type != NULL) {
				convert(cc, ast->op.lhso, type);
				convert(cc, ast->op.rhso, type);
				return cc->type_bol;
			}

			fatal(FATAL_UNIMPLEMENTED_OPERATOR, ast, lType, rType, ast);
			break;

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
			if (type != NULL) {
				convert(cc, ast->op.test, cc->type_bol);
				convert(cc, ast->op.lhso, type);
				convert(cc, ast->op.rhso, type);
				return type;
			}
			fatal(FATAL_UNIMPLEMENTED_OPERATOR, ast, lType, rType, ast);
			break;

		case OPER_com:	// ','
			lType = typeCheck(cc, loc, ast->op.lhso, raise);
			rType = typeCheck(cc, loc, ast->op.rhso, raise);
			if (!lType || !rType) {
				traceAst(ast);
				return NULL;
			}
			convert(cc, ast->op.lhso, lType);
			convert(cc, ast->op.rhso, rType);
			return cc->type_vid;

		// operator set
		case ASGN_set:		// ':='
			lType = typeCheck(cc, loc, ast->op.lhso, raise);
			rType = typeCheck(cc, NULL, ast->op.rhso, raise);
			sym = linkOf(ast->op.lhso);

			if (!lType || !rType || !sym) {
				traceAst(ast);
				return NULL;
			}

			if (isConst(sym)) {
				error(cc->rt, ast->file, ast->line, ERR_INVALID_CONST_ASSIGN, ast);
			}
			if (!canAssign(cc, lType, ast->op.rhso, 0)) {
				traceAst(ast);
				return NULL;
			}
			convert(cc, ast->op.rhso, lType);
			convert(cc, ast->op.lhso, lType);
			return lType;

		// operator get
		case TOKEN_var:
			ref = ast;
			if (ast->ref.link != NULL) {
				rType = ast->ref.link->type;
				return rType;
			}
			// do lookup
			break;
	}

	if (ref != NULL) {
		symn result = NULL;
		if (loc != NULL) {
			sym = loc->fields;
		}
		else {
			sym = cc->deft[ref->ref.hash];
		}

		if ((sym = lookup(cc, sym, ref, args, 1)) != NULL) {
			result = getResultType(sym, args);

			dieif(ref->kind != TOKEN_var, ERR_INTERNAL_ERROR);
			ref->ref.link = sym;
			ref->type = result;

			addUsage(sym, ref);

			if (dot != NULL) {
				dot->type = result;
			}
		}
		else if (raise) {
			error(cc->rt, ref->file, ref->line, ERR_UNDEFINED_REFERENCE, ref);
		}
		return result;
	}
	return NULL;
}

symn getResultType(symn sym, astn args) {
	if (isInvokable(sym) && args != NULL) {
		// resulting type is the type of result parameter
		return sym->params->type;
	}
	if (isInline(sym) && sym->init != NULL) {
		return sym->init->type;
	}
	if (isTypename(sym)) {
		// resulting type is the symbol itself
		return sym;
	}
	// resulting type is the type of the variable
	return sym->type;

	/*if (isVariable(sym)) {
		return sym->type;
	}
	if (isInline(sym)) {
		return sym->type;
	}
	return sym;*/
}

ccKind canAssign(ccContext cc, symn var, astn val, int strict) {
	symn typ, lnk = linkOf(val);
	ccKind varCast;

	dieif(!var, ERR_INTERNAL_ERROR);
	dieif(!val, ERR_INTERNAL_ERROR);

	if (val->type == NULL) {
		return CAST_any;
	}

	varCast = castOf(var);
	typ = getResultType(var, NULL);

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

	if (typ != var) {

		// assigning a function
		if (var->params != NULL) {
			symn fun = linkOf(val);
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
		else if (!strict) {
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

	trace("%?s:%?u: %T := %T(%t)", val->file, val->line, var, val->type, val);
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

/*******************************************************************************
 *   File: type.c
 *   Date: 2011/06/23
 *   Desc: type system
 *******************************************************************************
 * manage type declarations
 * type-check the syntax tree
 */

#include "internal.h"

static symn promote(symn lht, symn rht);

static symn aliasOf(symn sym) {
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

symn newDef(ccContext cc, ccKind kind) {
	rtContext rt = cc->rt;
	symn def = NULL;

	rt->_beg = padPointer(rt->_beg, vm_mem_align);
	if(rt->_beg >= rt->_end) {
		fatal(ERR_MEMORY_OVERRUN);
		return NULL;
	}

	def = (symn)rt->_beg;
	rt->_beg += sizeof(struct symNode);
	memset(def, 0, sizeof(struct symNode));
	def->kind = kind;

	return def;
}

void enter(ccContext cc, symn owner) {
	cc->nest += 1;
	if (owner != NULL) {
		owner->owner = cc->owner;
		cc->owner = owner;
	}
}

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
	for (int i = 0; i < hashTableSize; i++) {
		for (sym = cc->deft[i]; sym && sym->nest > cc->nest; sym = sym->next) {
		}
		cc->deft[i] = sym;
	}

	// clear from scope stack
	for (sym = cc->scope; sym && sym->nest > cc->nest; sym = sym->scope) {
		sym->kind |= mode & MASK_attr;
		sym->owner = owner;

		// create a virtual and static method
		if (!isStatic(sym) && isFunction(sym)) {
			dieif(sym->init == NULL, ERR_INTERNAL_ERROR);
			dieif(sym->init->kind != STMT_beg, ERR_INTERNAL_ERROR);
			if (owner && isTypename(owner)) {
				symn impl = newDef(cc, 0);
				memcpy(impl, sym, sizeof(struct symNode));

				impl->kind = ATTR_cnst | KIND_var | CAST_ref;
				impl->init = lnkNode(cc, sym);

				sym->kind |= ATTR_stat;
				sym->scope = impl;
			}
		}

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
		logif((kind & (ATTR_stat | ATTR_cnst)) == 0, "symbol `%s` should be declared as static and constant", name);
		kind = ATTR_stat | ATTR_cnst | kind;
	}

	if (size == 0 && (kind & MASK_kind) == KIND_var) {
		size = refSize(type);
	}

	symn def = newDef(cc, kind);
	if (def != NULL) {
		size_t length = strlen(name) + 1;
		unsigned hash = rehash(name, length) % hashTableSize;
		def->name = ccUniqueStr(cc, (char *) name, length, hash);
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

symn lookup(ccContext cc, symn sym, astn ref, astn arguments, ccKind filter, int raise) {
	symn byName = NULL;
	symn best = NULL;
	int found = 0;

	dieif(!ref || ref->kind != TOKEN_var, ERR_INTERNAL_ERROR);

	ccKind filterStat = filter & ATTR_stat;
	ccKind filterCnst = filter & ATTR_cnst;
	ccKind filterKind = filter & MASK_kind;
	ccKind filterCast = filter & MASK_cast;
	for (; sym; sym = sym->next) {
		symn parameter = sym->params;
		int hasCast = 0;

		if (filterStat && (sym->kind & ATTR_stat) != filterStat) {
			continue;
		}
		if (filterCnst && (sym->kind & ATTR_cnst) != filterCnst) {
			continue;
		}
		if (filterKind && (sym->kind & MASK_kind) != filterKind) {
			continue;
		}
		if (filterCast && (sym->kind & MASK_cast) != filterCast) {
			continue;
		}

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

			if (argument != NULL && parameter != NULL) {
				// parameters and arguments can not be assigned
				continue;
			}
			else if (parameter != NULL) {
				// more parameter than arguments
				continue;
			}
			else if (argument != NULL) {
				// more arguments than parameters
				if (isTypename(aliasOf(sym)) && argument == arguments) {
					// type(cast)
				}
				else if (sym == cc->emit_opc) {
					// emit(...)
				}
				else {
					continue;
				}
			}
			// else ok
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

	return aliasOf(sym);
}

/// change the type of a tree node (replace or implicit cast).
static astn convert(ccContext cc, astn ast, symn type) {
	if (type == NULL) {
		return ast;
	}
	if (ast->type == NULL) {
		ast->type = type;
		return ast;
	}
	if (ast->type == type) {
		// no update required.
		return ast;
	}

	warn(cc->rt, 6, ast->file, ast->line, WARN_ADDING_IMPLICIT_CAST, type, ast, ast->type);
	astn value = newNode(cc, OPER_fnc);
	value->file = ast->file;
	value->line = ast->line;
	value->op.rhso = ast;
	value->type = type;
	return value;
}

static symn typeCheckRef(ccContext cc, symn loc, astn ref, astn args, int raise) {
	if (ref == NULL || ref->kind != TOKEN_var) {
		traceAst(ref);
		return NULL;
	}

	symn sym = NULL;
	if (ref->ref.link != NULL) {
		sym = ref->ref.link;
	}
	else if (loc != NULL) {
		if (isVariable(loc)) {
			sym = lookup(cc, loc->type->fields, ref, args, KIND_var, 0);
			loc = loc->type;
		}
		if (sym == NULL && isTypename(loc)) {
			sym = lookup(cc, loc->fields, ref, args, ATTR_stat, 0);
		}
		if (sym == NULL) {
			sym = lookup(cc, loc->fields, ref, args, 0, raise);
		}
	}
	else {
		// first lookup in the current scope
		sym = cc->deft[ref->ref.hash];
		sym = lookup(cc, sym, ref, args, 0, raise);

		// lookup parameters, fields, etc.
		for (loc = cc->owner; loc != NULL; loc = loc->owner) {
			if (sym != NULL && sym->nest > loc->nest) {
				// symbol found: scope is higher than this parameter
				break;
			}
			symn field = lookup(cc, loc->fields, ref, args, 0, 0);
			if (field == NULL) {
				// symbol was not found at this location
				continue;
			}
			sym = field;
		}
	}

	if (sym == NULL) {
		if (raise) {
			error(cc->rt, ref->file, ref->line, ERR_UNDEFINED_DECLARATION, ref);
		}
		return NULL;
	}

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
				if (raise) {
					error(cc->rt, ast->file, ast->line, ERR_INVALID_TYPE, ast);
				}
				traceAst(ast);
				return NULL;
			}
			if (ref == NULL) {
				// int a = (3 + 6);
				rType = typeCheck(cc, linkOf(ref, 1), args, raise);
				ast->op.rhso = convert(cc, args, rType);
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
				if (loc && isVariable(loc)) {
					/* lookup order for a.add(b) => add(a, b)
						1. search for extension method: add(a, b)
						2. search for virtual method: a.add(a, b)
						3. search for static method: T.add(a, b)
					*/

					if (args == cc->void_tag) {
						args = ref->op.lhso;
					}
					else {
						args = argNode(cc, ref->op.lhso, args);
						args->type = cc->type_vid;
					}
					ref = ref->op.rhso;
					ast->op.rhso = args;

					type = typeCheckRef(cc, NULL, ref, args, 0);
					if (type != NULL) {
						ast->op.lhso = ref;
						debug("extension function: %t", ast);
						return ast->type = type;
					}

					type = typeCheckRef(cc, loc, ref, args, raise);
					if (type != NULL) {
						ast->op.lhso->type = type;
						return ast->type = type;
					}

					ast->type = type;
					return type;
				}
				else {
					ref->type = cc->type_fun;
					ref = ref->op.rhso;
				}
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
						ast->ref.hash = rehash(name, len + 1) % hashTableSize;
						ast->ref.name = ccUniqueStr(cc, name, len + 1, ast->ref.hash);
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
			type = typeCheckRef(cc, loc, ref, args, raise);
			if (type == NULL) {
				traceAst(ast);
				return NULL;
			}
			ast->type = type;
			return type;
		}

		case OPER_dot:
			if (loc == cc->emit_opc && ast->op.lhso->kind == TOKEN_var) {
				// Fixme: if we have add.i32 and add was already checked, force lookup
				ast->op.lhso->type = NULL;
				ast->op.lhso->ref.link = NULL;
			}
			lType = typeCheck(cc, loc, ast->op.lhso, raise);
			loc = linkOf(ast->op.lhso, 1);
			if (loc == NULL) {
				traceAst(ast);
				return NULL;
			}
			rType = typeCheck(cc, loc, ast->op.rhso, raise);

			if (!lType || !rType) {
				traceAst(ast);
				return NULL;
			}
			ast->op.lhso = convert(cc, ast->op.lhso, lType);
			ast->op.rhso = convert(cc, ast->op.rhso, rType);
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
			if ((type = promote(NULL, rType)) == NULL) {
				error(cc->rt, ast->file, ast->line, ERR_INVALID_TYPE, ast);
			}
			ast->op.rhso = convert(cc, ast->op.rhso, type);
			ast->type = type;
			return type;

		case OPER_not:		// '!'
			rType = typeCheck(cc, loc, ast->op.rhso, raise);

			if (!rType) {
				traceAst(ast);
				return NULL;
			}
			type = cc->type_bol;
			ast->op.rhso = convert(cc, ast->op.rhso, type);
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
			if ((type = promote(lType, rType)) == NULL) {
				error(cc->rt, ast->file, ast->line, ERR_INVALID_TYPE, ast);
			}
			ast->op.lhso = convert(cc, ast->op.lhso, type);
			ast->op.rhso = convert(cc, ast->op.rhso, type);
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
			if ((type = promote(lType, rType)) == NULL) {
				error(cc->rt, ast->file, ast->line, ERR_INVALID_TYPE, ast);
			}
			ast->op.lhso = convert(cc, ast->op.lhso, type);
			ast->op.rhso = convert(cc, ast->op.rhso, cc->type_i32);

			if (type != NULL) {
				switch (refCast(type)) {
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
			if ((type = promote(lType, rType)) == NULL) {
				error(cc->rt, ast->file, ast->line, ERR_INVALID_TYPE, ast);
			}
			ast->op.lhso = convert(cc, ast->op.lhso, type);
			ast->op.rhso = convert(cc, ast->op.rhso, type);

			if (type != NULL) {
				switch (refCast(type)) {
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
			if ((type = promote(lType, rType)) == NULL) {
				error(cc->rt, ast->file, ast->line, ERR_INVALID_TYPE, ast);
			}
			ast->op.lhso = convert(cc, ast->op.lhso, type);
			ast->op.rhso = convert(cc, ast->op.rhso, type);
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
			ast->op.lhso = convert(cc, ast->op.lhso, type);
			ast->op.rhso = convert(cc, ast->op.rhso, type);
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
			if ((type = promote(lType, rType)) == NULL) {
				error(cc->rt, ast->file, ast->line, ERR_INVALID_TYPE, ast);
			}
			ast->op.test = convert(cc, ast->op.test, cc->type_bol);
			ast->op.lhso = convert(cc, ast->op.lhso, type);
			ast->op.rhso = convert(cc, ast->op.rhso, type);
			ast->type = type;
			return type;

		case OPER_com:	// ','
			lType = typeCheck(cc, loc, ast->op.lhso, raise);
			rType = typeCheck(cc, loc, ast->op.rhso, raise);

			if (!lType || !rType) {
				traceAst(ast);
				return NULL;
			}
			ast->op.lhso = convert(cc, ast->op.lhso, lType);
			ast->op.rhso = convert(cc, ast->op.rhso, rType);
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

			if (isConstVar(ast->op.lhso) && ast->kind != INIT_set) {
				error(cc->rt, ast->file, ast->line, ERR_INVALID_CONST_ASSIGN, ast);
			}
			if (!canAssign(cc, lType, ast->op.rhso, 0)) {
				traceAst(ast);
				return NULL;
			}
			ast->op.rhso = convert(cc, ast->op.rhso, lType);
			ast->type = lType;
			return lType;

		// variable
		case TOKEN_var:
			type = typeCheckRef(cc, loc, ast, NULL, raise);
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

ccKind canAssign(ccContext cc, symn var, astn val, int strict) {
	symn typ = isTypename(var) ? var : var->type;
	symn lnk = linkOf(val, 1);

	dieif(!var, ERR_INTERNAL_ERROR);
	dieif(!val, ERR_INTERNAL_ERROR);

	if (val->type == NULL) {
		return CAST_any;
	}

	ccKind varCast = castOf(var);

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
					return CAST_var;
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
		return refCast(typ);
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
		return refCast(typ);
	}

	debug("%?s:%?u: %T := %T(%t)", val->file, val->line, var, val->type, val);
	return CAST_any;
}

size_t argsSize(symn function) {
	size_t result = 0;
	if (!isFunction(function)) {
		fatal(ERR_INTERNAL_ERROR);
		return 0;
	}
	for (symn prm = function->params; prm; prm = prm->next) {
		if (isInline(prm)) {
			continue;
		}
		size_t offs = prm->offs;
		if (result < offs) {
			result = offs;
		}
	}
	return result;
}

static inline ccKind castKind(symn typ) {
	if (typ == NULL) {
		return CAST_any;
	}
	switch (refCast(typ)) {
		default:
			return CAST_any;

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

		case CAST_ref:
			return CAST_ref;

		case CAST_val:
			return CAST_val;

		case KIND_var:
			return KIND_var;
	}
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

				case CAST_val:
					switch (castKind(lht)) {
						default:
							break;

						case CAST_val:
						case CAST_ref:
							return lht;
					}
					break;

				case CAST_ref:
					switch (castKind(lht)) {
						default:
							break;

						case CAST_val:
						case CAST_ref:
							return rht;
					}
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
							if (refCast(lht) == CAST_bit && lht->size == rht->size) {
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

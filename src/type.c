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
	symn result = NULL;

	dieif(rt->vm.nfc, "Can not create symbols while generating bytecode");
	rt->_beg = padPointer(rt->_beg, vm_mem_align);
	if (rt->_beg >= rt->_end) {
		fatal(ERR_MEMORY_OVERRUN);
		return NULL;
	}

	result = (symn)rt->_beg;
	rt->_beg += sizeof(struct symNode);
	memset(result, 0, sizeof(struct symNode));
	result->kind = kind;

	return result;
}

void enter(ccContext cc, symn owner) {
	dieif(cc->rt->vm.nfc, "Compiler state closed");
	cc->nest += 1;
	if (owner != NULL) {
		owner->owner = cc->owner;
		cc->owner = owner;
	}
}

symn leave(ccContext cc, ccKind mode, size_t align, size_t baseSize, size_t *outSize, symn result) {
	dieif(cc->rt->vm.nfc, "Compiler state closed");
	symn sym;
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
				if (sym->owner != owner) {
					break;
				}
				sym->offs = padOffset(offs, align < sym->size ? align : sym->size);
				if (offs < sym->offs) {
					warn(cc->rt, raise_warn_pad6, sym->file, sym->line, WARN_PADDING_ALIGNMENT, sym, sym->offs - offs, offs, sym->offs);
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
				warn(cc->rt, raise_warn_pad6, file, line, WARN_PADDING_ALIGNMENT, owner, padded - size, size, padded);
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
		def->owner = cc->owner;
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

	astn chainedArgs = chainArgs(arguments);
	if (arguments == cc->void_tag) {
		chainedArgs = NULL;
	}

	for (; sym; sym = sym->next) {

		if (sym->name == NULL || strcmp(sym->name, ref->ref.name) != 0) {
			// exclude anonymous symbols or non matching names
			continue;
		}

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

		// lookup function by name (without arguments)
		symn parameter = sym->params;
		if (parameter != NULL && arguments == NULL) {
			// keep the first match.
			if (byName == NULL) {
				byName = sym;
			}
			found += 1;
			continue;
		}

		int hasCast = 0;
		if (arguments != NULL) {
			astn argument = chainedArgs;

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

				if ((parameter->kind & ATTR_varg) != 0 && argument->kind != PNCT_dot3) {
					dieif(castOf(parameter->type) != CAST_arr, ERR_INTERNAL_ERROR);
					dieif(parameter->next != NULL, ERR_INTERNAL_ERROR);
					argument = argument->next;
					continue;
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
				if ((parameter->kind & ATTR_varg) == 0) {
					continue;
				}
			}
			else if (argument != NULL) {
				// type cast can have one single argument: `int(3.14)`
				if (argument != arguments) {
					continue;
				}
				symn base = aliasOf(sym);

				// type cast must have on the left a typename.
				if (!isTypename(base)) {
					continue;
				}

				// allow emit type cast like: `Complex(emit(...))`
				if (argument->type != cc->emit_opc) {
					// but only for types defined by user
					if (castOf(base) == CAST_val) {
						continue;
					}
				}
			}
		}

		// return first perfect match
		if (hasCast == 0) {
			break;
		}

		// keep first match as the best one
		if (best == NULL) {
			best = sym;
		}

		found += 1;
		// if we are here then sym is found, but it has implicit cast in it
		debug("%?s:%?u: %t(%t) is probably %T", ref->file, ref->line, ref, arguments, sym);
	}

	if (sym == NULL && best) {
		if (found > 1) {
			warn(cc->rt, raise_warn_typ3, ref->file, ref->line, WARN_USING_BEST_OVERLOAD, best, found);
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
	dieif(cc->rt->vm.nfc, "Compiler state closed");
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

	warn(cc->rt, raise_warn_typ6, ast->file, ast->line, WARN_ADDING_IMPLICIT_CAST, type, ast, ast->type);
	astn value = newNode(cc, OPER_fnc);
	value->file = ast->file;
	value->line = ast->line;
	value->op.rhso = ast;
	value->type = type;
	return value;
}

static symn typeCheckRef(ccContext cc, symn loc, astn ref, astn args, int raise) {
	dieif(cc->rt->vm.nfc, "Compiler state closed");
	if (ref == NULL || ref->kind != TOKEN_var) {
		traceAst(ref);
		return NULL;
	}

	symn sym = NULL;
	if (ref->ref.link != NULL) {
		sym = ref->ref.link;
	}
	else if (loc != NULL) {
		if (!isTypename(loc)) {
			// try to lookup members first.
			sym = lookup(cc, loc->type->fields, ref, args, KIND_var, 0);
			if (sym == NULL) {
				// length of fixed size array is an inline static value
				sym = lookup(cc, loc->type->fields, ref, args, 0, 0);
			}
		}

		if (sym == NULL) {
			ccKind kind = isTypename(loc) ? ATTR_stat : 0;
			sym = lookup(cc, loc->fields, ref, args, kind, raise);
			if (sym == NULL) {
				// hack: allow lookup of: vertices[i].x = 2;
				// int this case the location is a type, and x is a member
				sym = lookup(cc, loc->fields, ref, args, 0, 0);
			}
		}
	}
	else {
		// first lookup in the current scope
		sym = cc->deft[ref->ref.hash];
		sym = lookup(cc, sym, ref, args, 0, 0);

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

	symn type = sym->type;
	if (args != NULL) {
		if (isInvokable(sym)) {
			// resulting type is the type of result parameter
			type = sym->params->type;
		}
		if (isTypename(sym)) {
			// resulting type is the type of the cast
			type = sym;
		}
	}
	else if (isInline(sym) && sym->init != NULL && !isInvokable(sym)) {
		// resulting type is the type of the inline expression
		type = sym->init->type;
	}

	if (/*raise && */sym != NULL && args != NULL && isInvokable(sym)) {
		astn arg = chainArgs(args);
		if (args == cc->void_tag) {
			arg = NULL;
		}
		symn prm = sym->params->next;   // skip to first parameter
		if (sym == cc->libc_dbg) {
			prm = prm->next;            // skip hidden param: file
			prm = prm->next;            // skip hidden param: line
		}
		while (arg != NULL && prm != NULL) {
			// reference arguments must be given as: `&var`
			if (arg->kind != OPER_adr && byReference(prm) && !byReference(prm->type) && !isConst(prm)) {
				warn(cc->rt, raise_warn_typ3, arg->file, arg->line, WARN_PASS_ARG_BY_REF, arg, sym);
			}

			if ((prm->kind & ATTR_varg) == ATTR_varg) {
				arg = arg->next;
				continue;
			}
			arg = arg->next;
			prm = prm->next;
		}
		if (prm && (prm->kind & ATTR_varg) == ATTR_varg) {
			prm = prm->next;
		}
		dieif(arg || prm, ERR_INTERNAL_ERROR": %t(%t)[%t => %T]", ref, args, arg, prm);
	}

	// raise error or warning if accessing private symbols
	if (sym && sym->unit && sym->unit != cc->unit && sym->doc == NULL) {
		raiseLevel level = cc->genPrivate ? raiseWarn : raiseError;
		warn(cc->rt, level, ref->file, ref->line, ERR_PRIVATE_DECLARATION, sym);
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
				rType = typeCheck(cc, loc, args, raise);
				ast->op.rhso = convert(cc, args, rType);
				ast->type = rType;
				return rType;
			}
			if (args == NULL) {
				// int a = func();
				args = cc->void_tag;
			}

			if (ref->kind == RECORD_kwd) {
				if (loc != cc->emit_opc) {
					// struct(x) not inside emit
					traceAst(ast);
					return NULL;
				}
				// emit( ..., struct(x), ...)  => emit x by value
				type = typeCheck(cc, NULL, args, 1);
				ast->op.rhso->type = type;
				ast->type = type;
				return type;
			}
			if (ref->kind == TOKEN_var) {
				type = cc->deft[ref->ref.hash];
				type = lookup(cc, type, ref, NULL, 0, 0);

				// typename(identifier): returns null if identifier is not defined.
				if (type == cc->type_rec) {
					typeCheck(cc, loc, args, 0);
					typeCheck(cc, loc, ref, 0);
					ast->type = cc->type_rec;
					return cc->type_rec;
				}

				// emit may contain instructions, but those are hidden in emit (like: emit.sub.i64).
				if (type == cc->emit_opc) {
					// lookup first in current scope, than what failed in emit scope
					typeCheck(cc, loc, args, 0);
					loc = cc->emit_opc;
				}
			}

			// lookup arguments in the current scope
			rType = typeCheck(cc, loc, args, raise);
			if (rType == NULL) {
				traceAst(ast);
				return NULL;
			}

			if (ref->kind == OPER_dot) {    // float32.sin(args)
				if (!typeCheck(cc, loc, ref->op.lhso, raise)) {
					traceAst(ast);
					return NULL;
				}
				loc = linkOf(ref->op.lhso, 1);
				if (loc != NULL && isTypename(loc)) {
					ref->type = cc->type_fun;
					ref = ref->op.rhso;
				}
				else {
					/* lookup order for `a.add(b)`
						1. search for matching method: `a.add(b)`
						2. search for extension method: `add(a, b)`
						3. search for virtual method: `a.add(a, b)`
						4. search for static method: `T.add(a, b)`
					*/

					if (loc == NULL) {
						// in case `a` is an expression use its type
						loc = ref->op.lhso->type;
					}

					// 1. search for the matching method: a.add(b)
					type = typeCheckRef(cc, loc, ref->op.rhso, args, 0);
					if (type != NULL) {
						ref->type = type;
						debug("exact function: %t", ast);
						return ast->type = type;
					}

					astn argThis = ref->op.lhso;
					/* TODO: if (!byReference(argThis->type)) {
						// FIXME: do this after successful lookup: a.add(b) is converted to: add(&a, b)
						argThis = opNode(cc, OPER_adr, NULL, ref->op.lhso);
						argThis->type = argThis->op.rhso->type;
					}*/
					// convert instance to parameter and try to lookup again
					if (args == cc->void_tag) {
						args = argThis;
					}
					else {
						args = argNode(cc, argThis, args);
						args->type = cc->type_vid;
					}
					ref = ref->op.rhso;

					// 2. search for extension method: add(a, b)
					type = typeCheckRef(cc, NULL, ref, args, 0);
					if (type != NULL) {
						ast->op.lhso = ref;
						ast->op.rhso = args;
						debug("extension function: %t", ast);
						return ast->type = type;
					}

					// 3. search for virtual method: a.add(a, b)
					// 4. search for static method: T.add(a, b)
					type = typeCheckRef(cc, loc, ref, args, raise);
					if (type != NULL) {
						ast->op.rhso = args;
						ast->op.lhso->type = type;
						return ast->type = type;
					}

					ast->type = type;
					return type;
				}
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
				// Fixme: if we have add.i32 and add was already checked, force a new lookup
				ast->op.lhso->type = NULL;
				ast->op.lhso->ref.link = NULL;
			}
			lType = typeCheck(cc, loc, ast->op.lhso, raise);
			// if left side is a function or emit, we need to lookup parameters declared for the function
			loc = linkOf(ast->op.lhso, 1);
			if (loc == NULL || isVariable(loc)) {
				loc = lType;
			}
			rType = typeCheck(cc, loc, ast->op.rhso, raise);

			if (!lType || !rType || !loc) {
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
				rType = typeCheck(cc, NULL, ast->op.rhso, raise);
			}

			if (!lType || !rType) {
				traceAst(ast);
				return NULL;
			}
			// base type of array;
			type = lType->type;
			ast->type = type;
			return type;

		case OPER_adr:		// '&'
		case PNCT_dot3:		// '...'
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

			if (!lType || !rType) {
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
	if (var == NULL || val == NULL || val->type == NULL) {
		dieif(!var, ERR_INTERNAL_ERROR);
		dieif(!val, ERR_INTERNAL_ERROR);
		return CAST_any;
	}
	symn varType = isTypename(var) ? var : var->type;
	symn valName = linkOf(val, 1);
	ccKind varCast = castOf(var);

	// assign null or pass by reference
	if (valName == cc->null_ref) {
		if (varType != NULL) {
			switch (castOf(varType)) {
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
	if (valName != NULL && var->type == cc->type_rec) {
		switch (valName->kind & MASK_kind) {
			default:
				break;

			case KIND_typ:
			// case KIND_fun:
				return CAST_ref;
		}
	}
	// assigning a variable to a pointer or variant
	if (valName != NULL && (varType == cc->type_ptr || varType == cc->type_var)) {
		return refCast(varType);
	}

	if (varType != var) {
		// assigning a function
		if (var->params != NULL) {
			symn fun = linkOf(val, 1);
			symn arg1 = var->params;
			symn arg2 = NULL;
			struct astNode temp;

			temp.kind = TOKEN_var;
			temp.type = varType;
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
				trace("%T ~ %T", varType, fun);
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

	// assigning a value to an object or base type
	for (symn sym = val->type; sym != NULL; sym = sym->type) {
		if (sym == varType) {
			return varCast;
		}
		// TODO: there should be a check if object is in the type hierarchy
		if (varCast != CAST_ref || castOf(sym) != CAST_ref) {
			// check base type only if it is a reference type
			break;
		}
		if (isArrayType(sym)) {
			break;
		}
		if (isFunction(sym)) {
			break;
		}
		if (sym == cc->type_rec) {
			// typename is the last type in the chain
			break;
		}
	}

	/* FIXME: assign enum.
	if (valName && valName->cast == ENUM_kwd) {
		if (varType == valName->type->type) {
			return valName->type->type->cast;
		}
	}
	else if (varType->cast == ENUM_kwd) {
		symn t;
		for (t = varType->fields; t != NULL; t = t->next) {
			if (t == valName) {
				return valName->cast;
			}
		}
	}*/

	// Assign array
	if (castOf(varType) == CAST_arr) {
		struct astNode temp;
		symn vty = val->type;

		memset(&temp, 0, sizeof(temp));
		temp.kind = TOKEN_var;
		temp.type = vty ? vty->type : NULL;
		temp.ref.link = NULL;
		temp.ref.name = ".generated token";

		//~ check if subtypes are assignable
		if (canAssign(cc, varType->type, &temp, strict)) {
			// assign to dynamic array
			if (varType->init == NULL) {
				return CAST_arr;
			}
			if (varType->size == val->type->size) {
				// TODO: return <?>
				return KIND_var;
			}
		}

		if (!strict) {
			return canAssign(cc, var->type, val, strict);
		}
	}

	if (!strict && (varType = promote(varType, val->type))) {
		// TODO: return <?> val->cast ?
		return refCast(varType);
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

// determine the resulting type of a OP b
// TODO: remove this, and declare each operator in the language
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

/*******************************************************************************
 *   File: tree.c
 *   Date: 2011/06/23
 *   Desc: tree operations
 *******************************************************************************
 * create, modify and fold syntax tree
 */

#include "internal.h"

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ create and recycle node
astn newNode(ccContext cc, ccToken kind) {
	astn result = cc->tokPool;
	rtContext rt = cc->rt;

	dieif(rt->vm.nfc, "Can not create symbols while generating bytecode");

	if (result == NULL) {
		// allocate memory from temporary storage.
		rt->_end -= sizeof(struct astNode);
		if (rt->_beg >= rt->_end) {
			fatal(ERR_MEMORY_OVERRUN);
			return NULL;
		}
		result = (astn) rt->_end;
	}
	else {
		cc->tokPool = result->next;
	}

	memset(result, 0, sizeof(struct astNode));
	result->kind = kind;
	return result;
}

astn dupNode(ccContext cc, astn node) {
	astn result = newNode(cc, node->kind);
	*result = *node;
	return result;
}

void recycle(ccContext cc, astn ast) {
	dieif(cc->rt->vm.nfc, "Compiler state closed");
	if (ast == NULL) {
		return;
	}
	ast->next = cc->tokPool;
	cc->tokPool = ast;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ make a constant valued node
astn intNode(ccContext cc, int64_t value) {
	astn ast = newNode(cc, TOKEN_val);
	if (ast != NULL) {
		ast->type = cc->type_i64;
		ast->cInt = value;
	}
	return ast;
}

astn fltNode(ccContext cc, float64_t value) {
	astn ast = newNode(cc, TOKEN_val);
	if (ast != NULL) {
		ast->type = cc->type_f64;
		ast->cFlt = value;
	}
	return ast;
}

astn strNode(ccContext cc, char *value) {
	astn ast = newNode(cc, TOKEN_val);
	if (ast != NULL) {
		ast->type = cc->type_str;
		ast->ref.name = value;
		ast->ref.hash = -1;
	}
	return ast;
}

astn lnkNode(ccContext cc, symn ref) {
	astn result = newNode(cc, TOKEN_var);
	if (result != NULL) {
		result->type = isTypename(ref) ? ref : ref->type;
		result->ref.name = ref->name;
		result->ref.link = ref;
		result->ref.hash = -1;
	}
	return result;
}

astn opNode(ccContext cc, ccToken kind, astn lhs, astn rhs) {
	astn result = newNode(cc, kind);
	if (result != NULL) {
		result->op.lhso = lhs;
		result->op.rhso = rhs;
	}
	return result;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ get value of constant node
int bolValue(astn ast) {
	if (ast != NULL && ast->type != NULL && ast->kind == TOKEN_val) {
		switch (refCast(ast->type)) {
			default:
				break;

			case CAST_bit:
			case CAST_i32:
			case CAST_i64:
			case CAST_u32:
			case CAST_u64:
				return ast->cInt != 0;

			case CAST_f32:
			case CAST_f64:
				return ast->cFlt != 0;
		}
	}
	fatal(ERR_INVALID_CONST_EXPR, ast);
	return 0;
}

int64_t intValue(astn ast) {
	if (ast != NULL && ast->type != NULL && ast->kind == TOKEN_val) {
		switch (refCast(ast->type)) {
			default:
				break;

			case CAST_bit:
			case CAST_ref:
			case CAST_i32:
			case CAST_i64:
			case CAST_u32:
			case CAST_u64:
				return (int64_t) ast->cInt;

			case CAST_f32:
			case CAST_f64:
				return (int64_t) ast->cFlt;
		}
	}
	fatal(ERR_INVALID_CONST_EXPR, ast);
	return 0;
}

float64_t fltValue(astn ast) {
	if (ast != NULL && ast->type != NULL && ast->kind == TOKEN_val) {
		switch (refCast(ast->type)) {
			default:
				break;

			case CAST_bit:
			case CAST_i32:
			case CAST_i64:
			case CAST_u32:
			case CAST_u64:
				return (float64_t) ast->cInt;

			case CAST_f32:
			case CAST_f64:
				return (float64_t) ast->cFlt;
		}
	}
	fatal(ERR_INVALID_CONST_EXPR, ast);
	return 0;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ utility functions

ccKind eval(ccContext cc, astn res, astn ast) {
	ccKind cast;
	symn type = NULL;
	struct astNode lhs, rhs;

	if (ast == NULL || ast->type == NULL) {
		fatal(ERR_INTERNAL_ERROR);
		return CAST_any;
	}

	if (res == NULL) {
		// result may be null(not needed)
		res = &rhs;
	}

	type = ast->type;
	switch (castOf(type)) {
		default:
			fatal(ERR_INTERNAL_ERROR);
			return CAST_any;

		case CAST_bit:
			cast = CAST_bit;
			break;

		case CAST_i32:
		case CAST_u32:
		case CAST_i64:
		case CAST_u64:
			cast = CAST_i64;
			break;

		case CAST_f32:
		case CAST_f64:
			cast = CAST_f64;
			break;

		case CAST_ref:
			cast = CAST_ref;
			break;

		case CAST_val:
		case CAST_arr:
		case CAST_var:
		case CAST_vid:
			debug("not evaluable: %t", ast);
			return CAST_any;
	}

	memset(res, 0, sizeof(struct astNode));
	switch (ast->kind) {
		default:
			fatal(ERR_INTERNAL_ERROR);
			return CAST_any;

		case OPER_com:
		case STMT_beg:
		case TOKEN_opc:
			return CAST_any;

		case OPER_dot:
			// array.length is constant for fixed size arrays
			if (isArrayType(ast->op.lhso->type)) {
				symn arr = linkOf(ast->op.lhso, 0);
				if (!arr || castOf(arr) != CAST_val) {
					// not a fixed size array
					return CAST_any;
				}
				symn len = linkOf(ast->op.rhso, 0);
				if (!len || !isInline(len)) {
					// length must be an inline expression
					return CAST_any;
				}
				if (!isStatic(len) || !isConst(len)) {
					// extra caution, const and static should be also set for length
					return CAST_any;
				}
				return eval(cc, res, len->init);
			}
			if (!isTypeExpr(ast->op.lhso)) {
				return CAST_any;
			}
			return eval(cc, res, ast->op.rhso);

		case OPER_fnc: {
			astn func = ast->op.lhso;
			astn args = ast->op.rhso;

			// evaluate only: float(3) or (3 + 4).
			if (func && !isTypeExpr(func)) {
				return CAST_any;
			}
			// cast must convert something to the given type.
			if (args == NULL) {
				return CAST_any;
			}

			// typename(int64) => typename
			if (linkOf(func, 1) == cc->type_rec) {
				if (args == NULL) {
					traceAst(ast);
					return CAST_any;
				}
				if (args->kind == OPER_dot) {
					if (!isTypeExpr(args->op.lhso)) {
						traceAst(ast);
						return CAST_any;
					}
					args = args->op.rhso;
				}
				if (args->kind == TOKEN_var) {
					symn id = args->ref.link;
					res->kind = TOKEN_val;
					res->type = cc->type_rec;
					res->cInt = id && id->type ? id->type->offs : 0;
					break;
				}
				traceAst(ast);
				return CAST_any;
			}
			ccKind cast = eval(cc, res, args);
			if (cast == CAST_any) {
				traceAst(ast);
				return CAST_any;
			}
			break;
		}

		case OPER_adr:
		case OPER_idx:
			return CAST_any;

		case TOKEN_val:
			*res = *ast;
			break;

		case TOKEN_var: {
			symn var = ast->ref.link;		// link
			if (isInline(var)) {
				if (!eval(cc, res, var->init)) {
					return CAST_any;
				}
				cast = refCast(type);
			}
			else if (isTypename(var)) {
				cast = refCast(type);
				type = cc->type_rec;
				res->kind = TOKEN_val;
				res->cInt = var->offs;
			}
			else {
				return CAST_any;
			}
			break;
		}

		case OPER_pls:		// '+'
			if (!eval(cc, res, ast->op.rhso)) {
				return CAST_any;
			}
			break;

		case OPER_mns:		// '-'
			if (!eval(cc, res, ast->op.rhso)) {
				return CAST_any;
			}

			dieif(res->kind != TOKEN_val, ERR_INTERNAL_ERROR);
			switch (refCast(res->type)) {
				default:
					return CAST_any;

				case CAST_i32:
				case CAST_i64:
				case CAST_u32:
				case CAST_u64:
					res->cInt = -res->cInt;
					break;

				case CAST_f32:
				case CAST_f64:
					res->cFlt = -res->cFlt;
					break;
			}
			break;

		case OPER_cmt:			// '~'
			if (!eval(cc, res, ast->op.rhso)) {
				return CAST_any;
			}

			dieif(res->kind != TOKEN_val, ERR_INTERNAL_ERROR);
			switch (refCast(res->type)) {
				default:
					return CAST_any;

				case CAST_i32:
				case CAST_i64:
				case CAST_u32:
				case CAST_u64:
					res->cInt = ~res->cInt;
					break;
			}
			break;

		case OPER_not:			// '!'
			if (!eval(cc, res, ast->op.rhso)) {
				return CAST_any;
			}

			dieif(res->kind != TOKEN_val, ERR_INTERNAL_ERROR);
			switch (refCast(res->type)) {
				default:
					return CAST_any;

				case CAST_bit:
				case CAST_i32:
				case CAST_i64:
				case CAST_u32:
				case CAST_u64:
				case CAST_ref:
					res->type = cc->type_bol;
					res->cInt = !res->cInt;
					break;

				case CAST_f32:
				case CAST_f64:
					res->type = cc->type_bol;
					res->cInt = !res->cFlt;
					break;
			}
			break;

		case OPER_add:			// '+'
		case OPER_sub:			// '-'
		case OPER_mul:			// '*'
		case OPER_div:			// '/'
		case OPER_mod:			// '%'
			if (!eval(cc, &lhs, ast->op.lhso)) {
				return CAST_any;
			}

			if (!eval(cc, &rhs, ast->op.rhso)) {
				return CAST_any;
			}

			dieif(lhs.kind != TOKEN_val, ERR_INTERNAL_ERROR);
			dieif(rhs.kind != TOKEN_val, ERR_INTERNAL_ERROR);
			logif(lhs.type != rhs.type, ERR_INTERNAL_ERROR": %t", ast);

			res->kind = TOKEN_val;
			switch (refCast(lhs.type)) {
				default:
					return CAST_any;

				case CAST_i32:
				case CAST_i64:
				case CAST_u32:
				case CAST_u64:
					res->type = cc->type_i64;
					switch (ast->kind) {
						default:
							fatal(ERR_INTERNAL_ERROR);
							return CAST_any;

						case OPER_add:
							res->cInt = lhs.cInt + rhs.cInt;
							break;

						case OPER_sub:
							res->cInt = lhs.cInt - rhs.cInt;
							break;

						case OPER_mul:
							res->cInt = lhs.cInt * rhs.cInt;
							break;

						case OPER_div:
							if (rhs.cInt == 0) {
								error(cc->rt, ast->file, ast->line, "Division by zero: %t", ast);
								res->cInt = 0;
								break;
							}
							res->cInt = lhs.cInt / rhs.cInt;
							break;

						case OPER_mod:
							if (rhs.cInt == 0) {
								error(cc->rt, ast->file, ast->line, "Division by zero: %t", ast);
								res->cInt = 0;
								break;
							}
							res->cInt = lhs.cInt % rhs.cInt;
							break;
					}
					break;

				case CAST_f32:
				case CAST_f64:
					res->type = cc->type_f64;
					switch (ast->kind) {
						default:
							fatal(ERR_INTERNAL_ERROR);
							return CAST_any;

						case OPER_add:
							res->cFlt = lhs.cFlt + rhs.cFlt;
							break;

						case OPER_sub:
							res->cFlt = lhs.cFlt - rhs.cFlt;
							break;

						case OPER_mul:
							res->cFlt = lhs.cFlt * rhs.cFlt;
							break;

						case OPER_div:
							res->cFlt = lhs.cFlt / rhs.cFlt;
							break;

						/* FIXME add floating modulus
						case OPER_mod:
							res->cFlt = fmod(lhs.cFlt, rhs.cFlt);
							break;
						*/
					}
					break;
			}
			break;

		case OPER_ceq:			// '=='
		case OPER_cne:			// '!='
		case OPER_clt:			// '<'
		case OPER_cle:			// '<='
		case OPER_cgt:			// '>'
		case OPER_cge:			// '>='
			if (!eval(cc, &lhs, ast->op.lhso)) {
				return CAST_any;
			}

			if (!eval(cc, &rhs, ast->op.rhso)) {
				return CAST_any;
			}

			dieif(lhs.kind != TOKEN_val, ERR_INTERNAL_ERROR);
			dieif(rhs.kind != TOKEN_val, ERR_INTERNAL_ERROR);
			dieif(ast->type != cc->type_bol, ERR_INTERNAL_ERROR);
			logif(lhs.type != rhs.type, "%?s:%?u", ast->file, ast->line);	// might happen: (typename(int) == null)

			res->kind = TOKEN_val;
			res->type = ast->type;

			switch (refCast(rhs.type)) {
				default:
					fatal(ERR_INTERNAL_ERROR);
					res->kind = TOKEN_any;
					return CAST_any;

				case CAST_bit:
				case CAST_i32:
				case CAST_i64:
				case CAST_u32:
				case CAST_u64:
				case CAST_ref:
					switch (ast->kind) {
						default:
							fatal(ERR_INTERNAL_ERROR);
							res->kind = TOKEN_any;
							return CAST_any;

						case OPER_ceq:
							res->cInt = lhs.cInt == rhs.cInt;
							break;

						case OPER_cne:
							res->cInt = lhs.cInt != rhs.cInt;
							break;

						case OPER_clt:
							// FIXME: signed or unsigned compare
							res->cInt = lhs.cInt  < rhs.cInt;
							break;

						case OPER_cle:
							// FIXME: signed or unsigned compare
							res->cInt = lhs.cInt <= rhs.cInt;
							break;

						case OPER_cgt:
							// FIXME: signed or unsigned compare
							res->cInt = lhs.cInt  > rhs.cInt;
							break;

						case OPER_cge:
							// FIXME: signed or unsigned compare
							res->cInt = lhs.cInt >= rhs.cInt;
							break;
					}
					break;

				case CAST_f32:
				case CAST_f64:
					switch (ast->kind) {
						default:
							fatal(ERR_INTERNAL_ERROR);
							res->kind = TOKEN_any;
							return CAST_any;

						case OPER_ceq:
							res->cInt = lhs.cFlt == rhs.cFlt;
							break;

						case OPER_cne:
							res->cInt = lhs.cFlt != rhs.cFlt;
							break;

						case OPER_clt:
							res->cInt = lhs.cFlt < rhs.cFlt;
							break;

						case OPER_cle:
							res->cInt = lhs.cFlt <= rhs.cFlt;
							break;

						case OPER_cgt:
							res->cInt = lhs.cFlt > rhs.cFlt;
							break;

						case OPER_cge:
							res->cInt = lhs.cFlt >= rhs.cFlt;
							break;
					}
					break;
			}
			break;

		case OPER_shr:			// '>>'
		case OPER_shl:			// '<<'
		case OPER_and:			// '&'
		case OPER_ior:			// '|'
		case OPER_xor:			// '^'
			if (!eval(cc, &lhs, ast->op.lhso)) {
				return CAST_any;
			}

			if (!eval(cc, &rhs, ast->op.rhso)) {
				return CAST_any;
			}

			dieif(lhs.kind != TOKEN_val, ERR_INTERNAL_ERROR);
			dieif(rhs.kind != TOKEN_val, ERR_INTERNAL_ERROR);
			dieif(lhs.type != rhs.type, ERR_INTERNAL_ERROR);
			dieif(ast->type != cc->type_i32
				&& ast->type != cc->type_i64
				&& ast->type != cc->type_u32
				&& ast->type != cc->type_u64
			, ERR_INTERNAL_ERROR);

			res->kind = TOKEN_val;
			res->type = ast->type;
			switch (refCast(lhs.type)) {
				default:
					fatal(ERR_INTERNAL_ERROR);
					res->kind = TOKEN_any;
					return CAST_any;

				case CAST_bit:
				case CAST_i32:
				case CAST_i64:
				case CAST_u32:
				case CAST_u64:
					switch (ast->kind) {
						default:
							fatal(ERR_INTERNAL_ERROR);
							res->kind = TOKEN_any;
							return CAST_any;

						case OPER_shr:
							res->cInt = lhs.cInt >> rhs.cInt;
							break;

						case OPER_shl:
							// FIXME: signed or unsigned shift
							res->cInt = lhs.cInt << rhs.cInt;
							break;

						case OPER_and:
							res->cInt = lhs.cInt & rhs.cInt;
							break;

						case OPER_xor:
							res->cInt = lhs.cInt ^ rhs.cInt;
							break;

						case OPER_ior:
							res->cInt = lhs.cInt | rhs.cInt;
							break;
					}
					break;
			}
			break;

		case OPER_all:
		case OPER_any:
			if (!eval(cc, &lhs, ast->op.lhso)) {
				return CAST_any;
			}

			if (!eval(cc, &rhs, ast->op.rhso)) {
				return CAST_any;
			}

			dieif(lhs.kind != TOKEN_val, ERR_INTERNAL_ERROR);
			dieif(rhs.kind != TOKEN_val, ERR_INTERNAL_ERROR);
			dieif(lhs.type != cc->type_bol, ERR_INTERNAL_ERROR);
			dieif(rhs.type != cc->type_bol, ERR_INTERNAL_ERROR);
			dieif(ast->type != cc->type_bol, ERR_INTERNAL_ERROR);

			// TODO: try reconsidering these operators to behave like in lua or js
			res->kind = TOKEN_val;
			res->type = ast->type;
			switch (ast->kind) {
				default:
					fatal(ERR_INTERNAL_ERROR);
					res->kind = TOKEN_any;
					return CAST_any;

				case OPER_any:
					res->cInt = lhs.cInt || rhs.cInt;
					break;

				case OPER_all:
					res->cInt = lhs.cInt && rhs.cInt;
					break;
			}
			break;

		case OPER_sel:
			if (!eval(cc, &lhs, ast->op.test)) {
				return CAST_any;
			}

			dieif(lhs.kind != TOKEN_val, ERR_INTERNAL_ERROR);
			return eval(cc, res, bolValue(&lhs) ? ast->op.lhso : ast->op.rhso);

		case INIT_set:
		case ASGN_set:
			return CAST_any;
	}

	if (res->type != NULL && cast != refCast(res->type)) {
		res->kind = TOKEN_val;
		switch (cast) {
			default:
				fatal(ERR_INTERNAL_ERROR);
				return CAST_any;

			case CAST_bit:
				res->cInt = bolValue(res);
				res->type = type;
				break;

			case CAST_ref:
			case CAST_i64:
				res->cInt = intValue(res);
				res->type = type;
				break;

			case CAST_f64:
				res->cFlt = fltValue(res);
				res->type = type;
				break;
		}
	}

	switch (res->kind) {
		default:
			fatal(ERR_INTERNAL_ERROR": %t", res);
			res->type = NULL;
			return CAST_any;

		case TOKEN_val:
			res->type = type;
			break;
	}

	return cast;
}

symn linkOf(astn ast, int follow) {
	if (ast == NULL) {
		return NULL;
	}

	if (ast->kind == OPER_fnc) {
		if (ast->op.lhso == NULL) {
			// (rand(9.7)) => rand
			return linkOf(ast->op.rhso, follow);
		}
		// rand(9.7) => rand
		//return linkOf(ast->op.lhso);
	}

	if (ast->kind == OPER_dot) {
		// int.size => size
		return linkOf(ast->op.rhso, follow);
	}

	if (ast->kind == OPER_adr) {
		// &buff => buff
		return linkOf(ast->op.rhso, 0);
	}

	if (ast->kind == TOKEN_var) {
		// TODO: do we need to skip over aliases
		symn lnk = ast->ref.link;
		if (follow) {
			while (lnk != NULL) {
				if (lnk->kind != KIND_def) {
					break;
				}
				if (lnk->init == NULL || lnk->init->kind != TOKEN_var) {
					break;
				}
				lnk = linkOf(lnk->init, follow);
			}
		}
		return lnk;
	}

	return NULL;
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
		logif(usage == NULL, ERR_INTERNAL_ERROR);
#endif
		return;
	}
	if (sym->use != tag) {
		tag->ref.used = sym->use;
		sym->use = tag;
	}
}

int isTypeExpr(astn ast) {
	if (ast == NULL) {
		return 0;
	}

	switch (ast->kind) {
		default:
			break;

		case OPER_dot:
			return isTypeExpr(ast->op.lhso) && isTypeExpr(ast->op.rhso);

		case TOKEN_var:
			return ast->ref.link && isTypename(ast->ref.link);
	}

	return 0;
}

static int isConstAst(astn ast, int varOnly) {
	if (ast == NULL) {
		return 0;
	}

	switch (ast->kind) {
		default:
			break;

		case OPER_idx:
			return isConstAst(ast->op.lhso, 0);

		case OPER_dot:
			return isConstAst(ast->op.rhso, 0) || isConstAst(ast->op.lhso, 1);

		case TOKEN_var:
			if (ast->ref.link != NULL) {
				symn link = ast->ref.link;
				switch (link->kind & MASK_kind) {
					case KIND_def:
					case KIND_typ:
					case KIND_fun:
						if (varOnly) {
							break;
						}
						// fall through
					case KIND_var:
						return isConst(link);
				}
			}
			break;
	}

	return 0;
}
int isConstVar(astn ast) {
	return isConstAst(ast, 0);
}

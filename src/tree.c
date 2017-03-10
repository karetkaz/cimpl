/*******************************************************************************
 *   File: tree.c
 *   Date: 2011/06/23
 *   Desc: tree operations
 *******************************************************************************
source code representation using abstract syntax tree
*******************************************************************************/
#include <string.h>
#include "internal.h"

symn newDef(ccContext cc, ccKind kind) {
	rtContext rt = cc->rt;
	symn def = NULL;

	rt->_beg = padPointer(rt->_beg, pad_size);
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

astn newNode(ccContext cc, ccToken kind) {
	rtContext rt = cc->rt;
	astn ast = 0;
	if (cc->tokPool) {
		ast = cc->tokPool;
		cc->tokPool = ast->next;
	}

	// allocate memory from temporary storage.
	rt->_end -= sizeof(struct astNode);
	if (rt->_beg >= rt->_end) {
		fatal(ERR_MEMORY_OVERRUN);
		return NULL;
	}

	ast = (astn)rt->_end;
	memset(ast, 0, sizeof(struct astNode));
	ast->kind = kind;
	return ast;
}

void recycle(ccContext cc, astn ast) {
	if (!ast) return;
	ast->next = cc->tokPool;
	cc->tokPool = ast;
}

/// make a constant valued node
astn intNode(ccContext cc, int64_t v) {
	astn ast = newNode(cc, TOKEN_val);
	if (ast != NULL) {
		ast->type = cc->type_i64;
		ast->cInt = v;
	}
	return ast;
}
astn fltNode(ccContext cc, float64_t v) {
	astn ast = newNode(cc, TOKEN_val);
	if (ast != NULL) {
		ast->type = cc->type_f64;
		ast->cFlt = v;
	}
	return ast;
}
astn strNode(ccContext cc, char *v) {
	astn ast = newNode(cc, TOKEN_val);
	if (ast != NULL) {
		ast->type = cc->type_str;
		ast->ref.hash = -1;
		ast->ref.name = v;
	}
	return ast;
}
astn lnkNode(ccContext cc, symn ref) {
	astn result = newNode(cc, TOKEN_var);
	if (result != NULL) {
		result->type = ref->kind == KIND_var ? ref->type : ref;
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

/// get value of constant node
int bolValue(astn ast) {
	if (ast != NULL && ast->type != NULL && ast->kind == TOKEN_val) {
		switch (castOf(ast->type)) {
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
		switch (castOf(ast->type)) {
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
		switch (castOf(ast->type)) {
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

			// typename(int64) => &int64
			if (linkOf(func) == cc->type_rec) {
				if (args && args->kind == TOKEN_var) {
					symn id = args->ref.link;
					res->kind = TOKEN_val;
					res->type = cc->type_rec;
					res->cInt = id ? id->offs : 0;
					break;
				}
			}
			return eval(cc, res, args);
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
				cast = castOf(type);
			}
			else if (isTypename(var)) {
				cast = castOf(type);
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
			switch (castOf(res->type)) {
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
			switch (castOf(res->type)) {
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
			switch (castOf(res->type)) {
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
			dieif(lhs.type != rhs.type, ERR_INTERNAL_ERROR);

			switch (castOf(lhs.type)) {
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
								error(NULL, NULL, 0, "Division by zero: %t", ast);
								res->cInt = 0;
								break;
							}
							res->cInt = lhs.cInt / rhs.cInt;
							break;

						case OPER_mod:
							if (rhs.cInt == 0) {
								error(NULL, NULL, 0, "Division by zero: %t", ast);
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
			dieif(lhs.type != rhs.type, ERR_INTERNAL_ERROR);
			dieif(ast->type != cc->type_bol, ERR_INTERNAL_ERROR);

			res->kind = TOKEN_val;
			res->type = ast->type;

			switch (castOf(rhs.type)) {
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
			switch (castOf(lhs.type)) {
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

		case ASGN_set:
			return CAST_any;
	}

	if (res->type != NULL && cast != castOf(res->type)) {
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

symn linkOf(astn ast) {
	if (ast == NULL) {
		return NULL;
	}

	if (ast->kind == OPER_fnc) {
		if (ast->op.lhso == NULL) {
			// (rand(9.7)) => rand
			return linkOf(ast->op.rhso);
		}
		// rand(9.7) => rand
		//return linkOf(ast->op.lhso);
	}

	if (ast->kind == OPER_idx) {
		// buff[200] => buff
		return linkOf(ast->op.lhso);
	}

	if (ast->kind == OPER_dot) {
		// int.size => size
		return linkOf(ast->op.rhso);
	}

	if (ast->kind == OPER_adr) {
		// &buff => buff
		return linkOf(ast->op.rhso);
	}

	if (ast->kind == TOKEN_var) {
		// TODO: do we need to skip over aliases
		symn lnk = ast->ref.link;
		while (lnk != NULL) {
			if (lnk->kind != KIND_def) {
				break;
			}
			if (lnk->init == NULL || lnk->init->kind != TOKEN_var) {
				break;
			}
			lnk = linkOf(lnk->init);
		}
		return lnk;
	}

	return NULL;
}

int isTypeExpr(astn ast) {
	if (ast == NULL) {
		return 0;
	}

	if (ast->kind == TOKEN_var) {
		return isTypename(ast->ref.link);
	}

	if (ast->kind == OPER_dot) {
		if (isTypeExpr(ast->op.lhso)) {
			return isTypeExpr(ast->op.rhso);
		}
		return 0;
	}

	return 0;
}

/*******************************************************************************
 *   File: tree.c
 *   Date: 2011/06/23
 *   Desc: tree operations
 *******************************************************************************
source code representation using abstract syntax tree
*******************************************************************************/
#include <string.h>
#include "internal.h"

symn newDefn(ccContext cc, ccKind kind) {
	rtContext rt = cc->rt;
	symn def = NULL;

	rt->_beg = paddptr(rt->_beg, 8);
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
	if (cc->tokp) {
		ast = cc->tokp;
		cc->tokp = ast->next;
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
	ast->next = cc->tokp;
	cc->tokp = ast;
}

/// make a constant valued node
astn intNode(ccContext cc, int64_t v) {
	astn ast = newNode(cc, TOKEN_val);
	if (ast != NULL) {
		ast->type = cc->type_i64;
		ast->cint = v;
	}
	return ast;
}
astn fltNode(ccContext cc, float64_t v) {
	astn ast = newNode(cc, TOKEN_val);
	if (ast != NULL) {
		ast->type = cc->type_f64;
		ast->cflt = v;
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
			case CAST_bit:
			case CAST_i32:
			case CAST_i64:
			case CAST_u32:
			case CAST_u64:
				return ast->cint != 0;
			case CAST_f32:
			case CAST_f64:
				return ast->cflt != 0;
			default:
				break;
		}
	}
	fatal(ERR_INVALID_CONST_EXPR, ast);
	return 0;
}
int64_t intValue(astn ast) {
	if (ast != NULL && ast->type != NULL && ast->kind == TOKEN_val) {
		switch (castOf(ast->type)) {
			case CAST_bit:
			case CAST_i32:
			case CAST_i64:
			case CAST_u32:
			case CAST_u64:
				return (int64_t) ast->cint;
			case CAST_f32:
			case CAST_f64:
				return (int64_t) ast->cflt;
			default:
				break;
		}
	}
	fatal(ERR_INVALID_CONST_EXPR, ast);
	return 0;
}
float64_t fltValue(astn ast) {
	if (ast != NULL && ast->type != NULL && ast->kind == TOKEN_val) {
		switch (castOf(ast->type)) {
			case CAST_bit:
			case CAST_i32:
			case CAST_i64:
			case CAST_u32:
			case CAST_u64:
				return (float64_t) ast->cint;
			case CAST_f32:
			case CAST_f64:
				return (float64_t) ast->cflt;
			default:
				break;
		}
	}
	fatal(ERR_INVALID_CONST_EXPR, ast);
	return 0;
}
ccKind eval(astn res, astn ast) {
	ccKind cast;
	symn type = NULL;
	struct astNode lhs, rhs;

	if (ast == NULL) {
		fatal(ERR_INTERNAL_ERROR);
		return TYPE_any;
	}

	if (ast->type == NULL) {
		fatal(ERR_INTERNAL_ERROR);
		return TYPE_any;
	}

	if (res == NULL) {
		// result is not needed
		res = &rhs;
	}

	type = ast->type;
	switch (castOf(type)) {
		default:
			fatal(ERR_INTERNAL_ERROR);
			return TYPE_any;

		case TYPE_any:
			// this could be an alias to a constant
			// TODO: cast to type->cast ?
			cast = TYPE_any;
			break;

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

		case CAST_val:
		case CAST_ref:
		case CAST_arr:
		case CAST_var:
		case CAST_vid:
			return TYPE_any;
	}

	memset(res, 0, sizeof(struct astNode));
	switch (ast->kind) {
		default:
			fatal(ERR_INTERNAL_ERROR);
			return TYPE_any;

		case OPER_com:
		case STMT_beg:
			return TYPE_any;

		case OPER_dot:
			if (!isTypeExpr(ast->op.lhso))
				return TYPE_any;
			return eval(res, ast->op.rhso);

		case OPER_fnc: {
			astn func = ast->op.lhso;

			// evaluate only type casts.
			if (func && !isTypeExpr(func))
				return TYPE_any;

			if (!eval(res, ast->op.rhso))
				return TYPE_any;

		} break;
		case OPER_adr:
		case OPER_idx:
			return TYPE_any;

		case TOKEN_val:
			*res = *ast;
			break;

		case TOKEN_var: {
			symn var = ast->ref.link;		// link
			if (var && var->kind == KIND_def && var->init) {
				type = var->type;
				if (!eval(res, var->init))
					return TYPE_any;
			}
			else {
				return TYPE_any;
			}
		} break;

		case OPER_pls:		// '+'
			if (!eval(res, ast->op.rhso))
				return TYPE_any;
			break;

		case OPER_mns:		// '-'
			if (!eval(res, ast->op.rhso))
				return TYPE_any;

			dieif(res->kind != TOKEN_val, ERR_INTERNAL_ERROR);
			switch (castOf(res->type)) {
				default:
					return TYPE_any;
				case CAST_i32:
				case CAST_i64:
				case CAST_u32:
				case CAST_u64:
					res->cint = -res->cint;
					break;
				case CAST_f32:
				case CAST_f64:
					res->cflt = -res->cflt;
					break;
			}
			break;

		case OPER_cmt:			// '~'
			if (!eval(res, ast->op.rhso))
				return TYPE_any;

			dieif(res->kind != TOKEN_val, ERR_INTERNAL_ERROR);
			switch (castOf(res->type)) {
				default:
					return TYPE_any;
				case CAST_i32:
				case CAST_i64:
				case CAST_u32:
				case CAST_u64:
					res->cint = ~res->cint;
					break;
			}
			break;

		case OPER_not:			// '!'
			if (!eval(res, ast->op.rhso))
				return TYPE_any;

			dieif(res->kind != TOKEN_val, ERR_INTERNAL_ERROR);
			switch (castOf(res->type)) {
				default:
					return TYPE_any;
				/* TODO: no compiler context
				case CAST_i64:
					res->cint = !res->cint;
					res->type = cc->type_i64;
					break;
				case CAST_f64:
					res->cint = !res->cflt;
					res->type = cc->type_i64;
					break;
				*/
			}
			break;

		case OPER_add:			// '+'
		case OPER_sub:			// '-'
		case OPER_mul:			// '*'
		case OPER_div:			// '/'
		case OPER_mod:			// '%'
			if (!eval(&lhs, ast->op.lhso))
				return TYPE_any;

			if (!eval(&rhs, ast->op.rhso))
				return TYPE_any;

			dieif(lhs.kind != TOKEN_val, ERR_INTERNAL_ERROR);
			dieif(rhs.kind != TOKEN_val, ERR_INTERNAL_ERROR);
			dieif(lhs.type != rhs.type, ERR_INTERNAL_ERROR);

			/*
			switch (rhs.cast) {
				default:
					return TYPE_any;

				case CAST_i64:
					res->cast = CAST_i64;
					switch (ast->kind) {
						default:
							fatal(ERR_INTERNAL_ERROR);
							return TYPE_any;

						case OPER_add:
							res->cint = lhs.cint + rhs.cint;
							break;

						case OPER_sub:
							res->cint = lhs.cint - rhs.cint;
							break;

						case OPER_mul:
							res->cint = lhs.cint * rhs.cint;
							break;

						case OPER_div:
							if (rhs.cint == 0) {
								error(NULL, NULL, 0, "Division by zero: %t", ast);
								res->cint = 0;
								break;
							}
							res->cint = lhs.cint / rhs.cint;
							break;

						case OPER_mod:
							if (rhs.cint == 0) {
								error(NULL, NULL, 0, "Division by zero: %t", ast);
								res->cint = 0;
								break;
							}
							res->cint = lhs.cint % rhs.cint;
							break;
					}
					break;

				case CAST_f64:
					res->cast = CAST_f64;
					switch (ast->kind) {
						default:
							fatal(ERR_INTERNAL_ERROR);
							return TYPE_any;

						case OPER_add:
							res->cflt = lhs.cflt + rhs.cflt;
							break;

						case OPER_sub:
							res->cflt = lhs.cflt - rhs.cflt;
							break;

						case OPER_mul:
							res->cflt = lhs.cflt * rhs.cflt;
							break;

						case OPER_div:
							res->cflt = lhs.cflt / rhs.cflt;
							break;

						case OPER_mod:
							res->cflt = fmod(lhs.cflt, rhs.cflt);
							break;
					}
					break;
			}
			*/
			break;

		case OPER_ceq:			// '=='
		case OPER_cne:			// '!='
		case OPER_clt:			// '<'
		case OPER_cle:			// '<='
		case OPER_cgt:			// '>'
		case OPER_cge:			// '>='
			if (!eval(&lhs, ast->op.lhso))
				return TYPE_any;

			if (!eval(&rhs, ast->op.rhso))
				return TYPE_any;

			dieif(lhs.kind != TOKEN_val, ERR_INTERNAL_ERROR);
			dieif(rhs.kind != TOKEN_val, ERR_INTERNAL_ERROR);
			// TODO: no context: dieif(ast->type != cc->type_bol, ERR_INTERNAL_ERROR);
			dieif(lhs.type != rhs.type, ERR_INTERNAL_ERROR);

			switch (castOf(rhs.type)) {
				default:
					return TYPE_any;

				case CAST_bit:
				case CAST_i32:
				case CAST_i64:
				case CAST_u32:
				case CAST_u64:
					switch (ast->kind) {
						default:
							fatal(ERR_INTERNAL_ERROR);
							return TYPE_any;

						case OPER_ceq:
							res->kind = TOKEN_val;
							res->cint = lhs.cint == rhs.cint;
							break;

						case OPER_cne:
							res->kind = TOKEN_val;
							res->cint = lhs.cint != rhs.cint;
							break;

						case OPER_clt:
							res->kind = TOKEN_val;
							res->cint = lhs.cint  < rhs.cint;
							break;

						case OPER_cle:
							res->kind = TOKEN_val;
							res->cint = lhs.cint <= rhs.cint;
							break;

						case OPER_cgt:
							res->kind = TOKEN_val;
							res->cint = lhs.cint  > rhs.cint;
							break;

						case OPER_cge:
							res->kind = TOKEN_val;
							res->cint = lhs.cint >= rhs.cint;
							break;
					}
					break;

				case CAST_f32:
				case CAST_f64:
					switch (ast->kind) {
						default:
							fatal(ERR_INTERNAL_ERROR);
							return TYPE_any;

						case OPER_ceq:
							res->kind = TOKEN_val;
							res->cint = lhs.cflt == rhs.cflt;
							break;

						case OPER_cne:
							res->kind = TOKEN_val;
							res->cint = lhs.cflt != rhs.cflt;
							break;

						case OPER_clt:
							res->kind = TOKEN_val;
							res->cint = lhs.cflt < rhs.cflt;
							break;

						case OPER_cle:
							res->kind = TOKEN_val;
							res->cint = lhs.cflt <= rhs.cflt;
							break;

						case OPER_cgt:
							res->kind = TOKEN_val;
							res->cint = lhs.cflt > rhs.cflt;
							break;

						case OPER_cge:
							res->kind = TOKEN_val;
							res->cint = lhs.cflt >= rhs.cflt;
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
			if (!eval(&lhs, ast->op.lhso))
				return TYPE_any;

			if (!eval(&rhs, ast->op.rhso))
				return TYPE_any;

			dieif(lhs.kind != TOKEN_val, ERR_INTERNAL_ERROR);
			dieif(rhs.kind != TOKEN_val, ERR_INTERNAL_ERROR);
			dieif(lhs.type != rhs.type, ERR_INTERNAL_ERROR);

			/*
			switch (rhs.cast) {
				default:
					return TYPE_any;

				case CAST_i64:
					res->cast = CAST_i64;
					switch (ast->kind) {
						default:
							fatal(ERR_INTERNAL_ERROR);
							return TYPE_any;

						case OPER_shr:
							res->cint = lhs.cint >> rhs.cint;
							break;

						case OPER_shl:
							res->cint = lhs.cint << rhs.cint;
							break;

						case OPER_and:
							res->cint = lhs.cint & rhs.cint;
							break;

						case OPER_xor:
							res->cint = lhs.cint ^ rhs.cint;
							break;

						case OPER_ior:
							res->cint = lhs.cint | rhs.cint;
							break;
					}
					break;
			}
			*/
			break;

		case OPER_all:
		case OPER_any:
			if (!eval(&lhs, ast->op.lhso))
				return TYPE_any;

			if (!eval(&rhs, ast->op.rhso))
				return TYPE_any;

			dieif(lhs.kind != TOKEN_val, ERR_INTERNAL_ERROR);
			dieif(rhs.kind != TOKEN_val, ERR_INTERNAL_ERROR);
			// TODO: no context: dieif(ast->type != cc->type_bol, ERR_INTERNAL_ERROR);
			dieif(lhs.type != rhs.type, ERR_INTERNAL_ERROR);

			// TODO: try reconsidering these operators to behave like in lua or js
			switch (ast->kind) {
				default:
					fatal(ERR_INTERNAL_ERROR);
					return TYPE_any;

				case OPER_any:
					res->cint = lhs.cint || rhs.cint;
					break;

				case OPER_all:
					res->cint = lhs.cint && rhs.cint;
					break;
			}
			break;

		case OPER_sel:
			if (!eval(&lhs, ast->op.test))
				return TYPE_any;

			dieif(lhs.kind != TOKEN_val, ERR_INTERNAL_ERROR);
			return eval(res, bolValue(&lhs) ? ast->op.lhso : ast->op.rhso);

		case ASGN_set:
			return TYPE_any;
	}

	if (res->type != NULL && cast != castOf(res->type)) {
		res->kind = TOKEN_val;
		switch (cast) {
			default:
				fatal(ERR_INTERNAL_ERROR);
				return TYPE_any;

			case CAST_bit:
				res->cint = bolValue(res);
				res->type = type;
				break;

			case CAST_i64:
				res->cint = intValue(res);
				res->type = type;
				break;

			case CAST_f64:
				res->cflt = fltValue(res);
				res->type = type;
				break;
		}
	}

	switch (res->kind) {
		default:
			fatal(ERR_INTERNAL_ERROR": %k", res);
			res->type = NULL;
			return TYPE_any;

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

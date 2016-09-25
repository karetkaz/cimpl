/*******************************************************************************
 *   File: lexer.c
 *   Date: 2011/06/23
 *   Desc: parser
 *******************************************************************************

*/
#include <stddef.h>
#include "internal.h"

const struct tokenRec token_tbl[256] = {
	#define TOKEN_DEF(Name, Type, Args, Text) {Type, Args, Text},
	#include "defs.inl"
};

/** Construct reference node.
 * @brief construct a new node for variables.
 * @param cc compiler context.
 * @param name name of the node.
 * @return the new node.
 */
static inline astn tagNode(ccContext cc, char *name) {
	astn ast = NULL;
	if (cc != NULL && name != NULL) {
		ast = newNode(cc, TOKEN_var);
		if (ast != NULL) {
			size_t len = strlen(name);
			ast->file = cc->file;
			ast->line = cc->line;
			ast->type = NULL;
			ast->ref.link = NULL;
			ast->ref.hash = rehash(name, len + 1) % TBL_SIZE;
			ast->ref.name = mapstr(cc, name, len + 1, ast->ref.hash);
		}
	}
	return ast;
}

/** Construct arguments.
 * @brief construct a new node for function arguments
 * @param cc compiler context.
 * @param lhs arguments or first argument.
 * @param rhs next argument.
 * @return if lhs is null return (rhs) else return (lhs, rhs).
 */
static inline astn argNode(ccContext cc, astn lhs, astn rhs) {
	if (lhs == NULL) {
		return rhs;
	}
	return opNode(cc, OPER_com, lhs, rhs);
}

/// Add length property to arrays.
static inline symn addLength(ccContext cc, symn sym, astn init) {
	/*
	 * dynamic array size kind: ATTR_const | KIND_var
	 *  static array size kind: ATTR_const | ATTR_stat | KIND_def
	 *     using static we get the warning accessing static member through instance.
	 */
	ccKind kind = ATTR_cnst | (init == NULL ? KIND_var : ATTR_stat | KIND_def);
	symn newField, oldFields = sym->fields;

	enter(cc);
	newField = install(cc, "length", kind | CAST_i32, vm_size, cc->type_i32, init);
	sym->fields = leave(cc, sym, KIND_def, 0, NULL);
	newField->next = oldFields;
	return newField;
}

/**
 * @brief Install a new symbol: alias, type, variable or function.
 * @param kind Kind of sybol: (KIND_def, KIND_var,KIND_typ, CAST_arr)
 * @param tag Parsed tree node representing the sybol.
 * @param typ Type of symbol.
 * @return The symbol.
 */
static symn declare(ccContext cc, ccKind kind, astn tag, symn typ, symn prms) {
	symn def, ptr;
	size_t size = typ->size;

	if (!tag || tag->kind != TOKEN_var) {
		fatal(ERR_INTERNAL_ERROR": identifier expected, not: %t", tag);
		return 0;
	}

	switch (kind & MASK_cast) {
		default:
			break;

		case CAST_ref:
			size = vm_size;
			break;

		case CAST_arr:
		case CAST_var:
			size = 2 * vm_size;
			break;
	}


	def = install(cc, tag->ref.name, kind, size, typ, NULL);

	if (def != NULL) {
		struct astNode arg;
		tag->ref.link = def;
		tag->type = typ;

		def->file = tag->file;
		def->line = tag->line;
		def->params = prms;
		def->use = tag;
		def->tag = tag;

		addUsage(def, tag);

		memset(&arg, 0, sizeof(arg));
		arg.kind = TOKEN_var;
		for (ptr = def->next; ptr; ptr = ptr->next) {
			symn arg1 = ptr->params;
			symn arg2 = prms;

			if (ptr->name == NULL) {
				continue;
			}

			if (strcmp(def->name, ptr->name) != 0) {
				continue;
			}

			while (arg1 && arg2) {
				arg.ref.link = arg1;
				arg.type = arg1->type;
				if (!canAssign(cc, arg2, &arg, 1)) {
					arg.ref.link = arg2;
					arg.type = arg2->type;
					if (!canAssign(cc, arg1, &arg, 1)) {
						break;
					}
				}
				arg1 = arg1->next;
				arg2 = arg2->next;
			}

			if (arg1 == NULL && arg2 == NULL) {
				break;
			}
		}

		if (ptr != NULL && (ptr->nest >= def->nest)) {
			error(cc->rt, def->file, def->line, ERR_REDEFINED_REFERENCE, def);
			if (ptr->file && ptr->line) {
				info(cc->rt, ptr->file, ptr->line, "previously defined as `%T`", ptr);
			}
		}
	}

	return def;
}

/// expand compound assignment expressions like  `a += 1` into `a = a + 1`
static astn expandAssignment(ccContext cc, astn root) {
	astn tmp = NULL;
	switch (root->kind) {
		default:
			break;
		case ASGN_add:
			tmp = newNode(cc, OPER_add);
			break;
		case ASGN_sub:
			tmp = newNode(cc, OPER_sub);
			break;
		case ASGN_mul:
			tmp = newNode(cc, OPER_mul);
			break;
		case ASGN_div:
			tmp = newNode(cc, OPER_div);
			break;
		case ASGN_mod:
			tmp = newNode(cc, OPER_mod);
			break;
		case ASGN_shl:
			tmp = newNode(cc, OPER_shl);
			break;
		case ASGN_shr:
			tmp = newNode(cc, OPER_shr);
			break;
		case ASGN_and:
			tmp = newNode(cc, OPER_and);
			break;
		case ASGN_ior:
			tmp = newNode(cc, OPER_ior);
			break;
		case ASGN_xor:
			tmp = newNode(cc, OPER_xor);
			break;
	}
	if (tmp != NULL) {
		// convert `a += 1` into `a = a + 1`
		tmp->op.rhso = root->op.rhso;
		tmp->op.lhso = root->op.lhso;
		tmp->file = root->file;
		tmp->line = root->line;
		root->kind = ASGN_set;
		root->op.rhso = tmp;
	}
	return root;
}
static astn expand2Statement(ccContext cc, astn node, int block) {
	astn result = newNode(cc, block ? STMT_beg : STMT_end);
	if (result != NULL) {
		result->type = cc->type_vid;
		result->file = node->file;
		result->line = node->line;
		result->stmt.stmt = node;
	}
	return result;
}
//#{~~~~~~~~~ Parser ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

static astn statement(ccContext, ccKind attr);
static astn declaration(ccContext cc, ccKind attr, astn *args);
static astn expression(ccContext cc);

/** Parse qualifiers.
 * @brief scan for qualifiers: const static parallel
 * @param cc compiler context.
 * @param accept accepted qualifiers.
 * @return qualifiers.
 */
static ccKind qualifier(ccContext cc) {
	ccKind result = (ccKind) 0;
	astn ast;

	while ((ast = peekTok(cc, TOKEN_any))) {
		switch (ast->kind) {

			default:
				return result;

			// const
			case CONST_kwd:
				skipTok(cc, CONST_kwd, 1);
				if (result & ATTR_cnst) {
					error(cc->rt, ast->file, ast->line, ERR_UNEXPECTED_QUAL, ast);
				}
				result |= ATTR_cnst;
				break;

			// static
			case STATIC_kwd:
				skipTok(cc, STATIC_kwd, 1);
				if (result & ATTR_stat) {
					error(cc->rt, ast->file, ast->line, ERR_UNEXPECTED_QUAL, ast);
				}
				result |= ATTR_stat;
				break;

			// parallel
			case PARAL_kwd:
				skipTok(cc, PARAL_kwd, 1);
				if (result & ATTR_paral) {
					error(cc->rt, ast->file, ast->line, ERR_UNEXPECTED_QUAL, ast);
				}
				result |= ATTR_paral;
				break;
		}
	}
	return result;
}

/**
 * @brief Parse expression.
 * @param cc compiler context.
 * @return root of expression tree
 */
static astn expression(ccContext cc) {
	astn buff[CC_MAX_TOK], *const base = buff + CC_MAX_TOK;
	astn *ptr, ast, prev = NULL;
	astn *postfix = buff;			// postfix form of expression
	astn *stack = base;				// precedence stack
	char sym[CC_MAX_TOK];			// symbol stack {, [, (, ?
	int unary = 1;					// start in unary mode
	int level = 0;					// precedence and top of symbol stack

	sym[level] = 0;

	// parse expression using Shunting-yard algorithm
	while ((ast = nextTok(cc, TOKEN_any))) {
		int l2r, prec = level << 4;

		// statement tokens are not allowed in expressions !!!!
		if (ast->kind >= STMT_beg && ast->kind <= STMT_end) {
			backTok(cc, ast);
			break;
		}

		switch (ast->kind) {
			default:
				// operator
				if (token_tbl[ast->kind].argc > 0) {
					if (unary) {
						error(cc->rt, ast->file, ast->line, ERR_SYNTAX_ERR_BEFORE, ast);
						*postfix++ = NULL;
					}
					unary = 1;
					goto tok_operator;
				}

				// literal, variable or keyword
				if (!unary) {
					backTok(cc, ast);
					ast = NULL;
					break;
				}
				*postfix++ = ast;
				unary = 0;
				break;

			tok_operator:
				prec |= token_tbl[ast->kind].type & 0x0f;
				l2r = token_tbl[ast->kind].type & 0x10;
				ast->op.prec = prec;
				while (stack < base) {
					// associates left to right
					if (l2r && (*stack)->op.prec <= ast->op.prec) {
						break;
					}
					else if ((*stack)->op.prec < ast->op.prec) {
						break;
					}
					*postfix++ = *stack++;
				}
				if (ast->kind != STMT_end) {
					*--stack = ast;
				}
				break;

			case LEFT_par:			// '('
				if (unary) {		// a * (3 - b)
					*postfix++ = NULL;
				}
				ast->kind = OPER_fnc;
				sym[++level] = '(';
				unary = 1;
				goto tok_operator;

			case LEFT_sqr:			// '['
				if (unary) {		// x + [ptr]
					*postfix++ = NULL;
				}
				ast->kind = OPER_idx;
				sym[++level] = '[';
				unary = 1;
				goto tok_operator;

			case PNCT_qst:		// '?'
				if (unary) {
					error(cc->rt, ast->file, ast->line, ERR_SYNTAX_ERR_BEFORE, ast);
					*postfix++ = 0;
				}
				ast->kind = OPER_sel;
				sym[++level] = '?';
				unary = 1;
				goto tok_operator;

			case RIGHT_par:			// ')'
				if (unary && prev && prev->kind == OPER_fnc) {
					*postfix++ = NULL;
					unary = 0;
				}
				if (unary || (level > 0 && sym[level] != '(')) {
					error(cc->rt, ast->file, ast->line, ERR_SYNTAX_ERR_BEFORE, ast);
					break;
				}
				if (level <= 0) {
					backTok(cc, ast);
					ast = NULL;
					break;
				}
				ast->kind = STMT_end;
				level -= 1;
				unary = 0;
				goto tok_operator;

			case RIGHT_sqr:			// ']'
				if (unary && prev && prev->kind == OPER_idx) {
					*postfix++ = NULL;
					unary = 0;
				}
				if (unary || (level > 0 && sym[level] != '[')) {
					error(cc->rt, ast->file, ast->line, ERR_SYNTAX_ERR_BEFORE, ast);
					break;
				}
				if (level <= 0) {
					backTok(cc, ast);
					ast = NULL;
					break;
				}
				ast->kind = STMT_end;
				level -= 1;
				unary = 0;
				goto tok_operator;

			case PNCT_cln:		// ':'
				if (unary || (level > 0 && sym[level] != '?')) {
					error(cc->rt, ast->file, ast->line, ERR_SYNTAX_ERR_BEFORE, ast);
					break;
				}
				if (level <= 0) {
					backTok(cc, ast);
					ast = NULL;
					break;
				}
				ast->kind = STMT_end;
				level -= 1;
				unary = 1;
				goto tok_operator;

			case OPER_add:		// '+'
				if (unary) {
					ast->kind = OPER_pls;
				}
				unary = 1;
				goto tok_operator;

			case OPER_sub:		// '-'
				if (unary) {
					ast->kind = OPER_mns;
				}
				unary = 1;
				goto tok_operator;

			case OPER_and:		// '&'
				if (unary) {
					ast->kind = OPER_adr;
				}
				unary = 1;
				goto tok_operator;

			case OPER_not:		// '!'
			case OPER_cmt:		// '~'
				if (!unary) {
					error(cc->rt, ast->file, ast->line, ERR_SYNTAX_ERR_BEFORE, ast);
				}
				unary = 1;
				goto tok_operator;
		}

		if (postfix >= stack) {
			error(cc->rt, cc->file, cc->line, ERR_EXPR_TOO_COMPLEX);
			return NULL;
		}

		if (ast == NULL) {
			break;
		}

		prev = ast;
	}

	// expression is not finished: `3 + ` or `(2 + 3`
	if (unary || level > 0) {
		char *expected = "expression";
		if (level > 0) {
			switch (sym[level]) {
				default:
					fatal(ERR_INTERNAL_ERROR);
					return NULL;
				case '(':
					expected = "')'";
					break;
				case '[':
					expected = "']'";
					break;
				case '?':
					expected = "':'";
					break;
			}
		}
		error(cc->rt, cc->file, cc->line, ERR_EXPECTED_BEFORE_TOK, expected, peekTok(cc, TOKEN_any));
		return NULL;
	}

	// flush operators left on precedence stack
	while (stack < base) {
		*postfix++ = *stack++;
	}

	// build the abstract syntax tree from the postfix form
	for (ptr = buff; ptr < postfix; ++ptr) {
		int args;
		ast = *ptr;

		if (ast == NULL) {
			*--stack = ast;
			continue;
		}

		args = token_tbl[ast->kind].argc;
		switch (args) {
			default:
				fatal(ERR_INTERNAL_ERROR);
				return NULL;
			case 3:
				ast->op.test = stack[2];
			case 2:
				ast->op.lhso = stack[1];
			case 1:
				ast->op.rhso = stack[0];
			case 0:
				break;
		}
		// check for stack underflow
		if ((stack += args) > base) {
			fatal(ERR_INTERNAL_ERROR);
			return NULL;
		}
		*--stack = expandAssignment(cc, ast);
	}

	// stack must contain a single root element
	if (stack != base - 1) {
		fatal(ERR_INTERNAL_ERROR);
		return NULL;
	}

	return *stack;
}

/** Parse function parameters.
 * @brief Parse the parameters of a function declaration.
 * @param cc compiler context.
 * @param mode
 * @return expression tree.
 */
static astn parameters(ccContext cc, symn resType) {
	astn tok = NULL;

	if (resType != NULL) {
		symn res = install(cc, ".result", KIND_var, resType->size, resType, resType->init);
		tok = lnkNode(cc, res);
	}

	if (peekTok(cc, RIGHT_par) != NULL) {
		return tok;
	}

	// while there are tokens to read
	while (peekTok(cc, TOKEN_any) != NULL) {
		ccKind attr = qualifier(cc);
		astn arg = declaration(cc, attr, NULL);

		if (arg != NULL) {
			tok = argNode(cc, tok, arg);
			if (attr & ATTR_cnst) {
				arg->ref.link->kind |= ATTR_cnst;
			}
		}

		if (!skipTok(cc, OPER_com, 0)) {
			break;
		}
	}
	return tok;
}

/** Parse initialization.
 * @brief parse the initializer of a declaration.
 * @param cc compiler context.
 * @param var the declared variable symbol.
 * @return the initializer expression.
 */
static astn initializer(ccContext cc, symn var) {
	if (skipTok(cc, STMT_beg, 0)) {
		symn field;
		astn tag, ast, root = NULL;
		while ((ast = peekTok(cc, TOKEN_any))) {
			switch (ast->kind) {
				case RIGHT_crl:
					ast = NULL;
				default:
					break;
			}
			if (ast == NULL) {
				break;
			}

			// TODO: what if var is array

			// read property name
			if ((tag = nextTok(cc, TOKEN_var)) == NULL) {
				error(cc->rt, cc->file, cc->line, ERR_EXPECTED_BEFORE_TOK, "identifier", peekTok(cc, TOKEN_any));
				return NULL;
			}

			// lookup tag field
			if ((field = lookup(cc, var, tag, NULL, 1)) == NULL) {
				//return NULL;
			}

			if (peekTok(cc, OPER_beg) || skipTok(cc, PNCT_cln, 0)) {
				ast = initializer(cc, field);
			}
			else {
				error(cc->rt, cc->file, cc->line, ERR_UNEXPECTED_TOKEN, peekTok(cc, TOKEN_any));
				break;
			}
			skipTok(cc, STMT_end, 1);
			if (ast == NULL) {
				error(cc->rt, cc->file, cc->line, ERR_EXPECTED_BEFORE_TOK, "initializer", peekTok(cc, TOKEN_any));
				return NULL;
			}
			ast = opNode(cc, ASGN_set, tag, ast);
			ast->type = typeCheck(cc, var->type, ast, 1);
			if (ast->type == NULL) {
				break;
			}
			root = argNode(cc, root, ast);
		}
		skipTok(cc, RIGHT_crl, 1);
		return root;
	}

	// initialize with expression
	return expression(cc);
}

/** Parse variable or function declaration.
 * @brief parse the declaration of a variable or function.
 * @param cc compiler context.
 * @param argv out parameter for function arguments.
 * @param mode
 * @return parsed syntax tree.
 */
static astn declaration(ccContext cc, ccKind attr, astn *args) {
	symn def = NULL, type = NULL, prms = NULL;
	ccKind cast = CAST_val;
	astn tok, tag = NULL;

	// read typename
	tok = expression(cc);
	if (tok == NULL) {
		error(cc->rt, tok->file, tok->line, ERR_INVALID_TYPE, tok);
		return NULL;
	}

	type = typeCheck(cc, NULL, tok, 1);
	if (type == NULL) {
		error(cc->rt, tok->file, tok->line, ERR_INVALID_TYPE, tok);
		return NULL;
	}

	if ((tag = nextTok(cc, TOKEN_var)) == NULL) {
		error(cc->rt, cc->file, cc->line, ERR_EXPECTED_BEFORE_TOK, "identifier", peekTok(cc, TOKEN_any));
		tag = tagNode(cc, ".variable");
	}

	// TODO: inOut: int &&a;
	if (skipTok(cc, OPER_and, 0)) {
		// inOut: int &&a;
		// byRef: int &a;
		cast = CAST_ref;
	}

	// parse function parameters
	if (skipTok(cc, LEFT_par, 0)) {			// int a(...)
		astn argRoot;
		enter(cc);
		argRoot = parameters(cc, type);
		skipTok(cc, RIGHT_par, 1);

		prms = leave(cc, def, KIND_fun, vm_size, NULL);
		type = cc->type_fun;

		if (args != NULL) {
			*args = argRoot;
		}

		// parse function body
		if (peekTok(cc, STMT_beg)) {
			symn param;
			def = declare(cc, ATTR_stat | ATTR_cnst | KIND_fun | cast, tag, type, prms);

			enter(cc);
			// TODO: skip reinstalling all parameters, try to adapt lookup
			for (param = prms->next; param; param = param->next) {
				install(cc, param->name, KIND_def, 0, param, NULL);
				param->owner = def;
			}

			def->init = statement(cc, TYPE_any);
			backTok(cc, newNode(cc, STMT_end));
			leave(cc, def, KIND_def, -1, NULL);

			return tag;
		}
	}

	// parse array dimensions
	while (skipTok(cc, LEFT_sqr, 0)) {		// int a[...][][]...
		symn arr = newDefn(cc, KIND_typ);
		int64_t length = -1;

		if (!skipTok(cc, RIGHT_sqr, 0)) {
			arr->init = nextTok(cc, OPER_mul);
			if (arr->init == NULL) {
				astn dim = expression(cc);
				if (dim != NULL) {
					dim->type = typeCheck(cc, NULL, dim, 1);
				}
				arr->init = dim;
			}
			else {
				length = 0;
			}
			skipTok(cc, RIGHT_sqr, 1);
		}

		arr->type = type;
		type = arr;

		if (length != 0) {
			// add array length property
			addLength(cc, arr, arr->init);
		}
		if (arr->init != NULL) {
			if (length != 0) {
				struct astNode value;
				if (eval(&value, arr->init)) {
					length = intValue(&value);
				}
				if (length <= 0) {
					error(cc->rt, arr->init->file, arr->init->line, ERR_INVALID_ARRAY_LENGTH, arr->init);
				}
				arr->kind = ATTR_stat | KIND_typ | CAST_arr;
				arr->size = length * arr->type->size;
			}
			else {
				arr->kind = ATTR_stat | KIND_typ | CAST_arr;
				arr->size = vm_size;
			}
		}
		else {
			arr->kind = KIND_typ | CAST_arr;
			arr->size = 2 * vm_size;
		}
//		debug("%?s:%?u: array[size: %d]: %.t: %.T", arr->file, arr->line, arr->size, tag, arr);
	}

	if (cast == CAST_val) {
		cast = type->kind & MASK_cast;
	}

	def = declare(cc, (attr & MASK_attr) | KIND_var | cast, tag, type, prms);
	if (skipTok(cc, ASGN_set, 0)) {
		def->init = initializer(cc, def);
	}

	return tag;
}

/** Parse alias declaration.
 * @brief parse the declaration of an alias/inline expression.
 * @param cc compiler context.
 * @return root of declaration.
 */
static astn declare_alias(ccContext cc, ccKind attr) {
	symn def, type = NULL, params = NULL;
	astn tag, init = NULL;

	if (!skipTok(cc, INLINE_kwd, 1)) {
		traceAst(peekTok(cc, TOKEN_any));
		return NULL;
	}

	if (!(tag = nextTok(cc, TOKEN_var))) {
		error(cc->rt, cc->file, cc->line, ERR_EXPECTED_BEFORE_TOK, "identifier", peekTok(cc, TOKEN_any));
		skipTok(cc, STMT_end, 1);
		return NULL;
	}

	enter(cc);

	if (skipTok(cc, LEFT_par, 0)) {
		parameters(cc, cc->type_vid);
		skipTok(cc, RIGHT_par, 1);
	}

	skipTok(cc, ASGN_set, 1);
	init = initializer(cc, NULL);
	if (init != NULL) {
		type = typeCheck(cc, NULL, init, 1);
		init->type = type;
	}
	params = leave(cc, NULL, KIND_def, 0, NULL);
	if (params != NULL) {
		params->type = type;
	}
	skipTok(cc, STMT_end, 1);

	def = declare(cc, (attr & MASK_attr) | KIND_def, tag, cc->type_fun, params);
	if (def != NULL) {
		def->params = params;
		def->init = init;
		def->size = 0;
	}

	return tag;
}

/** Parse struct declaration.
 * @brief parse the declaration of a structure.
 * @param cc compiler context.
 * @param attr attributes: static or const.
 * @return root of declaration.
 */
static astn declare_record(ccContext cc, ccKind attr) {
	symn type = NULL, base = NULL;
	astn tok, tag = NULL;
	int pack = vm_size;

	if (!skipTok(cc, RECORD_kwd, 1)) {
		traceAst(peekTok(cc, 0));
		return NULL;
	}

	tag = nextTok(cc, TOKEN_var);

	if (skipTok(cc, PNCT_cln, 0)) {			// ':' base type or packing
		pack = -1;
		if ((tok = expression(cc)) != NULL) {
			if (tok->kind == TOKEN_val) {
				ccKind cast = tok->type->kind & MASK_cast;
				if (cast == CAST_i32 || cast == CAST_i64) {
					switch (tok->cint) {
						default:
							break;

						case 0:
						case 1:
						case 2:
						case 4:
						case 8:
						case 16:
						case 32:
							pack = (unsigned) tok->cint;
							break;
					}
				}
			}
			else if (typeCheck(cc, NULL, tok, 1) != NULL) {
				if (isTypeExpr(tok)) {
					base = linkOf(tok);
					pack = vm_size;
				}
			}
			if (pack < 0) {
				error(cc->rt, cc->file, cc->line, ERR_INVALID_BASE_TYPE, tok);
			}
		}
		else {
			error(cc->rt, cc->file, cc->line, ERR_INVALID_BASE_TYPE, peekTok(cc, TOKEN_any));
		}
	}

	if (!skipTok(cc, STMT_beg, 1)) {	// '{'
		traceAst(peekTok(cc, 0));
		return NULL;
	}
	if (tag == NULL) {
		// TODO: disable anonymous structure declarations ?
		tag = tagNode(cc, ".anonymous");
	}
	if (base == NULL) {
		base = cc->type_rec;
	}

	type = declare(cc, ATTR_stat | ATTR_cnst | KIND_typ | CAST_val, tag, base, NULL);
	enter(cc);
	while (peekTok(cc, TOKEN_any)) {	// fields
		if (peekTok(cc, RIGHT_crl)) {
			break;
		}
		if (declaration(cc, TYPE_any, NULL) == NULL) {
			error(cc->rt, cc->file, cc->line, ERR_INVALID_DECLARATION, peekTok(cc, TOKEN_any));
			break;
		}
		if (!skipTok(cc, STMT_end, 1)) {	// ';'
			break;
		}
	}
	type->fields = leave(cc, type, attr | KIND_typ, pack, &type->size);

	if (!skipTok(cc, RIGHT_crl, 1)) {	// '}'
		traceAst(peekTok(cc, 0));
		return NULL;
	}

	return tag;
}

/** Parse enum declaration.
 * @brief parse the declaration of an enumeration.
 * @param cc compiler context.
 * @return root of declaration.
 */
// TODO: implement
static astn declare_enum(ccContext cc) {
	//symn def = NULL, base = cc->type_i32;
	// astn tok, tag = NULL;
	skipTok(cc, ENUM_kwd, 1);
	return NULL;
}

static astn statement_list(ccContext cc) {
	astn ast, head = NULL, tail = NULL;

	while ((ast = peekTok(cc, TOKEN_any)) != NULL) {
		ccKind attr;

		switch (ast->kind) {
			default:
				break;

			//case TOKEN_any:	// error
			//case RIGHT_par:
			//case RIGHT_sqr:
			case RIGHT_crl:		// '}'
				ast = NULL;
				break;
		}
		if (ast == NULL) {
			break;
		}

		attr = qualifier(cc);
		if ((ast = statement(cc, attr))) {
			if (tail != NULL) {
				tail->next = ast;
			}
			else {
				head = ast;
			}
			tail = ast;
		}
	}

	return head;
}

// TODO: implement
static astn statement_for(ccContext cc, ccKind attr) {
	astn ast = nextTok(cc, STMT_for);
	if (ast == NULL) {
		return NULL;
	}

	enter(cc);
	skipTok(cc, LEFT_par, 1);
	ast->stmt.init = expression(cc);
	//
	if (isTypeExpr(ast->stmt.init)) {

	}

	skipTok(cc, RIGHT_par, 1);

	leave(cc, NULL, KIND_def, 0, NULL);

	ast->type = cc->type_vid;
	(void)attr;
	return ast;
}

static astn statement_if(ccContext cc, ccKind attr) {
	int enterThen = 1, enterElse = 1;
	astn test, ast = nextTok(cc, STMT_if);
	if (ast == NULL) {
		traceAst(ast);
		return NULL;
	}

	skipTok(cc, LEFT_par, 1);
	test = expression(cc);
	if (test != NULL) {
		test->type = typeCheck(cc, NULL, test, 1);
		ast->stmt.test = test;
	}
	skipTok(cc, RIGHT_par, 1);

	// static if statement true branch does not enter a new scope to expose declarations.
	if (attr & ATTR_stat) {
		struct astNode value;
		if (eval(&value, ast->stmt.test)) {
			if (bolValue(&value)) {
				enterThen = 0;
			}
			else {
				enterElse = 0;
			}
			ast->kind = STMT_sif;
		}
		else {
			error(cc->rt, ast->file, ast->line, ERR_INVALID_CONST_EXPR, ast->stmt.test);
		}
	}

	if (enterThen) {
		enter(cc);
	}

	ast->stmt.stmt = statement(cc, TYPE_any);

	// parse else part if next token is 'else'
	if (skipTok(cc, ELSE_kwd, 0)) {
		if (enterThen) {
			leave(cc, NULL, KIND_def, 0, NULL);
			enterThen = 0;
		}
		if (enterElse) {
			enter(cc);
			enterThen = 1;
		}
		ast->stmt.step = statement(cc, TYPE_any);
	}

	if (enterThen) {
		leave(cc, NULL, KIND_def, 0, NULL);
	}

	return ast;
}

/** Parse statement.
 * @brief parse a statement.
 * @param cc compiler context.
 * @param mode
 * @return parsed syntax tree.
 */
static astn statement(ccContext cc, ccKind attr) {
	int validStart = 1;
	astn ast, check = NULL;

	//~ skip to the first valid statement start
	while ((ast = peekTok(cc, TOKEN_any)) != NULL) {
		switch (ast->kind) {
			default:
				// valid statement start
				ast = NULL;
				break;

			// invalid statement start tokens
			case RIGHT_par:
			case RIGHT_sqr:
			case RIGHT_crl:
			case TOKEN_any:
				if (validStart == 1) {
					error(cc->rt, ast->file, ast->line, ERR_UNEXPECTED_TOKEN, ast);
					validStart = 0;
				}
				skipTok(cc, TOKEN_any, 0);
				break;
		}
		if (ast == NULL) {
			break;
		}
	}

	// scan the statement
	if (skipTok(cc, STMT_end, 0)) {             // ;
		warn(cc->rt, 2, cc->file, cc->line, WARN_EMPTY_STATEMENT);
	}
	else if ((ast = nextTok(cc, STMT_beg))) {   // { ... }
		ast->stmt.stmt = statement_list(cc);
		skipTok(cc, RIGHT_crl, 1);
		ast->type = cc->type_vid;

		if (ast->stmt.stmt == NULL) {
			// remove nodes such: {{;{;};{}{}}}
			recycle(cc, ast);
			ast = NULL;
		}
	}
	else if (peekTok(cc, STMT_if) != NULL) {    // if (...)
		ast = statement_if(cc, attr);
		ast->type = cc->type_vid;

		if (attr & ATTR_stat && ast->kind == STMT_sif) {
			attr &= ~ATTR_stat;
		}
	}
	else if (peekTok(cc, STMT_for) != NULL) {   // for (...)
		ast = statement_for(cc, attr);
		ast->type = cc->type_vid;

		if (attr & ATTR_paral && ast->kind == STMT_pfor) {
			attr &= ~ATTR_paral;
		}
	}
	else if ((ast = nextTok(cc, STMT_brk))) {   // break;
		skipTok(cc, STMT_end, 1);
		ast->type = cc->type_vid;
	}
	else if ((ast = nextTok(cc, STMT_con))) {   // continue;
		skipTok(cc, STMT_end, 1);
		ast->type = cc->type_vid;
	}
	else if ((ast = nextTok(cc, STMT_ret))) {   // return expression;
		if (!skipTok(cc, STMT_end, 0)) {
			ast->stmt.stmt = expression(cc);
			skipTok(cc, STMT_end, 1);
			check = ast->stmt.stmt;
		}
		ast->type = cc->type_vid;
	}

	// type, enum and alias declaration
	else if (peekTok(cc, INLINE_kwd) != NULL) { // alias
		ast = declare_alias(cc, attr);
		ast->type = cc->type_rec;
		//ast = expand2Statement(cc, ast, 0);
	}
	else if (peekTok(cc, RECORD_kwd) != NULL) { // struct
		ast = declare_record(cc, attr);
		ast->type = cc->type_rec;
		//ast = expand2Statement(cc, ast, 0);
	}
	else if (peekTok(cc, ENUM_kwd) != NULL) {   // enum
		ast = declare_enum(cc);
		ast->type = cc->type_rec;
		//ast = expand2Statement(cc, ast, 0);
	}
	else if ((ast = expression(cc))) {
		symn type = typeCheck(cc, NULL, ast, 1);
		if (type == NULL) {
			error(cc->rt, ast->file, ast->line, ERR_INVALID_TYPE, ast);
			type = cc->type_vid;
		}
		else if (isTypeExpr(ast)) {
			backTok(cc, ast);
			ast = declaration(cc, attr, NULL);
			check = ast->ref.link->init;
			attr = TYPE_any;
		}
		else {
			switch (ast->kind) {
				default:
					warn(cc->rt, 1, ast->file, ast->line, WARN_EXPRESSION_STATEMENT, ast);
					break;

				case OPER_fnc:
				case ASGN_set:
					//ast = expand2Statement(cc, ast, 0);
					type = cc->type_vid;
					break;
			}
			check = ast;
		}
		skipTok(cc, STMT_end, 1);
		ast->type = type;
	}

	// try to type check the statement
	if (check != NULL) {
		check->type = typeCheck(cc, NULL, check, 1);
		if (check->type == NULL) {
			error(cc->rt, check->file, check->line, ERR_INVALID_TYPE, check);
		}
	}

	if (attr != 0) {
		error(cc->rt, __FILE__, __LINE__, ERR_UNEXPECTED_ATTR, attr);
	}

	return ast;
}
//#}

/**
 * @brief parse the source setted by source.
 * @param cc compiler context
 * @param warn warning level
 * @return abstract syntax tree
 */
astn parse(ccContext cc, int warn) {
	astn ast, unit = NULL;

	cc->warn = warn;
	// pre read all tokens from source
	if (cc->_tok == NULL) {
		int tokens = 0;
		astn head = NULL, tail = NULL;
		while ((ast = nextTok(cc, TOKEN_any)) != NULL) {
			if (tail != NULL) {
				tail->next = ast;
			}
			else {
				head = ast;
			}
			tokens += 1;
			tail = ast;
		}
		cc->_tok = head;
	}

	if ((unit = statement_list(cc))) {
		unit = expand2Statement(cc, unit, 1);

		// add parsed unit to module
		if (cc->root != NULL && cc->root->kind == STMT_beg) {
			if (cc->root->lst.tail != NULL) {
				cc->root->lst.tail->next = unit;
			}
			else {
				cc->root->lst.head = unit;
			}
			cc->root->lst.tail = unit;
		}
	}

	return unit;
}

ccContext ccOpen(rtContext rt, char *file, int line, char *text) {
	ccContext cc = rt->cc;

	if (cc == NULL) {
		// initialize only once.
		cc = ccInit(rt, install_def, NULL);
		if (cc == NULL) {
			return NULL;
		}
	}

	if (source(cc, text == NULL, text ? text : file) != 0) {
		return NULL;
	}

	if (file != NULL) {
		cc->file = mapstr(cc, file, -1, -1);
	}
	else {
		cc->file = NULL;
	}

	cc->fin.nest = cc->nest;
	cc->line = line;
	return cc;
}

int ccClose(ccContext cc) {
	astn ast;

	// not initialized
	if (cc == NULL) {
		return -1;
	}

	// check no token left to read
	if ((ast = nextTok(cc, TOKEN_any))) {
		error(cc->rt, ast->file, ast->line, ERR_UNEXPECTED_TOKEN, ast);
	}
	// check we are on the same nesting level
	else if (cc->fin.nest != cc->nest) {
		error(cc->rt, cc->file, cc->line, ERR_INVALID_STATEMENT);
	}

	// close input
	source(cc, 0, NULL);

	// return errors
	return cc->rt->errors;
}

symn ccDefCall(rtContext rt, vmError call(nfcContext), const char *proto) {
	libc lc = rt->cc->libc;
	size_t nfcPos = lc ? lc->sym->offs + 1 : 0;

	symn nfc, sym = NULL;
	astn init, args = NULL;
	int warn = rt->cc->warn;

	dieif(call == NULL || !proto, ERR_INTERNAL_ERROR);

	//~ from: int64 zxt(int64 val, int offs, int bits)
	//~ make: inline zxt(int64 val, int offs, int bits) = int64(emit(libc(42), int64(val), int32(offs), int32(bits)));

	if (!ccOpen(rt, NULL, 0, (char*)proto)) {
		trace(ERR_INTERNAL_ERROR);
		return NULL;
	}

	// enable all warnings
	rt->cc->warn = 32;
	init = declaration(rt->cc, TYPE_any, &args);
	rt->cc->warn = warn;

	if (init == NULL) {
		error(rt, NULL, 0, ERR_INVALID_PROTO, proto);
		return NULL;
	}
	dieif(ccClose(rt->cc) != 0, ERR_INTERNAL_ERROR);
	dieif(init->kind != TOKEN_var, ERR_INTERNAL_ERROR);

	if ((sym = init->ref.link) == NULL) {
		error(rt, NULL, 0, ERR_INVALID_PROTO, proto);
		return NULL;
	}

	dieif(rt->_end - rt->_beg < (ptrdiff_t)sizeof(struct libc), ERR_MEMORY_OVERRUN);

	rt->_end -= sizeof(struct libc);
	lc = (struct libc*)rt->_end;
	lc->next = rt->cc->libc;
	rt->cc->libc = lc;

	nfc = newDefn(rt->cc, EMIT_opc);
	nfc->name = "nfc";
	nfc->offs = opc_nfc;
	nfc->init = intNode(rt->cc, nfcPos);
	nfc->type = sym->params ? sym->params->type : sym->type;

	// replace the result argument with the native call instruction
	if (args != NULL) {
		astn arg = args;
		while (arg->kind == OPER_com) {
			arg = arg->op.lhso;
		}

		// arg must be the return argument
		if (arg->kind == TOKEN_var && arg->ref.link == sym->params) {
			arg->type = nfc->type;
			arg->ref.name = NULL;
			arg->ref.link = nfc;
			arg->ref.hash = -1;
		}
		else {
			fatal(ERR_INTERNAL_ERROR);
		}
	}
	else {
		args = lnkNode(rt->cc, nfc);
	}

	init = newNode(rt->cc, OPER_fnc);
	init->op.lhso = rt->cc->emit_tag;
	init->type = nfc->type;
	init->op.rhso = args;

	sym->kind = ATTR_cnst | ATTR_stat | KIND_def;
	sym->init = init;
	sym->offs = nfcPos;

	lc->proto = proto;
	lc->call = call;
	lc->sym = sym;

	// the return argument
	if (sym->params != NULL) {
		lc->out = sym->params->size / vm_size;

		// the first argument
		if (sym->params->next) {
			lc->in = sym->params->next->offs / vm_size;
		} else {
			lc->in = 0;
		}

		// make all parameters symbolic by default
		sym->params->offs = 0;
		for (nfc = sym->params->next; nfc; nfc = nfc->next) {
			nfc->kind = (nfc->kind & ~MASK_kind) | KIND_def;
			nfc->tag = NULL;
		}
	}
	else {
		lc->out = 0;
		lc->in = 0;
	}

	return sym;
}

astn ccAddUnit(rtContext rt, int warn, char *file, int line, char *text) {
	ccContext cc = ccOpen(rt, file, line, text);
	if (cc == NULL) {
		error(rt, NULL, 0, ERR_OPENING_FILE, file);
		return 0;
	}
	astn unit = parse(cc, warn);
	if (ccClose(cc) != 0) {
		return NULL;
	}
	return unit;
}

int ccAddLib(rtContext rt, int warn, int init(rtContext), char *file) {
	int unitCode = init(rt);
	if (unitCode == 0 && file != NULL) {
		return ccAddUnit(rt, warn, file, 1, NULL) != NULL;
	}
	return unitCode == 0;
}

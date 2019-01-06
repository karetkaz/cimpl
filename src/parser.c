/*******************************************************************************
 *   File: parser.c
 *   Date: 2011/06/23
 *   Desc: parser
 *******************************************************************************
 * convert token stream to an abstract syntax tree.
 */

#include "internal.h"
#include <unistd.h>
#include <limits.h>

const struct tokenRec token_tbl[256] = {
	#define TOKEN_DEF(Name, Type, Args, Text) {Type, Args, Text},
	#include "defs.inl"
};

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ Utils
static inline void addTail(astn list, astn node) {
	dieif(list == NULL, ERR_INTERNAL_ERROR);
	dieif(list->kind != STMT_beg, ERR_INTERNAL_ERROR);

	if (list->lst.tail != NULL) {
		list->lst.tail->next = node;
	}
	else {
		list->lst.head = node;
	}
	list->lst.tail = node;
}
/// Add length property to arrays.
static inline void addLength(ccContext cc, symn sym, astn init) {
	dieif(sym->fields != NULL, ERR_INTERNAL_ERROR);

	if (init == NULL) {
		// add dynamic length field to slices
		sym->fields = cc->length_ref;
		return;
	}

	// add static length to array
	enter(cc, NULL);
	ccKind kind = ATTR_cnst | ATTR_stat | KIND_def | refCast(cc->type_idx);
	install(cc, "length", kind, cc->type_idx->size, cc->type_idx, init);
	sym->fields = leave(cc, KIND_def, 0, 0, NULL);
}

static inline symn tagType(ccContext cc, astn tag) {
	symn tagType = cc->deft[tag->ref.hash];
	tagType = lookup(cc, tagType, tag, NULL, 0, 0);
	if (tagType != NULL && isTypename(tagType)) {
		return tagType;
	}
	return NULL;
}

/**
 * Inline the content of a file relative to the path of the file inlines it.
 * @param cc compiler context.
 * @param tag ast node containing which file to inline
 */
static int ccInline(ccContext cc, astn tag) {
	static char *CWD = NULL;
	if (tag->type != cc->type_str) {
		return 0;
	}

	if (CWD == NULL) {
		// lazy init on first call
		CWD = getcwd(NULL, 0);
		if (CWD == NULL) {
			CWD = "";
		}
		for (char *ptr = CWD; *ptr; ++ptr) {
			// convert windows path names to uri
			if (*ptr == '\\') {
				*ptr = '/';
			}
		}
	}

	char buff[PATH_MAX];
	char *path = absolutePath(tag->file, buff, sizeof(buff));
	if (path != buff) {
		strncpy(buff, tag->file, sizeof(buff));
	}
	for (char *ptr = buff; *ptr; ++ptr) {
		// convert windows path names to uri
		if (*ptr == '\\') {
			*ptr = '/';
		}
	}

	// replace the file name
	path = strrchr(buff, '/');
	if (path != NULL) {
		*(path += 1) = 0;
	}
	strncat(buff, tag->ref.name, sizeof(buff) - (path - buff));

	path = buff;
	for (int i = 0; buff[i] != 0; ++i) {
		if (buff[i] == 0) {
			// use absolute path
			break;
		}
		if (CWD[i] == 0) {
			if (buff[i] == '/') {
				// use relative path
				path = buff + i + 1;
			}
			break;
		}
		if (buff[i] != CWD[i]) {
			break;
		}
	}

	printLog(cc->rt, raiseDebug, tag->file, tag->line, NULL, WARN_INLINE_FILE, path);
	return ccOpen(cc, path, 1, NULL);
}

/**
 * Install a new symbol: alias, type, variable or function.
 * 
 * @param kind Kind of symbol: (KIND_def, KIND_var, KIND_typ, CAST_arr)
 * @param tag Parsed tree node representing the symbol.
 * @param type Type of symbol.
 * @return The symbol.
 */
static symn declare(ccContext cc, ccKind kind, astn tag, symn type, symn params) {
	symn def, ptr;
	size_t size = type->size;

	if (tag == NULL || tag->kind != TOKEN_var) {
		fatal(ERR_INTERNAL_ERROR": identifier expected, not: %t", tag);
		return NULL;
	}

	if ((kind & MASK_cast) == CAST_ref) {
		size = vm_ref_size;
	}
	else if (type == cc->type_rec) {
		// struct Complex { ...
		size = 0;
	}
	def = install(cc, tag->ref.name, kind, size, type, NULL);

	if (def != NULL) {
		struct astNode arg;
		tag->ref.link = def;
		tag->type = type;

		def->file = tag->file;
		def->line = tag->line;
		def->params = params;
		def->use = tag;
		def->tag = tag;

		addUsage(def, tag);

		memset(&arg, 0, sizeof(arg));
		arg.kind = TOKEN_var;
		for (ptr = def->next; ptr; ptr = ptr->next) {
			symn arg1 = ptr->params;
			symn arg2 = params;
			int casts = 0;

			if (ptr->name == NULL) {
				continue;
			}

			// generated variable
			if (ptr->name[0] == '.') {
				continue;
			}

			if (strcmp(def->name, ptr->name) != 0) {
				continue;
			}

			while (arg1 && arg2) {
				if (arg1->type != arg2->type) {
					casts += 1;
					arg.ref.link = arg1;
					arg.type = arg1->type;
					if (!canAssign(cc, arg2, &arg, 1)) {
						arg.ref.link = arg2;
						arg.type = arg2->type;
						if (!canAssign(cc, arg1, &arg, 1)) {
							break;
						}
					}
				}
				arg1 = arg1->next;
				arg2 = arg2->next;
			}

			if (arg1 == NULL && arg2 == NULL) {
				if (ptr->params && ptr->init == NULL && casts == 0) {
					debug("Overwriting forward function: %T", ptr);
					ptr->init = lnkNode(cc, def);
					ptr = NULL;
				}
				break;
			}
		}

		if (ptr != NULL) {
			if (ptr->nest >= def->nest) {
				error(cc->rt, def->file, def->line, ERR_DECLARATION_REDEFINED, def);
				if (ptr->file && ptr->line) {
					info(cc->rt, ptr->file, ptr->line, "previously defined as `%T`", ptr);
				}
			}
			else if (cc->siff) {
				warn(cc->rt, raise_warn_typ3, def->file, def->line, WARN_DECLARATION_REDEFINED, def);
			}
		}

		// if the tag is a typename it should return an instance of this type
		if (params != NULL) {
			symn tagTyp = tagType(cc, tag);
			if (tagTyp != NULL && params->type != tagTyp) {
				error(cc->rt, tag->file, tag->line, WARN_FUNCTION_TYPENAME, tag, params->type);
			}
		}
	}

	return def;
}

/// expand compound assignment expressions like `a += 1` into `a = a + 1`
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

/// expand expression or declaration to statement 
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

static astn expandInitializerObj(ccContext cc, astn varNode, astn initObj, astn list) {
	astn keys = NULL;

	// TODO: if variable instance of object then allocate

	// translate initialization to assignment
	for (astn init = initObj->stmt.stmt; init != NULL; init = init->next) {
		if (init->kind != INIT_set) {
			error(cc->rt, init->file, init->line, ERR_INITIALIZER_EXPECTED, init);
			continue;
		}
		// key: value => variable.key = value;
		astn key = init->op.lhso;
		astn value = init->op.rhso;
		symn field = NULL;
		init->op.lhso = opNode(cc, OPER_dot, varNode, key);
		init->op.lhso->file = key->file;
		init->op.lhso->line = key->line;

		if (value->kind == STMT_beg) {
			// lookup key
			if (typeCheck(cc, NULL, init->op.lhso, 1) != NULL) {
				expandInitializerObj(cc, init->op.lhso, value, list);
			}
			init->op.rhso = init->op.lhso;
		} else {
			addTail(list, expand2Statement(cc, init, 0));

			// type check assignment
			if (typeCheck(cc, NULL, init, 1) != NULL) {
				field = linkOf(key, 1);
			}
			if (field != NULL && isStatic(field)) {
				error(cc->rt, init->file, init->line, ERR_INVALID_STATIC_FIELD_INIT, init);
			}
			init->type = cc->type_vid;
		}

		key->next = keys;
		keys = key;
	}

	// add default initializer of uninitialized fields
	size_t unionInitSize = varNode->type->size;
	for (symn field = varNode->type->fields; field; field = field->next) {
		if (isStatic(field)) {
			continue;
		}
		if (field->offs != 0) {
			unionInitSize = 0;
			break;
		}
		dieif(unionInitSize < field->size, ERR_INTERNAL_ERROR);
	}
	for (symn field = varNode->type->fields; field; field = field->next) {
		if (isStatic(field)) {
			continue;
		}
		int initialized = 0;
		// check if last usage is from the current initialization
		for (astn key = keys; key; key = key->next) {
			if (field->use == key) {
				initialized = 1;
				break;
			}
		}

		if (initialized || unionInitSize > 0) {
			if (initialized && field->size < unionInitSize) {
				warn(cc->rt, raiseWarn, varNode->file, varNode->line, ERR_PARTIAL_INITIALIZE_UNION, linkOf(varNode, 0), field);
			}
			continue;
		}

		astn defInit = field->init;
		if (defInit == NULL && !isConst(field) && !isConstVar(varNode)) {
			// only assignable members can be initialized with default type initializer.
			defInit = field->type->init;
		}

		if (defInit == NULL) {
			error(cc->rt, varNode->file, varNode->line, ERR_UNINITIALIZED_MEMBER, linkOf(varNode, 0), field);
			continue;
		}

		if (defInit == field->init) {
			warn(cc->rt, raiseDebug, varNode->file, varNode->line, WARN_USING_DEF_FIELD_INITIALIZER, field, defInit);
		} else {
			warn(cc->rt, raiseDebug, varNode->file, varNode->line, WARN_USING_DEF_TYPE_INITIALIZER, field, defInit);
		}

		astn init = newNode(cc, INIT_set);
		init->op.lhso = opNode(cc, OPER_dot, varNode, lnkNode(cc, field));
		init->op.rhso = defInit;

		addTail(list, init);

		typeCheck(cc, NULL, init, 1);
		init->type = cc->type_vid;
	}
	return list;
}

/** Expand literal initializer to initializer statement
 * complex c = {re: 5, im: 6}; => { c.re = 5; c.im = 6; };
 * TODO: int ar[3] = {1, 2, 3}; => { ar[0] = 1; ar[1] = 2; ar[2] = 3; };
 * TODO: int ar[string] = {a: 3, "b": 1}; => { ar["a"] = 3; ar["b"] = 1; };
 */
static astn expandInitializer(ccContext cc, symn variable, astn initializer) {
	dieif(initializer->kind != STMT_beg, ERR_INTERNAL_ERROR);
	dieif(initializer->lst.tail != NULL, ERR_INTERNAL_ERROR);
	astn varNode = lnkNode(cc, variable);
	varNode->file = variable->file;
	varNode->line = variable->line;
	return expandInitializerObj(cc, varNode, initializer, initializer);
}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ Parser
static astn statement_list(ccContext cc);
static astn statement(ccContext cc, ccKind attr);
static astn declaration(ccContext cc, ccKind attr, astn *args);

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
static astn expression(ccContext cc, int comma) {
	astn buff[maxTokenCount], *const base = buff + maxTokenCount;
	astn *ptr, ast, prev = NULL;
	astn *postfix = buff;			// postfix form of expression
	astn *stack = base;				// precedence stack
	char sym[maxTokenCount];		// symbol stack {, [, (, ?
	int unary = 1;					// start in unary mode
	int level = 0;					// precedence and top of symbol stack

	sym[level] = 0;

	// parse expression using Shunting-yard algorithm
	while ((ast = nextTok(cc, TOKEN_any, 0))) {
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
					if ((*stack)->op.prec < ast->op.prec) {
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

			case OPER_not:		// '!'
			case OPER_cmt:		// '~'
				if (!unary) {
					error(cc->rt, ast->file, ast->line, ERR_SYNTAX_ERR_BEFORE, ast);
				}
				unary = 1;
				goto tok_operator;

			case OPER_com:		// ','
				if (comma && level <= 0) {
					backTok(cc, ast);
					ast = NULL;
					break;
				}
				if (unary) {
					error(cc->rt, ast->file, ast->line, ERR_SYNTAX_ERR_BEFORE, ast);
					*postfix++ = NULL;
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
		char *file = cc->file;
		int line = cc->line;
		if (ast == NULL) {
			ast = peekTok(cc, TOKEN_any);
		}
		if (ast != NULL) {
			file = ast->file;
			line = ast->line;
		}
		error(cc->rt, file, line, ERR_EXPECTED_BEFORE_TOK, expected, peekTok(cc, TOKEN_any));
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
				// fall through
			case 2:
				ast->op.lhso = stack[1];
				// fall through
			case 1:
				ast->op.rhso = stack[0];
				// fall through
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

/** Parse initializer value, object and array literals.
 * @brief parse the initializer of a declaration.
 * @param cc compiler context.
 * @param var the declared variable symbol.
 * @return the initializer expression.
 */
static astn initializer(ccContext cc) {
	astn ast = peekTok(cc, STMT_beg) ? NULL : expression(cc, 1);
	symn type = NULL;
	if (ast != NULL) {
		if (typeCheck(cc, NULL, ast, 0) && isTypeExpr(ast)) {
			type = linkOf(ast, 1);
		}
	}

	if (!skipTok(cc, STMT_beg, 0)) {	// '{'
		// initializer is an expression
		return ast;
	}

	ccToken sep = STMT_end;
	astn head = NULL, tail = NULL;
	while ((ast = peekTok(cc, TOKEN_any)) != NULL) {
		switch (ast->kind) {
			default:
				break;

			case RIGHT_crl:	// '}'
				ast = NULL;
				break;
		}
		if (ast == NULL) {
			break;
		}

		// try to read: `<property> ':' <initializer>`
		ast = nextTok(cc, TOKEN_var, 0);
		astn op = nextTok(cc, PNCT_cln, 0);
		if (ast != NULL && op != NULL) {
			astn init = initializer(cc);
			op->kind = INIT_set;
			op->op.lhso = ast;
			op->op.rhso = init;
			ast = op;
		}
		else {
			if (ast != NULL) {
				backTok(cc, ast);
			}
			ast = expression(cc, 1);
		}

		if (ast == NULL) {
			// TODO: error
			break;
		}

		if (tail != NULL) {
			tail->next = ast;
		}
		else {
			head = ast;
		}
		tail = ast;

		// allow ';' or ',' as delimiter
		if (!skipTok(cc, sep, 0)) {
			if (sep == STMT_end) {
				if (head == tail && skipTok(cc, OPER_com, 0)) {
					sep = OPER_com;
				}
				else {
					astn next = peekTok(cc, TOKEN_any);
					if (head != tail && next != NULL) {
						warn(cc->rt, raiseWarn, next->file, next->line, ERR_UNMATCHED_SEPARATOR, next, STMT_end);
					}
					break;
				}
			}
			else {
				break;
			}
		}
	}

	skipTok(cc, RIGHT_crl, 1);
	ast = newNode(cc, STMT_beg);
	ast->stmt.stmt = head;
	ast->type = type;
	return ast;
}

/** Parse function parameter list.
 * @brief Parse the parameters of a function declaration.
 * @param cc compiler context.
 * @param mode
 * @return expression tree.
 */
static astn parameters(ccContext cc, symn returns, astn function) {
	astn tok = NULL;

	if (returns != NULL) {
		symn res = install(cc, ".result", KIND_var | refCast(returns), 0, returns, returns->init);
		tok = lnkNode(cc, res);
		if (function != NULL) {
			res->file = function->file;
			res->line = function->line;
		}
	}

	if (peekTok(cc, RIGHT_par) != NULL) {
		return tok;
	}

	// while there are tokens to read
	while (peekTok(cc, TOKEN_any) != NULL) {
		ccKind attr = qualifier(cc);
		astn arg = declaration(cc, attr, NULL);

		if (arg == NULL) {
			// probably parse error
			break;
		}

		symn parameter = arg->ref.link;

		// apply const qualifier
		if (attr & ATTR_cnst) {
			parameter->kind |= ATTR_cnst;
			attr &= ~ATTR_cnst;
		}

		// fixed size arrays are passed by reference
		if (castOf(parameter->type) == CAST_arr) {
			if (refCast(parameter) == CAST_val) {
				parameter->size = sizeof(vmOffs);
				parameter->kind &= ~MASK_cast;
				parameter->kind |= CAST_ref;
			}
		}

		tok = argNode(cc, tok, arg);
		if (attr != 0) {
			error(cc->rt, arg->file, arg->line, ERR_UNEXPECTED_ATTR, attr);
		}
		if (!skipTok(cc, OPER_com, 0)) {
			if (skipTok(cc, PNCT_dot3, 0)) {
				symn arr = newDef(cc, KIND_typ);

				// dynamic-size array: int a[]
				addLength(cc, arr, NULL);
				arr->size = 2 * sizeof(vmOffs);

				arr->kind = ATTR_stat | KIND_typ | CAST_arr;
				arr->offs = vmOffset(cc->rt, arr);
				arr->type = parameter->type;
				parameter->size = arr->size;
				parameter->type = arr;
				arg->type = arr;

				parameter->kind &= ~MASK_cast;
				parameter->kind |= CAST_arr;
				parameter->kind |= ATTR_varg;
			}
			break;
		}
	}
	return tok;
}

/** Parse variable or function declaration.
 * @brief parse the declaration of a variable or function.
 * @param cc compiler context.
 * @param argv out parameter for function arguments.
 * @param mode
 * @return parsed syntax tree.
 */
static astn declaration(ccContext cc, ccKind attr, astn *args) {
	// read typename
	astn tok = expression(cc, 0);
	if (tok == NULL) {
		error(cc->rt, tok->file, tok->line, ERR_INVALID_TYPE, tok);
		return NULL;
	}

	// check if the expression is a typename
	if (!typeCheck(cc, NULL, tok, 1)) {
		error(cc->rt, tok->file, tok->line, ERR_INVALID_TYPE, tok);
		return NULL;
	}
	symn type = linkOf(tok, 1);
	if (type == NULL) {
		error(cc->rt, tok->file, tok->line, ERR_INVALID_TYPE, tok);
		return NULL;
	}

	astn tag = nextTok(cc, TOKEN_var, 1);
	if (tag == NULL) {
		tag = tagNode(cc, ".variable");
	}

	ccKind cast = CAST_any;
	// TODO: inOut: int &&a;
	if (skipTok(cc, OPER_and, 0)) {
		// inOut: int &&a;
		// byRef: int &a;
		cast = CAST_ref;
	}

	// parse function parameters
	symn def = NULL, params = NULL;
	if (skipTok(cc, LEFT_par, 0)) {			// int a(...)
		astn argRoot;
		enter(cc, def);
		argRoot = parameters(cc, type, tag);
		skipTok(cc, RIGHT_par, 1);

		params = leave(cc, KIND_fun, vm_stk_align, 0, NULL);
		type = cc->type_fun;

		if (args != NULL) {
			*args = argRoot;
		}

		// parse function body
		if (peekTok(cc, STMT_beg)) {
			def = declare(cc, ATTR_stat | ATTR_cnst | KIND_fun | cast, tag, type, params);
			if ((attr & ATTR_stat) == 0 && cc->owner && isTypename(cc->owner)) {
				// mark as a virtual method
				def->kind &= ~ATTR_stat;
			}

			// enable parameter lookup
			def->fields = params;

			enter(cc, def);
			def->init = statement(cc, (ccKind) 0);
			if (def->init == NULL) {
				def->init = newNode(cc, STMT_beg);
				def->init->type = cc->type_vid;
			}
			backTok(cc, newNode(cc, STMT_end));
			leave(cc, KIND_def, -1, 0, NULL);

			// disable parameter lookup
			def->fields = NULL;
			return tag;
		}

		// unimplemented functions are function references
		cast = CAST_ref;
	}

	// parse array dimensions
	while (skipTok(cc, LEFT_sqr, 0)) {		// int a[...][][]...
		symn arr = newDef(cc, KIND_typ);

		if (peekTok(cc, RIGHT_sqr) != NULL) {
			// dynamic-size array: int a[]
			addLength(cc, arr, NULL);
			arr->size = 2 * sizeof(vmOffs);
			cast = CAST_arr;
		}
		else if (skipTok(cc, OPER_mul, 0)) {
			// unknown-size array: int a[*]
			arr->size = vm_ref_size;
			cast = CAST_ref;
		}
		else {
			// fixed-size array: int a[42]
			// associative array: TODO: int a[string]
			struct astNode value;
			int64_t length = -1;
			astn len = expression(cc, 0);
			if (len != NULL) {
				len->type = typeCheck(cc, NULL, len, 1);
			}
			if (eval(cc, &value, len)) {
				length = intValue(&value);
			}
			if (length <= 0) {
				error(cc->rt, len->file, len->line, ERR_INVALID_ARRAY_LENGTH, len);
			}
			addLength(cc, arr, len);
			arr->size = length * type->size;
			cast = CAST_val;
		}
		skipTok(cc, RIGHT_sqr, 1);

		arr->kind = ATTR_stat | KIND_typ | CAST_arr;
		arr->offs = vmOffset(cc->rt, arr);
		arr->type = type;
		type = arr;
	}

	if (cast == CAST_any) {
		cast = refCast(type);
		if (cast == CAST_any) {
			cast = CAST_val;
		}
	}

	def = declare(cc, (attr & MASK_attr) | KIND_var | cast, tag, type, params);
	if (skipTok(cc, ASGN_set, 0)) {
		astn init = initializer(cc);
		if (init && init->kind == STMT_beg) {
			init = expandInitializer(cc, def, init);
			init->type = type;
		}
		def->init = init;
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

	// inline "file.ci"
	if ((tag = nextTok(cc, TOKEN_val, 0))) {
		astn next = NULL, head = NULL, tail = NULL;
		ccToken optional = skipTok(cc, PNCT_qst, 0);
		skipTok(cc, STMT_end, 1);

		next = cc->tokNext;
		if (ccInline(cc, tag) != 0) {
			if (optional) {
				warn(cc->rt, raiseWarn, tag->file, tag->line, ERR_OPENING_FILE, tag->ref.name);
			} else {
				error(cc->rt, tag->file, tag->line, ERR_OPENING_FILE, tag->ref.name);
			}
			return NULL;
		}

		while ((tag = nextTok(cc, TOKEN_any, 0)) != NULL) {
			if (tail != NULL) {
				tail->next = tag;
			}
			else {
				head = tag;
			}
			tail = tag;
		}
		if (tail != NULL) {
			tail->next = next;
		}
		if (head != NULL) {
			cc->tokNext = head;
		}
		else {
			cc->tokNext = next;
		}
		return NULL;
	}

	if (!(tag = nextTok(cc, TOKEN_var, 1))) {
		skipTok(cc, STMT_end, 1);
		return NULL;
	}

	enter(cc, NULL);

	if (skipTok(cc, LEFT_par, 0)) {
		parameters(cc, cc->type_vid, tag);
		skipTok(cc, RIGHT_par, 1);
	}

	skipTok(cc, ASGN_set, 1);
	init = initializer(cc);
	params = leave(cc, KIND_fun, vm_stk_align, 0, NULL);
	if (init == NULL) {
		traceAst(init);
		return NULL;
	}
	if (init->kind == STMT_beg) {
		type = tagType(cc, tag);
		if (type != NULL && params != NULL) {
			// force result to be a variable
			params->type = type;
			params->size = type->size;
			params->init = type->init;
			params->kind = ATTR_paral | KIND_var;
		}
		init = expandInitializer(cc, params, init);
		for (astn n = init->stmt.stmt; n != NULL; n = n->next) {
			if (!typeCheck(cc, NULL, n, 0)) {
				error(cc->rt, n->file, n->line, ERR_INVALID_TYPE, n);
			}
			n->type = cc->type_vid;
		}
		fatal("%?s:%?u: "ERR_UNIMPLEMENTED_FEATURE": %-t", tag->file, tag->line, init);
	} else {
		type = typeCheck(cc, NULL, init, 1);
	}
	init->type = type;
	if (type == NULL) {
		// raise the error if lookup failed
		error(cc->rt, init->file, init->line, ERR_INVALID_TYPE, init);
	}
	if (params != NULL) {
		size_t offs = 0;
		if (type != NULL) {
			// update the result variable, make it inline
			params->type = type;
			params->size = type->size;
			params->init = type->init;
			params->kind = (params->kind & ~MASK_cast) | refCast(type);
		}
		for (symn param = params; param != NULL; param = param->next) {
			int usages = 0;
			for (astn use = param->use; use != NULL; use = use->ref.used) {
				if (use != param->tag) {
					usages += 1;
				}
			}

			if (!(param->kind & ATTR_paral) && (isInline(param) || usages < 2)) {
				// mark parameter as inline if it was not used more than once
				param->kind = (param->kind & ~MASK_kind) | KIND_def;
			}
			else {
				// mark params used more than once to be cached
				offs += padOffset(param->size, vm_stk_align);
				param->offs = offs;
			}
		}
		type = cc->type_fun;
	}
	else {
		type = cc->type_rec;
	}
	skipTok(cc, STMT_end, 1);

	def = declare(cc, (attr & MASK_attr) | KIND_def, tag, type, params);
	if (def != NULL) {
		def->params = params;
		def->init = init;
		def->size = 0;
	}

	return tag;
}

/** Parse record declaration.
 * @brief parse the declaration of a structure.
 * @param cc compiler context.
 * @param attr attributes: static or const.
 * @return root of declaration.
 */
static astn declare_record(ccContext cc, ccKind attr) {

	if (!skipTok(cc, RECORD_kwd, 1)) {
		traceAst(peekTok(cc, 0));
		return NULL;
	}

	astn tag = nextTok(cc, TOKEN_var, 0);
	int expose = 0;
	if (tag == NULL) {
		tag = tagNode(cc, ".anonymous");
		expose = 1;
	}

	size_t baseSize = 0;
	size_t pack = vm_mem_align;
	symn base = cc->type_rec;

	if (skipTok(cc, PNCT_cln, 0)) {			// ':' base type or packing
		astn tok = expression(cc, 0);
		if (tok != NULL) {
			// type-check the base type or packing
			if (!typeCheck(cc, NULL, tok, 0)) {
				error(cc->rt, tok->file, tok->line, ERR_INVALID_BASE_TYPE, tok);
				pack = vm_stk_align;
			}
			else if (isTypeExpr(tok)) {		// ':' extended type
				base = linkOf(tok, 1);
				for (symn ptr = base; ptr; ptr = ptr->type) {
					if (ptr == cc->type_rec) {
						error(cc->rt, tok->file, tok->line, ERR_INVALID_INHERITANCE, tok);
						break;
					}
					if (ptr == cc->type_obj) {
						baseSize = base->size;
						break;
					}
				}
				pack = vm_stk_align;
			}
			else if (tok->kind == TOKEN_val) {	// ':' packed type
				switch (refCast(tok->type)) {
					default:
						error(cc->rt, tok->file, tok->line, ERR_INVALID_BASE_TYPE, tok);
						pack = vm_stk_align;
						break;

					case CAST_i32:
					case CAST_u32:
					case CAST_i64:
					case CAST_u64:
						pack = tok->cInt;
						break;
				}
				switch (pack) {
					default:
						error(cc->rt, tok->file, tok->line, ERR_INVALID_PACK_SIZE, tok);
						pack = vm_stk_align;
						break;

					case 0:
					case 1:
					case 2:
					case 4:
					case 8:
					case 16:
					case 32:
						break;
				}
			}
			else {
				error(cc->rt, tok->file, tok->line, ERR_INVALID_BASE_TYPE, tok);
			}
		}
		else {
			error(cc->rt, cc->file, cc->line, ERR_INVALID_BASE_TYPE, peekTok(cc, TOKEN_any));
		}
	}

	skipTok(cc, STMT_beg, 1);	// '{'

	symn type = declare(cc, ATTR_stat | ATTR_cnst | KIND_typ | CAST_val, tag, base, NULL);
	enter(cc, type);
	statement_list(cc);
	type->fields = leave(cc, attr | KIND_typ, pack, baseSize, &type->size);
	if (expose) {
		// HACK: convert the type into a variable of it's own type ...?
		type->kind = KIND_var | CAST_val;
		type->type = type;

		for (symn field = type->fields; field; field = field->next) {
			symn link = install(cc, field->name, KIND_def, 0, field->type, field->tag);
			link->file = field->file;
			link->line = field->line;
		}
	}

	skipTok(cc, RIGHT_crl, 1);	// '}'
	return tag;
}

/** Parse enum declaration.
 * @brief parse the declaration of an enumeration.
 * @param cc compiler context.
 * @return root of declaration.
 */
static astn declare_enum(ccContext cc) {

	if (!skipTok(cc, ENUM_kwd, 1)) {
		traceAst(peekTok(cc, 0));
		return NULL;
	}

	astn tag = nextTok(cc, TOKEN_var, 0);
	symn base = cc->type_int;

	if (skipTok(cc, PNCT_cln, 0)) {			// ':' base type
		astn tok = expression(cc, 0);
		if (tok != NULL) {
			base = NULL;
			// type-check the base type
			if (typeCheck(cc, NULL, tok, 0) && isTypeExpr(tok)) {
				base = linkOf(tok, 1);
			}
			else {
				error(cc->rt, tok->file, tok->line, ERR_INVALID_BASE_TYPE, tok);
			}
		}
		else {
			error(cc->rt, cc->file, cc->line, ERR_INVALID_BASE_TYPE, peekTok(cc, TOKEN_any));
		}
	}

	symn type = NULL;
	if (tag != NULL) {
		type = declare(cc, ATTR_stat | ATTR_cnst | KIND_typ | CAST_val, tag, base, NULL);
		enter(cc, type);
	}

	int64_t nextValue = 0;
	astn ast = initializer(cc);
	for (astn prop = ast->stmt.stmt; prop != NULL; prop = prop->next) {
		astn id = prop->kind == INIT_set ? prop->op.lhso : prop;
		symn member = declare(cc, ATTR_stat | ATTR_cnst | KIND_def | CAST_val, id, base, NULL);
		astn value = NULL;
		if (id == prop) {
			value = intNode(cc, nextValue);
		}
		else {
			struct astNode temp;
			// TODO: type-check value
			value = prop->op.rhso;

			if (!typeCheck(cc, NULL, value, 1)) {
				continue;
			}
			switch (eval(cc, &temp, value)) {
				default:
					error(cc->rt, id->file, id->line, ERR_INVALID_VALUE_ASSIGN, member, value);
					break;

				case CAST_f32:
				case CAST_f64:
					break;

				case CAST_i32:
				case CAST_i64:
				case CAST_u32:
				case CAST_u64:
					nextValue = intValue(&temp);
					break;
			}
		}
		nextValue += 1;
		if (!canAssign(cc, member, value, 0)) {
			error(cc->rt, id->file, id->line, ERR_INVALID_VALUE_ASSIGN, member, value);
		}
		member->init = value;
		logif("ENUM", "%s:%u: enum.member: `%t` => %+T", prop->file, prop->line, prop, member);
	}

	if (type != NULL) {
		type->fields = leave(cc, KIND_typ, vm_stk_align, 0, &type->size);
	}

	return tag;
}

/** Parse if statement.
 * @brief parse: `if (condition) statement [else statement]`.
 * @param cc compiler context.
 * @param attr the qualifier of the statement (ie. static if)
 * @return the parsed syntax tree.
 */
static astn statement_if(ccContext cc, ccKind attr) {
	astn ast = nextTok(cc, STMT_if, 1);
	if (ast == NULL) {
		traceAst(ast);
		return NULL;
	}

	skipTok(cc, LEFT_par, 1);
	astn test = expression(cc, 0);
	if (test != NULL) {
		test->type = typeCheck(cc, NULL, test, 1);
		ast->stmt.test = test;
	}
	skipTok(cc, RIGHT_par, 1);

	// static if statement true branch does not enter a new scope to expose declarations.
	int enterThen = 1;
	int enterElse = 1;
	if (attr & ATTR_stat) {
		struct astNode value;
		if (eval(cc, &value, test)) {
			if (bolValue(&value)) {
				enterThen = 0;
			}
			else {
				enterElse = 0;
			}
		}
		else {
			error(cc->rt, test->file, test->line, ERR_INVALID_CONST_EXPR, test);
		}
	}

	if (enterThen) {
		enter(cc, NULL);
	}

	ast->stmt.stmt = statement(cc, (ccKind) 0);

	// parse else part if next token is 'else'
	if (skipTok(cc, ELSE_kwd, 0)) {
		if (enterThen) {
			leave(cc, KIND_def, 0, 0, NULL);
			enterThen = 0;
		}
		if (enterElse) {
			enter(cc, NULL);
			enterThen = 1;
		}
		ast->stmt.step = statement(cc, (ccKind) 0);
	}

	if (enterThen) {
		leave(cc, KIND_def, 0, 0, NULL);
	}

	return ast;
}

/** Parse for statement.
 * @brief parse: `for (init; test; step) statement`.
 * @param cc compiler context.
 * @param attr the qualifier of the statement (ie. parallel for)
 * @return the parsed syntax tree.
 */
static astn statement_for(ccContext cc, ccKind attr) {
	astn ast = nextTok(cc, STMT_for, 1);
	if (ast == NULL) {
		traceAst(ast);
		return NULL;
	}

	enter(cc, NULL);
	skipTok(cc, LEFT_par, 1);

	astn init = peekTok(cc, STMT_end) ? NULL : expression(cc, 0);
	if (init != NULL) {
		init->type = typeCheck(cc, NULL, init, 1);
		if (isTypeExpr(init)) {
			backTok(cc, init);
			init = declaration(cc, attr, NULL);
			if (init != NULL && init->ref.link->init != NULL) {
				astn ast = init->ref.link->init;
				ast->type = typeCheck(cc, NULL, ast, 1);
			}
		}
		ast->stmt.init = init;
	}

	if (peekTok(cc, PNCT_cln)) {
		// transform `for (iterator i: iterable) ...` to
		// `for (iterator $it = iterator(iterable), iterator i = $it; $it.next(); i = $it) ...`
		// transform `for (integer i: iterable) ...` to
		// `for (iterator $it = iterator(iterable), integer i; next($it, &&i); ) ...`
		fatal(ERR_UNIMPLEMENTED_FEATURE);
	}

	skipTok(cc, STMT_end, 1);
	astn test = peekTok(cc, STMT_end) ? NULL : expression(cc, 0);
	if (test != NULL) {
		test->type = typeCheck(cc, NULL, test, 1);
		ast->stmt.test = test;
	}

	skipTok(cc, STMT_end, 1);
	astn step = peekTok(cc, RIGHT_par) ? NULL : expression(cc, 0);
	if (step != NULL) {
		step->type = typeCheck(cc, NULL, step, 1);
		ast->stmt.step = step;
	}

	skipTok(cc, RIGHT_par, 1);

	ast->stmt.stmt = statement(cc, (ccKind) 0);

	leave(cc, KIND_def, 0, 0, NULL);

	ast->type = cc->type_vid;
	return ast;
}

/** Parse all the statements in the current scope.
 * @brief parse statements.
 * @param cc compiler context.
 * @return the parsed statements chained by ->next, NULL if no statements parsed.
 */
static astn statement_list(ccContext cc) {
	astn ast, head = NULL, tail = NULL;

	while ((ast = peekTok(cc, TOKEN_any)) != NULL) {

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

		ccKind attr = qualifier(cc);
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

/**
 * Parse statement.
 * 
 * @param cc compiler context.
 * @param attr the qualifier of the statement/declaration
 * @return parsed syntax tree.
 */
static astn statement(ccContext cc, ccKind attr) {
	char *file = cc->file;
	int line = cc->line;
	int validStart = 1;
	astn check = NULL;
	astn ast = NULL;

	//~ skip to the first valid statement start
	while ((ast = peekTok(cc, TOKEN_any)) != NULL) {
		switch (ast->kind) {
			default:
				// valid statement start
				file = ast->file;
				line = ast->line;
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
	if ((ast = nextTok(cc, STMT_end, 0))) {             // ;
		warn(cc->rt, raise_warn_par8, ast->file, ast->line, WARN_EMPTY_STATEMENT);
		recycle(cc, ast);
	}
	else if ((ast = nextTok(cc, STMT_beg, 0))) {   // { ... }
		ast->stmt.stmt = statement_list(cc);
		skipTok(cc, RIGHT_crl, 1);
		ast->type = cc->type_vid;

		if (attr & ATTR_paral) {
			ast->kind = STMT_pbeg;
			attr &= ~ATTR_paral;
		}

		if (ast->stmt.stmt == NULL) {
			// remove nodes such: {{;{;};{}{}}}
			recycle(cc, ast);
			ast = NULL;
		}
	}
	else if (peekTok(cc, STMT_if) != NULL) {    // if (...)
		ast = statement_if(cc, attr);
		if (ast != NULL) {
			ast->type = cc->type_vid;
			astn ifThen = ast->stmt.stmt;
			// force static if to use compound statements
			if (ifThen != NULL && ifThen->kind != STMT_beg) {
				ast->stmt.stmt = expand2Statement(cc, ifThen, 1);
				if (attr & ATTR_stat) {
					error(cc->rt, ifThen->file, ifThen->line, WARN_USE_BLOCK_STATEMENT, ifThen);
				}
				else {
					warn(cc->rt, raise_warn_par8, ifThen->file, ifThen->line, WARN_USE_BLOCK_STATEMENT, ifThen);
				}
			}
			astn ifElse = ast->stmt.step;
			if (ifElse != NULL && ifElse->kind != STMT_beg) {
				ast->stmt.step = expand2Statement(cc, ifElse, 1);
				if (attr & ATTR_stat) {
					error(cc->rt, ifElse->file, ifElse->line, WARN_USE_BLOCK_STATEMENT, ifElse);
				}
				else {
					warn(cc->rt, raise_warn_par8, ifElse->file, ifElse->line, WARN_USE_BLOCK_STATEMENT, ifElse);
				}
			}
			if (attr & ATTR_stat) {
				ast->kind = STMT_sif;
				attr &= ~ATTR_stat;
			}
		}
	}
	else if (peekTok(cc, STMT_for) != NULL) {   // for (...)
		ast = statement_for(cc, attr);
		if (ast != NULL) {
			ast->type = cc->type_vid;
			if (attr & ATTR_stat) {
				ast->kind = STMT_sfor;
				attr &= ~ATTR_stat;
			}
			else if (attr & ATTR_paral) {
				ast->kind = STMT_pfor;
				attr &= ~ATTR_paral;
			}
		}
	}
	else if ((ast = nextTok(cc, STMT_brk, 0))) {   // break;
		skipTok(cc, STMT_end, 1);
		ast->type = cc->type_vid;
	}
	else if ((ast = nextTok(cc, STMT_con, 0))) {   // continue;
		skipTok(cc, STMT_end, 1);
		ast->type = cc->type_vid;
	}
	else if ((ast = nextTok(cc, STMT_ret, 0))) {   // return expression;
		symn function = cc->owner;
		if (!skipTok(cc, STMT_end, 0)) {
			astn res = initializer(cc);

			if (res && function && isFunction(function)) {
				dieif(strcmp(function->params->name, ".result") != 0, ERR_INTERNAL_ERROR);
				if (res->kind == STMT_beg) {
					res = expandInitializer(cc, function->params, res);
					res->type = cc->type_vid;
				}
				ast->jmp.value = opNode(cc, ASGN_set, lnkNode(cc, function->params), res);
				ast->jmp.value->type = res->type;
			}
			else {
				// returning from a non function, or returning a statement?
				error(cc->rt, ast->file, ast->line, ERR_UNEXPECTED_TOKEN, ast);
			}
			skipTok(cc, STMT_end, 1);
		}
		ast->jmp.func = function;
		ast->type = cc->type_vid;
		check = ast->jmp.value;
	}

	// type, enum and alias declaration
	else if (peekTok(cc, INLINE_kwd) != NULL) { // alias
		ast = declare_alias(cc, attr);
		if (ast != NULL) {
			ast->type = cc->type_rec;
			//ast = expand2Statement(cc, ast, 0);
		}
	}
	else if (peekTok(cc, RECORD_kwd) != NULL) { // struct
		ast = declare_record(cc, attr);
		if (ast != NULL) {
			ast->type = cc->type_rec;
			//ast = expand2Statement(cc, ast, 0);
		}
		attr &= ~(ATTR_cnst | ATTR_stat);
	}
	else if (peekTok(cc, ENUM_kwd) != NULL) {   // enum
		ast = declare_enum(cc);
		if (ast != NULL) {
			ast->type = cc->type_rec;
			//ast = expand2Statement(cc, ast, 0);
		}
	}
	else if ((ast = expression(cc, 0))) {
		symn type = typeCheck(cc, NULL, ast, 1);
		if (type == NULL) {
			error(cc->rt, ast->file, ast->line, ERR_INVALID_TYPE, ast);
			type = cc->type_vid;
		}
		else if (isTypeExpr(ast)) {
			backTok(cc, ast);
			ast = declaration(cc, attr, NULL);
			if (ast != NULL) {
				type = ast->type;
				check = ast->ref.link->init;
				attr &= ~(ast->ref.link->kind & MASK_attr);
			}
		}
		else {
			check = ast;
			switch (ast->kind) {
				default:
					warn(cc->rt, raiseWarn, ast->file, ast->line, ERR_STATEMENT_EXPECTED, ast);
					break;

				case OPER_fnc:
				case ASGN_set:
					ast = expand2Statement(cc, ast, 0);
					type = cc->type_vid;
					break;
			}
		}
		skipTok(cc, STMT_end, 1);
		ast->type = type;
	}

	if (ast != NULL) {
		// raise an error if a statement is inside a record declaration
		if (cc->owner && !isFunction(cc->owner) && isTypename(cc->owner)) {
			if (ast->kind > STMT_beg && ast->kind <= STMT_end && ast->kind != STMT_sif) {
				error(cc->rt, ast->file, ast->line, ERR_DECLARATION_EXPECTED" in %T", ast, cc->owner);
			}
		}
	}
	// type check
	if (check != NULL) {
		check->type = typeCheck(cc, NULL, check, 1);
		if (check->type == NULL) {
			error(cc->rt, ast->file, ast->line, ERR_INVALID_TYPE, check);
		}
	}

	if (attr != 0) {
		if (ast != NULL) {
			file = ast->file;
			line = ast->line;
		}
		error(cc->rt, file, line, ERR_UNEXPECTED_ATTR, attr);
	}

	return ast;
}

astn ccAddUnit(ccContext cc, int init(ccContext), char *file, int line, char *text) {
	if (init != NULL && init(cc) != 0) {
		error(cc->rt, NULL, 0, ERR_OPENING_FILE, file);
		return NULL;
	}

	if (ccOpen(cc, file, line, text) != 0) {
		error(cc->rt, NULL, 0, ERR_OPENING_FILE, file);
		return NULL;
	}

	// pre read all tokens from source
	if (cc->tokNext == NULL) {
		astn ast, head = NULL, tail = NULL;
		while ((ast = nextTok(cc, TOKEN_any, 0)) != NULL) {
			if (tail != NULL) {
				tail->next = ast;
			}
			else {
				head = ast;
			}
			tail = ast;
		}
		cc->tokNext = head;
	}

	astn unit = statement_list(cc);
	if (unit != NULL) {
		unit = expand2Statement(cc, unit, 1);

		// add parsed unit to module
		if (cc->root != NULL && cc->root->kind == STMT_beg) {
			addTail(cc->root, unit);
		}
	}
	else {
		// in case of an empty file return an empty statement
		unit = newNode(cc, STMT_beg);
	}

	if (ccClose(cc) != 0) {
		return NULL;
	}
	return unit;
}

symn ccAddCall(ccContext cc, vmError call(nfcContext), const char *proto) {
	rtContext rt = cc->rt;
	size_t nfcPos = 0;
	libc nfc = NULL;
	list lst = NULL;

	symn sym = NULL;
	astn init, args = NULL;

	//~ from: int64 zxt(int64 val, int offs, int bits)
	//~ make: inline zxt(int64 val, int offs, int bits) = nfc(42);
	if(call == NULL || proto == NULL) {
		fatal(ERR_INTERNAL_ERROR);
		return NULL;
	}
	if (ccOpen(cc, NULL, 0, (char*)proto) != 0) {
		trace(ERR_INTERNAL_ERROR);
		return NULL;
	}

	init = declaration(cc, KIND_def, &args);

	if (ccClose(cc) != 0) {
		error(rt, NULL, 0, ERR_INVALID_DECLARATION, proto);
		return NULL;
	}
	if (init == NULL || init->kind != TOKEN_var) {
		error(rt, NULL, 0, ERR_INVALID_DECLARATION, proto);
		return NULL;
	}
	if ((sym = init->ref.link) == NULL) {
		error(rt, NULL, 0, ERR_INVALID_DECLARATION, proto);
		return NULL;
	}

	// allocate nodes
	rt->_end -= sizeof(struct list);
	lst = (list) rt->_end;

	rt->_beg = padPointer(rt->_beg, vm_mem_align);
	nfc = (libc) rt->_beg;
	rt->_beg += sizeof(struct libc);

	if(rt->_beg >= rt->_end) {
		fatal(ERR_MEMORY_OVERRUN);
		return NULL;
	}

	if (cc->native != NULL) {
		libc last = (libc) cc->native->data;
		nfcPos = last->offs + 1;
	}
	lst->data = (void *) nfc;
	lst->next = cc->native;
	cc->native = lst;

	init = newNode(cc, TOKEN_opc);
	init->type = sym->type;
	init->opc.code = opc_nfc;
	init->opc.args = nfcPos;

	sym->kind = ATTR_stat | ATTR_cnst | KIND_def;
	sym->init = init;

	// update the native call
	nfc->proto = proto;
	nfc->call = call;
	nfc->sym = sym;
	nfc->offs = nfcPos;
	nfc->out = 0;
	nfc->in = 0;

	if (isInvokable(sym)) {
		init->type = sym->params->type;
		sym->params->kind = (sym->params->kind & MASK_attr) | KIND_def;
		for (symn param = sym->params->next; param != NULL; param = param->next) {
			// TODO: param->kind = (param->kind & MASK_attr) | KIND_def;
			param->tag = NULL;

			// the result of a native functions is not pushed to the stack
			param->offs -= sym->params->size;

			// input is the highest parameter offset
			if (nfc->in < param->offs) {
				nfc->in = param->offs;
			}
		}
		nfc->in /= vm_stk_align;
		nfc->out = sym->params->size / vm_stk_align;
	}
	else {
		nfc->out = sym->type->size / vm_stk_align;
	}

	debug("nfc: %02X, in: %U, out: %U, func: %T", nfcPos, nfc->in, nfc->out, nfc->sym);
	return sym;
}

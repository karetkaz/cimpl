/*******************************************************************************
 *   File: parser.c
 *   Date: 2011/06/23
 *   Desc: parser
 *******************************************************************************
 * convert token stream to an abstract syntax tree.
 */

#include "internal.h"
#include <limits.h>

static const char *unknown_tag = "<?>";

const struct tokenRec token_tbl[256] = {
	#define TOKEN_DEF(Name, Type, Args, Text) {Text, Type, Args},
	#include "defs.inl"
};

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ Utils
static inline void addHead(astn list, astn node) {
	dieif(list == NULL, ERR_INTERNAL_ERROR);
	dieif(list->kind != STMT_beg, ERR_INTERNAL_ERROR);

	node->next = list->lst.head;
	list->lst.head = node;
	if (list->lst.tail == NULL) {
		list->lst.tail = node;
	}
}
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
	enter(cc, NULL, NULL);
	ccKind kind = ATTR_cnst | ATTR_stat | KIND_def | refCast(cc->type_idx);
	install(cc, "length", kind, cc->type_idx->size, cc->type_idx, init);
	sym->fields = leave(cc, KIND_def, 0, 0, NULL, NULL);
}

static inline symn tagType(ccContext cc, astn tag) {
	symn tagType = cc->symbolStack[tag->ref.hash];
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
	if (tag->type != cc->type_str) {
		return 0;
	}

	char buff[PATH_MAX];
	if (tag->ref.name && tag->ref.name[0] == '/') {
		absolutePath(cc->home, buff, sizeof(buff));
	}
	else {
		if (!absolutePath(tag->file, buff, sizeof(buff))) {
			strncpy(buff, tag->file, sizeof(buff) - 1);
		}
		// replace the file name
		char *path = strrchr(buff, '/');
		if (path == NULL) {
			// TODO: replace '\' with '/' on windows before this
			path = strrchr(buff, '\\');
		}
		if (path != NULL) {
			*(path += 1) = 0;
		}
	}
	// convert windows path names to linux path names
	for (char *ptr = buff; *ptr; ++ptr) {
		if (*ptr == '\\') {
			*ptr = '/';
		}
	}

	strncat(buff, tag->ref.name, sizeof(buff) - strlen(buff) - 1);
	char *path = relativeToCWD(buff);
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
	if (tag == NULL || tag->kind != TOKEN_var) {
		fatal(ERR_INTERNAL_ERROR": identifier expected, not: %t", tag);
		return NULL;
	}

	size_t size = type->size;
	switch (kind & MASK_kind) {
		case KIND_def:
		case KIND_typ:
			// declaring a new type or alias, start with size 0
			// struct Complex { ...
			size = 0;
			break;

		case KIND_var:
		case KIND_fun:
			if ((kind & MASK_cast) == CAST_ref) {
				size = vm_ref_size;
			}
			break;
	}
	symn def = install(cc, tag->ref.name, kind, size, type, NULL);

	if (def != NULL) {
		tag->ref.link = def;
		tag->type = type;

		def->unit = cc->unit;
		def->file = tag->file;
		def->line = tag->line;
		def->params = params;
		def->use = tag;
		def->tag = tag;

		addUsage(def, tag);

		for (symn ptr = def->next; ptr; ptr = ptr->next) {
			if (ptr->name == NULL) {
				continue;
			}

			// generated variable
			if (ptr->name[0] == '.') {
				continue;
			}

			if (def->tag == ptr->tag && def->params == ptr->params) {
				// same definition, static virtual method
				continue;
			}

			if (strcmp(def->name, ptr->name) != 0) {
				continue;
			}

			if (!canAssign(cc, ptr, tag, 1)) {
				continue;
			}

			// test if override is possible
			if (ptr->owner != NULL && def->owner != NULL && isObjectType(def->owner)) {
				if (isFunction(def) && !isStatic(def) && isVariable(ptr) && isInvokable(ptr)) {
					warn(cc->rt, raise_warn_redef, def->file, def->line, "Overriding virtual function: %T", ptr);
					def->override = ptr;
					break;
				}
			}

			if (kind == (ATTR_stat|KIND_typ|CAST_vid)) {
				warn(cc->rt, raise_warn_redef, def->file, def->line, "Extending static namespace: %T", ptr);
				def->fields = ptr->fields;
				break;
			}

			// test if overwrite is possible (forward function implementation)
			if (ptr->init == NULL && ptr->owner == def->owner && def->nest == ptr->nest) {
				if (isFunction(def) && isVariable(ptr) && isInvokable(ptr)) {
					warn(cc->rt, raise_warn_redef, def->file, def->line, "Overwriting forward function: %T", ptr);
					ptr->init = lnkNode(cc, def);
					break;
				}
			}

			if (ptr->owner == def->owner && ptr->nest >= def->nest && strcmp(def->name, unknown_tag) != 0) {
				error(cc->rt, def->file, def->line, ERR_DECLARATION_REDEFINED, def);
				if (ptr->file && ptr->line) {
					info(cc->rt, ptr->file, ptr->line, "previously defined as `%T`", ptr);
				}
				break;
			}
			else {
				warn(cc->rt, raise_warn_redef, def->file, def->line, WARN_DECLARATION_REDEFINED, def);
				if (ptr->file && ptr->line) {
					warn(cc->rt, raise_warn_redef, ptr->file, ptr->line, "previously defined as `%T`", ptr);
				}
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
	ssize_t length = 0;

	symn type = varNode->type;
	if (initObj->type != NULL) {
		type = initObj->type;
	}

	// translate initialization to assignment
	if (initObj->stmt.stmt != NULL && initObj->stmt.stmt->kind == INIT_set) {
		length = -1;
	}

	for (astn init = initObj->stmt.stmt; init != NULL; init = init->next) {
		astn key = NULL;
		astn value = init;
		if (length < 0 && init->kind == INIT_set) {
			key = init->op.lhso;
			value = init->op.rhso;
			// key: value => variable.key = value;
			init->op.lhso = opNode(cc, NULL, OPER_dot, varNode, key);
			if (!typeCheck(cc, NULL, init->op.lhso, 0)) {
				// HACK: type of variable may differ from the initializer type:
				// object view = Button{ ... };
				typeCheck(cc, type, key, 0);
			}
			init->op.lhso->file = key->file;
			init->op.lhso->line = key->line;
		} else {
			key = intNode(cc, length);
			key->file = value->file;
			key->line = value->line;

			key = opNode(cc, NULL, OPER_idx, varNode, key);
			key->file = value->file;
			key->line = value->line;

			init = opNode(cc, NULL, INIT_set, key, value);
			init->file = value->file;
			init->line = value->line;
			init->next = value->next;
			length += 1;
		}

		if (value->kind == STMT_beg) {
			// lookup key
			if (typeCheck(cc, NULL, init->op.lhso, 1) != NULL) {
				expandInitializerObj(cc, init->op.lhso, value, list);
			}
			init->op.rhso = init->op.lhso;
		} else {
			addTail(list, expand2Statement(cc, init, 0));

			// type check assignment
			symn field = NULL;
			if (typeCheck(cc, NULL, init, 1) != NULL) {
				field = linkOf(key, 1);
			}
			if (field != NULL && !canAssign(cc, field, value, 0)) {
				error(cc->rt, value->file, value->line, ERR_INVALID_INITIALIZER, init);
			}
			if (field != NULL && isStatic(field)) {
				error(cc->rt, init->file, init->line, ERR_INVALID_STATIC_FIELD_INIT, init);
			}
			init->type = cc->type_vid;
		}

		key->next = keys;
		keys = key;
	}

	if (isArrayType(type)) {
		symn len = type->fields;
		if (len == NULL || len == cc->length_ref) {
			symn ref = linkOf(varNode, 0);
			if (varNode->kind != TOKEN_var || (isMember(ref) && !isStatic(ref))) {
				// non static members can not expand their size, raise error, that this is not possible yet
				error(cc->rt, varNode->file, varNode->line, ERR_INVALID_INITIALIZER, varNode);
				return NULL;
			}

			// if slice or pointer is initialized with a literal,
			// create a fixed size array from the literal,
			// then assign it to the slice or pointer
			size_t initSize = length * refSize(type->type);
			symn arr = newDef(cc, ATTR_stat | KIND_typ | CAST_arr);
			symn var = newDef(cc, KIND_var | CAST_val);

			addLength(cc, arr, intNode(cc, length));
			var->file = arr->file = varNode->file;
			var->line = arr->line = varNode->line;
			var->name = "init";

			arr->type = type->type;
			var->owner = ref;
			var->type = arr;

			var->size = arr->size = initSize;
			var->offs = ref->size;
			ref->size += initSize;

			// slice = slice.array
			astn arrArr = opNode(cc, var->type, OPER_dot, lnkNode(cc, ref), lnkNode(cc, var));
			astn setLen = opNode(cc, cc->type_vid, INIT_set, lnkNode(cc, ref), arrArr);
			arrArr->op.lhso->type = cc->emit_opc;	// FIXME: force ignore indirection of slice
			setLen->file = ref->file;
			setLen->line = ref->line;
			addHead(list, setLen);

			// for performance initialize the fixed size array
			// slice[0] = 42 => slice.array[0] = 1
			*varNode = *arrArr;
		}
		else if (length > intValue(len->init)) {
			error(cc->rt, varNode->file, varNode->line, ERR_INVALID_ARRAY_VALUES, len->init);
		}
	}

	// if variable is instance of object then allocate it
	if (isObjectType(type)) {
		if (cc->libc_new == NULL) {
			error(cc->rt, varNode->file, varNode->line, "Failed to allocate variable `%t` allocator not found or disabled", varNode);
			return initObj;
		}

		astn new = lnkNode(cc, cc->libc_new);
		astn arg = lnkNode(cc, type);
		astn call = opNode(cc, type, OPER_fnc, new, arg);
		call->file = arg->file = new->file = initObj->file;
		call->line = arg->line = new->line = initObj->line;

		astn init = opNode(cc, cc->type_vid, INIT_set, varNode, call);
		init->file = varNode->file;
		init->line = varNode->line;

		// make allocation the first statement
		addHead(list, expand2Statement(cc, init, 0));
	}

	// add default initializer of uninitialized fields
	size_t unionInitSize = type->size;
	for (symn field = type->fields; field; field = field->next) {
		if (isStatic(field)) {
			continue;
		}
		if (field->offs != 0) {
			unionInitSize = 0;
			break;
		}
		dieif(unionInitSize != 0 && unionInitSize < field->size, ERR_INTERNAL_ERROR);
	}
	for (symn field = type->fields; field; field = field->next) {
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

		if (field->name != NULL && *field->name == '.') {
			// no need to initialize hidden internal fields (.result, .type)
			continue;
		}
		if (field == cc->length_ref) {
			// no need to initialize length field of a slice
			continue;
		}

		astn defInit = field->init;
		for (symn ovr = type->fields; ovr != NULL; ovr = ovr->next) {
			if (field->tag == ovr->tag) {
				continue;
			}
			if (field != ovr->override) {
				continue;
			}
			if (ovr->init != NULL) {
				defInit = lnkNode(cc, ovr);
				break;
			}
		}

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

		astn fld = opNode(cc, NULL, OPER_dot, varNode, lnkNode(cc, field));
		astn init = opNode(cc, NULL, INIT_set, fld, defInit);
		addTail(list, expand2Statement(cc, init, 0));

		typeCheck(cc, NULL, init, 1);
		init->type = cc->type_vid;
	}
	return list;
}

/** Expand literal initializer to initializer statement
 * complex c = {re: 5, im: 6}; => { c.re = 5; c.im = 6; };
 * int ar[3] = {1, 2, 3}; => { ar[0] = 1; ar[1] = 2; ar[2] = 3; };
 * TODO: int ar[string] = {a: 3, "b": 1}; => { ar["a"] = 3; ar["b"] = 1; };
 */
static astn expandInitializer(ccContext cc, symn variable, astn initializer) {
	dieif(initializer->kind != STMT_beg, ERR_INTERNAL_ERROR);
	dieif(initializer->lst.tail != NULL, ERR_INTERNAL_ERROR);
	if (initializer->type == NULL) {
		// initializer type not specified, use from declaration
		initializer->type = variable->type;
	}
	if (!canAssign(cc, variable, initializer, 0)) {
		error(cc->rt, variable->file, variable->line, ERR_INVALID_VALUE_ASSIGN, variable, initializer);
		return initializer;
	}
	astn varNode = lnkNode(cc, variable);
	varNode->file = variable->file;
	varNode->line = variable->line;
	return expandInitializerObj(cc, varNode, initializer, initializer);
}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ Parser
static astn statement_list(ccContext cc);
static astn statement(ccContext cc, const char *doc);
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
				error(cc->rt, ast->file, ast->line, ERR_UNEXPECTED_QUAL, ast);
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
		int l2r, precedence = level << 4;

		// statement tokens are not allowed in expressions !!!!
		if (ast->kind >= STMT_beg && ast->kind <= STMT_end) {
			backTok(cc, ast);
			break;
		}

		switch (ast->kind) {
			default:
				// operator
				if (token_tbl[ast->kind].args > 0) {
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
				precedence |= token_tbl[ast->kind].type & 0x0f;
				l2r = token_tbl[ast->kind].type & 0x10;
				ast->op.prec = precedence;
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

			case OPER_and:		// '&'
				if (unary) {
					ast->kind = OPER_adr;
				}
				unary = 1;
				goto tok_operator;

			case OPER_not:		// '!'
			case OPER_cmt:		// '~'
			case PNCT_dot3:		// '...'
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
		const char *file = cc->file;
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
		ast = *ptr;

		if (ast == NULL) {
			*--stack = ast;
			continue;
		}

		int args = token_tbl[ast->kind].args;
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
	if (ast != NULL && !isTypeExpr(ast)) {
		// avoid value initializers: Rect x = roi {};
		error(cc->rt, ast->file, ast->line, ERR_INVALID_BASE_TYPE, ast);
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

			case STATIC_kwd:
				// allow static declarations in initializer
				declaration(cc, qualifier(cc), NULL);
				skipTok(cc, STMT_end, 1);
				continue;
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
			ast = initializer(cc);
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
						error(cc->rt, next->file, next->line, ERR_UNMATCHED_SEPARATOR, next, STMT_end);
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
			res->unit = cc->unit;
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
		astn arg = declaration(cc, 0, NULL);

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
				parameter->size = vm_ref_size;
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
				arr->size = 2 * vm_ref_size;

				// int fun(int a, int b, int refs&...)
				if (castOf(parameter) == CAST_ref && castOf(parameter->type) != CAST_ref) {
					error(cc->rt, arg->file, arg->line, ERR_INVALID_TYPE, arg);
				}

				arr->kind = ATTR_stat | KIND_typ | CAST_arr;
				arr->offs = vmOffset(cc->rt, arr);
				arr->type = parameter->type;
				parameter->size = arr->size;
				parameter->type = arr;
				arg->type = arr;

				parameter->kind &= ~MASK_cast;
				parameter->kind |= CAST_arr;
				parameter->kind |= ARGS_varg;
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
		error(cc->rt, cc->file, cc->line, ERR_INVALID_TYPE, tok);
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
		enter(cc, tag, def);
		argRoot = parameters(cc, type, tag);
		skipTok(cc, RIGHT_par, 1);

		params = leave(cc, KIND_fun, vm_stk_align, 0, NULL, NULL);
		type = cc->type_fun;

		if (args != NULL) {
			*args = argRoot;
		}

		// parse function body
		if (peekTok(cc, STMT_beg)) {
			int isMember = (attr & ATTR_stat) == 0;
			def = declare(cc, KIND_fun | attr | cast, tag, type, params);
			if (isMember && def->override == NULL && cc->owner && isTypename(cc->owner) && !isStatic(cc->owner)) {
				warn(cc->rt, raise_warn_redef, def->file, def->line, "Creating virtual method for: %T", def);
				symn method = declare(cc, ATTR_cnst | KIND_var | CAST_ref, tag, type, params);
				method->init = lnkNode(cc, def);
			}

			// enable parameter lookup
			def->fields = params;

			enter(cc, tag, def);
			def->init = statement(cc, NULL);
			if (def->init == NULL) {
				def->init = newNode(cc, STMT_beg);
				def->init->type = cc->type_vid;
			}
			backTok(cc, newNode(cc, STMT_end));
			leave(cc, KIND_def, -1, 0, NULL, NULL);

			// mark as static and disable parameter lookup
			def->kind |= ATTR_stat;
			def->fields = NULL;
			return tag;
		}

		if (cast != CAST_any) {	// error: int a&(int x) {...}
			error(cc->rt, tag->file, tag->line, ERR_INVALID_TYPE, tag);
		}

		// unimplemented functions are function references
		cast = CAST_ref;
	}

	// parse array dimensions
	int arrDepth = 0;
	while (skipTok(cc, LEFT_sqr, 0)) {		// int a[...][][]...

		arrDepth += 1;
		if (cast != CAST_any && arrDepth == 1) {	// error: int a&[100]
			error(cc->rt, tag->file, tag->line, ERR_INVALID_TYPE, tag);
		}

		symn arr = newDef(cc, KIND_typ);
		if (peekTok(cc, RIGHT_sqr) != NULL) {
			// dynamic-size array: `int a[]`
			addLength(cc, arr, NULL);
			arr->size = 2 * vm_ref_size;
			cast = CAST_arr;
		}
		else if (skipTok(cc, OPER_mul, 0)) {
			// unknown-size array: `int a[*]`
			arr->size = vm_ref_size;
			cast = CAST_ref;
		}
		else {
			// TODO: dynamic allocated array: `int a[n]`
			// TODO: associative array: `int a[string]`
			// TODO: sparse array: `int a[int]`
			// fixed-size array: `int a[42]`
			int64_t length = -1;
			const char *file = cc->file;
			int line = cc->line;
			astn len = expression(cc, 0);
			if (len != NULL) {
				len->type = typeCheck(cc, NULL, len, 1);
				line = len->line;
				file = len->file;
				if (eval(cc, len, len)) {
					length = intValue(len);
				}
			}
			if (length <= 0) {
				error(cc->rt, file, line, ERR_INVALID_ARRAY_LENGTH, len);
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
		if (init != NULL && init->kind == STMT_beg) {
			init = expandInitializer(cc, def, init);
			if (init != NULL) {
				init->type = type;
			}
		}
		else if (init != NULL && init->type != cc->emit_opc) {
			if (!canAssign(cc, def, init, 0)) {
				error(cc->rt, tag->file, tag->line, ERR_INVALID_INITIALIZER, init);
			}
		}
		def->init = init;
	}
	else if ((attr & ATTR_cnst) != 0) {
		// constant variables must be initialized
		if (cc->owner == NULL || cc->owner->nest == cc->nest) {
			error(cc->rt, tag->file, tag->line, ERR_UNINITIALIZED_CONSTANT, def);
		}
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
		ccToken optional = skipTok(cc, PNCT_qst, 0);
		skipTok(cc, STMT_end, 1);

		if (!cc->inStaticIfFalse && ccInline(cc, tag) != 0) {
			if (optional) {
				warn(cc->rt, raiseWarn, tag->file, tag->line, ERR_OPENING_FILE, tag->ref.name);
			} else {
				error(cc->rt, tag->file, tag->line, ERR_OPENING_FILE, tag->ref.name);
			}
			return NULL;
		}
		return NULL;
	}

	if (!(tag = nextTok(cc, TOKEN_var, 1))) {
		skipTok(cc, STMT_end, 1);
		return NULL;
	}

	enter(cc, tag, NULL);

	if (skipTok(cc, LEFT_par, 0)) {
		parameters(cc, cc->type_vid, tag);
		skipTok(cc, RIGHT_par, 1);
	}

	skipTok(cc, ASGN_set, 1);
	init = initializer(cc);
	params = leave(cc, KIND_fun, vm_stk_align, 0, NULL, NULL);
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
		/* TODO: evaluate inline expressions
		struct astNode value;
		if (eval(cc, &value, init)) {
			init = dupNode(cc, &value);
		}*/
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
		// allow: `inline sin = double.sin;`
		symn initLnk = linkOf(init, 0);
		if (initLnk && initLnk->params) {
			params = initLnk->params;
		}
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
		tag = tagNode(cc, unknown_tag);
		expose = 1;
	}

	size_t baseSize = 0;
	size_t pack = vm_mem_align;
	ccKind cast = CAST_val;
	symn base = NULL;

	if (skipTok(cc, PNCT_cln, 0)) {			// ':' base type or packing
		astn tok = expression(cc, 0);
		if (tok != NULL) {
			// type-check the base type or packing
			if (!typeCheck(cc, NULL, tok, 0)) {
				error(cc->rt, tok->file, tok->line, ERR_INVALID_BASE_TYPE, tok);
				pack = vm_stk_align;
			}
			else if (isTypeExpr(tok)) {		// ':' extended type
				cast = CAST_ref;
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

	if (attr & ATTR_stat) {
		// make not instantiable
		cast = CAST_vid;
	}

	symn type = base == NULL ? cc->type_rec : base;
	type = declare(cc, KIND_typ | attr | cast, tag, type, NULL);

	symn fields = NULL;
	if (attr == ATTR_stat && type->fields != NULL) {
		fields = type->fields;
	} else {
		fields = base == NULL ? NULL : base->fields;
		type->fields = fields; // allow lookup of fields from base type
	}
	enter(cc, tag, type);
	statement_list(cc);
	type->fields = leave(cc, KIND_typ | attr, pack, baseSize, &type->size, fields);
	type->kind |= ATTR_stat | ATTR_cnst;
	if (expose) {
		// HACK: convert the type into a variable of it's own type ...?
		type->kind = KIND_var | CAST_val;
		type->type = type;

		for (symn field = type->fields; field; field = field->next) {
			symn link = install(cc, field->name, KIND_def, 0, field->type, field->tag);
			link->unit = field->unit;
			link->file = field->file;
			link->line = field->line;
			link->doc = field->doc;
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
		enter(cc, tag, type);
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

				case CAST_bit:
				case CAST_f32:
				case CAST_f64:
					value = dupNode(cc, &temp);
					break;

				case CAST_i32:
				case CAST_i64:
				case CAST_u32:
				case CAST_u64:
					nextValue = intValue(&temp);
					value = dupNode(cc, &temp);
					break;
			}
		}
		nextValue += 1;
		if (!canAssign(cc, member, value, 0)) {
			error(cc->rt, id->file, id->line, ERR_INVALID_VALUE_ASSIGN, member, value);
		}
		member->init = castTo(cc, value, type == NULL ? base : type);
		// enumeration values are documented public symbols
		member->doc = member->name;

		debug("%s:%u: enum.member: `%t` => %+T", prop->file, prop->line, prop, member);
	}

	if (type != NULL) {
		type->fields = leave(cc, KIND_typ, vm_stk_align, 0, &type->size, NULL);
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
	// static if statement true branch does not enter a new scope to expose declarations.
	int staticIf = attr & ATTR_stat;
	int enterThen = 1;
	int enterElse = 1;

	if (test != NULL) {
		test->type = typeCheck(cc, NULL, test, 1);
		if (staticIf) {
			struct astNode value;
			if (eval(cc, &value, test)) {
				if (bolValue(&value)) {
					enterThen = 0;
				} else {
					enterElse = 0;
				}
			} else {
				error(cc->rt, test->file, test->line, ERR_INVALID_CONST_EXPR, test);
			}
			if (enterThen) {
				enter(cc, ast, NULL);
			}
		} else {
			if (enterThen) {
				enter(cc, ast, NULL);
			}
			if (isTypeExpr(test)) {
				backTok(cc, test);
				astn def = declaration(cc, 0, NULL);
				if (def != NULL && def->ref.link->init != NULL) {
					def->type = typeCheck(cc, NULL, def, 1);
					ast->stmt.init = def;
					if (castOf(def->ref.link) == CAST_ref) {
						test = opNode(cc, NULL, OPER_cne, lnkNode(cc, def->ref.link), tagNode(cc, "null"));
						test->type = typeCheck(cc, NULL, test, 1);
					} else {
						test = lnkNode(cc, def->ref.link);
						test->type = typeCheck(cc, NULL, test, 1);
					}
				}
			}
		}
		ast->stmt.test = test;
	}
	skipTok(cc, RIGHT_par, 1);

	int insideStaticIf = cc->inStaticIfFalse;
	cc->inStaticIfFalse = insideStaticIf || (staticIf && enterThen);
	ast->stmt.stmt = statement(cc, NULL);

	// parse else part if next token is 'else'
	if (skipTok(cc, ELSE_kwd, 0)) {
		if (enterThen) {
			leave(cc, KIND_def, 0, 0, NULL, NULL);
			enterThen = 0;
		}
		if (enterElse) {
			enter(cc, ast, NULL);
			enterThen = 1;
		}
		cc->inStaticIfFalse = insideStaticIf || (staticIf && enterElse);
		ast->stmt.step = statement(cc, NULL);
	}

	if (enterThen) {
		leave(cc, KIND_def, 0, 0, NULL, NULL);
	}
	cc->inStaticIfFalse = insideStaticIf;

	return ast;
}

/** Parse for statement.
 * @brief parse: `for (init; test; step) statement`.
 * @param cc compiler context.
 * @param attr the qualifier of the statement (ie. parallel for)
 * @return the parsed syntax tree.
 */
static astn statement_for(ccContext cc) {
	astn ast = nextTok(cc, STMT_for, 1);
	if (ast == NULL) {
		traceAst(ast);
		return NULL;
	}

	enter(cc, ast, NULL);
	skipTok(cc, LEFT_par, 1);

	astn init = peekTok(cc, STMT_end) ? NULL : expression(cc, 0);
	if (init != NULL) {
		init->type = typeCheck(cc, NULL, init, 1);
		if (isTypeExpr(init)) {
			backTok(cc, init);
			init = declaration(cc, 0, NULL);
			if (init != NULL && init->ref.link->init != NULL) {
				astn ast = init->ref.link->init;
				ast->type = typeCheck(cc, NULL, ast, 1);
			}
		}
		ast->stmt.init = init;
	}

	if (peekTok(cc, PNCT_cln)) {
		// transform `for (type value : values) {...}` to
		// `for (type value, iterator .it = iterator(values); .it.next(&value); ) {...}`
		// `for (iterator value = iterator(values); value.next(); ) {...}`
		// depending on the result type of: `iterator(values)`
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

	ast->stmt.stmt = statement(cc, NULL);

	leave(cc, KIND_def, 0, 0, NULL, NULL);

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

	const char *doc = NULL;
	while ((ast = cc->tokNext) != NULL) {
		switch (ast->kind) {
			default:
				break;

			case TOKEN_doc:
				cc->tokNext = cc->tokNext->next;
				doc = ast->ref.name;
				continue;

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

		if ((ast = statement(cc, doc))) {
			if (tail != NULL) {
				tail->next = ast;
			}
			else {
				head = ast;
			}
			tail = ast;
		}
		doc = NULL;
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
static astn statement(ccContext cc, const char *doc) {
	const char *file = cc->file;
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

	// parse the statement
	ccKind attr = qualifier(cc);
	if ((ast = nextTok(cc, STMT_end, 0))) {             // ;
		warn(cc->rt, raise_warn_par8, ast->file, ast->line, WARN_EMPTY_STATEMENT);
		recycle(cc, ast);
		ast = NULL;
	}
	else if ((ast = nextTok(cc, STMT_beg, 0))) {   // { ... }
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
				else if (ifElse->kind != STMT_if) {
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
		ast = statement_for(cc);
		if (ast != NULL) {
			ast->type = cc->type_vid;
			if (attr & ATTR_stat) {
				ast->kind = STMT_sfor;
				attr &= ~ATTR_stat;
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
			astn result = initializer(cc);

			if (result && function && isFunction(function)) {
				dieif(strcmp(function->params->name, ".result") != 0, ERR_INTERNAL_ERROR);
				if (result->kind == STMT_beg) {
					result = expandInitializer(cc, function->params, result);
					result->type = cc->type_vid;
				}
				ast->jmp.value = opNode(cc, result->type, ASGN_set, lnkNode(cc, function->params), result);
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
			dieif(ast->kind != TOKEN_var, ERR_INTERNAL_ERROR);
			ast->type = cc->type_rec;
			ast->ref.link->doc = doc;
			doc = NULL;
			//ast = expand2Statement(cc, ast, 0);
		}
	}
	else if (peekTok(cc, RECORD_kwd) != NULL) { // struct
		ast = declare_record(cc, attr);
		if (ast != NULL) {
			dieif(ast->kind != TOKEN_var, ERR_INTERNAL_ERROR);
			ast->type = cc->type_rec;
			ast->ref.link->doc = doc;
			doc = NULL;
			//ast = expand2Statement(cc, ast, 0);
		}
		attr &= ~(ATTR_cnst | ATTR_stat);
	}
	else if (peekTok(cc, ENUM_kwd) != NULL) {   // enum
		ast = declare_enum(cc);
		if (ast != NULL) {
			dieif(ast->kind != TOKEN_var, ERR_INTERNAL_ERROR);
			ast->type = cc->type_rec;
			ast->ref.link->doc = doc;
			doc = NULL;
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
				dieif(ast->kind != TOKEN_var, ERR_INTERNAL_ERROR);
				attr &= ~(ast->ref.link->kind & MASK_attr);
				check = ast->ref.link->init;
				type = ast->type;
				ast->ref.link->doc = doc;
				doc = NULL;
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
		if (ast != NULL) {
			ast->type = type;
		}
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

	if (ast != NULL) {
		file = ast->file;
		line = ast->line;
	}
	if (doc != NULL) {
		warn(cc->rt, raiseWarn, file, line, ERR_INVALID_DOC_COMMENT);
	}
	if (attr != 0) {
		error(cc->rt, file, line, ERR_UNEXPECTED_ATTR, attr);
	}

	return ast;
}

astn ccAddUnit(ccContext cc, int init(ccContext), const char *file, int line, char *text) {
	cc->unit = file;
	if (init != NULL && init(cc) != 0) {
		error(cc->rt, NULL, 0, ERR_OPENING_FILE, file);
		return NULL;
	}

	int nest = cc->nest;
	if (ccOpen(cc, file, line, text) != 0) {
		error(cc->rt, NULL, 0, ERR_OPENING_FILE, file);
		return NULL;
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
	if (nest != cc->nest) {
		return NULL;
	}
	return unit;
}

symn ccAddCall(ccContext cc, vmError call(nfcContext), const char *proto) {

	//~ from: int64 zxt(int64 val, int offs, int bits)
	//~ make: inline zxt(int64 val, int offs, int bits) = nfc(42);
	if (call == NULL || proto == NULL) {
		fatal(ERR_INTERNAL_ERROR);
		return NULL;
	}

	int nest = cc->nest;
	if (ccOpen(cc, NULL, 0, (char*)proto) != 0) {
		trace(ERR_INTERNAL_ERROR);
		return NULL;
	}

	astn args = NULL;
	astn init = declaration(cc, KIND_def, &args);

	rtContext rt = cc->rt;
	if (ccClose(cc) != 0) {
		error(rt, NULL, 0, ERR_INVALID_DECLARATION, proto);
		return NULL;
	}
	if (init == NULL || init->kind != TOKEN_var) {
		error(rt, NULL, 0, ERR_INVALID_DECLARATION, proto);
		return NULL;
	}
	symn sym = init->ref.link;
	if (sym == NULL) {
		error(rt, NULL, 0, ERR_INVALID_DECLARATION, proto);
		return NULL;
	}
	if (nest != cc->nest) {
		error(rt, NULL, 0, ERR_INVALID_DECLARATION, proto);
		return NULL;
	}

	// allocate nodes
	rt->_end -= sizeof(struct list);
	list lst = (list) rt->_end;

	rt->_beg = padPointer(rt->_beg, vm_mem_align);
	libc nfc = (libc) rt->_beg;
	rt->_beg += sizeof(struct libc);

	if(rt->_beg >= rt->_end) {
		fatal(ERR_MEMORY_OVERRUN);
		return NULL;
	}

	size_t nfcPos = 0;
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
	// native calls are undocumented public symbols
	sym->unit = NULL;

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

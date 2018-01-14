
#ifndef CMPL_TYPE_H
#define CMPL_TYPE_H

#include "ccCore.h"


//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ Types

/// Symbol node: types and variables
struct symNode {
	char	*name;		// symbol name
	char	*file;		// declared in file
	int32_t line;		// declared on line
	int32_t nest;		// declared on scope level
	size_t	size;		// variable or function size
	size_t	offs;		// address of variable or function

	symn	next;		// next symbol: field / param / ... / (in scope table)
	symn	type;		// base type of array / typename (void, int, float, struct, function, ...)
	symn	owner;		// the type that declares the current symbol (DeclaringType)
	symn	fields;		// all fields: static + non static
	symn	params;		// function parameters, return value is the first parameter.

	ccKind	kind;		// KIND_def / KIND_typ / KIND_var / KIND_fun + ATTR_xxx + CAST_xxx

	// TODO: merge scope and global attributes
	symn	scope;		// global variables and functions / while_compiling variables of the block in reverse order
	symn	global;		// all static variables and functions
	astn	init;		// VAR init / FUN body, this should be null after code generation?

	astn	use, tag;		// TEMP: usage / declaration reference
	const char*	format;		// TEMP: print format
};

static inline int isConst(symn sym) {
	return (sym->kind & ATTR_cnst) != 0;
}
static inline int isStatic(symn sym) {
	return (sym->kind & ATTR_stat) != 0;
}
static inline int isInline(symn sym) {
	return (sym->kind & MASK_kind) == KIND_def;
}
static inline int isFunction(symn sym) {
	return (sym->kind & MASK_kind) == KIND_fun;
}
static inline int isVariable(symn sym) {
	return (sym->kind & MASK_kind) == KIND_var;
}
static inline int isTypename(symn sym) {
	return (sym->kind & MASK_kind) == KIND_typ;
}
static inline int isArrayType(symn sym) {
	return (sym->kind & (MASK_kind | MASK_cast)) == (KIND_typ | CAST_arr);
}
static inline int isInvokable(symn sym) {
	return sym->params != NULL;
}
static inline ccKind castOf(symn sym) {
	return sym->kind & MASK_cast;
}

size_t argsSize(symn function);

/// Allocate a symbol node.
symn newDef(ccContext cc, ccKind kind);

/// Enter a new scope.
void enter(ccContext cc, symn owner);

/// Leave current scope.
symn leave(ccContext cc, ccKind mode, size_t align, size_t baseSize, size_t *size);

/// Install a new symbol in the current scope (typename, variable, function or alias).
symn install(ccContext cc, const char *name, ccKind kind, size_t size, symn type, astn init);

/// Lookup an identifier.
symn lookup(ccContext cc, symn sym, astn ast, astn args, int raise);

/**
 * Check if a value can be assigned to a symbol.
 * 
 * @param rhs variable to be assigned to.
 * @param val value to be assigned.
 * @param strict Strict mode: casts are not enabled.
 * @return cast of the assignment if it can be done.
 */
ccKind canAssign(ccContext cc, symn rhs, astn val, int strict);

/**
 * Type Check an expression.
 * 
 * @param loc Override scope.
 * @param ast Tree to check.
 * @return Type of expression.
 */
symn typeCheck(ccContext cc, symn loc, astn ast, int raise);

/**
 * Type Check initialization of a variable.
 * 
 * @param sym the variable to be checked.
 * @param raise Report errors.
 * @return Type of expression.
 */
symn initCheck(ccContext cc, symn sym, int raise);

/**
 * @brief Add usage of the symbol.
 * @param sym Symbol typename or variable.
 * @param tag The node of the usage.
 */
void addUsage(symn sym, astn tag);

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ Syntax

/// Abstract syntax tree node
struct astNode {
	ccToken		kind;				// token kind: operator, statement, ...
	symn		type;				// token type: return
	astn		next;				// next token / next argument / next statement
	char		*file;				// file name of the token belongs to
	int32_t		line;				// line position of token
	union {							// token value
		//char *cStr;				// constant string value (use: ref.name)
		int64_t cInt;				// constant integer value
		float64_t cFlt;				// constant floating point value
		struct {					// STMT_xxx: statement
			astn	stmt;			// statement / then block
			astn	step;			// increment / else block
			astn	test;			// condition: if, for
			astn	init;			// for statement init
		} stmt;
		struct {					// OPER_xxx: operator
			astn	rhso;			// right hand side operand
			astn	lhso;			// left hand side operand
			astn	test;			// ?: operator condition
			int		prec;			// precedence level
		} op;
		struct {					// KIND_var: identifier
			char	*name;			// name of identifier
			unsigned hash;			// hash code for 'name'
			symn	link;			// symbol to variable
			astn	used;			// next usage of variable
		} ref;
		struct {					// STMT_ret, STMT_brk, STMT_con
			symn func;				// return from function
			astn value;				// return the value of expression
			size_t offs;			// jump instruction offset
			size_t stks;			// stack size
		} jmp;
		struct {					// OPER_opc
			vmOpcode code;			// instruction code
			size_t args;			// instruction argument
		} opc;
		struct {					// STMT_beg: list
			astn head;
			astn tail;
		} lst;
	};
};

/// Allocate a tree node.
astn newNode(ccContext cc, ccToken kind);

/// Recycle node, so it may be reused.
void recycle(ccContext cc, astn ast);

/// Allocate a constant integer node.
astn intNode(ccContext cc, int64_t value);

/// Allocate a constant float node.
astn fltNode(ccContext cc, float64_t value);

/// Allocate a constant string node.
astn strNode(ccContext cc, char *value);

/// Allocate node which is a link to a reference.
astn lnkNode(ccContext cc, symn ref);

/// Allocate an operator tree node.
astn opNode(ccContext cc, ccToken kind, astn lhs, astn rhs);

// return constant values of nodes
int bolValue(astn ast);
int64_t intValue(astn ast);
float64_t fltValue(astn ast);

/**
 * Try to evaluate a constant expression.
 * 
 * @param res Place the result here.
 * @param ast Abstract syntax tree to be evaluated.
 * @return Type of result: [TYPE_err, CAST_i64, CAST_f64, ...]
 * TODO: to be deleted; use vm to evaluate constants.
 */
ccKind eval(ccContext cc, astn res, astn ast);

/**
 * Get the symbol(variable) linked to expression.
 * @param ast Abstract syntax tree to be checked.
 * @param follow Follow symbol links.
 * @return null if not an identifier.
 * @note if input is a the symbol for variable a is returned.
 * @note if input is a.b the symbol for member b is returned.
 * @note if input is a(1, 2) the symbol for function a is returned.
 */
symn linkOf(astn ast, int follow);

/**
 * Check if an expression is a type.
 * 
 * @param ast Abstract syntax tree to be checked.
 * @return true or false.
 */
int isTypeExpr(astn ast);

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ Parser

/**
 * Read the next token from input.
 * 
 * @param cc compiler context.
 * @param match: read next token only if matches.
 * @return next token, or null.
 */
astn nextTok(ccContext cc, ccToken match, int raise);

/**
 * Peek the next token.
 * 
 * @param cc compiler context.
 * @param match read next token only if matches the given kind.
 * @return next token, or null.
 */
astn peekTok(ccContext cc, ccToken match);

/**
 * Read the next token and recycle it.
 * 
 * @brief read the next token from input.
 * @param cc compiler context.
 * @param match read next token only if matches.
 * @param raise raise an error if the token was not skipped.
 * @return kind of read token.
 */
ccToken skipTok(ccContext cc, ccToken match, int raise);

/**
 * Push back a token, to be read next time.
 * 
 * @param cc compiler context.
 * @param token the token to be pushed back.
 */
astn backTok(ccContext cc, astn token);

#endif

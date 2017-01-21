/*******************************************************************************
 *   File: core.h
 *   Date: 2007/04/20
 *   Desc: internal header
 *******************************************************************************
 *
 *
*******************************************************************************/
#ifndef CC_INTERNAL_H
#define CC_INTERNAL_H

#include "vmCore.h"
#include "ccCore.h"

#include <stdlib.h> //4 abort

/* Debugging the compiler:
	0: show where errors were raised
	1: trace errors to their root
	2: include debug messages
	3: include debug messages from code generator
	4: include debug messages from code emitter
 	if not defined no extra messages and extra checks are performed.
*/
#define DEBUGGING 1

// limit the count of printed elements(stacktrace, array elements)
#define LOG_MAX_ITEMS 25

// enable parallel execution stuff
#define VM_MAX_PROC 0

// maximum token count in expressions & nesting level
#define CC_MAX_TOK 1024

// hash table size
#define TBL_SIZE 512

// helper macros
#define lengthOf(__ARRAY) (sizeof(__ARRAY) / sizeof(*(__ARRAY)))
#define offsetOf(__TYPE, __FIELD) ((size_t) &((__TYPE)0)->__FIELD)

// linked list
typedef struct list {
	struct list *next;
	unsigned char *data;
} *list;

// native function call
typedef struct libc {
	vmError (*call)(nfcContext);
	const char *proto;
	symn sym;
	size_t offs;
	size_t in, out;		// stack parameters
} *libc;

// A simple dynamic array
struct arrBuffer {
	char *ptr;		// data
	int esz;		// element size
	int cap;		// capacity
	int cnt;		// length
};

int initBuff(struct arrBuffer *buff, int initCount, int elemSize);
void freeBuff(struct arrBuffer *buff);
static inline void *getBuff(struct arrBuffer *buff, int idx) {
	int pos = idx * buff->esz;
	if (pos >= buff->cap || !buff->ptr)
		return NULL;
	return buff->ptr + pos;
}

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
	const char*	pfmt;		// TEMP: print format
};

/// Abstract syntax tree node
struct astNode {
	ccToken		kind;				// token kind: operator, statement, ...
	symn		type;				// token type: return
	astn		next;				// next token / next argument / next statement
	char*		file;				// file name of the token belongs to
	int32_t		line;				// line position of token
	union {							// token value
		int64_t cint;				// constant integer value
		float64_t cflt;				// constant floating point value
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
			char*	name;			// name of identifier
			unsigned hash;			// hash code for 'name'
			symn	link;			// symbol to variable
			astn	used;			// next usage of variable
		} ref;
		struct {					// STMT_brk, STMT_con
			size_t stks;			// stack size
			size_t offs;			// jump instruction offset
		} go2;
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

/// Debug info node
struct dbgNode {
	// the statement tree.
	astn stmt;
	// the declared symbol.
	symn decl;

	// position in code
	size_t start, end;

	// position in file.
	char *file;
	int line;

	// break execution?
	int bp;

	// execution information
	int64_t total, self;  // time spent executing function / statement
	int64_t hits, exec;  // hit count and successful executions
};

/// Compiler context
struct ccContextRec {
	rtContext	rt;
	astn	root;		// statements
	symn	scope;		// scope variables and functions
	symn	global;		// global variables and functions
	list	native;		// list of native functions

	// lists and tables
	astn	jmps;		// list of break and continue statements to fix
	list	strt[TBL_SIZE];		// string hash table
	symn	deft[TBL_SIZE];		// symbol hash stack

	int		nest;		// nest level: modified by (enter/leave)
	int		warn;		// warning level
	int		siff:1;		// TODO: remove: inside a static if false
	int		_padd:30;	//

	// Parser
	char*	file;		// current file name
	int		line;		// current line number
	size_t	lpos;		// current line position
	size_t	fpos;		// current file position
	//~ current column = fpos - lpos

	// Lexer
	struct {
		astn	tokp;			// list of reusable tokens
		astn	_tok;			// one token look-ahead
		int		_chr;			// one char look-ahead
		struct {				// Input
			int		nest;		// nesting level on open.
			int		_fin;		// file handle
			size_t	_cnt;		// chars left in buffer
			char*	_ptr;		// pointer parsing trough source
			uint8_t	_buf[1024];	// memory file buffer
		} fin;
	};

	// Type cache
	symn	type_vid;		// void
	symn	type_bol;		// boolean
	symn	type_chr;		// character
	symn	type_i32;		// 32bit signed integer
	symn	type_i64;		// 64bit signed integer
	symn	type_u32;		// 32bit unsigned integer
	symn	type_u64;		// 64bit unsigned integer
	symn	type_f32;		// 32bit floating point
	symn	type_f64;		// 64bit floating point
	symn	type_ptr;		// pointer
	symn	type_var;		// variant
	symn	type_rec;		// typename
	symn	type_fun;		// function
	symn	type_obj;		// object
	symn	type_str;		// string

	symn	type_int;		// integer: 32/64 bit signed
	symn	type_idx;		// length / index: 32/64 bit unsigned

	symn	null_ref;		// variable null
	symn	true_ref;		// variable true
	symn	false_ref;		// variable false

	symn	emit_opc;		// emit intrinsic function, or whatever it is.
	astn	emit_tag;		// "emit"

	symn	libc_dbg;		// debug function libcall: raise(int level, string message, variant inspect, int maxTrace);
};

/// Debugger context
struct dbgContextRec {
	rtContext rt;
	userContext extra;		// extra data for debugger and-or profiler
	dbgn (*debug)(dbgContext ctx, vmError, size_t ss, void *sp, size_t caller, size_t callee);

	struct arrBuffer functions;
	struct arrBuffer statements;
	size_t freeMem, usedMem;
	const int checked;			// execution is inside an try catch
	dbgn abort;
};


//             *** Type related functions
/// Enter a new scope.
void enter(ccContext cc);

/// Leave current scope.
symn leave(ccContext cc, symn dcl, ccKind mode, int align, size_t *size);

/**
 * @brief Install a new symbol: alias, typename, variable or function.
 * @param name Symbol name.
 * @param kind Kind of sybol: (KIND_xxx + ATTR_xxx + CAST_xxx)
 * @param size Size of symbol
 * @param type Type of symbol (typename base type, variable type).
 * @param init Initializer, function body, alias link.
 * @return The installed symbol.
 */
symn install(ccContext, const char *name, ccKind kind, size_t size, symn type, astn init);

/**
 * @brief Lookup an identifier.
 * @param sym List of symbols to search in.
 * @param ast Identifier node to search for.
 * @param args List of arguments, use null for variables.
 * @param raise may raise error if more than one result found.
 * @return Symbol found.
 */
symn lookup(ccContext, symn sym, astn ast, astn args, int raise);

/**
 * @brief Type Check an expression.
 * @param loc Override scope.
 * @param ast Tree to check.
 * @return Type of expression.
 */
symn typeCheck(ccContext cc, symn loc, astn ast, int raise);

/**
 * @brief check if a value can be assigned to a symbol.
 * @param rhs variable to be assigned to.
 * @param val value to be assigned.
 * @param strict Strict mode: casts are not enabled.
 * @return cast of the assignment if it can be done.
 */
ccKind canAssign(ccContext, symn rhs, astn val, int strict);

/**
 * @brief Add usage of the symbol.
 * @param sym Symbol typename or variable.
 * @param tag The node of the usage.
 */
void addUsage(symn sym, astn tag);

extern const char * const type_fmt_character;
extern const char * const type_fmt_signed32;
extern const char * const type_fmt_signed64;
extern const char * const type_fmt_unsigned32;
extern const char * const type_fmt_unsigned64;
extern const char * const type_fmt_float32;
extern const char * const type_fmt_float64;
extern const char * const type_fmt_string;
extern const char * const type_fmt_typename;


//             *** Tree related functions
/// Allocate a symbol node.
symn newDefn(ccContext, ccKind kind);
/// Allocate a tree node.
astn newNode(ccContext, ccToken kind);
/// Recycle node, so it may be reused.
void recycle(ccContext, astn ast);

/// Allocate a constant integer node.
astn intNode(ccContext, int64_t v);

/// Allocate a constant float node.
astn fltNode(ccContext, float64_t v);

/// Allocate a constant string node.
astn strNode(ccContext, char *v);

/// Allocate node which is a link to a reference
astn lnkNode(ccContext, symn ref);

/// Allocate an operator tree node.
astn opNode(ccContext, ccToken kind, astn lhs, astn rhs);

// return constant values of nodes
int32_t bolValue(astn ast);
int64_t intValue(astn ast);
float64_t fltValue(astn ast);
/**
 * @brief Try to evaluate a constant expression.
 * @param res Place the result here.
 * @param ast Abstract syntax tree to be evaluated.
 * @return Type of result: [TYPE_err, CAST_i64, CAST_f64, ...]
 */
// TODO: to be deleted; use vm to evaluate constants.
ccKind eval(ccContext cc, astn res, astn ast);

/**
 * @brief Get the symbol(variable) linked to expression.
 * @param ast Abstract syntax tree to be checked.
 * @return null if not an identifier.
 * @note if input is a the symbol for variable a is returned.
 * @note if input is a.b the symbol for member b is returned.
 * @note if input is a(1, 2) the symbol for function a is returned.
 */
symn linkOf(astn ast);

/**
 * @brief Check if an expression is a type.
 * @param ast Abstract syntax tree to be checked.
 * @return true or false.
 */
int isTypeExpr(astn ast);

/**
 * Chain the arguments trough astn.next link.
 * @param args root node of arguments tree.
 */
static inline astn chainArgs(astn args) {
	astn next = NULL;
//	printFmt(stdout, NULL, "%?s:%?u: arguments: %-t\n", __FILE__, __LINE__, args);
	while (args != NULL) {
		astn arg = args;
		if (arg->kind == OPER_com) {
			args = arg->op.lhso;
			arg = arg->op.rhso;
		}
		else {
			args = NULL;
		}
		arg->next = next;
		next = arg;
	}
	return next;
}


//             *** Parsing section
/**
 * @brief set source(file or string) to be parsed.
 * @param cc compiler context.
 * @param src file name or text to pe parsed.
 * @param isFile src is a filename.
 * @return boolean value of success.
 */
int source(ccContext, int isFile, char *src);

/**
 * @brief Open a stream (file or text) for compilation.
 * @param rt runtime context.
 * @param file file name of input.
 * @param line first line of input.
 * @param text if not null, this will be compiled instead of the file.
 * @return compiler context or null on error.
 * @note invokes ccInit if not initialized.
 */
ccContext ccOpen(rtContext rt, char *file, int line, char *text);

astn parse(ccContext, int warn);

/**
 * @brief Close stream, ensuring it ends correctly.
 * @param cc compiler context.
 * @return number of errors.
 */
int ccClose(ccContext);

/** Read the next token.
 * @brief read the next token from input.
 * @param cc compiler context.
 * @param match: read next token only if matches.
 * @return next token, or null.
 */
astn nextTok(ccContext, ccToken match);

/** Peek the next token.
 * @brief read the next token from input.
 * @param cc compiler context.
 * @param match: read next token only if matches.
 * @return next token, or null.
 */
astn peekTok(ccContext, ccToken match);

/** Read the next token and recycle it.
 * @brief read the next token from input.
 * @param cc compiler context.
 * @param match: read next token only if matches.
 * @return kind of read token.
 */
ccToken skipTok(ccContext, ccToken match, int raise);

/** Push back a token, to be read next time.
 * @brief put back token to be read next time.
 * @param cc compiler context.
 * @param tok the token to be pushed back.
 */
astn backTok(ccContext, astn token);

//             *** Print and dump
enum Format {
	prAsmCode = 0x00000f,   // print code bytes (0-15)
	prAsmAddr = 0x000010,   // print global address: (@0x003d8c)
	prAsmName = 0x000040,   // use symbol names instead of addresses: <main+80>

	prOneLine = 0x000080,   // force printing on a single line (skip: function body, typename fields, statement blocks, ...)

	prSymQual = 0x000100,   // print qualified symbol names.
	prSymArgs = 0x000200,   // print functions parameters.
	prSymType = 0x000400,   // print variable type, function return type, typename base type.
	prSymInit = 0x000800,   // print variable initializer, function body, typename fields.

	prAttr    = 0x001000,   // print attributes: const, static, member
	prAstType = 0x002000,   // print type cast of each subexpression
	nlAstBody = 0x004000,   // print compound statements on new line (like in cs)
	nlAstElIf = 0x008000,   // don't keep `else if` constructs on the same line.

	prName = 0,		// print operator or symbol name only.
	prValue = prOneLine,
	prShort = prSymQual | prSymArgs | prSymType | prOneLine ,	// %t, %T
	prFull = prAttr | prSymQual | prSymArgs | prSymType | prSymInit,		// %+t, %-t, %+T, %-T
	prDbg = prAttr | prAstType | prSymQual | prSymArgs | prSymType | prSymInit | prAsmAddr | prAsmName | 9
};

void printSym(FILE *out, const char **esc, symn sym, int mode, int indent);

void printAst(FILE *out, const char **esc, astn ast, int mode, int indent);

/**
 * @brief Print an instruction to the output stream.
 * @param out Output stream.
 * @param ptr Instruction pointer.
 * @param len Length to represent binary the instruction.
 * @param offs Base offset to be used for relative instructions.
 *    negative value forces relative offsets. (ex: jmp +5)
 *    positive or zero value forces absolute offsets. (ex: jmp @0255d0)
 * @param rt Runtime context (optional).
 */
void printAsm(FILE *out, const char **esc, rtContext, void *ptr, int mode);
void printOpc(FILE *out, const char **esc, vmOpcode opc, int64_t args);

/**
 * @brief Print the value of a variable at runtime.
 * @param rt Runtime context.
 * @param out Output stream.
 * @param var Variable to be printed. (May be a typename also)
 * @param ref Base offset of variable.
 * @param level Identation level. (Used for members and arrays)
 */
void printVal(FILE *out, const char **esc, rtContext, symn var, stkval *ref, int mode, int indent);

/**
 * @brief Print and add compiler and runtime errors.
 */
void printErr(rtContext rt, int level, const char *file, int line, const char *msg, ...);

/**
 * @brief Dump all accessible symbols
 */
void dumpApi(rtContext rt, userContext ctx, void action(userContext, symn));


//             *** General

/** Calculate hash of string.
 * @brief calculate the hash of a string.
 * @param str string to be hashed.
 * @param len length of string.
 * @return hash code of the input.
 */
unsigned rehash(const char *str, size_t size);
/** Add string to string table.
 * @brief add string to string table.
 * @param cc compiler context.
 * @param str the string to be mapped.
 * @param len the length of the string, -1 recalculates using strlen.
 * @param hash precalculated hashcode, -1 recalculates.
 * @return the mapped string in the string table.
 */
char *mapstr(ccContext cc, const char *str, size_t size/* = -1*/, unsigned hash/* = -1*/);

static inline size_t padOffset(size_t offs, unsigned align) {
	return (offs + (align - 1)) & ~(align - 1);
}

static inline void *padPointer(void *offs, unsigned align) {
	return (void *) padOffset((size_t) offs, align);
}


int vmSelfTest();

/**
 * @brief Optimize an assignment by removing extra copy of the value if it is on the top of the stack.
 * @param Runtime context.
 * @param offsBegin Begin of the byte code.
 * @param offsEnd End of the byte code.
 * @return non zero if the code was optimized.
 */
int optimizeAssign(rtContext, size_t offsBegin, size_t offsEnd);


static inline int isStatic(symn sym) {
	return (sym->kind & ATTR_stat) != 0;
}
static inline int isConst(symn sym) {
	return (sym->kind & ATTR_cnst) != 0;
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

//             *** Plugins
int importLib(rtContext rt, const char *path);
void closeLibs();


//             *** Error and warning messages

#define ERR_INTERNAL_ERROR "Internal Error"
#define ERR_MEMORY_OVERRUN "Memory Overrun"
#define ERR_OPENING_FILE "can not open file: %s"

// Lexer errors
#define ERR_INVALID_STATEMENT "unterminated block statement"
#define ERR_INVALID_COMMENT "unterminated block comment"
#define ERR_INVALID_CHARACTER "invalid character: '%c'"
#define ERR_INVALID_HEX_SEQ "hex escape sequence invalid '%c'"
#define ERR_INVALID_ESC_SEQ "invalid escape sequence '\\%c'"
#define ERR_INVALID_LITERAL "unclosed '%c' literal"
#define ERR_EMPTY_CHAR_CONSTANT "empty character constant"
#define ERR_INVALID_EXPONENT "no exponent in numeric constant"
#define ERR_INVALID_SUFFIX "invalid suffix in numeric constant '%s'"

// Parser errors
#define ERR_EXPR_TOO_COMPLEX "expression too complex"
#define ERR_SYNTAX_ERR_BEFORE "syntax error before token `%?.t`"
#define ERR_EXPECTED_BEFORE_TOK "expected `%s` before token `%?.t`"
#define ERR_EXPECTED_BEFORE_END "expected `%s` before end of input"
#define ERR_UNEXPECTED_TOKEN "unexpected token `%.t`"
#define ERR_UNMATCHED_TOKEN "unexpected token `%?.t`, matching `%.k`"
#define ERR_UNEXPECTED_QUAL "unexpected qualifier `%.t` declared more than once"
#define ERR_UNEXPECTED_ATTR "unexpected attribute `%?K`"

// Type checker errors
#define ERR_INVALID_PROTO "invalid declaration: `%s`"
#define ERR_UNDEFINED_REFERENCE "undefined reference `%t`"
#define ERR_REDEFINED_REFERENCE "redefinition of `%T`"
#define ERR_UNINITIALIZED_CONSTANT "uninitialized constant `%T`"
#define ERR_UNIMPLEMENTED_FUNCTION "unimplemented function `%T`"
#define ERR_INVALID_CONST_ASSIGN "assignment of constant variable `%t`"
#define ERR_INVALID_CONST_EXPR "constant expression expected, got: `%t`"
//#define ERR_INVALID_CONST_INIT "invalid constant initialization `%t`"

#define ERR_TYPE_TOO_COMPLEX "declaration too complex: `%T`"
#define ERR_INVALID_BASE_TYPE "invalid struct base type, got `%t`"
#define ERR_INVALID_DECLARATION "declaration expected, got: `%t`"
#define ERR_INVALID_ARRAY_LENGTH "positive integer constant expected, got `%t`"
#define ERR_INVALID_FIELD_ACCESS "object reference is required to access the member `%T`"
#define ERR_INVALID_TYPE "can not type check expression: %t"
#define ERR_MULTIPLE_OVERLOADS "there are %d overloads for `%T`"

// Code generator errors
#define ERR_INVALID_JUMP "`%t` statement is invalid due to previous variable declaration within loop"
#define ERR_CAST_EXPRESSION "can not emit expression: %t, invalid cast(%K -> %K)"
#define ERR_EMIT_EXPRESSION "can not emit expression: %t"
#define ERR_EMIT_STATEMENT "can not emit statement: %t"
#define ERR_EMIT_FUNCTION "can not emit function: %T"

#define ERR_INVALID_INSTRUCTION "invalid instruction: %.A @%06x"
#define ERR_INVALID_OFFSET "invalid reference: %06x"

#define ERR_EXEC_INSTRUCTION "%s at .%06x in function: <%?.T%?+d> executing instruction: %.A"

#define WARN_EMPTY_STATEMENT "empty statement `;`."
#define WARN_USE_BLOCK_STATEMENT "statement should be a block statement {%t}."
#define WARN_TRAILING_COMMA "skipping trailing comma before `%t`"
#define WARN_EXPRESSION_STATEMENT "expression statement expected, got: `%t`"
#define WARN_DISCARD_DATA "converting `%t` to %T is discarding one property"
#define WARN_PASS_ARG_BY_REF "argument `%t` is not explicitly passed by reference"
#define WARN_SHORT_CIRCUIT "operators `&&` and `||` does not short-circuit yet"
#define WARN_NO_CODE_GENERATED "no code will be generated for statement: %t"
#define WARN_STATIC_FIELD_ACCESS "accessing static member using instance variable `%T`/ %T"
#define WARN_COMMENT_MULTI_LINE "multi-line comment: `%s`"
#define WARN_COMMENT_IGNORE_NESTED "ignoring nested comment"
#define WARN_OCT_ESC_SEQ_OVERFLOW "octal escape sequence overflow"
#define WARN_CHR_CONST_TRUNCATED "character constant truncated"
#define WARN_MULTI_CHAR_CONSTANT "multi character constant"
#define WARN_VALUE_OVERFLOW "value overflow"
#define WARN_EXPONENT_OVERFLOW "exponent overflow"
#define WARN_FUNCTION_MARKED_STATIC "marking function to be static: `%T`"
#define WARN_BEST_OVERLOAD "using overload `%T` of %d declared symbols."
#define WARN_INLINE_ALL_PARAMS "all parameters will be inlined for: %t"

#define FATAL_UNIMPLEMENTED_OPERATOR "operator %.t (%T, %T): %t"

//~ disable warning messages
#ifdef _MSC_VER
// C4996: The POSIX ...
#pragma warning(disable: 4996)
#endif

static inline void _break() {/* Add a breakpoint to break on compiler errors. */}
static inline void _abort() {/* Add a breakpoint to break on fatal errors. */
	_break();
#ifndef DEBUGGING	// abort on first internal error
	abort();
#endif
}
#define prerr(__DBG, __MSG, ...) do { printFmt(stdout, NULL, "%s:%u: " __DBG ": %s: " __MSG "\n", __FILE__, __LINE__, __FUNCTION__, ##__VA_ARGS__); } while(0)

#define fatal(msg, ...) do { prerr("todo", msg, ##__VA_ARGS__); _abort(); } while(0)
#define dieif(__EXP, msg, ...) do { if (__EXP) { prerr("todo("#__EXP")", msg, ##__VA_ARGS__); _abort(); } } while(0)

// compilation errors
#define error(__ENV, __FILE, __LINE, msg, ...) do { printErr(__ENV, -1, __FILE, __LINE, msg, ##__VA_ARGS__); trace(msg, ##__VA_ARGS__); _break(); } while(0)
#define warn(__ENV, __LEVEL, __FILE, __LINE, msg, ...) do { printErr(__ENV, __LEVEL, __FILE, __LINE, msg, ##__VA_ARGS__); } while(0)
#define info(__ENV, __FILE, __LINE, msg, ...) do { printErr(__ENV, 0, __FILE, __LINE, msg, ##__VA_ARGS__); } while(0)

#ifndef DEBUGGING	// disable debugging
#define logif(__EXP, msg, ...) do {} while(0)
#define debug(msg, ...) do {} while(0)
#define trace(msg, ...) do {} while(0)
#else

#define logif(__EXP, msg, ...) do { if (__EXP) { prerr("todo("#__EXP")", msg, ##__VA_ARGS__); _break(); } } while(0)

#if DEBUGGING >= 1	// enable trace
#define trace(msg, ...) do { prerr("trace", msg, ##__VA_ARGS__); } while(0)
#else
#define trace(msg, ...) do {} while(0)
#endif

#if DEBUGGING >= 2	// enable debug
#define debug(msg, ...) do { prerr("debug", msg, ##__VA_ARGS__); } while(0)
#else
#define debug(msg, ...) do {} while(0)
#endif

#if DEBUGGING >= 3	// enable debug
#define dbgCgen(msg, ...) do { prerr("debug", msg, ##__VA_ARGS__); } while(0)
#else
#define dbgCgen(msg, ...) do {} while(0)
#endif

#if DEBUGGING >= 4	// enable debug
#define dbgEmit(msg, ...) do { prerr("debug", msg, ##__VA_ARGS__); } while(0)
#else
#define dbgEmit(msg, ...) do {} while(0)
#endif

#endif

#define traceAst(__AST) do { trace("%t", __AST); } while(0)

#if defined __WATCOMC__

#pragma disable_message(136);	// Warning! W136: Comparison equivalent to 'unsigned == 0'

#include <math.h>
static inline float fmodf(float x, float y) { return (float) fmod(x, y); }
static inline float sinf(float x) { return (float) sin((float) x); }
static inline float cosf(float x) { return (float) cos((float) x); }
static inline float tanf(float x) { return (float) tan((float) x); }
static inline float logf(float x) { return (float) log((float) x); }
static inline float expf(float x) { return (float) exp((float) x); }
static inline float powf(float x, float y) { return (float) pow((float) x, (float) y); }
static inline float sqrtf(float x) { return (float) sqrt((float) x); }
static inline float atan2f(float x, float y) { return (float) atan2((float) x, (float) y); }

#endif

#endif

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

#include "cmplVm.h"
#include "cmplCc.h"
#include "parser.h"
#include "cmplDbg.h"
#include "printer.h"

/* Debugging the compiler:
	0: show where errors were raised
	1: trace errors to their root
	2: include debug messages
	3: include debug messages from code generator
	4: include debug messages from code emitter
 	if not defined no extra messages and extra checks are performed.
*/
//#define DEBUGGING 1

enum Settings {
	hashTableSize = 512,

	// limit the count of printed elements(stacktrace, array elements)
	maxLogItems = 25,

	// maximum token count in expressions & nesting level
	maxTokenCount = 1024,

	// pre allocate space for argument on the stack
	// faster execution if each argument is pushed when calculated
	preAllocateArgs = 0,

	// uint8 a = 130;   // a: uint8(130)
	// uint32 b = a;    // b: uint32(4294967170)
	// by default load.m8 will sign extend to 32 bits
	zeroExtendUnsigned = 0
};

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

/// Compiler context
struct ccContextRec {
	rtContext	rt;
	astn	root;		// statements
	symn	owner;		// scope variables and functions
	symn	scope;		// scope variables and functions
	symn	global;		// global variables and functions
	list	native;		// list of native functions

	// lists and tables
	astn	jumps;		// list of break and continue statements to fix
	list	strt[hashTableSize];		// string hash table
	symn	deft[hashTableSize];		// symbol hash stack

	int		nest;		// nest level: modified by (enter/leave)
	int		siff:1;		// TODO: remove: inside a static if false

	// Parser
	char*	file;		// current file name
	int		line;		// current line number
	size_t	lPos;		// current line position
	size_t	fPos;		// current file position
	//~ current column = fPos - lPos

	// Lexer
	struct {
		astn	tokPool;		// list of recycled tokens
		astn	tokNext;		// next token: look-ahead
		int		chrNext;		// next character: look-ahead
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
	symn	length_ref;		// slice length attribute

	symn	emit_opc;		// emit intrinsic function, or whatever it is.
	astn	void_tag;		// used to lookup function call with 0 argument

	symn	libc_dbg;		// raise(char file[*], int line, int level, int trace, char message[*], variant inspect);
	symn	libc_try;		// tryExec(pointer args, void action(pointer args));
};

/// Debugger context
struct dbgContextRec {
	rtContext rt;
	userContext extra;		// extra data for debugger and-or profiler
	dbgn (*debug)(dbgContext ctx, vmError, size_t ss, void *sp, size_t caller, size_t callee);

	struct arrBuffer functions;
	struct arrBuffer statements;
	size_t freeMem, usedMem;
	dbgn abort;
	symn tryExec;	// the symbol of tryExec function
};

// helper macros
#define lengthOf(__ARRAY) (sizeof(__ARRAY) / sizeof(*(__ARRAY)))
#define offsetOf(__TYPE, __FIELD) ((size_t) &((__TYPE*)NULL)->__FIELD)

/// Bit count: number of bits set to 1
static inline int32_t bitcnt(uint32_t value) {
	value -= ((value >> 1) & 0x55555555);
	value = (((value >> 2) & 0x33333333) + (value & 0x33333333));
	value = (((value >> 4) + value) & 0x0f0f0f0f);
	value += (value >> 8) + (value >> 16);
	return value & 0x3f;
}

/// Bit scan forward: position of the Least Significant Bit
static inline int32_t bitsf(uint32_t value) {
	if (value == 0) {
		return -1;
	}
	int32_t result = 0;
	if ((value & 0x0000ffff) == 0) {
		result += 16;
		value >>= 16;
	}
	if ((value & 0x000000ff) == 0) {
		result += 8;
		value >>= 8;
	}
	if ((value & 0x0000000f) == 0) {
		result += 4;
		value >>= 4;
	}
	if ((value & 0x00000003) == 0) {
		result += 2;
		value >>= 2;
	}
	if ((value & 0x00000001) == 0) {
		result += 1;
	}
	return result;
}

/// Bit scan reverse: position of the Most Significant Bit
static inline int32_t bitsr(uint32_t value) {
	if (value == 0) {
		return -1;
	}
	int32_t result = 0;
	if (value & 0xffff0000) {
		result += 16;
		value >>= 16;
	}
	if (value & 0x0000ff00) {
		result +=  8;
		value >>= 8;
	}
	if (value & 0x000000f0) {
		result += 4;
		value >>= 4;
	}
	if (value & 0x0000000c) {
		result += 2;
		value >>= 2;
	}
	if (value & 0x00000002) {
		result += 1;
	}
	return result;
}

/// Utility function to align offset
static inline size_t padOffset(size_t offs, size_t align) {
	return (offs + (align - 1)) & -align;
}

/// Utility function to align pointer
static inline void *padPointer(void *offs, size_t align) {
	return (void*)padOffset((size_t)offs, align);
}

/// Utility function to swap memory
static inline void memSwap(void *_a, void *_b, size_t size) {
	register char *a = _a;
	register char *b = _b;
	register char *end = a + size;
	while (a < end) {
		char c = *a;
		*a = *b;
		*b = c;
		a += 1;
		b += 1;
	}
}

/// Check if the pointer is inside the vm.
static inline int isValidOffset(rtContext rt, void *ptr) {
	if ((unsigned char*)ptr > rt->_mem + rt->_size) {
		return 0;
	}
	if ((unsigned char*)ptr < rt->_mem) {
		return 0;
	}
	return 1;
}

void nfcCheckArg(nfcContext nfc, ccKind cast, char *name);

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

extern const char * const type_fmt_signed32;
extern const char * const type_fmt_signed64;
extern const char * const type_fmt_unsigned32;
extern const char * const type_fmt_unsigned64;
extern const char * const type_fmt_float32;
extern const char * const type_fmt_float64;
extern const char * const type_fmt_character;
extern const char * const type_fmt_string;
extern const char * const type_fmt_typename;
extern const char type_fmt_string_chr;
extern const char type_fmt_character_chr;

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

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
 * @param hash pre-calculated hashcode, -1 recalculates.
 * @return the mapped string in the string table.
 */
char *ccUniqueStr(ccContext cc, const char *str, size_t size/* = -1*/, unsigned hash/* = -1*/);

// open log file for both compiler and runtime
void logFILE(rtContext ctx, FILE *file);
FILE *logFile(rtContext ctx, char *file, int append);

/**
 * Chain the arguments trough ast.next link.
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

/**
 * Optimize an assignment by removing extra copy of the value if it is on the top of the stack.
 * 
 * @param Runtime context.
 * @param offsBegin Begin of the byte code.
 * @param offsEnd End of the byte code.
 * @return non zero if the code was optimized.
 */
int optimizeAssign(rtContext, size_t offsBegin, size_t offsEnd);

/**
 * Test the virtual machine instruction set(compare implementation with definition `OPCODE_DEF`).
 * 
 * @param cb callback executed for each instruction.
 * @return number of errors found during the test.
 */
int vmSelfTest(void cb(const char *error, const struct opcodeRec *info));

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ Plugins
int importLib(rtContext rt, const char *path);
void closeLibs();

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ Error and warning messages
#define ERR_INTERNAL_ERROR "Internal Error"
#define ERR_MEMORY_OVERRUN "Memory Overrun"
#define ERR_OPENING_FILE "can not open file: %s"
#define ERR_UNIMPLEMENTED_FEATURE "Unimplemented feature"

// Lexer errors
#define ERR_EMPTY_CHAR_CONSTANT "empty character constant"
#define ERR_INVALID_CHARACTER "invalid character: '%c'"
#define ERR_INVALID_COMMENT "unterminated block comment"
#define ERR_INVALID_ESC_SEQ "invalid escape sequence '\\%c'"
#define ERR_INVALID_EXPONENT "no exponent in numeric constant"
#define ERR_INVALID_HEX_SEQ "hex escape sequence invalid '%c'"
#define ERR_INVALID_LITERAL "unclosed '%c' literal"
#define ERR_INVALID_STATEMENT "unterminated block statement"
#define ERR_INVALID_SUFFIX "invalid suffix in numeric constant '%s'"

// Parser errors
#define ERR_EXPECTED_BEFORE_TOK "expected `%s` before token `%?.t`"
#define ERR_EXPR_TOO_COMPLEX "expression too complex"
#define ERR_SYNTAX_ERR_BEFORE "syntax error before token `%?.t`"
#define ERR_UNEXPECTED_ATTR "unexpected attribute `%?K`"
#define ERR_UNEXPECTED_QUAL "unexpected qualifier `%.t` declared more than once"
#define ERR_UNEXPECTED_TOKEN "unexpected token `%.t`"
#define ERR_UNMATCHED_TOKEN "unexpected token `%?.t`, matching `%.k`"
#define ERR_UNMATCHED_SEPARATOR "unexpected separator `%?.t`, matching `%.k`"
#define ERR_DECLARATION_EXPECTED "declaration expected, got: `%t`"

// Type checker errors
#define ERR_DECLARATION_COMPLEX "declaration too complex: `%T`"
#define ERR_DECLARATION_REDEFINED "redefinition of `%T`"
#define ERR_UNDEFINED_DECLARATION "undefined reference `%t`"
#define ERR_INVALID_ARRAY_LENGTH "positive integer constant expected, got `%t`"
#define ERR_INVALID_INHERITANCE "type must be inherited from object, got `%t`"
#define ERR_INVALID_BASE_TYPE "invalid struct base type, got `%t`"
#define ERR_INVALID_PACK_SIZE "invalid struct pack size, got `%t`"
#define ERR_INVALID_VALUE_ASSIGN "invalid assignment: `%T` := `%t`"
#define ERR_INVALID_CONST_ASSIGN "assignment of constant variable `%t`"
#define ERR_INVALID_CONST_EXPR "constant expression expected, got: `%t`"
#define ERR_INVALID_DECLARATION "invalid declaration: `%s`"
#define ERR_INVALID_FIELD_ACCESS "object reference is required to access the member `%T`"
#define ERR_INVALID_TYPE "can not type check expression: `%t`"
#define ERR_INVALID_CAST "can not use cast to construct instance: `%t`"
#define ERR_INVALID_OPERATOR "invalid operator %.t(%T, %T)"
#define ERR_MULTIPLE_OVERLOADS "there are %d overloads for `%T`"
#define ERR_UNIMPLEMENTED_FUNCTION "unimplemented function `%T`"
#define ERR_UNINITIALIZED_CONSTANT "uninitialized constant `%T`"
#define ERR_UNINITIALIZED_VARIABLE "uninitialized variable `%T`"

// Code generator errors
#define ERR_CAST_EXPRESSION "can not emit expression: %t, invalid cast(%K -> %K)"
#define ERR_EMIT_LENGTH "can not emit length of array: %T"
#define ERR_EMIT_VARIABLE "can not emit variable: %T"
#define ERR_EMIT_FUNCTION "can not emit function: %T"
#define ERR_EMIT_STATEMENT "can not emit statement: %t"
#define ERR_INVALID_INSTRUCTION "invalid instruction: %.A @%06x"
#define ERR_INVALID_JUMP "`%t` statement is invalid due to previous variable declaration within loop"
#define ERR_INVALID_OFFSET "invalid reference: %06x"

// Code execution errors
#define ERR_EXEC_INSTRUCTION "%s at .%06x in function: <%?.T%?+d> executing instruction: %.A"
#define ERR_EXEC_FUNCTION "can not execute function: %T"

#define WARN_EMPTY_STATEMENT "empty statement `;`"
#define WARN_USE_BLOCK_STATEMENT "statement should be a block statement {%t}"
#define WARN_EXPRESSION_STATEMENT "expression statement expected, got: `%t`"
#define WARN_VARIANT_TO_REF "converting `%T` to reference or pointer discards type information"
#define WARN_LOCAL_MIGHT_ESCAPE "local variable `%t` can not be referenced outside of its scope"
#define WARN_PASS_ARG_NO_CAST "argument `%t` is passed to emit without cast as `%T`"
#define WARN_SHORT_CIRCUIT "operators `&&` and `||` does not short-circuit yet"
#define WARN_NO_CODE_GENERATED "no code will be generated for statement: %t"
#define WARN_PADDING_ALIGNMENT "padding `%?T` with %d bytes: (%d -> %d)"
#define WARN_ADDING_IMPLICIT_CAST "adding implicit cast %T(%t: %T)"
#define WARN_STATIC_FIELD_ACCESS "accessing static member using instance variable `%T`/ %T"
#define WARN_COMMENT_MULTI_LINE "multi-line comment: `%s`"
#define WARN_IGNORING_NESTED_COMMENT "ignoring nested comment"
#define WARN_MO_NEW_LINE_AT_END "expected `%s` before end of input"
#define WARN_OCT_ESC_SEQ_OVERFLOW "octal escape sequence overflow"
#define WARN_CHR_CONST_TRUNCATED "character constant truncated"
#define WARN_MULTI_CHAR_CONSTANT "multi character constant"
#define WARN_VALUE_OVERFLOW "value overflow"
#define WARN_EXPONENT_OVERFLOW "exponent overflow"
#define WARN_FUNCTION_MARKED_STATIC "marking function to be static: `%T`"
#define WARN_USING_BEST_OVERLOAD "using overload `%T` of %d declared symbols"
#define WARN_USING_DEFAULT_INITIALIZER "using default type initializer: %T := %t"
#define WARN_DECLARATION_REDEFINED "variable `%T` hides previous declaration"

static inline void _break() {/* Add a breakpoint to break on compiler errors. */}
static inline void _abort() {/* Add a breakpoint to break on fatal errors. */
	_break();
#ifndef DEBUGGING	// abort on first internal error
	abort();
#endif
}
#define prerr(__DBG, __MSG, ...) do { printFmt(stdout, NULL, "%?s:%?u: " __DBG ": %s: " __MSG "\n", __FILE__, __LINE__, __FUNCTION__, ##__VA_ARGS__); } while(0)

#define fatal(msg, ...) do { prerr("err", msg, ##__VA_ARGS__); _abort(); } while(0)
#define dieif(__EXP, msg, ...) do { if (__EXP) { prerr("err("#__EXP")", msg, ##__VA_ARGS__); _abort(); } } while(0)

// compilation errors
#define error(__ENV, __FILE, __LINE, msg, ...) do { logif("ERROR", msg, ##__VA_ARGS__); printErr(__ENV, -1, __FILE, __LINE, msg, ##__VA_ARGS__); _break(); } while(0)
#define warn(__ENV, __LEVEL, __FILE, __LINE, msg, ...) do { printErr(__ENV, __LEVEL, __FILE, __LINE, msg, ##__VA_ARGS__); } while(0)
#define info(__ENV, __FILE, __LINE, msg, ...) do { printErr(__ENV, 0, __FILE, __LINE, msg, ##__VA_ARGS__); } while(0)

#ifdef DEBUGGING	// enable compiler debugging

#define logif(__EXP, msg, ...) do { if (__EXP) { prerr("log("#__EXP")", msg, ##__VA_ARGS__); _break(); } } while(0)

#if DEBUGGING >= 1	// enable trace
#define trace(msg, ...) do { prerr("trace", msg, ##__VA_ARGS__); } while(0)
#endif

#if DEBUGGING >= 2	// enable debug
#define debug(msg, ...) do { prerr("debug", msg, ##__VA_ARGS__); } while(0)
#endif

#if DEBUGGING >= 3	// enable debug cgen
#define dbgCgen(msg, ...) do { prerr("debug", msg, ##__VA_ARGS__); } while(0)
#endif

#if DEBUGGING >= 4	// enable debug emit
#define dbgEmit(msg, ...) do { prerr("debug", msg, ##__VA_ARGS__); } while(0)
#endif

#endif

#ifndef logif
#define logif(__EXP, msg, ...) do { (void)(__EXP); } while(0)
#endif
#ifndef trace
#define trace(msg, ...) do {} while(0)
#endif
#ifndef debug
#define debug(msg, ...) do {} while(0)
#endif
#ifndef dbgCgen
#define dbgCgen(msg, ...) do {} while(0)
#endif
#ifndef dbgEmit
#define dbgEmit(msg, ...) do {} while(0)
#endif

#define traceAst(__AST) do { trace("%.*t", prDbg, __AST); } while(0)

// Compiler specific settings
#ifdef __WATCOMC__
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

#ifdef _MSC_VER
// disable warning messages
// C4996: The POSIX ...
#pragma warning(disable: 4996)
#endif

#endif

/**
 * Cmpl - A very "simple" C like language.
 * Extension libraries should use only this header file.
 */

#ifndef CMPL_API_H
#define CMPL_API_H 2

#include <stdio.h>
#include <string.h>

#if defined(__WATCOMC__) && defined(_WIN32)
typedef signed ssize_t;
#endif

#ifdef _MSC_VER
typedef signed char			int8_t;
typedef signed short		int16_t;
typedef signed long			int32_t;
typedef signed long long	int64_t;
typedef unsigned char		uint8_t;
typedef unsigned short		uint16_t;
typedef unsigned long		uint32_t;
typedef unsigned long long	uint64_t;
#include <BaseTsd.h>
typedef SSIZE_T ssize_t;
#define inline __inline
#else
#include <stdint.h>
#include <stdlib.h>

#endif

typedef float float32_t;
typedef double float64_t;

#ifdef __cplusplus
extern "C" {
#endif

typedef uint32_t vmOffs;                        // offset TODO: 32/64 bit offsets support
typedef struct symNode *symn;                   // symbol
typedef struct astNode *astn;                   // syntax tree
typedef struct dbgNode *dbgn;                   // debug node
typedef struct rtContextRec *rtContext;         // runtimeContext
typedef struct ccContextRec *ccContext;         // compilerContext
typedef struct dbgContextRec *dbgContext;       // debuggerContext
typedef struct nfcContextRec *nfcContext;       // nativeCallContext
typedef struct userContextRec *userContext;     // customUserContext

typedef enum {
	raiseFatal = -2,
	raiseError = -1,
	raisePrint = 0,
	raiseWarn = 1,          // treat these warnings as errors

	raise_warn_lex2 = 2,    // WARN_<VALUE>_OVERFLOW
	raise_warn_lex3 = 3,    // WARN_MULTI_CHAR_CONSTANT
	raise_warn_lex9 = 9,    // WARN_NO_NEW_LINE_AT_END / WARN_COMMENT_MULTI_LINE / WARN_COMMENT_NESTED

	raise_warn_par8 = 8,    // empty statement / use block statement
	raise_warn_pad6 = 6,    // WARN_PADDING_ALIGNMENT
	raise_warn_gen8 = 8,    // WARN_NO_CODE_GENERATED
	raise_warn_var8 = 8,    // UNINITIALIZED_VARIABLE

	raise_warn_typ2 = 2,    // WARN_STATIC_FIELD_ACCESS
	raise_warn_typ3 = 3,    // WARN_USING_BEST_OVERLOAD / PASS_ARG_BY_REFERENCE
	raise_warn_typ4 = 4,    // WARN_USING_SIGNED_CAST
	raise_warn_typ6 = 6,    // WARN_ADDING_IMPLICIT_CAST / WARN_USING_DEF_TYPE_INITIALIZER
	raise_warn_redef = 6,   // WARN_DECLARATION_REDEFINED
	raiseInfo = 13,
	raiseDebug = 14,
	raiseVerbose = 15,
} raiseLevel;

/**
 * Runtime error codes.
 *
 * one of these errors is returned by the bytecode execution.
 */
typedef enum {
	noError,
	illegalState,
	stackOverflow,
	divisionByZero,
	illegalInstruction,
	illegalMemoryAccess,
	//+ illegalArrayAccess, => TODO: array index out of bounds
	nativeCallError,
} vmError;

/**
 * Value translated from virtual machine to be used in runtime
 * @note pointers are transformed to offsets by subtracting the pointer to vm memory.
 */
typedef union {
	int32_t i32;
	int64_t i64;
	uint32_t u32;
	uint64_t u64;
	float32_t f32;
	float64_t f64;
	struct {
		void *ref;
		union {
			symn type;
			size_t length;
		};
	};
} rtValue;


/**
 * Runtime context.
 */
struct rtContextRec {
	unsigned foldConst: 1;  // fold constant expressions (3 + 4 => 7)
	unsigned foldCasts: 1;  // fold cast expressions (float(int(3.2)) => 3.f)
	unsigned foldInstr: 1;  // replace instructions with a faster or shorter version (`load 1`, `add.i32` with `inc 1`)
	unsigned foldMemory: 1; // replace `load address`, `load indirect` with `dup`, `set`, `load` or `store` instruction
	unsigned foldAssign: 1; // replace `dup x`, `set y` instructions with `move x, y` instruction
	unsigned logLevel: 4;   // runtime logging level (0-15)
	unsigned traceLevel: 8; // runtime backtrace level (0-255)
	unsigned closeLog: 1;   // close log file
	unsigned freeMem: 1;    // release memory

	int32_t errors;         // error count
	FILE *logFile;          // log file

	/**
	 * Compiler context.
	 *
	 * @note After code generation it is set to null.
	 */
	ccContext cc;

	/**
	 * Debugger context.
	 *
	 * contains:
	 *  * profiler function
	 *  * debugger function
	 *  * code line mapping
	 *  * break point lists
	 */
	dbgContext dbg;

	/**
	 * User context.
	 *
	 * contains custom context set by library
	 */
	userContext usr;

	// virtual machine state
	struct {
		void *nfc;         // native call vector
		void *cell;        // execution units
		void *heap;        // heap memory

		size_t pc;         // exec: entry point / cgen: program counter
		size_t px;         // exec: exit point / cgen: max jump offset

		size_t ro;         // size of read only memory
		size_t cs;         // size of code (functions)
		size_t ds;         // size of data (static var)
		size_t ss;         // size of stack
	} vm;

	/**
	 * Extension support api.
	 *
	 * These functions can be used in extension libraries (dll or so)
	 */
	struct {
		/// Begin a namespace; @see ccBegin
		symn (*const ccBegin)(ccContext ctx, const char *name);

		/// Extend a namespace; @see ccExtend
		symn (*const ccExtend)(ccContext ctx, symn sym);

		/// Close a namespace; @see ccEnd
		symn (*const ccEnd)(ccContext ctx, symn sym);

		/// Declare int constant; @see ccDefInt
		symn (*const ccDefInt)(ccContext ctx, const char *name, int64_t value);

		/// Declare float constant; @see ccDefFlt
		symn (*const ccDefFlt)(ccContext ctx, const char *name, float64_t value);

		/// Declare string constant; @see ccDefStr
		symn (*const ccDefStr)(ccContext ctx, const char *name, char *value);

		/// Declare variable; @see ccDefVar
		symn (*const ccDefVar)(ccContext ctx, const char *name, symn type);

		/// Declare a typename; @see ccAddType
		symn (*const ccAddType)(ccContext ctx, const char *name, unsigned size, int refType);

		/// Declare a native function; @see ccAddCall
		symn (*const ccAddCall)(ccContext ctx, vmError libc(nfcContext), const char *proto);

		/// Compile code snippet; @see ccAddUnit
		astn (*const ccAddUnit)(ccContext cc, int init(ccContext cc), char *file, int line, char *text);

		/// Lookup function by name; @see ccLookup
		symn (*const ccLookup)(rtContext ctx, symn scope, const char *name);

		void (*const raise)(rtContext ctx, raiseLevel level, const char *msg, ...);

		/// Lookup function by offset; @see rtLookup
		symn (*const rtLookup)(rtContext ctx, size_t offset);

		/// Invoke a function; @see invoke
		vmError (*const invoke)(rtContext ctx, symn fun, void *res, void *args, void *extra);

		/// Alloc, resize or free memory; @see rtAlloc
		void *(*const rtAlloc)(rtContext ctx, void *ptr, size_t size);

		/// Reset to the first argument inside a native function
		size_t (*const nfcFirstArg)(nfcContext ctx);

		/// Advance to the next argument inside a native function
		size_t (*const nfcNextArg)(nfcContext ctx);

		/// Read the argument, transforming offsets to pointers
		rtValue (*const nfcReadArg)(nfcContext ctx, size_t argOffs);
	} api;

	/**
	 * Main initializer function.
	 *
	 * This function initializes global variables.
	 */
	symn main;

	// memory management for the compiler and code generator
	unsigned char *_beg;        // permanent memory (increments)
	unsigned char *_end;        // temporary memory (decrements)

	// memory available for the compiler and runtime
	const size_t _size;         // size of available memory
	unsigned char _mem[];       // this is where the memory begins.
};

/// Symbol node: types and variables
typedef enum {
	CAST_any   = 0x0000,		// error
	CAST_vid   = 0x0001,		// void
	CAST_bit   = 0x0002,		// bool
	CAST_i32   = 0x0003,		// int32, int16, int8
	CAST_u32   = 0x0004,		// uint32, uint16, uint8
	CAST_i64   = 0x0005,		// int64
	CAST_u64   = 0x0006,		// uint64
	CAST_f32   = 0x0007,		// float32
	CAST_f64   = 0x0008,		// float64
	CAST_val   = 0x0009,		// value, record
	CAST_ref   = 0x000a,		// reference, pointer, array
	CAST_var   = 0x000b,		// variant: pair of {type, data}
	CAST_arr   = 0x000c,		// slice: pair of {size, data}
	//CAST_d   = 0x000d,		// dictionary
	CAST_enm   = 0x000e,		// enumeration
	//CAST_f   = 0x000f,		// unused(function)

	KIND_def   = 0x0000,		// alias (/ error at runtime)
	KIND_typ   = 0x0010,		// typename: struct metadata info.
	KIND_fun   = 0x0020,		// function
	KIND_var   = 0x0030,		// variable: function and typename are also variables

	ATTR_stat  = 0x0040,		// static attribute
	ATTR_cnst  = 0x0080,		// constant attribute
	ATTR_paral = 0x0100,		// inline argument
	ARGS_varg  = 0x0100,		// variable argument
	ARGS_this  = 0x1000,		// first argument is this (used at lookup)

	MASK_cast  = 0x000f,
	MASK_kind  = 0x0030,
	MASK_attr  = 0x00c0,
} ccKind;

struct symNode {
	const char *name;  // symbol name
	const char *unit;  // declared in unit
	const char *file;  // declared in file
	int32_t line;      // declared on line
	int32_t nest;      // declared on scope level
	size_t size;       // variable or function size
	size_t offs;       // address of variable or function

	symn next;          // next symbol: field / param / ... / (in scope table)
	symn type;          // base type of array / typename (void, int, float, struct, function, ...)
	symn owner;         // the type that declares the current symbol (DeclaringType)
	symn fields;        // all fields: static + non static
	symn params;        // function parameters, return value is the first parameter.
	symn override;      // function overridest this other function

	ccKind kind;        // KIND_def / KIND_typ / KIND_fun / KIND_var + ATTR_xxx + CAST_xxx

	// TODO: merge scope and global attributes
	symn scope;         // global variables and functions / while_compiling variables of the block in reverse order
	symn global;        // all static variables and functions
	astn init;          // VAR init / FUN body, this should be null after code generation?

	astn use;           // TEMP: usages
	astn tag;           // TEMP: declaration reference
	const char *doc;    // document comment
	const char *fmt;    // print format
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
static inline int isEnumType(symn sym) {
	// TODO: enumerations are also arrays
	return (sym->kind & (MASK_kind | MASK_cast)) == (KIND_typ | CAST_enm);
}
static inline int isArrayType(symn sym) {
	return (sym->kind & (MASK_kind | MASK_cast)) == (KIND_typ | CAST_arr);
}
static inline int byReference(symn sym) {
	return (sym->kind & MASK_cast) == CAST_ref;
}
static inline int isInvokable(symn sym) {
	return sym->params != NULL;
}
static inline int isMember(symn sym) {
	if (sym->owner == NULL) {
		// global variable
		return 0;
	}
	return isTypename(sym->owner);
}
static inline ccKind castOf(symn sym) {
	return sym->kind & MASK_cast;
}
/// same as castOf forcing arrays to cast as reference
static inline ccKind refCast(symn sym) {
	ccKind got = castOf(sym);
	if (got == CAST_arr && isArrayType(sym)) {
		symn len = sym->fields;
		if (len == NULL || isStatic(len)) {
			// pointer or fixed size array
			got = CAST_ref;
		}
	}
	return got;
}

static inline size_t refSize(symn sym) {
	switch (refCast(sym)) {
		default:
			return sym->size;

		case CAST_ref:
			return sizeof(vmOffs);

		case CAST_arr:
		case CAST_var:
			return 2 * sizeof(vmOffs);
	}
}

/**
 * check if the given filter excludes the given symbol or not
 * isFiltered(staticVariable, KIND_var | ATTR_stat) == 0
 * isFiltered(localVariable, KIND_var | ATTR_stat) != 0
 * @param sym the symbol to be checked
 * @param filter the filter to be applied
 * @return 0 if is not excluded
 */
static inline int isFiltered(symn sym, ccKind filter) {
	ccKind filterStat = filter & ATTR_stat;
	if (filterStat != 0 && (sym->kind & ATTR_stat) != filterStat) {
		return 1;
	}
	ccKind filterCnst = filter & ATTR_cnst;
	if (filterCnst != 0 && (sym->kind & ATTR_cnst) != filterCnst) {
		return 1;
	}
	ccKind filterKind = filter & MASK_kind;
	if (filterKind != 0 && (sym->kind & MASK_kind) != filterKind) {
		return 1;
	}
	ccKind filterCast = filter & MASK_cast;
	if (filterCast != 0 && (sym->kind & MASK_cast) != filterCast) {
		return 1;
	}
	return 0;
}

/**
 * Check if the given symbol is extending the object type
 */
int isObjectType(symn sym);

/**
 * Native function invocation context.
 */
struct nfcContextRec {
	const rtContext rt;         // runtime context
	const symn sym;             // invoked function (returned by ccAddCall)
	const char *proto;          // static data (passed to ccAddCall)
	const void *extra;          // extra data (passed to execute or invoke)
	const void *args;           // arguments
	const size_t argc;          // argument count in bytes
	symn param;           // the current parameter (modified by nfcFirstArg and nfcNextArg)
};

/**
 * Get the pointer to an internal offset inside the vm.
 *
 * @param rt Runtime context.
 * @param offset global offset inside the vm.
 * @return pointer to the memory.
 */
static inline void *vmPointer(rtContext rt, size_t offset) {
	if (offset == 0) {
		return NULL;
	}
	/* TODO: if (offset >= rt->_size) {
		abort();
	}*/
	return rt->_mem + offset;
}

/**
 * Get the internal offset inside the vm of a reference.
 *
 * @param rt Runtime context.
 * @param ptr Memory location.
 * @return The internal offset.
 * @note Aborts if ptr is not null and outside the vm memory.
 */
static inline size_t vmOffset(rtContext rt, const void *ptr) {
	if (ptr == NULL) {
		return 0;
	}
	size_t offset = (unsigned char*)ptr - rt->_mem;
	if (offset >= rt->_size) {
		abort();
	}
	return offset;
}

/**
 * Get the value of a native function argument.
 *
 * @param args Native function call arguments context.
 * @param offset Relative offset of argument.
 * @param result Optionally copy here the result.
 * @param size Size of the argument to copy to result.
 * @return Pointer where the result is located or copied.
 * @note Getting an argument must be used with padded offset.
 */
static inline void *argget(nfcContext args, size_t offset, void *result, size_t size) {
	char *ptr = (char *) args->args + offset;
	// if result is not null, make a copy
	if (result != NULL && size > 0) {
		memcpy(result, ptr, size);
		return result;
	}
	return ptr;
}

// speed up of getting arguments of known types
#define argget(__ARGV, __OFFS, __TYPE) (*(__TYPE*)((char*)(__ARGV)->args + (__OFFS)))
static inline int32_t argi32(nfcContext ctx, size_t offs) { return argget(ctx, offs, int32_t); }
static inline int64_t argi64(nfcContext ctx, size_t offs) { return argget(ctx, offs, int64_t); }
static inline uint32_t argu32(nfcContext ctx, size_t offs) { return argget(ctx, offs, uint32_t); }
static inline uint64_t argu64(nfcContext ctx, size_t offs) { return argget(ctx, offs, uint64_t); }
static inline float32_t argf32(nfcContext ctx, size_t offs) { return argget(ctx, offs, float32_t); }
static inline float64_t argf64(nfcContext ctx, size_t offs) { return argget(ctx, offs, float64_t); }
static inline void *arghnd(nfcContext ctx, size_t offs) { return argget(ctx, offs, void*); }
static inline size_t argref(nfcContext ctx, size_t offs) { return argget(ctx, offs, vmOffs); }
#undef argget

/**
 * Set the return value of a wrapped native call.
 *
 * @param args arguments context.
 * @param result Pointer containing the result value.
 * @param size Size of the argument to copy to result.
 * @return Pointer where the result is located or copied.
 * @note Setting the return value may overwrite some arguments.
 */
static inline void *retset(nfcContext args, void *result, size_t size) {
	char *ptr = (char *) args->args + args->argc - size;
	if (result != NULL && size > 0) {
		memcpy(ptr, result, size);
		return result;
	}
	return ptr;
}

// speed up of setting result of known types.
#define retset(__ARGV, __TYPE, __VAL) ((__TYPE*)((char *)(__ARGV)->args + (__ARGV)->argc))[-1] = (__TYPE)(__VAL)
static inline void reti32(nfcContext ctx, int32_t val) { retset(ctx, int32_t, val); }
static inline void reti64(nfcContext ctx, int64_t val) { retset(ctx, int64_t, val); }
static inline void retu32(nfcContext ctx, uint32_t val) { retset(ctx, uint32_t, val); }
static inline void retu64(nfcContext ctx, uint64_t val) { retset(ctx, uint64_t, val); }
static inline void retf32(nfcContext ctx, float32_t val) { retset(ctx, float32_t, val); }
static inline void retf64(nfcContext ctx, float64_t val) { retset(ctx, float64_t, val); }
static inline void retref(nfcContext ctx, size_t val) { retset(ctx, vmOffs, val); }
static inline void rethnd(nfcContext ctx, void *val) { retset(ctx, char *, val); }
#undef retset

#ifdef __cplusplus
}
#endif
#endif

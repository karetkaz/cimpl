/**
 * Cmpl - A very "simple" C like language.
 * Extension libraries should use only this header file.
 */

#ifndef CMPL_API_H
#define CMPL_API_H 2

#include <stdio.h>
#include <string.h>

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
#endif

typedef float float32_t;
typedef double float64_t;

#ifdef __cplusplus
extern "C" {
#endif

typedef uint32_t vmOffs;                        // offset
typedef uint32_t *stkptr;                       // stack
typedef struct symNode *symn;                   // symbol
typedef struct astNode *astn;                   // syntax tree
typedef struct dbgNode *dbgn;                   // debug node
typedef struct rtContextRec *rtContext;         // runtimeContext
typedef struct ccContextRec *ccContext;         // compilerContext
typedef struct dbgContextRec *dbgContext;       // debuggerContext
typedef struct nfcContextRec *nfcContext;       // nativeCallContext
typedef struct userContextRec *userContext;     // customUserContext

/**
 * Runtime error codes.
 * 
 * one of these errors is returned by the bytecode execution.
 */
typedef enum {
	noError,
	invalidIP,
	invalidSP,
	stackOverflow,
	memReadError,
	memWriteError,
	divisionByZero,
	illegalInstruction,
	nativeCallError,
	executionAborted		// execution aborted by debugger
	//+ ArrayBoundsExceeded
} vmError;

/**
 * Value inside the virtual machine
 * @note pointers are transformed to offsets by subtracting the pointer to vm memory.
 */
typedef union {
	int8_t i08;
	int16_t i16;
	int32_t i32;
	int64_t i64;
	uint8_t u08;
	uint16_t u16;
	uint32_t u32;
	uint64_t u64;
	float32_t f32;
	float64_t f64;
	int32_t i24:24;
	struct {
		vmOffs ref;
		union {
			vmOffs type;
			vmOffs length;
		};
	};
} vmValue;

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
	const char *str;
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
	int foldConst: 1;       // fold constant expressions (3 + 4 => 7)
	int foldInstr: 1;       // replace some instructions with a faster or shorter version (load 1, add => inc 1)
	int fastMemory: 1;      // fast memory access: use dup, set, load and store instructions instead of `load address` + `load indirect`.
	int fastAssign: 1;      // remove dup and set instructions when modifying the last declared variable.
	int genGlobals: 1;      // generate global variables as static variables

	unsigned warnLevel: 4;  // compile logging level (0-15)
	unsigned logLevel: 3;   // runtime logging level (0-7)
	int logClose: 1;        // close log file
	int freeMem: 1;         // release memory

	int32_t errors;         // error count
	FILE *logFile;          // log file

	/**
	 * Main initializer function.
	 * 
	 * This function initializes global variables.
	 */
	symn main;

	/**
	 * Compiler context.
	 * 
	 * @note After code generation it is set to null.
	 */
	ccContext cc;

	/**
	 * Debugger context.
	 * 
	 * this holds:
	 *  * profiler function
	 *  * debugger function
	 *  * code line mapping
	 *  * break point lists
	 */
	dbgContext dbg;

	// virtual machine state
	struct {
		void *nfc;		// native call vector
		void *cell;		// execution units
		void *heap;		// heap memory

		size_t pc;			// exec: entry point / cgen: program counter
		size_t px;			// exec: exit point / cgen: -

		size_t ro;			// size of read only memory
		size_t ss;			// size of stack
	} vm;

	/**
	 * Extension support api.
	 * 
	 * These functions can be used in extension libraries (dll or so)
	 */
	struct {
		/// Extend a namespace; @see Core#ccExtend
		symn (*const ccExtend)(ccContext ctx, symn cls);
		/// Begin a namespace; @see Core#ccBegin
		symn (*const ccBegin)(ccContext ctx, const char *name);
		/// Close a namespace; @see Core#ccEnd
		symn (*const ccEnd)(ccContext ctx, symn cls);

		/// Declare int constant; @see Core#ccDefInt
		symn (*const ccDefInt)(ccContext ctx, const char *name, int64_t value);
		/// Declare float constant; @see Core#ccDefFlt
		symn (*const ccDefFlt)(ccContext ctx, const char *name, float64_t value);
		/// Declare string constant; @see Core#ccDefStr
		symn (*const ccDefStr)(ccContext ctx, const char *name, char *value);

		/// Declare a typename; @see Core#ccDefType
		symn (*const ccDefType)(ccContext ctx, const char *name, unsigned size, int refType);
		/// Declare a native function; @see Core#ccDefCall
		symn (*const ccDefCall)(ccContext ctx, vmError libc(nfcContext), const char *proto);

		/// Lookup function by offset; @see Core#rtFindSym
		symn (*const rtFindSym)(rtContext ctx, size_t offset);

		/// Invoke a function; @see Core#invoke
		vmError (*const invoke)(rtContext ctx, symn fun, void *res, void *args, void *extra);

		/// Alloc, resize or free memory; @see Core#rtAlloc
		void *(*const rtAlloc)(rtContext ctx, void *ptr, size_t size);

		/// Reset to the first argument inside a native function
		size_t (*const nfcFirstArg)(nfcContext ctx);
		/// Advance to the next argument inside a native function
		size_t (*const nfcNextArg)(nfcContext ctx);

		/// get the argument, transforming offsets to pointers
		rtValue (*const nfcReadArg)(nfcContext ctx, size_t argOffs);
	} api;

	// memory management for the compiler and code generator
	unsigned char *_beg;        // permanent memory (increments)
	unsigned char *_end;        // temporary memory (decrements)

	// memory available for the compiler and runtime
	const size_t _size;         // size of available memory
	unsigned char _mem[];       // this is where the memory begins.
};

/**
 * Native function invocation context.
 */
struct nfcContextRec {
	rtContext rt;         // runtime context
	symn sym;             // the invoked function
	symn param;           // the current parameter
	char *proto;          // static data (passed to install)
	void *extra;          // extra data (passed to invoke)
	stkptr args;          // arguments
	size_t argc;          // argument count
};

/**
 * Get the pointer to an internal offset inside the vm.
 * 
 * @param ctx Runtime context.
 * @param offset global offset inside the vm.
 * @return pointer to the memory.
 */
static inline void *vmPointer(rtContext ctx, size_t offset) {
	if (offset == 0) {
		return NULL;
	}
	return (void*)(ctx->_mem + offset);
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
	// if result is not null copy
	if (result != NULL && size > 0) {
		memcpy(result, (char*)args->args + offset, size);
	}
	else {
		result = (void*)((char*)args->args + offset);
	}
	return result;
}

// speed up of getting arguments of known types
#define argget(__ARGV, __OFFS, __TYPE) (*(__TYPE*)((char*)(__ARGV)->args + (__OFFS)))
static inline int32_t argi32(nfcContext args, size_t offs) { return argget(args, offs, int32_t); }
static inline int64_t argi64(nfcContext args, size_t offs) { return argget(args, offs, int64_t); }
static inline uint32_t argu32(nfcContext args, size_t offs) { return argget(args, offs, uint32_t); }
static inline uint64_t argu64(nfcContext args, size_t offs) { return argget(args, offs, uint64_t); }
static inline float32_t argf32(nfcContext args, size_t offs) { return argget(args, offs, float32_t); }
static inline float64_t argf64(nfcContext args, size_t offs) { return argget(args, offs, float64_t); }
static inline void *arghnd(nfcContext args, size_t offs) { return argget(args, offs, void*); }
static inline size_t argref(nfcContext args, size_t offs) { return argget(args, offs, vmOffs); }
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
	if (result != NULL) {
		memcpy(args->args + args->argc, result, size);
	}
	return args->args + args->argc;
}

// speed up of setting result of known types.
#define retset(__ARGV, __TYPE, __VAL) ((__TYPE*)((__ARGV)->args + (__ARGV)->argc))[-1] = (__TYPE)(__VAL)
static inline void reti32(nfcContext args, int32_t val) { retset(args, int32_t, val); }
static inline void reti64(nfcContext args, int64_t val) { retset(args, int64_t, val); }
static inline void retu32(nfcContext args, uint32_t val) { retset(args, uint32_t, val); }
static inline void retu64(nfcContext args, uint64_t val) { retset(args, uint64_t, val); }
static inline void retf32(nfcContext args, float32_t val) { retset(args, float32_t, val); }
static inline void retf64(nfcContext args, float64_t val) { retset(args, float64_t, val); }
static inline void retref(nfcContext args, size_t val) { retset(args, vmOffs, val); }
static inline void rethnd(nfcContext args, void *val) { retset(args, char *, val); }
#undef retset

#ifdef __cplusplus
}
#endif
#endif

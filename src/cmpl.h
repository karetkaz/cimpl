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

typedef uint32_t vmOffs;                        // offset TODO: 32/64 bit offsets support
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
	illegalState,
	stackOverflow,
	divisionByZero,
	illegalInstruction,
	illegalMemoryAccess,
	//+ illegalArrayAccess, => TODO: array index out of bounds
	nativeCallError,
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
	unsigned foldCasts: 1;  // fold fold cast expressions (float(int(3.2)) => 3.f)
	unsigned foldInstr: 1;  // replace some instructions with a faster or shorter version (load 1, add => inc 1)
	unsigned fastMemory: 1; // fast memory access: use dup, set, load and store instructions instead of `load address` + `load indirect`.
	unsigned fastAssign: 1; // remove dup and set instructions when modifying the last declared variable.
	unsigned genGlobals: 1; // generate global variables as static variables

	unsigned warnLevel: 4;  // compile logging level (0-15)
	unsigned logLevel: 3;   // runtime logging level (0-7)
	int closeLog: 1;        // close log file
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
	 * contains:
	 *  * profiler function
	 *  * debugger function
	 *  * code line mapping
	 *  * break point lists
	 */
	dbgContext dbg;

	// virtual machine state
	struct {
		void *nfc;         // native call vector
		void *cell;        // execution units
		void *heap;        // heap memory

		size_t pc;         // exec: entry point / cgen: program counter
		size_t px;         // exec: exit point / cgen: -

		size_t ro;         // size of read only memory
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

		/// Declare a typename; @see ccDefType
		symn (*const ccDefType)(ccContext ctx, const char *name, unsigned size, int refType);

		/// Declare a native function; @see ccDefCall
		symn (*const ccDefCall)(ccContext ctx, vmError libc(nfcContext), const char *proto);

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
	const rtContext rt;         // runtime context
	const symn sym;             // invoked function (returned by ccDefCall)
	const void *proto;          // static data (passed to ccDefCall)
	const void *extra;          // extra data (passed to execute or invoke)
	const void *args;           // arguments
	const size_t argc;          // argument count in bytes
	symn param;           // the current parameter (modified by nfcFirstArg and nfcNextArg)
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
	return ctx->_mem + offset;
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
		memcpy(result, (char *) args->args + offset, size);
	}
	else {
		result = (void *) ((char *) args->args + offset);
	}
	return result;
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
	if (result != NULL) {
		memcpy((char *) args->args + args->argc, result, size);
	}
	return (char *) args->args + args->argc;
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

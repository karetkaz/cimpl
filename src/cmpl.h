/**
 * Plugin sources should include only this header file.
 */

#ifndef CMPL_API_H
#define CMPL_API_H 2

#include <stdio.h> // FILE
#include <string.h> // memcpy

#ifdef _MSC_VER
typedef signed char			int8_t;
typedef signed short		int16_t;
typedef signed long			int32_t;
typedef signed long long	int64_t;
typedef unsigned char		uint8_t;
typedef unsigned short		uint16_t;
typedef unsigned long		uint32_t;
typedef unsigned long long	uint64_t;
#define inline __inline
#else
#include <stdint.h>
#include <stddef.h>

#endif
typedef float float32_t;
typedef double float64_t;

#ifdef __cplusplus
extern "C" {
#endif

typedef uint32_t vmOffs;						// offset
typedef uint32_t *stkptr;						// stack
typedef struct symNode *symn;					// symbol
typedef struct rtContextRec *rtContext;			// runtimeContext
typedef struct ccContextRec *ccContext; 		// compilerContext
typedef struct dbgContextRec *dbgContext;		// debuggerContext
typedef struct nfcContextRec *nfcContext;		// nativeCallContext
typedef struct userContextRec *userContext;		// customUserContext

/**
 * @brief Runtime error codes
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
 * @brief Runtime context
 */
struct rtContextRec {
	int32_t errors;         // error count
	// disable code generation optimization
	unsigned foldConst: 1;  // fold constant expressions (3 + 4 => 7)
	unsigned foldInstr: 1;  // replace some instructions with a faster or shorter version (load 1, add => inc 1)
	unsigned fastAssign: 1; // remove dup and set instructions when modifying the last declared variable.
	unsigned genGlobals: 1; // generate global variables as static variables

	unsigned logLevel: 3;   // runtime logging level (0-7)
	unsigned logClose: 1;   // close log file
	unsigned freeMem: 1;    // release memory
	FILE *logFile;          // log file

	/**
	 * main initializer function
	 */
	symn main;

	/**
	 * @brief Compiler context.
	 * @note After code generation it is set to null.
	 */
	ccContext cc;

	/**
	 * @brief Debug context.
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

		size_t pc;			// exec: entry point / cgen: prev program counter
		size_t px;			// exec: exit point / cgen: program counter

		size_t ro;			// exec: read only memory / cgen: function parameters
		size_t ss;			// exec: stack size / cgen: stack size

		size_t sm;			// exec: - / cgen: minimum stack size
		size_t su;			// exec: - / cgen: stack access (parallel processing)
	} vm;

	/**
	 * @brief External library API support.
	 */
	struct {
		/// Begin a namespace; @see Core#ccBegin
		symn (*const ccBegin)(ccContext, const char *name);
		/// Close a namespace; @see Core#ccEnd
		void (*const ccEnd)(ccContext, symn cls);

		/// Declare int constant; @see Core#ccDefInt
		symn (*const ccDefInt)(ccContext, const char *name, int64_t value);
		/// Declare float constant; @see Core#ccDefFlt
		symn (*const ccDefFlt)(ccContext, const char *name, float64_t value);
		/// Declare string constant; @see Core#ccDefStr
		symn (*const ccDefStr)(ccContext, const char *name, char *value);

		/// Declare a typename; @see Core#ccDefType
		symn (*const ccDefType)(ccContext, const char *name, unsigned size, int refType);
		/// Declare a native function; @see Core#ccDefCall
		symn (*const ccDefCall)(ccContext, vmError libc(nfcContext), void *extra, const char *proto);

		/// Lookup symbol by offset; @see Core#rtFindSym
		symn (*const rtFindSym)(rtContext, void *ptr);

		/// Invoke a function; @see Core#invoke
		vmError (*const invoke)(rtContext, symn fun, void *res, void *args, void *extra);

		/// Alloc, resize or free memory; @see Core#rtAlloc
		void *(*const rtAlloc)(rtContext, void *ptr, size_t size);
	} api;

	// memory related
	const size_t _size;         // size of available memory
	unsigned char *_beg;        // permanent memory (increments)
	unsigned char *_end;        // temporary memory (decrements)
	unsigned char _mem[];       // this is where the memory begins.
};

/**
 * @brief Native function invocation context.
 */
struct nfcContextRec {
	rtContext rt;         // runtime context
	symn sym;             // the invoked function
	char *proto;          // static data (passed to install)
	void *extra;          // extra data (passed to invoke)
	stkptr args;          // arguments
	int argc;             // argument count
};

/**
 * @brief Get the value of a native function argument.
 * @param args Native function call arguments context.
 * @param result Optionally copy here the result.
 * @param offset Relative offset of argument.
 * @param size Size of the argument to copy to result.
 * @return Pointer where the result is located or copied.
 * @note Getting an argument must be used with padded offset.
 */
static inline void *argget(nfcContext args, void *result, size_t offset, size_t size) {
	// if result is not null copy
	if (result != NULL) {
		memcpy(result, (char*)args->args + offset, size);
	}
	else {
		result = (void*)((char*)args->args + offset);
	}
	return result;
}

// speed up of getting arguments of known types
#define argget(__ARGV, __OFFS, __TYPE) (*(__TYPE*)((char*)__ARGV->args + (__OFFS)))
static inline int32_t argi32(nfcContext args, size_t offs) { return argget(args, offs, int32_t); }
static inline int64_t argi64(nfcContext args, size_t offs) { return argget(args, offs, int64_t); }
static inline float32_t argf32(nfcContext args, size_t offs) { return argget(args, offs, float32_t); }
static inline float64_t argf64(nfcContext args, size_t offs) { return argget(args, offs, float64_t); }
static inline void *arghnd(nfcContext args, size_t offs) { return argget(args, offs, void*); }
static inline size_t argref(nfcContext args, size_t offs) { return argget(args, offs, vmOffs); }
#undef argget

/**
 * @brief Set the return value of a wrapped native call.
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
#define retset(__ARGV, __TYPE, __VAL) ((__TYPE*)(__ARGV->args + __ARGV->argc))[-1] = (__TYPE)(__VAL)
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

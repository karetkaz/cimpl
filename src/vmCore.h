/**
 * virtual machine core functions.
 */
#ifndef VM_CORE_H
#define VM_CORE_H

#include "cmpl.h"

#ifdef __cplusplus
extern "C" {
#endif

// Opcodes - VM
typedef enum {
	#define OPCODE_DEF(Name, Code, Size, In, Out, Text) Name = Code,
	#include "defs.inl"
	opc_last,		// first invalid opcode

	opc_ldi,		// argument is the size
	opc_sti,		// argument is the size
	opc_drop,		// convert this to opcode.spc

	markIP,

	b32_bit_and = 0 << 6,
	b32_bit_shl = 1 << 6,
	b32_bit_shr = 2 << 6,
	b32_bit_sar = 3 << 6,

	vm_size = 4,	// stack alignment: size of one element on stack; must be 4: 32bits
	pad_size = 8,	// memory alignment: sizeof(void*), // value used to pad pointers
	px_size = 4,	// size in bytes of the exit instruction halt()
	vm_regs = 255	// maximum registers for dup, set, pop, ...
} vmOpcode;
struct opcodeRec {
	signed int const code;			// opcode value (0..255)
	unsigned int const size;		// length of opcode with args
	unsigned int const stack_in;	// operation requires n elements on stack
	unsigned int const stack_out;	// operation produces n elements on stack
	const char *const name;			// mnemonic for the opcode
};
extern const struct opcodeRec opcode_tbl[256];

/// Abstract Syntax Tree Node
typedef struct astNode *astn;
/// Debug Node
typedef struct dbgNode *dbgn;

/**
 * @brief Initialize runtime context.
 * @param mem optionally use pre allocated memory.
 * @param size the size of memory in bytes to be used.
 * @return runtime context.
 */
rtContext rtInit(void *mem, size_t size);

/**
 * @brief Close runtime context.
 * @param Runtime context.
 * release resources.
 */
void rtClose(rtContext);

// open log file for both compiler and runtime
void logFILE(rtContext, FILE *file);
int logfile(rtContext, char *file, int append);

/** Generate executable bytecode.
 * @param Runtime context.
 * @param debug generate debug info
 * @return boolean value of success.
 */
int gencode(rtContext, int debug);

/**
 * @brief Execute the compiled script.
 * @param Runtime context.
 * @param argc number of arguments.
 * @param argv the program arguments.
 * @param extra Extra data for native calls.
 * @return Error code of execution.
 */
vmError execute(rtContext, int argc, char *argv[], void *extra);

/**
 * @brief Invoke a function inside the vm.
 * @param Runtime context.
 * @param fun Symbol of the function.
 * @param res Result value of the invoked function. (May be null.)
 * @param args Arguments for the function. (May be null.)
 * @param extra Extra data for each native call executed from here.
 * @return Error code of execution. (0 means success.)
 * @usage see @rtFindSym example.
 * @note Invocation to execute must precede this call.
 */
vmError invoke(rtContext, symn fun, void *ret, void *args, const void *extra);

/**
 * @brief Lookup a static symbol by offset.
 * @param Runtime context.
 * @param ptr Pointer to the variable.
 * @param callsOnly lookup only functions (always true from api calls).
 * @note Useful for callbacks.
 * @usage see also: test.gl/gl.c

	static symn onMouse = NULL;

	static int setMouseCb(libcContext rt) {
		void *fun = argref(rt, 0);

		// un-register event callback
		if (fun == NULL) {
			onMouse = NULL;
			return 0;
		}

		// register event callback using the symbol of the function.
		onMouse = rt->api.rtFindSym(rt, fun);

		// runtime error if symbol is not valid.
		return onMouse != NULL;
	}

	static int mouseCb(int btn, int x, int y) {
		if (onMouse != NULL && rt != NULL) {
			// invoke the callback with arguments.
			struct {int32_t btn, x, y;} args = {btn, x, y};
			rt->api.invoke(rt, onMouse, NULL, &args, NULL);
		}
	}

	// expose the callback register function to the compiler
	if (!rt->api.ccDefCall(rt, setMouseCb, NULL, "void setMouseCallback(void Callback(int32 b, int32 x, int32 y)")) {
		error...
	}
 */
symn rtFindSym(rtContext, size_t offs, int callsOnly);

/**
 * @brief Allocate or free memory inside the vm.
 * @param Runtime context.
 * @param ptr Allocated memory address in the vm, or null.
 * @param size New size to reallocate or 0 to free memory.
 * @param dbg debug memory callback to be invoked for each memory block (feature is disabled for plugins).
 * @return Pointer to the allocated memory.
 * cases:
 * 		size == 0 && ptr == null: debug
 * 		size == 0 && ptr != null: free
 * 		size >  0 && ptr == null: alloc
 * 		size >  0 && ptr != null: re-alloc
 * @note Invocation to execute must precede this call.
 */
void *rtAlloc(rtContext, void *ptr, size_t size, void dbg(dbgContext rt, void *mem, size_t size, char *kind));

size_t nfcNextArg(nfcContext nfc);
size_t nfcFirstArg(nfcContext nfc);
size_t nfcNextNamedArg(nfcContext nfc, char *name);

vmValue *nfcPeekArg(nfcContext nfc, size_t offs);
rtValue nfcReadArg(nfcContext nfc, size_t offs);

/// returns a pointer to an offset inside the vm.
static inline void *vmPointer(rtContext rt, size_t offset) {
	if (offset == 0) {
		return NULL;
	}
	return (void*)(rt->_mem + offset);
}

/**
 * @brief Get the internal offset of a reference.
 * @param ptr Memory location.
 * @return The internal offset.
 * @note Aborts if ptr is not null and not inside the context.
 */
size_t vmOffset(rtContext, void *ptr);

/**
 * @brief Emit an instruction.
 * @param Runtime context.
 * @param opc Opcode.
 * @param arg Argument.
 * @return Program counter.
 */
size_t emitOpc(rtContext, vmOpcode opc, vmValue arg);

/**
 * @brief Test for an instruction at the given offset.
 * @param Runtime context.
 * @param offs Offset of the opcode.
 * @param opc Operation code to check.
 * @param arg Copy the argument of the opcode.
 * @return non zero if at the given location the opc was found.
 */
int testOcp(rtContext rt, size_t offs, vmOpcode opc, vmValue *arg);

/**
 * @brief Emit an instruction with int argument.
 * @param Runtime context.
 * @param opc Opcode.
 * @param arg Integer argument.
 * @return Program counter.
 */
static inline size_t emitInt(rtContext rt, vmOpcode opc, int64_t value) {
	vmValue arg;
	arg.i64 = value;
	return emitOpc(rt, opc, arg);
}

/**
 * @brief Fix a jump instruction.
 * @param Runtime context.
 * @param src Location of the instruction.
 * @param dst Absolute position to jump.
 * @param stc Fix also stack size.
 * @return
 */
int fixJump(rtContext, size_t src, size_t dst, ssize_t stc);

/**
 * print stack trace
 */
void traceCalls(dbgContext dbg, FILE *out, int indent, size_t skip, size_t );

/**
 * @brief Print formatted text to the output stream.
 * @param out Output stream.
 * @param esc Escape translation (format string will be not escaped).
 * @param fmt Format text.
 * @param ... Format variables.
 * @note %[?]?[+-]?[0 ]?[0-9]*(\.([*]|[0-9]*))?[tTKAIbBoOxXuUdDfFeEsScC]
 *    skip: [?]? skip printing `(null)` or `0` (may print pad character if specified)
 *    sign: [+-]? sign flag / alignment.
 *    pad:  [0 ]? padding character.
 *    size: [0-9]* length / indent.
 *    mode: (\.[*]|[0-9]*)? precision / mode.
 *
 *    T: symbol (inline, typename, function or variable)
 *      %T: print qualified symbol name with arguments
 *      %.T: print simple symbol name
 *      %+T: expands fields on multiple lines
 *      %-T: same as '%+T', but skips first indent
 *
 *    t: token (abstract syntax tree)
 *      %t: print statement or expression
 *      %.t: print only the token
 *      %+t: expand statements on multiple lines
 *      %-t: same as '%+t', but skip first indent
 *
 *    K: symbol kind
 *    k: token kind
 *    A: instruction (asm)
 *    I: indent
 *    b: 32 bit bin value
 *    B: 64 bit bin value
 *    o: 32 bit oct value
 *    O: 64 bit oct value
 *    x: 32 bit hex value
 *    X: 64 bit hex value
 *    u: 32 bit unsigned value
 *    U: 64 bit unsigned value
 *    d: 32 bit signed value (decimal)
 *    D: 64 bit signed value (decimal)
 *    f: 32 bit floating point value
 *    F: 64 bit floating point value
 *    e: 32 bit floating point value (Scientific notation (mantissa/exponent))
 *    E: 64 bit floating point value (Scientific notation (mantissa/exponent))
 *    s: ansi string
 *    c: ansi character
 *    S: ?wide string
 *    C: ?wide character
 * flags
 *    +-
 *        oOuU: ignored
 *        bBxX: lowerCase / upperCase
 *        dDfFeE: forceSign /
 *
 */
void printFmt(FILE *out, const char **esc, const char *fmt, ...);

// Debugging.
dbgn getDbgStatement(rtContext rt, char *file, int line);
dbgn mapDbgStatement(rtContext rt, size_t position);
dbgn addDbgStatement(rtContext rt, size_t start, size_t end, astn tag);
dbgn mapDbgFunction(rtContext rt, size_t position);
dbgn addDbgFunction(rtContext rt, symn fun);

// translate error to message
char* vmErrorMessage(vmError error);

#ifdef __cplusplus
}
#endif
#endif

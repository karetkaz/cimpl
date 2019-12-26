/**
 * Compiler core functions.
 */

#ifndef CMPL_CC_H
#define CMPL_CC_H

#include "cmpl.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
	#define TOKEN_DEF(NAME, TYPE, SIZE, STR) NAME,
	#include "defs.inl"
	tok_last,

	TOKEN_opc = EMIT_kwd,
	OPER_beg = OPER_fnc,
	OPER_end = OPER_com,
} ccToken;

struct tokenRec {
	int const	type;
	int const	argc;
	char *const	name;
};
extern const struct tokenRec token_tbl[256];

typedef enum {
	installBase = 0x0000,                       // base type system only

	install_ptr = 0x0001,                       // pointers with malloc(memory manager)
	install_var = 0x0002,                       // variants with reflection(runtime type system)
	install_obj = 0x0004,                       // objects with inheritance(counted references)

	installLibs = 0x0008,                       // install standard native functions and extensions

	installEmit = 0x0010 | installBase,         // emit intrinsic: emit(...)
	installEopc = 0x0020 | installEmit,         // emit opcodes: emit.i32.add
	installEswz = 0x0040 | installEopc,         // swizzle constants: emit.swz.(xxxx, ... xyzw, ... wwww)

	// register defaults if ccInit not invoked explicitly.
	install_min = install_ptr | install_var | install_obj | installLibs,
	install_def = install_min | installEopc,
} ccInstall;

/**
 * Initialize compiler context.
 *
 * @param rt Runtime context.
 * @param mode specify what to install.
 * @param onHalt function to be executed when execution and external function invocation terminates.
 * @return compiler context.
 * @note installs: builtin types, builtin functions, emit intrinsic, ...
 */
ccContext ccInit(rtContext rt, ccInstall mode, vmError onHalt(nfcContext));

/**
 * Open a stream (file or text) for compilation.
 *
 * @param cc Compiler context.
 * @param file file name of input.
 * @param line first line of input.
 * @param text if not null, this will be compiled instead of the file.
 * @return error code, 0 on success.
 */
int ccOpen(ccContext cc, char *file, int line, char *text);

/**
 * Close stream, ensuring it ends correctly.
 *
 * @param cc Compiler context.
 * @return number of errors.
 */
int ccClose(ccContext cc);

/**
 * Begin a namespace (static struct) or scope.
 *
 * @param cc Compiler context.
 * @param name Name of the namespace.
 * @return Defined symbol, null on error (error is logged).
 */
symn ccBegin(ccContext cc, const char *name);
/**
 * Extend a namespace (static struct) with static members.
 *
 * @param cc Compiler context.
 * @param sym Namespace to be extended.
 * @return type if it can be extended.
 */
symn ccExtend(ccContext cc, symn sym);
/**
 * Close the namespace returned by ccBegin or ccExtend.
 *
 * @param cc Compiler context.
 * @param cls Namespace to be closed.
 * @note Makes all declared variables static.
*/
symn ccEnd(ccContext cc, symn sym);

/**
 * Define an integer constant.
 *
 * @param cc Compiler context.
 * @param name Name of the constant.
 * @param value Value of the constant.
 * @return Defined symbol, null on error.
 */
symn ccDefInt(ccContext cc, const char *name, int64_t value);
/**
 * Define a floating point constant.
 *
 * @param cc Compiler context.
 * @param name Name of the constant.
 * @param value Value of the constant.
 * @return Defined symbol, null on error.
 */
symn ccDefFlt(ccContext cc, const char *name, double value);
/**
 * Define a string constant.
 *
 * @param cc Compiler context.
 * @param name Name of the constant.
 * @param value Value of the constant.
 * @return Defined symbol, null on error.
 */
symn ccDefStr(ccContext cc, const char *name, char *value);

/**
 * Define a variable or reference.
 *
 * @param cc Compiler context.
 * @param name Name of the variable.
 * @param type Type of the variable.
 * @return Defined symbol, null on error.
 */
symn ccDefVar(ccContext cc, const char *name, symn type);

/**
 * Define a new type.
 *
 * @param cc Compiler context.
 * @param name Name of the type.
 * @param size Size of the type.
 * @param refType Value or Reference type.
 * @return Defined symbol, null on error.
 * @see plugins/file.c
 */
symn ccAddType(ccContext cc, const char *name, unsigned size, int refType);

/**
 * Define a native function.
 *
 * @param cc Compiler context.
 * @param call The native c function wrapper.
 * @param proto Prototype of the function.
 * @return Defined symbol, null on error.
 * @usage
	static vmError f64sin(nfcContext rt) {
		float64_t x = argf64(rt, 0);	// get first argument
		retf64(rt, sin(x));				// set the return value
		return noError;					// do not abort execution
	}
	...
	if (!rt->api.ccDefCall(rt, f64sin, "float64 sin(float64 x)")) {
		// error was reported, terminate by returning error count.
		return rt->errors;
	}
 */
symn ccAddCall(ccContext cc, vmError call(nfcContext), const char *proto);

/**
 * Compile the given file or text as a unit.
 *
 * @param cc Compiler context.
 * @param init function installing types and native functions.
 * @param file File name of input.
 * @param line First line of input.
 * @param text If not null, this will be compiled instead of the file.
 * @return Root node of the compilation unit.
 */
astn ccAddUnit(ccContext cc, int init(ccContext), char *file, int line, char *text);

/**
 * Generate bytecode from the compiled syntax tree.
 *
 * @param cc Compiler context.
 * @param debug generate debug info
 * @return error code/count, 0 on success.
 */
int ccGenCode(ccContext cc, int debug);

/**
 * Lookup a symbol by name.
 *
 * @param rt Runtime context.
 * @param scope search in the members of this symbol.
 * @param name name of the symbol to be found.
 * @return the first found symbol.
 */
symn ccLookup(rtContext rt, symn scope, char *name);

/// standard modules for `ccAddLib`
int ccLibStd(ccContext cc);

#ifdef __cplusplus
}
#endif
#endif

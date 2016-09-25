/**
 * compiler core functions.
 */
#ifndef CC_CORE_H
#define CC_CORE_H

#include "plugin.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
	//CAST_any   = 0x0000,		// null
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
	CAST_arr   = 0x000b,		// slice: pair of {size, data}
	CAST_var   = 0x000c,		// variant: pair of {type, data}
	//CAST_d   = 0x000d,		// map: double constants[string]
	//CAST_e   = 0x000e,		// unused
	EMIT_opc   = 0x000f,		// TODO: remove

	KIND_def   = 0x0000,		// alias (/ error at runtime)
	KIND_typ   = 0x0010,		// typename: struct metadata info.
	KIND_fun   = 0x0020,		// function
	KIND_var   = 0x0030,		// variable: function and typename are also variables

	ATTR_stat  = 0x0040,		// static attribute
	ATTR_cnst  = 0x0080,		// constant attribute
	ATTR_memb  = 0x0100,		// member operator, push also reference to this
	ATTR_paral = 0x0100,		// parallel

	MASK_cast  = 0x000f,
	MASK_kind  = 0x0030,
	MASK_attr  = 0x00c0,

	// TODO: to delete
	TYPE_any = KIND_def,
} ccKind;
// Tokens - CC
typedef enum {
	#define TOKEN_DEF(NAME, TYPE, SIZE, STR) NAME,
	#include "defs.inl"
	tok_last,

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
	installBase = 0x0000,                       // basic type system only

	install_ptr = 0x0001,                       // pointers with malloc(memory manager)
	install_var = 0x0002,                       // variants with reflection(runtime type system)
	install_obj = 0x0004,                       // objects with inheritance(counted references)

	installEmit = 0x0010 | installBase,         // emit intrinsic: emit(...)
	installEopc = 0x0020 | installEmit,         // emit opcodes: emit.i32.add
	installEswz = 0x0040 | installEopc,         // swizzle constants: emit.swz.(xxxx, ... xyzw, ... wwww)

	// register defaults if ccInit not invoked explicitly.
	install_min = install_ptr | install_var | install_obj,
	install_def = install_ptr | install_var | install_obj,// | installEopc,
	install_all = install_ptr | install_var | install_obj | installEswz,
} ccInstall;

/**
 * @brief Initialize compiler context.
 * @param runtime context.
 * @param mode specify what to install.
 * @param onHalt function to be executed when execution and external function invokation terminates.
 * @return compiler context.
 * @note installs: builtin types, builtin functions, emit intrinsic, ...
 */
ccContext ccInit(rtContext, ccInstall mode, vmError onHalt(nfcContext));

/**
 * @brief Begin a namespace (static struct) or scope.
 * @param rt Runtime context.
 * @param name Name of the namespace.
 * @return Defined symbol, null on error (erorr is logged).
 */
symn ccBegin(rtContext rt, const char *name);
/**
 * @brief Close the namespace.
 * @param rt Runtime context.
 * @param cls Namespace to be closed. (The returned by ccBegin.)
 * @note Makes all declared variables static.
*/
symn ccEnd(rtContext rt, symn cls);

/**
 * @brief Define an integer constant.
 * @param rt Runtime context.
 * @param name Name of the constant.
 * @param value Value of the constant.
 * @return Defined symbol, null on error.
 */
symn ccDefInt(rtContext rt, const char *name, int64_t value);
/**
 * @brief Define a floating point constant.
 * @param rt Runtime context.
 * @param name Name of the constant.
 * @param value Value of the constant.
 * @return Defined symbol, null on error.
 */
symn ccDefFlt(rtContext rt, const char *name, double value);
/**
 * @brief Define a string constant.
 * @param rt Runtime context.
 * @param name Name of the constant.
 * @param value Value of the constant.
 * @return Defined symbol, null on error.
 */
symn ccDefStr(rtContext rt, const char *name, char *value);

/**
 * @brief Add a type to the runtime.
 * @param rt Runtime context.
 * @param name Name of the type.
 * @param size Size of the type.
 * @param refType Value or Reference type.
 * @return Defined symbol, null on error.
 * @see: lstd.File;
 */
symn ccDefType(rtContext rt, const char *name, unsigned size, int refType);
/**
 * @brief Add a native function (libcall) to the runtime.
 * @param rt Runtime context.
 * @param call The native c function.
 * @param proto Prototype of the function.
 * @return Defined symbol, null on error.
 * @usage
	static vmError f64sin(libcContext rt) {
		float64_t x = argf64(rt, 0);	// get first argument
		retf64(rt, sin(x));				// set the return value
		return noError;					// no runtime error in call
	}
	...
	if (!rt->api.ccDefCall(rt, f64sin, "float64 sin(float64 x)")) {
		// error reported, terminate the execution, retun error count.
		return rt->errors;
	}
 */
symn ccDefCall(rtContext rt, vmError call(nfcContext), const char *proto);

/**
 * @brief Compile the given file or text as a unit.
 * @param rt Runtime context.
 * @param warn Warning level.
 * @param file File name of input.
 * @param line First line of input.
 * @param text If not null, this will be compiled instead of the file.
 * @return Root node of the compilation unit.
 */
astn ccAddUnit(rtContext rt, int warn, char *file, int line, char *text);

/** Add a module / library
 * @brief Execute the libInit function then optionally compile the unit.
 * @param rt Runtime context.
 * @param warn warning level.
 * @param init function installing types and native functions.
 * @param unit extension file to be compiled with this library.
 * @return boolean value of success.
 */
int ccAddLib(rtContext rt, int warn, int init(rtContext), char *unit);

/**
 * @brief Find symbol by name.
 * @param cc Compiler context.
 * @param in search in the members of this symbol.
 * @param name name of the symbol to be found.
 * @return the first found symbol.
 */
symn ccLookupSym(ccContext cc, symn in, char *name);

/// standard functions
int ccLibStdc(rtContext);
/// file access
int ccLibFile(rtContext);

#ifdef __cplusplus
}
#endif
#endif

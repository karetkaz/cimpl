/*******************************************************************************
 *   File: core.c
 *   Date: 2011/06/23
 *   Desc: core functionality
 *******************************************************************************
 * initializations
 * memory management
 */

#include "internal.h"

const char *const pluginLibInstall = "cmplInit";
const char *const pluginLibDestroy = "cmplDone";

const char *const type_fmt_signed32 = "%d";
const char *const type_fmt_signed64 = "%D";
const char *const type_fmt_unsigned32 = "%u";
const char *const type_fmt_unsigned64 = "%U";
const char *const type_fmt_float32 = "%f";
const char *const type_fmt_float64 = "%F";
const char *const type_fmt_character = "%c";
const char *const type_fmt_string = "%s";
const char *const type_fmt_typename = "<%T>";
const char type_fmt_string_chr = '\"';
const char type_fmt_character_chr = '\'';

// return type for file and name will be converted to string
static const char *const type_get_file = "typename file(typename type)";
static const char *const type_get_line = "int32 line(typename type)";
static const char *const type_get_name = "typename name(typename type)";
static const char *const type_get_base = "typename base(typename type)";

/// Private dummy on exit native function.
static vmError haltDummy(nfcContext args) {
	(void)args;
	return noError;
}

/// Private native function for reflection.
static vmError typenameGetField(nfcContext ctx) {
	size_t symOffs = argref(ctx, 0);
	symn sym = rtLookup(ctx->rt, symOffs, 0);
	if (sym == NULL || sym->offs != symOffs) {
		// invalid symbol offset
		return nativeCallError;
	}
	if (ctx->proto == type_get_file) {
		retref(ctx, vmOffset(ctx->rt, sym->file));
		return noError;
	}
	if (ctx->proto == type_get_line) {
		reti32(ctx, sym->line);
		return noError;
	}
	if (ctx->proto == type_get_name) {
		retref(ctx, vmOffset(ctx->rt, sym->name));
		return noError;
	}
	if (ctx->proto == type_get_base) {
		retref(ctx, vmOffset(ctx->rt, sym->type));
		return noError;
	}
	return nativeCallError;
}

/// Private rtAlloc wrapper for the api
static void *rtAllocApi(rtContext rt, void *ptr, size_t size) {
	return rtAlloc(rt, ptr, size, NULL);
}

/// Private raise wrapper for the api
static void raiseApi(nfcContext ctx, int level, const char *msg, ...) {
	rtContext rt = ctx->rt;
	va_list vaList;
	va_start(vaList, msg);
	print_log(rt, level, NULL, 0, NULL, msg, vaList);
	va_end(vaList);

	// print stack trace including this function
	if (rt->dbg != NULL && rt->traceLevel > 0) {
		traceCalls(rt->dbg, rt->logFile, 1, rt->traceLevel, 0);
	}
}

/// Private function lookup  wrapper for the api
static symn rtLookupApi(rtContext rt, size_t offset) {
	symn sym = rtLookup(rt, offset, 0);
	if (sym == NULL || !isFunction(sym)) {
		return NULL;
	}
	return sym;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ Native

size_t nfcFirstArg(nfcContext nfc) {
	nfc->param = (void *) -1;
	return nfcNextArg(nfc);
}

size_t nfcNextArg(nfcContext nfc) {
	symn param = nfc->param;
	if (param == (void *) -1) {
		param = nfc->sym->params;
		nfc->param = param;
	}

	if (param == NULL || param->next == NULL) {
		fatal(ERR_INTERNAL_ERROR);
		return 0;
	}

	nfc->param = param = param->next;
	return nfc->argc - param->offs;
}

/**
 * Get a pointer to the native call argument.
 * 
 * @param nfc the native call context.
 * @param offs relative offset of the argument.
 * @return pointer to the argument at the relative offset.
 * @note the values on the stack will contain offsets, not pointers
 */
static inline vmValue *nfcPeekArg(nfcContext nfc, size_t argOffs) {
	return (vmValue *) (((char *) nfc->args) + argOffs);
}

rtValue nfcReadArg(nfcContext nfc, size_t offs) {
	vmValue *value = nfcPeekArg(nfc, offs);
	rtValue result;
	memset(&result, 0, sizeof(result));
	switch (castOf(nfc->param)) {
		default:
			fatal(ERR_INTERNAL_ERROR);
			break;

		case CAST_bit:
		case CAST_i32:
		case CAST_u32:
		case CAST_f32:
			// copy 32 bits
			result.i32 = value->i32;
			break;

		case CAST_i64:
		case CAST_u64:
		case CAST_f64:
			// copy 64 bits
			result.i64 = value->i64;
			break;

		case CAST_ref:
			result.ref = vmPointer(nfc->rt, value->ref);
			break;

		case CAST_arr:
			result.ref = vmPointer(nfc->rt, value->ref);
			result.length = value->length;  // FIXME: length may be missing || static
			break;

		case CAST_var:
			result.ref = vmPointer(nfc->rt, value->ref);
			result.type = vmPointer(nfc->rt, value->type);
			break;

		case CAST_val:
			if (nfc->param->size > sizeof(result)) {
				fatal(ERR_UNIMPLEMENTED_FEATURE);
				return result;
			}
			memcpy(&result, value, nfc->param->size);
			break;
	}
	return result;
}

void nfcCheckArg(nfcContext nfc, ccKind cast, char *name) {
	symn param = nfc->param;
	if (param == (void *) -1) {
		fatal(ERR_INTERNAL_ERROR);
	}
	if (param == NULL) {
		fatal(ERR_INTERNAL_ERROR);
	}
	if (cast && cast != refCast(param)) {
		fatal(ERR_INTERNAL_ERROR);
	}
	if (name && strcmp(name, param->name) != 0) {
		fatal(ERR_INTERNAL_ERROR);
	}
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ Language

/// Install instruction for the emit intrinsic
static inline symn ccDefOpCode(ccContext cc, const char *name, symn type, vmOpcode code, size_t args) {
	astn opc = newNode(cc, TOKEN_opc);
	if (opc != NULL) {
		opc->type = type;
		opc->opc.code = code;
		opc->opc.args = args;
		return install(cc, name, ATTR_cnst | ATTR_stat | KIND_def, 0, type, opc);
	}
	return NULL;
}

/// Install type system.
static void install_type(ccContext cc, ccInstall mode) {
	symn type_ptr = NULL, type_var = NULL, type_obj = NULL;
	astn init_val = intNode(cc, 0);

	symn type_rec = install(cc, "typename", ATTR_stat | ATTR_cnst | KIND_typ | CAST_ref, vm_ref_size, NULL, NULL);
	// update required variables to be able to install other types.
	cc->type_rec = type_rec->type = type_rec;   // TODO: !cycle: typename is instance of typename
	type_rec->size = sizeof(struct symNode);    // expose the real size of the internal representation.
	init_val->type = type_rec;	// HACK: the only type we have right now ...

	symn type_vid = install(cc, "void", ATTR_stat | ATTR_cnst | KIND_typ | CAST_vid, 0, type_rec, NULL);
	symn type_bol = install(cc, "bool", ATTR_stat | ATTR_cnst | KIND_typ | CAST_bit, 1, type_rec, init_val);
	symn type_chr = install(cc, "char", ATTR_stat | ATTR_cnst | KIND_typ | CAST_i32, 1, type_rec, init_val);
	symn type_i08 = install(cc, "int8", ATTR_stat | ATTR_cnst | KIND_typ | CAST_i32, 1, type_rec, init_val);
	symn type_i16 = install(cc, "int16", ATTR_stat | ATTR_cnst | KIND_typ | CAST_i32, 2, type_rec, init_val);
	symn type_i32 = install(cc, "int32", ATTR_stat | ATTR_cnst | KIND_typ | CAST_i32, 4, type_rec, init_val);
	symn type_i64 = install(cc, "int64", ATTR_stat | ATTR_cnst | KIND_typ | CAST_i64, 8, type_rec, init_val);
	symn type_u08 = install(cc, "uint8", ATTR_stat | ATTR_cnst | KIND_typ | CAST_u32, 1, type_rec, init_val);
	symn type_u16 = install(cc, "uint16", ATTR_stat | ATTR_cnst | KIND_typ | CAST_u32, 2, type_rec, init_val);
	symn type_u32 = install(cc, "uint32", ATTR_stat | ATTR_cnst | KIND_typ | CAST_u32, 4, type_rec, init_val);
	symn type_u64 = install(cc, "uint64", ATTR_stat | ATTR_cnst | KIND_typ | CAST_u64, 8, type_rec, init_val);
	symn type_f32 = install(cc, "float32", ATTR_stat | ATTR_cnst | KIND_typ | CAST_f32, 4, type_rec, init_val);
	symn type_f64 = install(cc, "float64", ATTR_stat | ATTR_cnst | KIND_typ | CAST_f64, 8, type_rec, init_val);
	init_val->type = type_i64;

	if (mode & install_ptr) {
		type_ptr = install(cc, "pointer", ATTR_stat | ATTR_cnst | KIND_typ | CAST_ref, 1 * vm_ref_size, type_rec, NULL);
	}
	if (mode & install_var) {
		type_var = install(cc, "variant", ATTR_stat | ATTR_cnst | KIND_typ | CAST_var, 2 * vm_ref_size, type_rec, NULL);
	}
	symn type_fun = install(cc, "function", ATTR_stat | ATTR_cnst | KIND_typ | CAST_ref, 1 * vm_ref_size, type_rec, NULL);
	if (mode & install_obj) {
		type_obj = install(cc,  "object", ATTR_stat | ATTR_cnst | KIND_typ | CAST_ref, 1 * vm_ref_size, type_rec, NULL);
	}

	type_vid->fmt = NULL;
	type_bol->fmt = type_fmt_signed32;
	type_chr->fmt = type_fmt_character;
	type_i08->fmt = type_fmt_signed32;
	type_i16->fmt = type_fmt_signed32;
	type_i32->fmt = type_fmt_signed32;
	type_i64->fmt = type_fmt_signed64;
	type_u08->fmt = type_fmt_unsigned32;
	type_u16->fmt = type_fmt_unsigned32;
	type_u32->fmt = type_fmt_unsigned32;
	type_u64->fmt = type_fmt_unsigned64;
	type_f32->fmt = type_fmt_float32;
	type_f64->fmt = type_fmt_float64;
	type_rec->fmt = type_fmt_typename;

	cc->type_vid = type_vid;
	cc->type_bol = type_bol;
	cc->type_chr = type_chr;
	cc->type_i08 = type_i08;
	cc->type_i16 = type_i16;
	cc->type_i32 = type_i32;
	cc->type_i64 = type_i64;
	cc->type_u08 = type_u08;
	cc->type_u16 = type_u16;
	cc->type_u32 = type_u32;
	cc->type_u64 = type_u64;
	cc->type_f32 = type_f32;
	cc->type_f64 = type_f64;
	cc->type_ptr = type_ptr;
	cc->type_fun = type_fun;
	cc->type_var = type_var;
	cc->type_obj = type_obj;
	if (vm_ref_size == 8) {
		cc->type_int = type_i64;
		cc->type_idx = type_u64;
	}
	else {
		cc->type_int = type_i32;
		cc->type_idx = type_u32;
	}

	if (type_ptr != NULL) {
		cc->null_ref = install(cc, "null", ATTR_stat | ATTR_cnst | KIND_def, 0, type_ptr, intNode(cc, 0));
		cc->null_ref->init->type = type_ptr;
	}
	cc->true_ref = install(cc, "true", ATTR_stat | ATTR_cnst | KIND_def, 0, type_bol, intNode(cc, 1));    // 0 == 0
	cc->true_ref->init->type = type_bol;

	cc->false_ref = install(cc, "false", ATTR_stat | ATTR_cnst | KIND_def, 0, type_bol, intNode(cc, 0));  // 0 != 0
	cc->false_ref->init->type = type_bol;

	enter(cc, NULL);
	cc->length_ref = install(cc, "length", ATTR_cnst | KIND_var, cc->type_idx->size, cc->type_idx, NULL);
	leave(cc, KIND_typ, 0, 0, NULL);
	cc->length_ref->offs = offsetOf(vmValue, length);
	cc->length_ref->next = NULL;

	// aliases
	install(cc, "int", ATTR_stat | ATTR_cnst | KIND_def, 0, type_rec, lnkNode(cc, cc->type_int));
	install(cc, "byte", ATTR_stat | ATTR_cnst | KIND_def, 0, type_rec, lnkNode(cc, type_u08));
	install(cc, "float", ATTR_stat | ATTR_cnst | KIND_def, 0, type_rec, lnkNode(cc, type_f32));
	install(cc, "double", ATTR_stat | ATTR_cnst | KIND_def, 0, type_rec, lnkNode(cc, type_f64));

	cc->type_str = install(cc, ".cstr", ATTR_stat | ATTR_cnst | KIND_typ | CAST_arr, vm_ref_size, type_chr, NULL);
	if (cc->type_str != NULL) {
		// arrays without length property are c-like pointers
		cc->type_str->fmt = type_fmt_string;
	}
	if (cc->type_vid != NULL) {
		cc->void_tag = lnkNode(cc, cc->type_vid);
		if (cc->void_tag == NULL) {
			error(cc->rt, NULL, 0, ERR_INTERNAL_ERROR);
		}
	}
}

/// Install emit intrinsic.
static void install_emit(ccContext cc, ccInstall mode) {
	rtContext rt = cc->rt;

	cc->emit_opc = install(cc, "emit", ATTR_stat | ATTR_cnst | KIND_typ | CAST_vid, 0, cc->type_fun, NULL);

	if ((mode & installEopc) == installEopc && ccExtend(cc, cc->emit_opc)) {
		symn opc, type_p4x;
		symn type_vid = cc->type_vid;
		symn type_bol = cc->type_bol;
		symn type_u32 = cc->type_u32;
		symn type_i32 = cc->type_i32;
		symn type_i64 = cc->type_i64;
		symn type_u64 = cc->type_u64;
		symn type_f32 = cc->type_f32;
		symn type_f64 = cc->type_f64;

		ccDefOpCode(cc, "nop", type_vid, opc_nop, 0);
		ccDefOpCode(cc, "not", type_bol, opc_not, 0);
		ccDefOpCode(cc, "set", type_vid, opc_set1, 1);
		ccDefOpCode(cc, "ret", type_vid, opc_jmpi, 0);
		ccDefOpCode(cc, "call", type_vid, opc_call, 0);

		type_p4x = install(cc, "p4x", ATTR_stat | ATTR_cnst | KIND_typ | CAST_val, 16, cc->type_rec, NULL);

		if ((opc = ccBegin(cc, "dup")) != NULL) {
			ccDefOpCode(cc, "x1", type_i32, opc_dup1, 0);
			ccDefOpCode(cc, "x2", type_i64, opc_dup2, 0);
			ccDefOpCode(cc, "x4", type_p4x, opc_dup4, 0);
			ccEnd(cc, opc);
		}
		if ((opc = ccBegin(cc, "load")) != NULL) {
			// load zero
			ccDefOpCode(cc, "z32", type_i32, opc_lzx1, 0);
			ccDefOpCode(cc, "z64", type_i64, opc_lzx2, 0);
			ccDefOpCode(cc, "z128", type_p4x, opc_lzx4, 0);

			// load memory indirect
			ccDefOpCode(cc, "i8", type_i32, opc_ldi1, 0);
			ccDefOpCode(cc, "i16", type_i32, opc_ldi2, 0);
			ccDefOpCode(cc, "i32", type_i32, opc_ldi4, 0);
			ccDefOpCode(cc, "i64", type_i64, opc_ldi8, 0);
			ccDefOpCode(cc, "i128", type_p4x, opc_ldiq, 0);
			ccEnd(cc, opc);
		}
		if ((opc = ccBegin(cc, "store")) != NULL) {
			// store memory indirect
			ccDefOpCode(cc, "i8", type_vid, opc_sti1, 0);
			ccDefOpCode(cc, "i16", type_vid, opc_sti2, 0);
			ccDefOpCode(cc, "i32", type_vid, opc_sti4, 0);
			ccDefOpCode(cc, "i64", type_vid, opc_sti8, 0);
			ccDefOpCode(cc, "i128", type_vid, opc_stiq, 0);
			ccEnd(cc, opc);
		}
		if ((opc = ccBegin(cc, "cmt")) != NULL) {
			ccDefOpCode(cc, "u32", type_u32, b32_cmt, 0);
			ccDefOpCode(cc, "u64", type_u64, b64_cmt, 0);
			ccEnd(cc, opc);
		}
		if ((opc = ccBegin(cc, "and")) != NULL) {
			ccDefOpCode(cc, "u32", type_u32, b32_and, 0);
			ccDefOpCode(cc, "u64", type_u64, b64_and, 0);
			ccEnd(cc, opc);
		}
		if ((opc = ccBegin(cc, "or")) != NULL) {
			ccDefOpCode(cc, "u32", type_u32, b32_ior, 0);
			ccDefOpCode(cc, "u64", type_u64, b64_ior, 0);
			ccEnd(cc, opc);
		}
		if ((opc = ccBegin(cc, "xor")) != NULL) {
			ccDefOpCode(cc, "u32", type_u32, b32_xor, 0);
			ccDefOpCode(cc, "u64", type_u64, b64_xor, 0);
			ccEnd(cc, opc);
		}
		if ((opc = ccBegin(cc, "shl")) != NULL) {
			ccDefOpCode(cc, "u32", type_u32, b32_shl, 0);
			ccDefOpCode(cc, "u64", type_u64, b64_shl, 0);
			ccEnd(cc, opc);
		}

		if ((opc = ccBegin(cc, "shr")) != NULL) {
			ccDefOpCode(cc, "i32", type_i32, b32_sar, 0);
			ccDefOpCode(cc, "i64", type_i64, b64_sar, 0);
			ccDefOpCode(cc, "u32", type_u32, b32_shr, 0);
			ccDefOpCode(cc, "u64", type_u64, b64_shr, 0);
			ccEnd(cc, opc);
		}
		if ((opc = ccBegin(cc, "neg")) != NULL) {
			ccDefOpCode(cc, "i32", type_i32, i32_neg, 0);
			ccDefOpCode(cc, "i64", type_i64, i64_neg, 0);
			ccDefOpCode(cc, "f32", type_f32, f32_neg, 0);
			ccDefOpCode(cc, "f64", type_f64, f64_neg, 0);
			ccDefOpCode(cc, "p4f", type_p4x, v4f_neg, 0);
			ccDefOpCode(cc, "p2d", type_p4x, v2d_neg, 0);
			ccEnd(cc, opc);
		}
		if ((opc = ccBegin(cc, "add")) != NULL) {
			ccDefOpCode(cc, "i32", type_i32, i32_add, 0);
			ccDefOpCode(cc, "i64", type_i64, i64_add, 0);
			ccDefOpCode(cc, "f32", type_f32, f32_add, 0);
			ccDefOpCode(cc, "f64", type_f64, f64_add, 0);
			ccDefOpCode(cc, "p4f", type_p4x, v4f_add, 0);
			ccDefOpCode(cc, "p2d", type_p4x, v2d_add, 0);
			ccEnd(cc, opc);
		}
		if ((opc = ccBegin(cc, "sub")) != NULL) {
			ccDefOpCode(cc, "i32", type_i32, i32_sub, 0);
			ccDefOpCode(cc, "i64", type_i64, i64_sub, 0);
			ccDefOpCode(cc, "f32", type_f32, f32_sub, 0);
			ccDefOpCode(cc, "f64", type_f64, f64_sub, 0);
			ccDefOpCode(cc, "p4f", type_p4x, v4f_sub, 0);
			ccDefOpCode(cc, "p2d", type_p4x, v2d_sub, 0);
			ccEnd(cc, opc);
		}
		if ((opc = ccBegin(cc, "mul")) != NULL) {
			ccDefOpCode(cc, "i32", type_i32, i32_mul, 0);
			ccDefOpCode(cc, "i64", type_i64, i64_mul, 0);
			ccDefOpCode(cc, "u32", type_u32, u32_mul, 0);
			ccDefOpCode(cc, "u64", type_u32, u64_mul, 0);
			ccDefOpCode(cc, "f32", type_f32, f32_mul, 0);
			ccDefOpCode(cc, "f64", type_f64, f64_mul, 0);
			ccDefOpCode(cc, "p4f", type_p4x, v4f_mul, 0);
			ccDefOpCode(cc, "p2d", type_p4x, v2d_mul, 0);
			ccEnd(cc, opc);
		}
		if ((opc = ccBegin(cc, "div")) != NULL) {
			ccDefOpCode(cc, "i32", type_i32, i32_div, 0);
			ccDefOpCode(cc, "i64", type_i64, i64_div, 0);
			ccDefOpCode(cc, "u32", type_u32, u32_div, 0);
			ccDefOpCode(cc, "u64", type_u32, u64_div, 0);
			ccDefOpCode(cc, "f32", type_f32, f32_div, 0);
			ccDefOpCode(cc, "f64", type_f64, f64_div, 0);
			ccDefOpCode(cc, "p4f", type_p4x, v4f_div, 0);
			ccDefOpCode(cc, "p2d", type_p4x, v2d_div, 0);
			ccEnd(cc, opc);
		}
		if ((opc = ccBegin(cc, "mod")) != NULL) {
			ccDefOpCode(cc, "i32", type_i32, i32_mod, 0);
			ccDefOpCode(cc, "i64", type_i64, i64_mod, 0);
			ccDefOpCode(cc, "u32", type_u32, u32_mod, 0);
			ccDefOpCode(cc, "u64", type_u32, u64_mod, 0);
			ccDefOpCode(cc, "f32", type_f32, f32_mod, 0);
			ccDefOpCode(cc, "f64", type_f64, f64_mod, 0);
			ccEnd(cc, opc);
		}
		if ((opc = ccBegin(cc, "ceq")) != NULL) {
			ccDefOpCode(cc, "i32", type_bol, i32_ceq, 0);
			ccDefOpCode(cc, "i64", type_bol, i64_ceq, 0);
			ccDefOpCode(cc, "f32", type_bol, f32_ceq, 0);
			ccDefOpCode(cc, "f64", type_bol, f64_ceq, 0);
			ccDefOpCode(cc, "p4f", type_bol, v4f_ceq, 0);
			ccDefOpCode(cc, "p2d", type_bol, v2d_ceq, 0);
			ccEnd(cc, opc);
		}
		if ((opc = ccBegin(cc, "clt")) != NULL) {
			ccDefOpCode(cc, "i32", type_bol, i32_clt, 0);
			ccDefOpCode(cc, "i64", type_bol, i64_clt, 0);
			ccDefOpCode(cc, "u32", type_bol, u32_clt, 0);
			ccDefOpCode(cc, "u64", type_bol, u64_clt, 0);
			ccDefOpCode(cc, "f32", type_bol, f32_clt, 0);
			ccDefOpCode(cc, "f64", type_bol, f64_clt, 0);
			ccEnd(cc, opc);
		}
		if ((opc = ccBegin(cc, "cgt")) != NULL) {
			ccDefOpCode(cc, "i32", type_bol, i32_cgt, 0);
			ccDefOpCode(cc, "i64", type_bol, i64_cgt, 0);
			ccDefOpCode(cc, "u32", type_bol, u32_cgt, 0);
			ccDefOpCode(cc, "u64", type_bol, u64_cgt, 0);
			ccDefOpCode(cc, "f32", type_bol, f32_cgt, 0);
			ccDefOpCode(cc, "f64", type_bol, f64_cgt, 0);
			ccEnd(cc, opc);
		}
		if ((opc = ccBegin(cc, "min")) != NULL) {
			ccDefOpCode(cc, "p4f", type_p4x, v4f_min, 0);
			ccDefOpCode(cc, "p2d", type_p4x, v2d_min, 0);
			ccEnd(cc, opc);
		}
		if ((opc = ccBegin(cc, "max")) != NULL) {
			ccDefOpCode(cc, "p4f", type_p4x, v4f_max, 0);
			ccDefOpCode(cc, "p2d", type_p4x, v2d_max, 0);
			ccEnd(cc, opc);
		}
		if ((opc = ccExtend(cc, type_p4x)) != NULL) {
			ccDefOpCode(cc, "dp3", type_f32, v4f_dp3, 0);
			ccDefOpCode(cc, "dp4", type_f32, v4f_dp4, 0);
			ccDefOpCode(cc, "dph", type_f32, v4f_dph, 0);
			if ((mode & installEswz) == installEswz) {
				for (size_t i = 0; i < 256; i += 1) {
					char *name;
					if (rt->_end - rt->_beg < 5) {
						fatal(ERR_MEMORY_OVERRUN);
						return;
					}
					rt->_beg[0] = (unsigned char) "xyzw"[(i >> 0) & 3];
					rt->_beg[1] = (unsigned char) "xyzw"[(i >> 2) & 3];
					rt->_beg[2] = (unsigned char) "xyzw"[(i >> 4) & 3];
					rt->_beg[3] = (unsigned char) "xyzw"[(i >> 6) & 3];
					rt->_beg[4] = 0;
					name = ccUniqueStr(cc, (char *) rt->_beg, -1, -1);
					ccDefOpCode(cc, name, type_p4x, p4x_swz, i);
				}
			}
			ccEnd(cc, opc);
		}
		ccEnd(cc, cc->emit_opc);
	}
}

/**
 * Install base functions.
 * 
 * @param rt Runtime context.
 * @param onHalt the function to be invoked when execution stops.
 * @returns 0 on success
 */
static int install_base(rtContext rt, vmError onHalt(nfcContext)) {
	int error = 0;
	ccContext cc = rt->cc;
	symn field;

	// !!! halt must be the first native call.
	error = error || !ccAddCall(cc, onHalt ? onHalt : haltDummy, "void halt()");

	// 4 reflection
	if (cc->type_rec != NULL && cc->type_var != NULL) {
		enter(cc, cc->type_rec);

		if ((field = install(cc, "size", ATTR_cnst | KIND_var, vm_stk_align, cc->type_i32, NULL))) {
			field->offs = offsetOf(struct symNode, size);
		}
		else {
			error = 1;
		}

		if ((field = install(cc, "offset", ATTR_cnst | KIND_var, vm_stk_align, cc->type_i32, NULL))) {
			field->offs = offsetOf(struct symNode, offs);
			field->fmt = "@%06x";
		}
		else {
			error = 1;
		}

		error = error || !(field = ccAddCall(cc, typenameGetField, type_get_base));
		error = error || !(field = ccAddCall(cc, typenameGetField, type_get_file));
		if (field != NULL) {// hack: change return type from pointer to string
			field->params->type = cc->type_str;
		}
		error = error || !(field = ccAddCall(cc, typenameGetField, type_get_line));
		error = error || !(field = ccAddCall(cc, typenameGetField, type_get_name));
		if (field != NULL) {// hack: change return type from pointer to string
			field->params->type = cc->type_str;
		}

		/* TODO: more 4 reflection
		error = error || !ccAddCall(rt, typenameReflect, "variant lookup(variant &obj, int options, string name, variant args...)");
		error = error || !ccAddCall(rt, typenameReflect, "variant invoke(variant &obj, int options, string name, variant args...)");
		// setValue and getValue can be done with lookup and invoke
		error = error || !ccAddCall(rt, typenameReflect, "variant setValue(typename field, variant value)");
		error = error || !ccAddCall(rt, typenameReflect, "variant getValue(typename field)");

		error = error || !ccAddCall(rt, typenameReflect, "bool canAssign(typename type, variant value, bool strict)");
		error = error || !ccAddCall(rt, typenameReflect, "bool instanceOf(typename type, variant obj)");
		//~ */

		dieif(cc->type_rec->fields, ERR_INTERNAL_ERROR);
		cc->type_rec->fields = leave(cc, KIND_def, 0, 0, NULL);
	}
	return error;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ Core

rtContext rtInit(void *mem, size_t size) {
	rtContext rt = padPointer(mem, vm_mem_align);

	if (rt != mem) {
		size_t diff = (char*)rt - (char*) mem;
		size -= diff;
	}

	if (rt == NULL && size > sizeof(struct rtContextRec)) {
		rt = malloc(size);
	}

	if (rt != NULL) {
		memset(rt, 0, sizeof(struct rtContextRec));

		*(void**)&rt->api.ccExtend = ccExtend;
		*(void**)&rt->api.ccBegin = ccBegin;
		*(void**)&rt->api.ccEnd = ccEnd;

		*(void**)&rt->api.ccDefInt = ccDefInt;
		*(void**)&rt->api.ccDefFlt = ccDefFlt;
		*(void**)&rt->api.ccDefStr = ccDefStr;
		*(void**)&rt->api.ccDefVar = ccDefVar;

		*(void**)&rt->api.ccAddType = ccAddType;
		*(void**)&rt->api.ccAddCall = ccAddCall;
		*(void**)&rt->api.ccAddUnit = ccAddUnit;
		*(void**)&rt->api.ccLookup = ccLookup;

		*(void**)&rt->api.raise = raiseApi;
		*(void**)&rt->api.invoke = invoke;
		*(void**)&rt->api.rtAlloc = rtAllocApi;
		*(void**)&rt->api.rtLookup = rtLookupApi;

		*(void**)&rt->api.nfcFirstArg = nfcFirstArg;
		*(void**)&rt->api.nfcNextArg = nfcNextArg;
		*(void**)&rt->api.nfcReadArg = nfcReadArg;

		// default values
		rt->logLevel = 5;
		rt->traceLevel = 15;
		rt->foldCasts = 1;
		rt->foldConst = 1;
		rt->foldInstr = 1;
		rt->fastMemory = 1;
		rt->fastAssign = 1;
		rt->genGlobals = 1;
		rt->freeMem = (unsigned) (mem == NULL);

		*(size_t*)&rt->_size = size - sizeof(struct rtContextRec);
		rt->_end = rt->_mem + rt->_size;
		rt->_beg = rt->_mem + 1;

		logFILE(rt, stdout);
	}
	return rt;
}

int rtClose(rtContext rt) {
	// close log file
	logFile(rt, NULL, 0);

	int errors = rt->errors;
	// release debugger memory
	if (rt->dbg != NULL) {
		freeBuff(&rt->dbg->functions);
		freeBuff(&rt->dbg->statements);
	}
	// release memory
	if (rt->freeMem) {
		free(rt);
	}
	return errors;
}

size_t vmInit(rtContext rt, int debug, vmError onHalt(nfcContext)) {
	ccContext cc = rt->cc;

	// debug info
	if (debug != 0) {
		rt->dbg = (dbgContext)(rt->_beg = padPointer(rt->_beg, vm_mem_align));
		rt->_beg += sizeof(struct dbgContextRec);

		dieif(rt->_beg >= rt->_end, ERR_MEMORY_OVERRUN);
		memset(rt->dbg, 0, sizeof(struct dbgContextRec));

		rt->dbg->rt = rt;
		rt->dbg->abort = (dbgn)-1;
		rt->dbg->tryExec = cc->libc_try;
		initBuff(&rt->dbg->functions, 128, sizeof(struct dbgNode));
		initBuff(&rt->dbg->statements, 128, sizeof(struct dbgNode));
	}

	// initialize native calls
	if (cc != NULL && cc->native != NULL) {
		list lst = cc->native;
		libc last = (libc) lst->data;
		libc *calls = (libc*)(rt->_beg = padPointer(rt->_beg, vm_mem_align));

		rt->_beg += (last->offs + 1) * sizeof(libc);
		if(rt->_beg >= rt->_end) {
			fatal(ERR_MEMORY_OVERRUN);
			return 0;
		}

		rt->vm.nfc = calls;
		for (; lst != NULL; lst = lst->next) {
			libc nfc = (libc) lst->data;
			calls[nfc->offs] = nfc;

			// relocate native call offsets to be debuggable and traceable.
			nfc->sym->offs = vmOffset(rt, nfc);
			nfc->sym->size = 0;
			addDbgFunction(rt, nfc->sym);
		}
	}
	else {
		libc stop = (libc)(rt->_beg = padPointer(rt->_beg, vm_mem_align));
		rt->_beg += sizeof(struct libc);
		if(rt->_beg >= rt->_end) {
			fatal(ERR_MEMORY_OVERRUN);
			return 0;
		}

		libc *calls = (libc*)(rt->_beg = padPointer(rt->_beg, vm_mem_align));
		rt->_beg += sizeof(libc);
		if(rt->_beg >= rt->_end) {
			fatal(ERR_MEMORY_OVERRUN);
			return 0;
		}

		memset(stop, 0, sizeof(struct libc));
		stop->call = haltDummy;
		calls[0] = stop;

		rt->vm.nfc = calls;

		// add the main function
		if (rt->main == NULL) {
			symn sym = (symn) (rt->_beg = padPointer(rt->_beg, vm_mem_align));
			rt->_beg += sizeof(struct symNode);
			if (rt->_beg >= rt->_end) {
				fatal(ERR_MEMORY_OVERRUN);
				return 0;
			}
			memset(sym, 0, sizeof(struct symNode));
			sym->kind = ATTR_stat | ATTR_cnst | CAST_ref | KIND_fun;
			sym->offs = vmOffset(rt, rt->_beg);
			sym->name = ".main";
			rt->main = sym;
		}
	}

	// use custom halt function
	if (onHalt != NULL) {
		libc *calls = rt->vm.nfc;
		calls[0]->call = onHalt;
	}

	return rt->_beg - rt->_mem;
}

void *rtAlloc(rtContext rt, void *ptr, size_t size, void dbg(dbgContext, void *, size_t, char *)) {
	/* memory manager
	 * using one linked list containing both used and unused memory chunks.
	 * The idea is when allocating a block of memory we always must to traverse the list of chunks.
	 * when freeing a block not. Because of this if a block is used prev points to the previous block.
	 * a block is free when prev is null.
	 .
	 :
	 +------+------+
	 | next        | next chunk (allocated or free)
	 | prev        | previous chunk (null for free chunks)
	 +------+------+
	 |             |
	 .             .
	 : ...         :
	 +-------------+
	 | next        |
	 | prev        |
	 +------+------+
	 |             |
	 .             .
	 : ...         :
	 +------+------+
	 :
	*/

	typedef struct memChunk *memChunk;
	struct memChunk {
		memChunk prev;      // null for free chunks
		memChunk next;      // next chunk
		char data[];      // here begins the user data
	};

	const ssize_t minAllocationSize = sizeof(struct memChunk);
	size_t allocSize = padOffset(size + minAllocationSize, minAllocationSize);
	memChunk chunk = (memChunk)((char*)ptr - offsetOf(struct memChunk, data));

	// memory manager is not initialized, initialize it first
	if (rt->vm.heap == NULL) {
		memChunk heap = padPointer(rt->_beg, minAllocationSize);
		memChunk last = padPointer(rt->_end - 2 * minAllocationSize, minAllocationSize);

		heap->next = last;
		heap->prev = NULL;

		last->next = NULL;
		last->prev = NULL;

		rt->vm.heap = heap;
	}

	// chop or free.
	if (ptr != NULL) {
		size_t chunkSize = chunk->next ? ((char*)chunk->next - (char*)chunk) : 0;

		if ((unsigned char*)ptr < rt->_beg || (unsigned char*)ptr > rt->_end) {
			dieif((unsigned char*)ptr < rt->_beg, ERR_INVALID_OFFSET, vmOffset(rt, ptr));
			dieif((unsigned char*)ptr > rt->_end, ERR_INVALID_OFFSET, vmOffset(rt, ptr));
			return NULL;
		}

		if (dbg != NULL && rt->dbg != NULL) { // extra check if ptr is in used list.
			memChunk find = rt->vm.heap;
			memChunk prev = find;
			while (find && find != chunk) {
				prev = find;
				find = find->next;
			}
			if (find != chunk || chunk->prev != prev) {
				dbg(rt->dbg, ptr, -1, "unallocated pointer to resize");
				return NULL;
			}
		}

		if (size == 0) {							// free
			memChunk next = chunk->next;
			memChunk prev = chunk->prev;

			// merge with next block if free
			if (next && next->prev == NULL) {
				next = next->next;
				chunk->next = next;
				if (next && next->prev != NULL) {
					next->prev = chunk;
				}
			}

			// merge with previous block if free
			if (prev && prev->prev == NULL) {
				chunk = prev;
				chunk->next = next;
				if (next && next->prev != NULL) {
					next->prev = chunk;
				}
			}

			// mark chunk as free.
			chunk->prev = NULL;
			chunk = NULL;
		}
		else if (allocSize < chunkSize) {			// chop
			memChunk next = chunk->next;
			memChunk free = (memChunk)((char*)chunk + allocSize);

			// do not make a small free unaligned block (chop 161 to 160 bytes)
			if (((char*)next - (char*)free) > minAllocationSize) {
				chunk->next = free;
				free->next = next;
				free->prev = NULL;

				// merge with next block if free
				if (next->prev == NULL) {
					next = next->next;
					free->next = next;
					if (next && next->prev != NULL) {
						next->prev = free;
					}
				}
				else {
					next->prev = free;
				}
			}
		}
		else { // grow: reallocation to a bigger chunk is not supported.
			error(rt, NULL, 0, ERR_INTERNAL_ERROR);
			chunk = NULL;
		}
	}
		// allocate.
	else if (size > 0) {
		memChunk prev = chunk = rt->vm.heap;

		while (chunk != NULL) {
			memChunk next = chunk->next;

			// check if block is free.
			if (chunk->prev == NULL && next != NULL) {
				size_t chunkSize = (char*)next - (char*)chunk - allocSize;
				if (allocSize < chunkSize) {
					ssize_t diff = chunkSize - allocSize;
					if (diff > minAllocationSize) {
						memChunk free = (memChunk)((char*)chunk + allocSize);
						chunk->next = free;
						free->prev = NULL;
						free->next = next;
					}
					chunk->prev = prev;
					break;
				}
			}
			prev = chunk;
			chunk = next;
		}
	}
	else {
		chunk = NULL;
	}

	if (rt->dbg != NULL) {
		memChunk mem;
		size_t free = 0, used = 0;
		for (mem = rt->vm.heap; mem; mem = mem->next) {
			if (mem->next) {
				size_t chunkSize = (char*)mem->next - (char*)mem - sizeof(struct memChunk);
				if (mem->prev != NULL) {
					used += chunkSize;
					if (dbg != NULL) {
						dbg(rt->dbg, mem->data, chunkSize, "used");
					}
				}
				else {
					free += chunkSize;
					if (dbg != NULL) {
						dbg(rt->dbg, mem->data, chunkSize, "free");
					}
				}
			}
		}
		rt->dbg->freeMem = free;
		rt->dbg->usedMem = used;
	}

	return chunk ? chunk->data : NULL;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ Compiler

ccContext ccInit(rtContext rt, ccInstall mode, vmError onHalt(nfcContext)) {
	ccContext cc;

	dieif(rt->cc != NULL, ERR_INTERNAL_ERROR);
	dieif(rt->_beg != rt->_mem + 1, ERR_INTERNAL_ERROR);
	dieif(rt->_end != rt->_mem + rt->_size, ERR_INTERNAL_ERROR);

	cc = (ccContext)(rt->_end - sizeof(struct ccContextRec));
	rt->_end -= sizeof(struct ccContextRec);

	if (rt->_end < rt->_beg) {
		fatal(ERR_MEMORY_OVERRUN);
		return NULL;
	}

	memset(rt->_end, 0, sizeof(struct ccContextRec));

	cc->rt = rt;
	rt->cc = cc;

	cc->owner = NULL;
	cc->scope = NULL;
	cc->global = NULL;

	cc->chrNext = -1;

	cc->fin._ptr = 0;
	cc->fin._cnt = 0;
	cc->fin._fin = -1;

	install_type(cc, mode);
	install_emit(cc, mode);
	install_base(rt, onHalt);

	cc->root = newNode(cc, STMT_beg);
	cc->root->type = cc->type_vid;

	return cc;
}

symn ccBegin(ccContext cc, const char *name) {
	if (cc == NULL) {
		trace(ERR_INTERNAL_ERROR);
		return NULL;
	}
	symn result = NULL;
	if (name != NULL) {
		result = install(cc, name, ATTR_stat | ATTR_cnst | KIND_typ | CAST_vid, 0, cc->type_vid, NULL);
		if (result == NULL) {
			return NULL;
		}
	}
	enter(cc, result);
	return result;
}

symn ccExtend(ccContext cc, symn sym) {
	if (cc == NULL) {
		trace(ERR_INTERNAL_ERROR);
		return NULL;
	}
	if (sym == NULL) {
		// can not extend nothing
		return NULL;
	}
	if (sym->fields != NULL) {
		trace(ERR_INTERNAL_ERROR);
		return NULL;
	}
	enter(cc, sym);
	return sym;
}

symn ccEnd(ccContext cc, symn sym) {
	if (cc == NULL) {
		trace(ERR_INTERNAL_ERROR);
		return NULL;
	}
	if (cc->owner != sym) {
		trace(ERR_INTERNAL_ERROR);
		return NULL;
	}
	symn fields = leave(cc, sym->kind & (ATTR_stat | ATTR_cnst | MASK_kind), 0, 0, NULL);
	if (sym != NULL) {
		sym->fields = fields;
	}
	return fields;
}

symn ccDefInt(ccContext cc, const char *name, int64_t value) {
	if (!cc || !name) {
		trace(ERR_INTERNAL_ERROR);
		return NULL;
	}
	name = ccUniqueStr(cc, name, -1, -1);
	return install(cc, name, ATTR_stat | ATTR_cnst | KIND_def | CAST_i64, 0, cc->type_i64, intNode(cc, value));
}

symn ccDefFlt(ccContext cc, const char *name, double value) {
	if (!cc || !name) {
		trace(ERR_INTERNAL_ERROR);
		return NULL;
	}
	name = ccUniqueStr(cc, name, -1, -1);
	return install(cc, name, ATTR_stat | ATTR_cnst | KIND_def | CAST_f64, 0, cc->type_f64, fltNode(cc, value));
}

symn ccDefStr(ccContext cc, const char *name, char *value) {
	if (!cc || !name) {
		trace(ERR_INTERNAL_ERROR);
		return NULL;
	}
	name = ccUniqueStr(cc, name, -1, -1);
	if (value != NULL) {
		value = ccUniqueStr(cc, value, -1, -1);
	}
	return install(cc, name, ATTR_stat | ATTR_cnst | KIND_def | CAST_ref, 0, cc->type_str, strNode(cc, value));
}

symn ccDefVar(ccContext cc, const char *name, symn type) {
	if (!cc || !name) {
		trace(ERR_INTERNAL_ERROR);
		return NULL;
	}
	name = ccUniqueStr(cc, name, -1, -1);
	return install(cc, name, KIND_var | refCast(type), 0, type, NULL);
}

symn ccAddType(ccContext cc, const char *name, unsigned size, int refType) {
	if (!cc || !name) {
		trace(ERR_INTERNAL_ERROR);
		return NULL;
	}
	return install(cc, name, ATTR_stat | ATTR_cnst | KIND_typ | (refType ? CAST_ref : CAST_val), size, cc->type_rec, NULL);
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ Lookup

symn ccLookup(rtContext rt, symn scope, char *name) {
	struct astNode ast;
	memset(&ast, 0, sizeof(struct astNode));
	ast.kind = TOKEN_var;
	ast.ref.name = name;
	ast.ref.hash = rehash(name, -1) % hashTableSize;
	if (scope == NULL) {
		if (rt->main != NULL) {
			// code was generated, globals are in the main fields
			scope = rt->main->fields;
		}
		else if (rt->cc != NULL) {
			// code was not executed, main not generated
			scope = rt->cc->deft[ast.ref.hash];
		}
	}
	return lookup(rt->cc, scope, &ast, NULL, 0, 1);
}

symn rtLookup(rtContext rt, size_t offs, ccKind filter) {
	dieif(offs > rt->_size, ERR_INVALID_OFFSET, offs);
	if (offs > rt->vm.px + px_size) {
		// local variable on stack ?
		return NULL;
	}
	symn sym = rt->main;
	if (offs >= sym->offs && offs < sym->offs + sym->size) {
		// is the main function ?
		return sym;
	}
	ccKind filterStat = filter & ATTR_stat;
	ccKind filterCnst = filter & ATTR_cnst;
	ccKind filterKind = filter & MASK_kind;
	ccKind filterCast = filter & MASK_cast;
	for (sym = sym->fields; sym; sym = sym->global) {
		if (filterStat && (sym->kind & ATTR_stat) != filterStat) {
			continue;
		}
		if (filterCnst && (sym->kind & ATTR_cnst) != filterCnst) {
			continue;
		}
		if (filterKind && (sym->kind & MASK_kind) != filterKind) {
			continue;
		}
		if (filterCast && (sym->kind & MASK_cast) != filterCast) {
			continue;
		}
		if (offs == sym->offs) {
			return sym;
		}
		if (offs > sym->offs && offs < sym->offs + sym->size) {
			return sym;
		}
	}
	return NULL;
}

char *ccUniqueStr(ccContext cc, const char *str, size_t len, unsigned hash) {
	rtContext rt = cc->rt;
	list node, next, prev = 0;

	if (len == (size_t)-1) {
		len = strlen(str) + 1;
	}
	else if (str[len - 1] != 0) {
		fatal(ERR_INTERNAL_ERROR);
		return NULL;
	}

	if (hash == (unsigned)-1) {
		hash = rehash(str, len) % hashTableSize;
	}
	else if (hash >= hashTableSize) {
		fatal(ERR_INTERNAL_ERROR);
		return NULL;
	}

	for (next = cc->strt[hash]; next; next = next->next) {
		int cmp = memcmp(next->data, str, len);
		if (cmp == 0) {
			return (char*)next->data;
		}
		if (cmp > 0) {
			break;
		}
		prev = next;
	}

	if (rt->_beg >= rt->_end - (sizeof(struct list) + len)) {
		fatal(ERR_MEMORY_OVERRUN);
		return NULL;
	}
	if (str != (char *)rt->_beg) {
		// copy data from constants ?
		memcpy(rt->_beg, str, len + 1);
		str = (char*)rt->_beg;
	}

	// allocate list node as temp data (end of memory)
	rt->_end -= sizeof(struct list);
	node = (list)rt->_end;

	rt->_beg += len;

	if (!prev) {
		cc->strt[hash] = node;
	}
	else {
		prev->next = node;
	}

	node->next = next;
	node->data = (unsigned char*)str;

	return (char*)str;
}


//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ Debugger

char* vmErrorMessage(vmError error) {
	switch (error) {
		case noError:
			return NULL;

		case illegalState:
			return "Invalid state";

		case illegalMemoryAccess:
			return "Invalid memory access";

		case illegalInstruction:
			return "Invalid instruction";

		case stackOverflow:
			return "Stack Overflow";

		case divisionByZero:
			return "Division by Zero";

		case nativeCallError:
			return "External call aborted execution";
	}

	return "Unknown error";
}

dbgn mapDbgFunction(rtContext rt, size_t position) {
	if (rt->dbg != NULL) {
		dbgn result = (dbgn)rt->dbg->functions.ptr;
		size_t n = rt->dbg->functions.cnt;
		for (size_t i = 0; i < n; ++i) {
			if (position == result->start) {
				return result;
			}
			if (position >= result->start) {
				if (position < result->end) {
					return result;
				}
			}
			result++;
		}
	}
	return NULL;
}
dbgn addDbgFunction(rtContext rt, symn fun) {
	dbgn result = NULL;
	if (rt->dbg != NULL && fun != NULL) {
		size_t i = 0;
		for ( ; i < rt->dbg->functions.cnt; ++i) {
			result = getBuff(&rt->dbg->functions, i);
			if (fun->offs <= result->start) {
				break;
			}
		}

		if (result == NULL || fun->offs != result->start) {
			result = insBuff(&rt->dbg->functions, i, NULL);
		}

		if (result != NULL) {
			memset(result, 0, rt->dbg->functions.esz);
			result->func = fun;
			result->file = fun->file;
			result->line = fun->line;
			result->start = fun->offs;
			result->end = fun->offs + fun->size;
		}
	}
	return result;
}

dbgn getDbgStatement(rtContext rt, char *file, int line) {
	if (rt->dbg != NULL) {
		dbgn result = (dbgn)rt->dbg->statements.ptr;
		for (size_t i = 0; i < rt->dbg->statements.cnt; ++i) {
			if (result->file && strcmp(file, result->file) == 0) {
				if (line == result->line) {
					return result;
				}
			}
			result++;
		}
	}
	return NULL;
}
dbgn mapDbgStatement(rtContext rt, size_t position) {
	if (rt->dbg != NULL) {
		// TODO: use binary search to speed up mapping
		dbgn result = (dbgn)rt->dbg->statements.ptr;
		size_t n = rt->dbg->statements.cnt;
		for (size_t i = 0; i < n; ++i) {
			if (position >= result->start) {
				if (position < result->end) {
					return result;
				}
			}
			result++;
		}
	}
	return NULL;
}
dbgn addDbgStatement(rtContext rt, size_t start, size_t end, astn tag) {
	dbgn result = NULL;
	if (tag != NULL) {
		// do not add block statement to debug.
		if (tag->kind == STMT_beg) {
			return NULL;
		}
		// do not add `static if` to debug.
		if (tag->kind == STMT_sif) {
			return NULL;
		}
	}
	if (rt->dbg != NULL && start < end) {
		size_t i = 0;
		for ( ; i < rt->dbg->statements.cnt; ++i) {
			result = getBuff(&rt->dbg->statements, i);
			if (start <= result->start) {
				break;
			}
		}

		if (result == NULL || start != result->start) {
			result = insBuff(&rt->dbg->statements, i, NULL);
		}

		if (result != NULL) {
			memset(result, 0, rt->dbg->statements.esz);
			if (tag != NULL) {
				result->stmt = tag;
				result->file = tag->file;
				result->line = tag->line;
			}
			result->start = start;
			result->end = end;
		}
	}
	return result;
}

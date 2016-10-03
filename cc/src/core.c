/*******************************************************************************
 *   File: core.c
 *   Date: 2011/06/23
 *   Desc: type system
 *******************************************************************************
the core:
	convert ast to bytecode
	initializations, memory management

	emit:
	emit is a low level function like construct. (intrinsic)
	emit's type is the type of the first argument's.
	emit can have as parameters values and opcodes.
		as the first argument of emit we can pass a type or the struct keyword.
			if the first argument is a type, static cast can be done
				ex emit(float32, int32(2))
			if the first argument is struct in a declaration,
			the resulting type will match the declared type.
			size of variable must match the size generated by emit.
				ex complex a = emit(struct, f64(1), f64(-1));
	emit is used for libcalls, constructors, ..., optimizations
*******************************************************************************/
#include <stddef.h>
#include "internal.h"

const char *const type_fmt_signed32 = "%d";
const char *const type_fmt_signed64 = "%D";
const char *const type_fmt_unsigned32 = "%u";
const char *const type_fmt_unsigned64 = "%u";
const char *const type_fmt_float32 = "%f";
const char *const type_fmt_float64 = "%F";
const char *const type_fmt_character = "'%c'";
const char *const type_fmt_string = "\"%s\"";
const char *const type_fmt_variant = "%T: ";
const char *const type_fmt_typename = "%T";

static const char *const type_get_file = "char file[*]";
static const char *const type_get_line = "int line";
static const char *const type_get_name = "char name[*]";
static const char *const type_get_base = "typename base(typename type)";

/// Allocate, resize or free memory; @see rtContext.api.rtAlloc
void *rtAlloc(rtContext rt, void *ptr, size_t size, void dbg(rtContext, void *, size_t, char *)) {
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

	typedef struct memChunk {
		struct memChunk *prev;		// null for free chunks
		struct memChunk *next;		// next chunk
		char data[0];				// here begins the user data
		//~ struct memChunk *free;		// TODO: next free chunk ordered by size
		//~ struct memChunk *free_skip;	// TODO: next free which is 2x bigger than this
	} *memChunk;

	const ptrdiff_t minAllocationSize = sizeof(struct memChunk);
	size_t allocSize = padded(size + minAllocationSize, minAllocationSize);
	memChunk chunk = (memChunk)((char*)ptr - offsetOf(memChunk, data));

	// memory manager is not initialized, initialize it first
	if (rt->vm.heap == NULL) {
		memChunk heap = paddptr(rt->_beg, minAllocationSize);
		memChunk last = paddptr(rt->_end - minAllocationSize, minAllocationSize);

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

		if (1) { // extra check if ptr is in used list.
			memChunk find = rt->vm.heap;
			memChunk prev = find;
			while (find && find != chunk) {
				prev = find;
				find = find->next;
			}
			if (find != chunk || chunk->prev != prev) {
				dieif(find != chunk, "unallocated reference(%06x)", vmOffset(rt, ptr));
				dieif(chunk->prev != prev, "unallocated reference(%06x)", vmOffset(rt, ptr));
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
					ptrdiff_t diff = chunkSize - allocSize;
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
	// Debug.
	else {
		chunk = NULL;
		if (dbg != NULL) {
			memChunk mem;
			for (mem = rt->vm.heap; mem; mem = mem->next) {
				if (mem->next) {
					size_t chunkSize = (char*)mem->next - (char*)mem - sizeof(struct memChunk);
					dbg(rt, mem->data, chunkSize, mem->prev != NULL ? "used" : "free");
				}
			}
		}
	}

	if (rt->dbg != NULL) {
		memChunk mem;
		size_t free = 0, used = 0;
		for (mem = rt->vm.heap; mem; mem = mem->next) {
			if (mem->next) {
				size_t chunkSize = (char*)mem->next - (char*)mem - sizeof(struct memChunk);
				if (mem->prev != NULL) {
					used += chunkSize;
				}
				else {
					free += chunkSize;
				}
			}
		}
		rt->dbg->freeMem = free;
		rt->dbg->usedMem = used;
	}


	return chunk ? chunk->data : NULL;
}

static void *rtAllocApi(rtContext rt, void *ptr, size_t size) {
	return rtAlloc(rt, ptr, size, NULL);
}

static symn rtFindSymApi(rtContext rt, void *offs) {
	size_t vmOffs = vmOffset(rt, offs);
	symn sym = rtFindSym(rt, vmOffs, 0);
	if (sym != NULL && sym->offs == vmOffs) {
		return sym;
	}
	return NULL;
}

/// Initialize runtime context; @see header
rtContext rtInit(void *mem, size_t size) {
	rtContext rt = paddptr(mem, rt_size);

	if (rt != mem) {
		size_t diff = (char*)rt - (char*) mem;
		size -= diff;
	}

	if (rt == NULL && size > sizeof(struct rtContextRec)) {
		rt = malloc(size);
	}

	if (rt != NULL) {
		memset(rt, 0, sizeof(struct rtContextRec));

		*(void**)&rt->api.ccBegin = ccBegin;
		*(void**)&rt->api.ccEnd = ccEnd;

		*(void**)&rt->api.ccDefInt = ccDefInt;
		*(void**)&rt->api.ccDefFlt = ccDefFlt;
		*(void**)&rt->api.ccDefStr = ccDefStr;

		*(void**)&rt->api.ccDefType = ccDefType;
		*(void**)&rt->api.ccDefCall = ccDefCall;
		*(void**)&rt->api.ccAddUnit = ccAddUnit;

		*(void**)&rt->api.invoke = invoke;
		*(void**)&rt->api.rtAlloc = rtAllocApi;
		*(void**)&rt->api.rtFindSym = rtFindSymApi;

		// default values
		rt->logLevel = 7;
		rt->foldConst = 1;
		rt->foldInstr = 1;
		rt->fastAssign = 1;
		rt->genGlobals = 1;
		rt->freeMem = mem == NULL;

		*(size_t*)&rt->_size = size - sizeof(struct rtContextRec);
		rt->_end = rt->_mem + rt->_size;
		rt->_beg = rt->_mem;

		logFILE(rt, stdout);
	}
	return rt;
}

void rtClose(rtContext rt) {
	// close log file
	logfile(rt, NULL, 0);

	// release debugger memory
	if (rt->dbg != NULL) {
		freeBuff(&rt->dbg->functions);
		freeBuff(&rt->dbg->statements);
	}
	// release memory
	if (rt->freeMem) {
		free(rt);
	}
}

/// private dummy on exit function.
static vmError haltDummy(nfcContext args) {
	(void)args;
	return noError;
}
static vmError typenameGetField(nfcContext args) {
	symn sym = argsym(args, 0);
	if (sym == NULL) {
		return executionAborted;
	}
	if (args->proto == type_get_file) {
		retref(args, vmOffset(args->rt, sym->file));
		return noError;
	}
	if (args->proto == type_get_line) {
		reti32(args, sym->line);
		return noError;
	}
	if (args->proto == type_get_name) {
		retref(args, vmOffset(args->rt, sym->name));
		return noError;
	}
	if (args->proto == type_get_base) {
		retref(args, vmOffset(args->rt, sym->type));
		return noError;
	}
	return executionAborted;
}

static inline symn ccDefMember(rtContext rt, vmError libc(nfcContext), const char * const proto) {
	symn result = ccDefCall(rt, libc, proto);
	if (result != NULL) {
		result->kind &= ~ATTR_stat;
		result->kind |= ATTR_memb;
		rt->cc->libc->in += 1;
	}
	return result;
}

// install type system
static void install_type(ccContext cc, int mode) {
	symn type_vid, type_bol, type_chr;
	symn type_i08, type_i16, type_i32, type_i64;
	symn type_u08, type_u16, type_u32, type_u64;
	symn type_f32, type_f64;
	symn type_ptr = NULL, type_var = NULL;
	symn type_rec, type_fun, type_obj = NULL;

	type_rec = install(cc, "typename", ATTR_stat | ATTR_cnst | KIND_typ | CAST_ref, 0, NULL, NULL);
	// update required variables to be able to install other types.
	cc->type_rec = type_rec->type = type_rec;  // TODO: !cycle: typename is instance of typename

	type_vid = install(cc, "void", ATTR_stat | ATTR_cnst | KIND_typ | CAST_vid, 0, type_rec, NULL);
	type_bol = install(cc, "bool", ATTR_stat | ATTR_cnst | KIND_typ | CAST_bit, 1, type_rec, NULL);
	type_chr = install(cc, "char", ATTR_stat | ATTR_cnst | KIND_typ | CAST_u32, 1, type_rec, NULL);
	type_i08 = install(cc, "int8", ATTR_stat | ATTR_cnst | KIND_typ | CAST_i32, 1, type_rec, NULL);
	type_i16 = install(cc, "int16", ATTR_stat | ATTR_cnst | KIND_typ | CAST_i32, 2, type_rec, NULL);
	type_i32 = install(cc, "int32", ATTR_stat | ATTR_cnst | KIND_typ | CAST_i32, 4, type_rec, NULL);
	type_i64 = install(cc, "int64", ATTR_stat | ATTR_cnst | KIND_typ | CAST_i64, 8, type_rec, NULL);
	type_u08 = install(cc, "uint8", ATTR_stat | ATTR_cnst | KIND_typ | CAST_u32, 1, type_rec, NULL);
	type_u16 = install(cc, "uint16", ATTR_stat | ATTR_cnst | KIND_typ | CAST_u32, 2, type_rec, NULL);
	type_u32 = install(cc, "uint32", ATTR_stat | ATTR_cnst | KIND_typ | CAST_u32, 4, type_rec, NULL);
	type_u64 = install(cc, "uint64", ATTR_stat | ATTR_cnst | KIND_typ | CAST_u64, 8, type_rec, NULL);
	type_f32 = install(cc, "float32", ATTR_stat | ATTR_cnst | KIND_typ | CAST_f32, 4, type_rec, NULL);
	type_f64 = install(cc, "float64", ATTR_stat | ATTR_cnst | KIND_typ | CAST_f64, 8, type_rec, NULL);

	if (mode & install_ptr) {
		type_ptr = install(cc, "pointer", ATTR_stat | ATTR_cnst | KIND_typ | CAST_ref, 1 * vm_size, type_rec, NULL);
		cc->null_ref = install(cc, "null", ATTR_stat | ATTR_cnst | KIND_var | CAST_val, vm_size, type_ptr, NULL);
	}
	if (mode & install_var) {
		type_var = install(cc, "variant", ATTR_stat | ATTR_cnst | KIND_typ | CAST_var, 2 * vm_size, type_rec, NULL);
		type_var->pfmt = type_fmt_variant;
	}
	if (mode & install_obj) {
		type_obj = install(cc,  "object", ATTR_stat | ATTR_cnst | KIND_typ | CAST_ref, 1 * vm_size, type_rec, NULL);
	}
	type_fun = install(cc, "function", ATTR_stat | ATTR_cnst | KIND_typ | CAST_ref, 2 * vm_size, type_rec, NULL);

	type_vid->pfmt = NULL;
	type_bol->pfmt = type_fmt_signed32;
	type_chr->pfmt = type_fmt_character;
	type_i08->pfmt = type_fmt_signed32;
	type_i16->pfmt = type_fmt_signed32;
	type_i32->pfmt = type_fmt_signed32;
	type_i64->pfmt = type_fmt_signed64;
	type_u08->pfmt = type_fmt_unsigned32;
	type_u16->pfmt = type_fmt_unsigned32;
	type_u32->pfmt = type_fmt_unsigned32;
	type_u64->pfmt = type_fmt_unsigned64;
	type_f32->pfmt = type_fmt_float32;
	type_f64->pfmt = type_fmt_float64;
	type_rec->pfmt = type_fmt_typename;

	cc->type_vid = type_vid;
	cc->type_bol = type_bol;
	cc->type_chr = type_chr;
	cc->type_i32 = type_i32;
	cc->type_i64 = type_i64;
	cc->type_u32 = type_u32;
	cc->type_u64 = type_u64;
	cc->type_f32 = type_f32;
	cc->type_f64 = type_f64;
	cc->type_ptr = type_ptr;
	cc->type_fun = type_fun;
	cc->type_var = type_var;
	cc->type_obj = type_obj;

	// aliases.
	install(cc, "int", ATTR_stat | ATTR_cnst | KIND_def, 0, type_i32, NULL);
	install(cc, "byte", ATTR_stat | ATTR_cnst | KIND_def, 0, type_i08, NULL);
	install(cc, "long", ATTR_stat | ATTR_cnst | KIND_def, 0, type_i64, NULL);
	install(cc, "float", ATTR_stat | ATTR_cnst | KIND_def, 0, type_f32, NULL);
	install(cc, "double", ATTR_stat | ATTR_cnst | KIND_def, 0, type_f64, NULL);

	install(cc, "true", ATTR_stat | ATTR_cnst | KIND_def, 0, type_bol, intNode(cc, 1));   // 0 == 0
	install(cc, "false", ATTR_stat | ATTR_cnst | KIND_def, 0, type_bol, intNode(cc, 0));  // 0 != 0

	cc->type_str = install(cc, ".cstr", ATTR_stat | ATTR_cnst | KIND_typ | CAST_ref, vm_size, type_chr, NULL);
	if (cc->type_str != NULL) {
		cc->type_str->init = intNode(cc, -1); // hack: strings are static sized arrays with a length of -1.
		cc->type_str->pfmt = type_fmt_string;
	}
}

// install emit operator
static void install_emit(ccContext cc, int mode) {
	rtContext rt = cc->rt;

	cc->emit_opc = install(cc, "emit", EMIT_opc, 0, NULL, NULL);

	if (cc->emit_opc && (mode & installEopc) == installEopc) {
		symn opc, type_p4x;
		symn type_vid = cc->type_vid;
		symn type_bol = cc->type_bol;
		symn type_u32 = cc->type_u32;
		symn type_i32 = cc->type_i32;
		symn type_i64 = cc->type_i64;
		symn type_u64 = cc->type_u64;
		symn type_f32 = cc->type_f32;
		symn type_f64 = cc->type_f64;

		enter(cc);
		install(cc, "nop", EMIT_opc, opc_nop, type_vid, NULL);
		install(cc, "not", EMIT_opc, opc_not, type_bol, NULL);
		install(cc, "set", EMIT_opc, opc_set1, type_vid, intNode(cc, 1));
		install(cc, "join", EMIT_opc, opc_sync, type_vid, intNode(cc, 1));
		install(cc, "call", EMIT_opc, opc_call, type_vid, NULL);

		type_p4x = install(cc, "p4x", ATTR_stat | ATTR_cnst | KIND_typ | CAST_val, 16, cc->type_rec, NULL);

		if ((opc = install(cc, "dupp", ATTR_stat | ATTR_cnst | KIND_def, 0, NULL, NULL)) != NULL) {
			enter(cc);
			install(cc, "x1", EMIT_opc, opc_dup1, type_i32, intNode(cc, 0));
			install(cc, "x2", EMIT_opc, opc_dup2, type_i64, intNode(cc, 0));
			install(cc, "x4", EMIT_opc, opc_dup4, type_p4x, intNode(cc, 0));
			opc->fields = leave(cc, opc, ATTR_stat | KIND_typ, 0, NULL);
		}
		if ((opc = install(cc, "load", ATTR_stat | ATTR_cnst | KIND_def, 0, NULL, NULL)) != NULL) {
			enter(cc);
			// load zero
			install(cc, "z32", EMIT_opc, opc_lzx1, type_i32, NULL);
			install(cc, "z64", EMIT_opc, opc_lzx2, type_i64, NULL);
			install(cc, "z128", EMIT_opc, opc_lzx4, type_p4x, NULL);

			// load memory
			install(cc, "i8",   EMIT_opc, opc_ldi1, type_i32, NULL);
			install(cc, "i16",  EMIT_opc, opc_ldi2, type_i32, NULL);
			install(cc, "i32",  EMIT_opc, opc_ldi4, type_i32, NULL);
			install(cc, "i64",  EMIT_opc, opc_ldi8, type_i64, NULL);
			install(cc, "i128", EMIT_opc, opc_ldiq, type_p4x, NULL);
			opc->fields = leave(cc, opc, ATTR_stat | KIND_typ, 0, NULL);
		}
		if ((opc = install(cc, "store", ATTR_stat | ATTR_cnst | KIND_def, 0, NULL, NULL)) != NULL) {
			enter(cc);
			install(cc, "i8",   EMIT_opc, opc_sti1, type_vid, NULL);
			install(cc, "i16",  EMIT_opc, opc_sti2, type_vid, NULL);
			install(cc, "i32",  EMIT_opc, opc_sti4, type_vid, NULL);
			install(cc, "i64",  EMIT_opc, opc_sti8, type_vid, NULL);
			install(cc, "i128", EMIT_opc, opc_stiq, type_vid, NULL);
			opc->fields = leave(cc, opc, ATTR_stat | KIND_typ, 0, NULL);
		}
		if ((opc = install(cc, "cmt", ATTR_stat | ATTR_cnst | KIND_def, 0, NULL, NULL)) != NULL) {   // complement
			enter(cc);
			install(cc, "u32", EMIT_opc, b32_cmt, type_u32, NULL);
			install(cc, "u64", EMIT_opc, b64_cmt, type_u64, NULL);
			opc->fields = leave(cc, opc, ATTR_stat | KIND_typ, 0, NULL);
		}
		if ((opc = install(cc, "and", ATTR_stat | ATTR_cnst | KIND_def, 0, NULL, NULL)) != NULL) {
			enter(cc);
			install(cc, "u32", EMIT_opc, b32_and, type_u32, NULL);
			install(cc, "u64", EMIT_opc, b64_and, type_u64, NULL);
			opc->fields = leave(cc, opc, ATTR_stat | KIND_typ, 0, NULL);
		}
		if ((opc = install(cc, "or", ATTR_stat | ATTR_cnst | KIND_def, 0, NULL, NULL)) != NULL) {
			enter(cc);
			install(cc, "u32", EMIT_opc, b32_ior, type_u32, NULL);
			install(cc, "u64", EMIT_opc, b64_ior, type_u64, NULL);
			opc->fields = leave(cc, opc, ATTR_stat | KIND_typ, 0, NULL);
		}
		if ((opc = install(cc, "xor", ATTR_stat | ATTR_cnst | KIND_def, 0, NULL, NULL)) != NULL) {
			enter(cc);
			install(cc, "u32", EMIT_opc, b32_xor, type_u32, NULL);
			install(cc, "u64", EMIT_opc, b64_xor, type_u64, NULL);
			opc->fields = leave(cc, opc, ATTR_stat | KIND_typ, 0, NULL);
		}
		if ((opc = install(cc, "shl", ATTR_stat | ATTR_cnst | KIND_def, 0, NULL, NULL)) != NULL) {
			enter(cc);
			install(cc, "u32", EMIT_opc, b32_shl, type_u32, NULL);
			install(cc, "u64", EMIT_opc, b64_shl, type_u64, NULL);
			opc->fields = leave(cc, opc, ATTR_stat | KIND_typ, 0, NULL);
		}

		if ((opc = install(cc, "shr", ATTR_stat | ATTR_cnst | KIND_def, 0, NULL, NULL)) != NULL) {
			enter(cc);
			install(cc, "i32", EMIT_opc, b32_sar, type_i32, NULL);
			install(cc, "i64", EMIT_opc, b64_sar, type_i64, NULL);
			install(cc, "u32", EMIT_opc, b32_shr, type_u32, NULL);
			install(cc, "u64", EMIT_opc, b64_shr, type_u64, NULL);
			opc->fields = leave(cc, opc, ATTR_stat | KIND_typ, 0, NULL);
		}
		if ((opc = install(cc, "neg", ATTR_stat | ATTR_cnst | KIND_def, 0, NULL, NULL)) != NULL) {
			enter(cc);
			install(cc, "i32", EMIT_opc, i32_neg, type_i32, NULL);
			install(cc, "i64", EMIT_opc, i64_neg, type_i64, NULL);
			install(cc, "f32", EMIT_opc, f32_neg, type_f32, NULL);
			install(cc, "f64", EMIT_opc, f64_neg, type_f64, NULL);
			install(cc, "p4f", EMIT_opc, v4f_neg, type_p4x, NULL);
			install(cc, "p2d", EMIT_opc, v2d_neg, type_p4x, NULL);
			opc->fields = leave(cc, opc, ATTR_stat | KIND_typ, 0, NULL);
		}
		if ((opc = install(cc, "add", ATTR_stat | ATTR_cnst | KIND_def, 0, NULL, NULL)) != NULL) {
			enter(cc);
			install(cc, "i32", EMIT_opc, i32_add, type_i32, NULL);
			install(cc, "i64", EMIT_opc, i64_add, type_i64, NULL);
			install(cc, "f32", EMIT_opc, f32_add, type_f32, NULL);
			install(cc, "f64", EMIT_opc, f64_add, type_f64, NULL);
			install(cc, "p4f", EMIT_opc, v4f_add, type_p4x, NULL);
			install(cc, "p2d", EMIT_opc, v2d_add, type_p4x, NULL);
			opc->fields = leave(cc, opc, ATTR_stat | KIND_typ, 0, NULL);
		}
		if ((opc = install(cc, "sub", ATTR_stat | ATTR_cnst | KIND_def, 0, NULL, NULL)) != NULL) {
			enter(cc);
			install(cc, "i32", EMIT_opc, i32_sub, type_i32, NULL);
			install(cc, "i64", EMIT_opc, i64_sub, type_i64, NULL);
			install(cc, "f32", EMIT_opc, f32_sub, type_f32, NULL);
			install(cc, "f64", EMIT_opc, f64_sub, type_f64, NULL);
			install(cc, "p4f", EMIT_opc, v4f_sub, type_p4x, NULL);
			install(cc, "p2d", EMIT_opc, v2d_sub, type_p4x, NULL);
			opc->fields = leave(cc, opc, ATTR_stat | KIND_typ, 0, NULL);
		}
		if ((opc = install(cc, "mul", ATTR_stat | ATTR_cnst | KIND_def, 0, NULL, NULL)) != NULL) {
			enter(cc);
			install(cc, "i32", EMIT_opc, i32_mul, type_i32, NULL);
			install(cc, "i64", EMIT_opc, i64_mul, type_i64, NULL);
			install(cc, "u32", EMIT_opc, u32_mul, type_u32, NULL);
			install(cc, "u64", EMIT_opc, u64_mul, type_u32, NULL);
			install(cc, "f32", EMIT_opc, f32_mul, type_f32, NULL);
			install(cc, "f64", EMIT_opc, f64_mul, type_f64, NULL);
			install(cc, "p4f", EMIT_opc, v4f_mul, type_p4x, NULL);
			install(cc, "p2d", EMIT_opc, v2d_mul, type_p4x, NULL);
			opc->fields = leave(cc, opc, ATTR_stat | KIND_typ, 0, NULL);
		}
		if ((opc = install(cc, "div", ATTR_stat | ATTR_cnst | KIND_def, 0, NULL, NULL)) != NULL) {
			enter(cc);
			install(cc, "i32", EMIT_opc, i32_div, type_i32, NULL);
			install(cc, "i64", EMIT_opc, i64_div, type_i64, NULL);
			install(cc, "u32", EMIT_opc, u32_div, type_u32, NULL);
			install(cc, "u64", EMIT_opc, u64_div, type_u32, NULL);
			install(cc, "f32", EMIT_opc, f32_div, type_f32, NULL);
			install(cc, "f64", EMIT_opc, f64_div, type_f64, NULL);
			install(cc, "p4f", EMIT_opc, v4f_div, type_p4x, NULL);
			install(cc, "p2d", EMIT_opc, v2d_div, type_p4x, NULL);
			opc->fields = leave(cc, opc, ATTR_stat | KIND_typ, 0, NULL);
		}
		if ((opc = install(cc, "mod", ATTR_stat | ATTR_cnst | KIND_def, 0, NULL, NULL)) != NULL) {
			enter(cc);
			install(cc, "i32", EMIT_opc, i32_mod, type_i32, NULL);
			install(cc, "i64", EMIT_opc, i64_mod, type_i64, NULL);
			install(cc, "u32", EMIT_opc, u32_mod, type_u32, NULL);
			install(cc, "u64", EMIT_opc, u64_mod, type_u32, NULL);
			install(cc, "f32", EMIT_opc, f32_mod, type_f32, NULL);
			install(cc, "f64", EMIT_opc, f64_mod, type_f64, NULL);
			opc->fields = leave(cc, opc, ATTR_stat | KIND_typ, 0, NULL);
		}
		if ((opc = install(cc, "ceq", ATTR_stat | ATTR_cnst | KIND_def, 0, NULL, NULL)) != NULL) {
			enter(cc);
			install(cc, "i32", EMIT_opc, i32_ceq, type_bol, NULL);
			install(cc, "i64", EMIT_opc, i64_ceq, type_bol, NULL);
			install(cc, "f32", EMIT_opc, f32_ceq, type_bol, NULL);
			install(cc, "f64", EMIT_opc, f64_ceq, type_bol, NULL);
			install(cc, "p4f", EMIT_opc, v4f_ceq, type_bol, NULL);
			install(cc, "p2d", EMIT_opc, v2d_ceq, type_bol, NULL);
			opc->fields = leave(cc, opc, ATTR_stat | KIND_typ, 0, NULL);
		}
		if ((opc = install(cc, "clt", ATTR_stat | ATTR_cnst | KIND_def, 0, NULL, NULL)) != NULL) {
			enter(cc);
			install(cc, "i32", EMIT_opc, i32_clt, type_bol, NULL);
			install(cc, "i64", EMIT_opc, i64_clt, type_bol, NULL);
			install(cc, "u32", EMIT_opc, u32_clt, type_bol, NULL);
			install(cc, "u64", EMIT_opc, u64_clt, type_bol, NULL);
			install(cc, "f32", EMIT_opc, f32_clt, type_bol, NULL);
			install(cc, "f64", EMIT_opc, f64_clt, type_bol, NULL);
			opc->fields = leave(cc, opc, ATTR_stat | KIND_typ, 0, NULL);
		}
		if ((opc = install(cc, "cgt", ATTR_stat | ATTR_cnst | KIND_def, 0, NULL, NULL)) != NULL) {
			enter(cc);
			install(cc, "i32", EMIT_opc, i32_cgt, type_bol, NULL);
			install(cc, "i64", EMIT_opc, i64_cgt, type_bol, NULL);
			install(cc, "u32", EMIT_opc, u32_cgt, type_bol, NULL);
			install(cc, "u64", EMIT_opc, u64_cgt, type_bol, NULL);
			install(cc, "f32", EMIT_opc, f32_cgt, type_bol, NULL);
			install(cc, "f64", EMIT_opc, f64_cgt, type_bol, NULL);
			opc->fields = leave(cc, opc, ATTR_stat | KIND_typ, 0, NULL);
		}
		if ((opc = install(cc, "min", ATTR_stat | ATTR_cnst | KIND_def, 0, NULL, NULL)) != NULL) {
			enter(cc);
			install(cc, "p4f", EMIT_opc, v4f_min, type_p4x, NULL);
			install(cc, "p2d", EMIT_opc, v2d_min, type_p4x, NULL);
			opc->fields = leave(cc, opc, ATTR_stat | KIND_typ, 0, NULL);
		}
		if ((opc = install(cc, "max", ATTR_stat | ATTR_cnst | KIND_def, 0, NULL, NULL)) != NULL) {
			enter(cc);
			install(cc, "p4f", EMIT_opc, v4f_max, type_p4x, NULL);
			install(cc, "p2d", EMIT_opc, v2d_max, type_p4x, NULL);
			opc->fields = leave(cc, opc, ATTR_stat | KIND_typ, 0, NULL);
		}
		if ((opc = type_p4x) != NULL) {
			enter(cc);
			install(cc, "dp3", EMIT_opc, v4f_dp3, type_f32, NULL);
			install(cc, "dp4", EMIT_opc, v4f_dp4, type_f32, NULL);
			install(cc, "dph", EMIT_opc, v4f_dph, type_f32, NULL);
			if ((mode & installEswz) == installEswz) {
				unsigned i;
				struct { char *name; astn node; } swz[256];
				for (i = 0; i < lengthOf(swz); i += 1) {
					if (rt->_end - rt->_beg < 5) {
						fatal(ERR_MEMORY_OVERRUN);
						return;
					}
					rt->_beg[0] = (unsigned char) "xyzw"[(i >> 0) & 3];
					rt->_beg[1] = (unsigned char) "xyzw"[(i >> 2) & 3];
					rt->_beg[2] = (unsigned char) "xyzw"[(i >> 4) & 3];
					rt->_beg[3] = (unsigned char) "xyzw"[(i >> 6) & 3];
					rt->_beg[4] = 0;
					swz[i].name = mapstr(cc, (char*)rt->_beg, -1, -1);
					swz[i].node = intNode(cc, i);
				}
				for (i = 0; i < lengthOf(swz); i += 1) {
					install(cc, swz[i].name, EMIT_opc, p4x_swz, type_p4x, swz[i].node);
				}
			}
			opc->fields = leave(cc, opc, ATTR_stat | KIND_typ, 0, NULL);
		}
		dieif(cc->emit_opc->fields, ERR_INTERNAL_ERROR);
		cc->emit_opc->fields = leave(cc, cc->emit_opc, ATTR_stat | KIND_typ, 0, NULL);
	}

	// export emit to the compiler context
	if (cc->emit_opc && !(cc->emit_tag = lnkNode(cc, cc->emit_opc))) {
		error(rt, NULL, 0, ERR_INTERNAL_ERROR);
	}
}

/**
 * @brief Instal type system.
 * @param runtime context.
 + @param level warning level.
 + @param file additional file extending module
 */
static int install_base(rtContext rt, vmError onHalt(nfcContext)) {
	int error = 0;
	ccContext cc = rt->cc;

	// !!! halt must be the first native call.
	error = error || !ccDefCall(rt, onHalt ? onHalt : haltDummy, "void halt()");

	// 4 reflection
	if (cc->type_rec != NULL && cc->type_var != NULL) {
		symn arg = NULL;
		enter(cc);

		if ((arg = install(cc, "size", ATTR_cnst | KIND_var, vm_size, cc->type_i32, NULL))) {
			arg->offs = offsetOf(symn, size);
		}
		else {
			error = 1;
		}

		if ((arg = install(cc, "offset", ATTR_cnst | KIND_var, vm_size, cc->type_i32, NULL))) {
			arg->offs = offsetOf(symn, offs);
			arg->pfmt = "@%06x";
		}
		else {
			error = 1;
		}

		// HACK: `operator(typename type).file = typename.file(type);`
		error = error || !ccDefMember(rt, typenameGetField, type_get_file);
		// HACK: `operator(typename type).line = typename.line(type);`
		error = error || !ccDefMember(rt, typenameGetField, type_get_line);
		// HACK: `operator(typename type).name = typename.name(type);`
		error = error || !ccDefMember(rt, typenameGetField, type_get_name);

		error = error || !ccDefCall(rt, typenameGetField, type_get_base);

		/* TODO: more 4 reflection
		error = error || !ccDefCall(rt, typenameReflect, "variant lookup(variant &obj, int options, string name, variant args...)");
		error = error || !ccDefCall(rt, typenameReflect, "variant invoke(variant &obj, int options, string name, variant args...)");
		// setvalue and getvalue can be done with lookup and invoke
		error = error || !ccDefCall(rt, typenameReflect, "variant setValue(typename field, variant value)");
		error = error || !ccDefCall(rt, typenameReflect, "variant getValue(typename field)");

		error = error || !ccDefCall(rt, typenameReflect, "bool canAssign(typename type, variant value, bool strict)");
		error = error || !ccDefCall(rt, typenameReflect, "bool instanceOf(typename type, variant obj)");
		//~ */

		dieif(cc->type_rec->fields, ERR_INTERNAL_ERROR);
		cc->type_rec->fields = leave(cc, cc->type_rec, KIND_typ, 0, NULL);
	}
	return error;
}

///// Compiler

/// Initialze compiler context; @see header
ccContext ccInit(rtContext rt, ccInstall mode, vmError onHalt(nfcContext)) {
	ccContext cc;

	dieif(rt->cc != NULL, ERR_INTERNAL_ERROR);
	dieif(rt->_beg != rt->_mem, ERR_INTERNAL_ERROR);
	dieif(rt->_end != rt->_mem + rt->_size, ERR_INTERNAL_ERROR);

	cc = (ccContext)(rt->_end - sizeof(struct ccContextRec));
	rt->_end -= sizeof(struct ccContextRec);
	rt->_beg += 1;	// HACK: make first symbol start not at null.

	if (rt->_end < rt->_beg) {
		fatal(ERR_MEMORY_OVERRUN);
		return NULL;
	}

	memset(rt->_end, 0, sizeof(struct ccContextRec));

	cc->rt = rt;
	rt->cc = cc;

	cc->scope = NULL;
	cc->global = NULL;

	cc->_chr = -1;

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

/// Begin a namespace; @see rtContext.api.ccBegin
symn ccBegin(rtContext rt, const char *name) {
	symn result = NULL;
	if (rt->cc == NULL) {
		trace(ERR_INTERNAL_ERROR);
		return NULL;
	}
	if (name != NULL) {
		result = install(rt->cc, name, ATTR_stat | ATTR_cnst | KIND_typ | CAST_vid, 0, rt->cc->type_vid, NULL);
	}
	enter(rt->cc);
	return result;
}

/// Close a namespace; @see rtContext.api.ccEnd
symn ccEnd(rtContext rt, symn cls) {
	if (rt->cc == NULL) {
		trace(ERR_INTERNAL_ERROR);
		return NULL;
	}
	return leave(rt->cc, cls, ATTR_stat | KIND_typ, vm_size, NULL);
}

/// Declare int constant; @see rtContext.api.ccDefInt
symn ccDefInt(rtContext rt, const char *name, int64_t value) {
	if (!rt || !rt->cc || !name) {
		return NULL;
	}
	name = mapstr(rt->cc, name, -1, -1);
	return install(rt->cc, name, KIND_def | CAST_i32, 0, rt->cc->type_i32, intNode(rt->cc, value));
}
/// Declare float constant; @see rtContext.api.ccDefFlt
symn ccDefFlt(rtContext rt, const char *name, double value) {
	if (!rt || !rt->cc || !name) {
		return NULL;
	}
	name = mapstr(rt->cc, name, -1, -1);
	return install(rt->cc, name, KIND_def | CAST_f64, 0, rt->cc->type_f64, fltNode(rt->cc, value));
}
/// Declare string constant; @see rtContext.api.ccDefStr
symn ccDefStr(rtContext rt, const char *name, char *value) {
	if (!rt || !rt->cc || !name) {
		return NULL;
	}
	name = mapstr(rt->cc, name, -1, -1);
	if (value != NULL) {
		value = mapstr(rt->cc, value, -1, -1);
	}
	return install(rt->cc, name, KIND_def | CAST_ref, 0, rt->cc->type_str, strNode(rt->cc, value));
}

/// Install a type; @see rtContext.api.ccDefType
symn ccDefType(rtContext rt, const char *name, unsigned size, int refType) {
	return install(rt->cc, name, ATTR_stat | ATTR_cnst | KIND_typ | (refType ? CAST_ref : CAST_val), size, rt->cc->type_rec, NULL);
}

/// Find symbol by name; @see header
symn ccLookupSym(ccContext cc, symn in, char *name) {
	struct astNode ast;
	memset(&ast, 0, sizeof(struct astNode));
	ast.kind = TOKEN_var;
	ast.ref.name = name;
	ast.ref.hash = rehash(name, -1) % TBL_SIZE;
	return lookup(cc, in ? in->fields : cc->scope, &ast, NULL, 1);
}

/// Lookup symbol by offset; @see rtContext.api.rtFindSym
symn rtFindSym(rtContext rt, size_t offs, int callsOnly) {
	symn sym = rt->main;
	dieif(offs > rt->_size, ERR_INVALID_OFFSET, offs);
	if (offs > rt->vm.px + px_size) {
		// local variable on stack ?
		return NULL;
	}
	if (offs >= sym->offs && offs < sym->offs + sym->size) {
		// is the main function ?
		return sym;
	}
	for (sym = sym->fields; sym; sym = sym->global) {
		if (callsOnly && sym->params == NULL) {
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

///// Debugger

// arrayBuffer
static void *setBuff(struct arrBuffer *buff, int idx, void *data) {
	void *newPtr;
	int pos = idx * buff->esz;

	if (pos >= buff->cap) {
		buff->cap <<= 1;
		if (pos > 2 * buff->cap) {
			buff->cap = pos << 1;
		}
		newPtr = realloc(buff->ptr, (size_t)buff->cap);
		if (newPtr != NULL) {
			buff->ptr = newPtr;
		}
		else {
			freeBuff(buff);
		}
	}

	if (buff->cnt >= idx) {
		buff->cnt = idx + 1;
	}

	if (buff->ptr && data) {
		memcpy(buff->ptr + pos, data, (size_t) buff->esz);
	}

	return buff->ptr ? buff->ptr + pos : NULL;
}
static void *insBuff(struct arrBuffer *buff, int idx, void *data) {
	void *newPtr;
	int pos = idx * buff->esz;
	int newCap = buff->cnt * buff->esz;
	if (newCap < pos) {
		newCap = pos;
	}

	// resize buffer
	if (newCap >= buff->cap) {
		if (newCap > 2 * buff->cap) {
			buff->cap = newCap;
		}
		buff->cap *= 2;
		newPtr = realloc(buff->ptr, (size_t) buff->cap);
		if (newPtr != NULL) {
			buff->ptr = newPtr;
		}
		else {
			freeBuff(buff);
		}
	}

	if (idx < buff->cnt) {
		memmove(buff->ptr + pos + buff->esz, buff->ptr + pos, (size_t) (buff->esz * (buff->cnt - idx)));
		idx = buff->cnt;
	}

	if (buff->cnt >= idx) {
		buff->cnt = idx + 1;
	}

	if (buff->ptr && data) {
		memcpy(buff->ptr + pos, data, (size_t) buff->esz);
	}

	return buff->ptr ? buff->ptr + pos : NULL;
}
int initBuff(struct arrBuffer *buff, int initCount, int elemSize) {
	buff->cnt = 0;
	buff->ptr = 0;
	buff->esz = elemSize;
	buff->cap = initCount * elemSize;
	return setBuff(buff, initCount, NULL) != NULL;
}
void freeBuff(struct arrBuffer *buff) {
	free(buff->ptr);
	buff->ptr = 0;
	buff->cap = 0;
	buff->esz = 0;
}

dbgn getDbgStatement(rtContext rt, char *file, int line) {
	if (rt->dbg != NULL) {
		int i;
		dbgn result = (dbgn)rt->dbg->statements.ptr;
		for (i = 0; i < rt->dbg->statements.cnt; ++i) {
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
		dbgn result = (dbgn)rt->dbg->statements.ptr;
		int i, n = rt->dbg->statements.cnt;
		for (i = 0; i < n; ++i) {
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
		int i = 0;
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

dbgn mapDbgFunction(rtContext rt, size_t position) {
	if (rt->dbg != NULL) {
		dbgn result = (dbgn)rt->dbg->functions.ptr;
		int i, n = rt->dbg->functions.cnt;
		for (i = 0; i < n; ++i) {
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
		int i;
		for (i = 0; i < rt->dbg->functions.cnt; ++i) {
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
			result->decl = fun;
			result->file = fun->file;
			result->line = fun->line;
			result->start = fun->offs;
			result->end = fun->offs + fun->size;
		}
	}
	return result;
}

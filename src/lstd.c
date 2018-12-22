/*******************************************************************************
 *   File: lstd.c
 *   Date: 2011/06/23
 *   Desc: standard library
 *******************************************************************************
 * basic math functions
 * standard functions
 */

#include "internal.h"
#include <math.h>
#include <time.h>

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ math functions
static vmError f64sin(nfcContext args) {
	float64_t x = argf64(args, 0);
	retf64(args, sin(x));
	return noError;
}
static vmError f64cos(nfcContext args) {
	float64_t x = argf64(args, 0);
	retf64(args, cos(x));
	return noError;
}
static vmError f64tan(nfcContext args) {
	float64_t x = argf64(args, 0);
	retf64(args, tan(x));
	return noError;
}
static vmError f64log(nfcContext args) {
	float64_t x = argf64(args, 0);
	retf64(args, log(x));
	return noError;
}
static vmError f64exp(nfcContext args) {
	float64_t x = argf64(args, 0);
	retf64(args, exp(x));
	return noError;
}
static vmError f64pow(nfcContext args) {
	float64_t x = argf64(args, 8);
	float64_t y = argf64(args, 0);
	retf64(args, pow(x, y));
	return noError;
}
static vmError f64sqrt(nfcContext args) {
	float64_t x = argf64(args, 0);
	retf64(args, sqrt(x));
	return noError;
}
static vmError f64atan2(nfcContext args) {
	float64_t x = argf64(args, 8);
	float64_t y = argf64(args, 0);
	retf64(args, atan2(x, y));
	return noError;
}

static vmError f32sin(nfcContext args) {
	float32_t x = argf32(args, 0);
	retf32(args, sinf(x));
	return noError;
}
static vmError f32cos(nfcContext args) {
	float32_t x = argf32(args, 0);
	retf32(args, cosf(x));
	return noError;
}
static vmError f32tan(nfcContext args) {
	float32_t x = argf32(args, 0);
	retf32(args, tanf(x));
	return noError;
}
static vmError f32log(nfcContext args) {
	float32_t x = argf32(args, 0);
	retf32(args, logf(x));
	return noError;
}
static vmError f32exp(nfcContext args) {
	float32_t x = argf32(args, 0);
	retf32(args, expf(x));
	return noError;
}
static vmError f32pow(nfcContext args) {
	float32_t x = argf32(args, 4);
	float32_t y = argf32(args, 0);
	retf32(args, powf(x, y));
	return noError;
}
static vmError f32sqrt(nfcContext args) {
	float32_t x = argf32(args, 0);
	retf32(args, sqrtf(x));
	return noError;
}
static vmError f32atan2(nfcContext args) {
	float32_t x = argf32(args, 4);
	float32_t y = argf32(args, 0);
	retf32(args, atan2f(x, y));
	return noError;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ bit operations
// sign extend
static vmError b32sxt(nfcContext args) {
	int32_t val = argi32(args, 8);
	int32_t ofs = argi32(args, 4);
	int32_t cnt = argi32(args, 0);
	reti32(args, (val << (32 - (ofs + cnt))) >> (32 - cnt));
	return noError;
}
// zero extend
static vmError b32zxt(nfcContext args) {
	uint32_t val = argu32(args, 8);
	int32_t ofs = argi32(args, 4);
	int32_t cnt = argi32(args, 0);
	retu32(args, (val << (32 - (ofs + cnt))) >> (32 - cnt));
	return noError;
}
// count bit population
static vmError b32pop(nfcContext args) {
	uint32_t val = argu32(args, 0);
	retu32(args, bitcnt(val));
	return noError;
}
// swap bits
static vmError b32swp(nfcContext args) {
	uint32_t val = argu32(args, 0);
	val = ((val >> 1) & 0x55555555) | ((val & 0x55555555) << 1);
	val = ((val >> 2) & 0x33333333) | ((val & 0x33333333) << 2);
	val = ((val >> 4) & 0x0F0F0F0F) | ((val & 0x0F0F0F0F) << 4);
	val = ((val >> 8) & 0x00FF00FF) | ((val & 0x00FF00FF) << 8);
	retu32(args, (val >> 16) | (val << 16));
	return noError;
}
// bit scan reverse: position of the Most Significant Bit
static vmError b32sr(nfcContext args) {
	uint32_t val = argu32(args, 0);
	reti32(args, bitsr(val));
	return noError;
}
// bit scan forward: position of the Least Significant Bit
static vmError b32sf(nfcContext args) {
	uint32_t val = argu32(args, 0);
	reti32(args, bitsf(val));
	return noError;
}
// keep the highest: Most Significant Bit
static vmError b32hi(nfcContext args) {
	uint32_t val = argu32(args, 0);
	val |= val >> 1;
	val |= val >> 2;
	val |= val >> 4;
	val |= val >> 8;
	val |= val >> 16;
	retu32(args, val - (val >> 1));
	return noError;
}
// keep the lowest: Least Significant Bit
static vmError b32lo(nfcContext args) {
	uint32_t val = argu32(args, 0);
	retu32(args, val & -val);
	return noError;
}

static vmError b64sxt(nfcContext args) {
	int64_t val = argi64(args, 12);
	int32_t ofs = argi32(args, 8);
	int32_t cnt = argi32(args, 0);
	reti64(args, (val << (64 - (ofs + cnt))) >> (64 - cnt));
	return noError;
}
static vmError b64zxt(nfcContext args) {
	uint64_t val = (uint64_t) argi64(args, 12);
	int32_t ofs = argi32(args, 8);
	int32_t cnt = argi32(args, 0);
	retu64(args, (val << (64 - (ofs + cnt))) >> (64 - cnt));
	return noError;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ system functions (exit, rand, clock, debug)
static vmError sysExit(nfcContext args) {
	exit(argi32(args, 0));
	return noError;
}

static vmError sysRand(nfcContext args) {
	reti32(args, rand());
	return noError;
}
static vmError sysSRand(nfcContext args) {
	int seed = argi32(args, 0);
	srand((unsigned) seed);
	return noError;
}

static vmError sysTime(nfcContext args) {
	reti32(args, time(NULL));
	return noError;
}
static vmError sysClock(nfcContext args) {
	reti32(args, clock());
	return noError;
}
static vmError sysMillis(nfcContext args) {
	retu64(args, timeMillis());
	return noError;
}
static vmError sysMSleep(nfcContext args) {
	sleepMillis(argi64(args, 0));
	return noError;
}

// void raise(char file[*], int line, int level, int trace, char message[*], variant inspect);
static vmError sysRaise(nfcContext ctx) {
	rtContext rt = ctx->rt;
	char *file = nfcReadArg(ctx, nfcNextArg(ctx)).ref;
	nfcCheckArg(ctx, CAST_ref, "file");
	int line = argi32(ctx, nfcNextArg(ctx));
	nfcCheckArg(ctx, CAST_i32, "line");
	int logLevel = argi32(ctx, nfcNextArg(ctx));
	nfcCheckArg(ctx, CAST_i32, "level");
	int trace = argi32(ctx, nfcNextArg(ctx));
	nfcCheckArg(ctx, CAST_i32, "trace");
	char *message = nfcReadArg(ctx, nfcNextArg(ctx)).ref;
	nfcCheckArg(ctx, CAST_ref, "message");
	rtValue inspect = nfcReadArg(ctx, nfcNextArg(ctx));
	nfcCheckArg(ctx, CAST_var, "inspect");

	// logging is disabled or log level not reached.
	if (rt->logFile == NULL || logLevel > (int)rt->logLevel) {
		return noError;
	}

	printLog(rt, logLevel, file, line, &inspect, "%?s", message);

	// print stack trace excluding this function
	if (rt->dbg != NULL && trace > 0) {
		traceCalls(rt->dbg, rt->logFile, 1, trace - 1, 1);
	}

	// abort the execution
	if (logLevel == raiseFatal) {
		return nativeCallError;
	}

	return noError;
}

// int tryExec(pointer args, void action(pointer args));
static vmError sysTryExec(nfcContext ctx) {
	vmError result;
	rtContext rt = ctx->rt;
	size_t args = argref(ctx, nfcNextArg(ctx));
	size_t actionOffs = argref(ctx, nfcNextArg(ctx));
	symn action = rtLookup(rt, actionOffs, KIND_fun);

	if (action != NULL && action->offs == actionOffs) {
		#pragma pack(push, 4)
		struct { vmOffs ptr; } cbArg;
		cbArg.ptr = (vmOffs) args;
		#pragma pack(pop)
		result = invoke(rt, action, NULL, &cbArg, ctx->extra);
	}
	else {
		result = illegalState;
	}
	reti32(ctx, result);
	return noError;
}

// pointer alloc(pointer ptr, int32 size);
static vmError sysMemMgr(nfcContext ctx) {
	void *old = nfcReadArg(ctx, nfcNextArg(ctx)).ref;
	int size = nfcReadArg(ctx, nfcNextArg(ctx)).i32;
	void *res = rtAlloc(ctx->rt, old, (size_t) size, NULL);
	retref(ctx, vmOffset(ctx->rt, res));
	return noError;
}
// pointer fill(pointer dst, int value, int32 size)
static vmError sysMemSet(nfcContext ctx) {
	void *dst = nfcReadArg(ctx, nfcNextArg(ctx)).ref;
	int value = nfcReadArg(ctx, nfcNextArg(ctx)).i32;
	int size = nfcReadArg(ctx, nfcNextArg(ctx)).i32;
	void *res = memset(dst, value, (size_t) size);
	retref(ctx, vmOffset(ctx->rt, res));
	return noError;
}
// pointer copy(pointer dst, pointer src, int32 size);
static vmError sysMemCpy(nfcContext ctx) {
	void *dst = nfcReadArg(ctx, nfcNextArg(ctx)).ref;
	void *src = nfcReadArg(ctx, nfcNextArg(ctx)).ref;
	int size = nfcReadArg(ctx, nfcNextArg(ctx)).i32;
	void *res = memcpy(dst, src, (size_t) size);
	retref(ctx, vmOffset(ctx->rt, res));
	return noError;
}
// pointer move(pointer dst, pointer src, int32 size);
static vmError sysMemMove(nfcContext ctx) {
	void *dst = nfcReadArg(ctx, nfcNextArg(ctx)).ref;
	void *src = nfcReadArg(ctx, nfcNextArg(ctx)).ref;
	int size = nfcReadArg(ctx, nfcNextArg(ctx)).i32;
	void *res = memmove(dst, src, (size_t) size);
	retref(ctx, vmOffset(ctx->rt, res));
	return noError;
}
//#}#endregion

int ccLibStd(ccContext cc) {
	symn nsp = NULL;		// namespace
	int err = 0;
	size_t i;

	struct {
		vmError (*fun)(nfcContext);
		char *def;
	}
	bit32[] = {      // 32 bit operations
		{b32zxt,    "int32 zxt(int32 value, int32 offs, int32 count)"},
		{b32sxt,    "int32 sxt(int32 value, int32 offs, int32 count)"},
		{b32pop,    "int32 pop(int32 value)"},
		{b32swp,    "int32 swap(int32 value)"},
		{b32sr,     "int32 bsr(int32 value)"},
		{b32sf,     "int32 bsf(int32 value)"},
		{b32hi,     "int32 hib(int32 value)"},
		{b32lo,     "int32 lob(int32 value)"},
	},
	bit64[] = {      // 64 bit operations
		{b64zxt,    "int64 zxt(int64 value, int32 offs, int32 count)"},
		{b64sxt,    "int64 sxt(int64 value, int32 offs, int32 count)"},
	},
	flt32[] = {      // sin, cos, sqrt, ...
		{f32sin,    "float32 sin(float32 x)"},
		{f32cos,    "float32 cos(float32 x)"},
		{f32tan,    "float32 tan(float32 x)"},
		{f32log,    "float32 log(float32 x)"},
		{f32exp,    "float32 exp(float32 x)"},
		{f32pow,    "float32 pow(float32 x, float32 y)"},
		{f32sqrt,   "float32 sqrt(float32 x)"},
		{f32atan2,  "float32 atan2(float32 x, float32 y)"},
	},
	flt64[] = {      // sin, cos, sqrt, ...
		{f64sin,    "float64 sin(float64 x)"},
		{f64cos,    "float64 cos(float64 x)"},
		{f64tan,    "float64 tan(float64 x)"},
		{f64log,    "float64 log(float64 x)"},
		{f64exp,    "float64 exp(float64 x)"},
		{f64pow,    "float64 pow(float64 x, float64 y)"},
		{f64sqrt,   "float64 sqrt(float64 x)"},
		{f64atan2,  "float64 atan2(float64 x, float64 y)"},
	},
	misc[] = {       // IO/MEM/EXIT
		{sysExit,   "void exit(int code)"},
		{sysSRand,  "void srand(int seed)"},
		{sysRand,   "int32 rand()"},

		{sysTime,   "int32 time()"},
		{sysClock,  "int32 clock()"},
		{sysMillis, "int64 millis()"},
		{sysMSleep, "void sleep(int64 millis)"},
	};
	struct {
		int64_t value;
		char *name;
	}
	constants[] = {
			{CLOCKS_PER_SEC, "CLOCKS_PER_SEC"},
			{RAND_MAX, "RAND_MAX"},
	},
	logLevels[] = {
		{raiseFatal, "abort"},
		{raiseError, "error"},
		{raiseWarn, "warn"},
		{raiseInfo, "info"},
		{raiseDebug, "debug"},
		{raiseVerbose, "verbose"},
		// trace levels
		{0, "noTrace"},
		{128, "defTrace"},
	};

	for (i = 0; i < lengthOf(constants); i += 1) {
		if (!ccDefInt(cc, constants[i].name, constants[i].value)) {
			err = 1;
			break;
		}
	}

	if (!err && cc->type_var != NULL) {		// debug, trace, assert, fatal, ...
		cc->libc_dbg = ccAddCall(cc, sysRaise, "void raise(char file[*], int line, int level, int trace, char message[*], variant inspect)");
		if (cc->libc_dbg == NULL) {
			err = 2;
		}
		if (!err && ccExtend(cc, cc->libc_dbg)) {
			for (i = 0; i < lengthOf(logLevels); i += 1) {
				if (!ccDefInt(cc, logLevels[i].name, logLevels[i].value)) {
					err = 1;
					break;
				}
			}
			ccEnd(cc, cc->libc_dbg);
		}
	}

	if (!err && cc->type_ptr != NULL) {		// tryExecute
		cc->libc_try = ccAddCall(cc, sysTryExec, "int tryExec(pointer args, void action(pointer args))");
		if (cc->libc_try == NULL) {
			err = 2;
		}
	}

	// re-alloc, malloc, free, memset, memcpy
	if (!err && ccExtend(cc, cc->type_ptr)) {
		if (!ccAddCall(cc, sysMemMgr, "pointer alloc(pointer ptr, int32 size)")) {
			err = 3;
		}
		if (!ccAddCall(cc, sysMemSet, "pointer fill(pointer dst, int value, int32 size)")) {
			err = 3;
		}
		if (!ccAddCall(cc, sysMemCpy, "pointer copy(pointer dst, pointer src, int32 size)")) {
			err = 3;
		}
		if (!ccAddCall(cc, sysMemMove, "pointer move(pointer dst, pointer src, int32 size)")) {
			err = 3;
		}
		ccEnd(cc, cc->type_ptr);
	}

	// System.exit(int code), ...
	if (!err && (nsp = ccBegin(cc, "System"))) {
		for (i = 0; i < lengthOf(misc); i += 1) {
			if (!ccAddCall(cc, misc[i].fun, misc[i].def)) {
				err = 4;
				break;
			}
		}
		//~ install(cc, "Arguments", CAST_arr, 0, 0);	// string Args[];
		//~ install(cc, "Environment", KIND_def, 0, 0);	// string Env[string];
		ccEnd(cc, nsp);
	}

	// Add extra operations to int32
	if (!err && ccExtend(cc, cc->type_u32)) {
		for (i = 0; i < lengthOf(bit32); i += 1) {
			if (!ccAddCall(cc, bit32[i].fun, bit32[i].def)) {
				err = 5;
				break;
			}
		}
		ccEnd(cc, cc->type_u32);
	}
	// Add extra operations to int64
	if (!err && ccExtend(cc, cc->type_u64)) {
		for (i = 0; i < lengthOf(bit64); i += 1) {
			if (!ccAddCall(cc, bit64[i].fun, bit64[i].def)) {
				err = 5;
				break;
			}
		}
		ccEnd(cc, cc->type_u64);
	}
	// add math functions to float32
	if (!err && ccExtend(cc, cc->type_f32)) {
		for (i = 0; i < lengthOf(flt32); i += 1) {
			if (!ccAddCall(cc, flt32[i].fun, flt32[i].def)) {
				err = 7;
				break;
			}
		}
		ccEnd(cc, cc->type_f32);
	}
	// add math functions to float64
	if (!err && ccExtend(cc, cc->type_f64)) {
		for (i = 0; i < lengthOf(flt64); i += 1) {
			if (!ccAddCall(cc, flt64[i].fun, flt64[i].def)) {
				err = 6;
				break;
			}
		}
		ccEnd(cc, cc->type_f64);
	}
	return err;
}

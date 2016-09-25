/*******************************************************************************
 *   File: lstd.c
 *   Date: 2011/06/23
 *   Desc: standard library
 *******************************************************************************
basic math, debug, and system functions
*******************************************************************************/
#include <math.h>
#include <time.h>
#include <stdlib.h>
#include "internal.h"

//#{#region math functions
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
	float64_t x = argf64(args, 0);
	float64_t y = argf64(args, 8);
	retf64(args, pow(x, y));
	return noError;
}
static vmError f64sqrt(nfcContext args) {
	float64_t x = argf64(args, 0);
	retf64(args, sqrt(x));
	return noError;
}
static vmError f64atan2(nfcContext args) {
	float64_t x = argf64(args, 0);
	float64_t y = argf64(args, 8);
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
	float32_t x = argf32(args, 0);
	float32_t y = argf32(args, 4);
	retf32(args, powf(x, y));
	return noError;
}
static vmError f32sqrt(nfcContext args) {
	float32_t x = argf32(args, 0);
	retf32(args, sqrtf(x));
	return noError;
}
static vmError f32atan2(nfcContext args) {
	float32_t x = argf32(args, 0);
	float32_t y = argf32(args, 4);
	retf32(args, atan2f(x, y));
	return noError;
}
//#}#endregion

//#{#region bit operations
static vmError b32sxt(nfcContext args) {
	int32_t val = (uint32_t) argi32(args, 0);
	int32_t ofs = (uint32_t) argi32(args, 4);
	int32_t cnt = (uint32_t) argi32(args, 8);
	reti32(args, (val << (32 - (ofs + cnt))) >> (32 - cnt));
	return noError;
}
static vmError b32zxt(nfcContext args) {
	uint32_t val = (uint32_t) argi32(args, 0);
	int32_t ofs = (uint32_t) argi32(args, 4);
	int32_t cnt = (uint32_t) argi32(args, 8);
	retu32(args, (val << (32 - (ofs + cnt))) >> (32 - cnt));
	return noError;
}
static vmError b64sxt(nfcContext args) {
	int64_t val = (uint32_t) argi32(args, 0);
	int32_t ofs = (uint32_t) argi32(args, 4);
	int32_t cnt = (uint32_t) argi32(args, 8);
	reti64(args, (val << (64 - (ofs + cnt))) >> (64 - cnt));
	return noError;
}
static vmError b64zxt(nfcContext args) {
	uint64_t val = (uint32_t) argi32(args, 0);
	int32_t ofs = (uint32_t) argi32(args, 4);
	int32_t cnt = (uint32_t) argi32(args, 8);
	retu64(args, (val << (64 - (ofs + cnt))) >> (64 - cnt));
	return noError;
}
/* unused bit operations
static vmError b64not(nfcContext args) {
	int64_t x = argi64(args, 0);
	reti64(args, ~x);
	return noError;
}
static vmError b64and(nfcContext args) {
	int64_t x = argi64(args, 0);
	int64_t y = argi64(args, 8);
	reti64(args, x & y);
	return noError;
}
static vmError b64ior(nfcContext args) {
	int64_t x = argi64(args, 0);
	int64_t y = argi64(args, 8);
	reti64(args, x | y);
	return noError;
}
static vmError b64xor(nfcContext args) {
	int64_t x = argi64(args, 0);
	int64_t y = argi64(args, 8);
	reti64(args, x ^ y);
	return noError;
}
static vmError b64shl(nfcContext args) {
	int64_t x = argi64(args, 0);
	int32_t y = argi32(args, 8);
	reti64(args, x << y);
	return noError;
}
static vmError b64shr(nfcContext args) {
	uint64_t x = (uint64_t) argi64(args, 0);
	int32_t y = argi32(args, 8);
	reti64(args, (int64_t) (x >> y));
	return noError;
}
static vmError b64sar(nfcContext args) {
	int64_t x = argi64(args, 0);
	int32_t y = argi32(args, 8);
	reti64(args, x >> y);
	return noError;
}

static int b32cnt(nfcContext args) {
	uint32_t x = (uint32_t) argi32(args, 0);
	x -= ((x >> 1) & 0x55555555);
	x = (((x >> 2) & 0x33333333) + (x & 0x33333333));
	x = (((x >> 4) + x) & 0x0f0f0f0f);
	x += (x >> 8) + (x >> 16);
	reti32(args, x & 0x3f);
	return 0;
}
static int b32sf(nfcContext args) {
	int32_t result = 0;
	uint32_t x = (uint32_t) argi32(args, 0);
	if ((x & 0x0000ffff) == 0) { result += 16; x >>= 16; }
	if ((x & 0x000000ff) == 0) { result +=  8; x >>=  8; }
	if ((x & 0x0000000f) == 0) { result +=  4; x >>=  4; }
	if ((x & 0x00000003) == 0) { result +=  2; x >>=  2; }
	if ((x & 0x00000001) == 0) { result +=  1; }
	reti32(args, x ? result : -1);
	return 0;
}
static int b32sr(nfcContext args) {
	int32_t result = 0;
	uint32_t x = (uint32_t) argi32(args, 0);
	if ((x & 0xffff0000) != 0) { result += 16; x >>= 16; }
	if ((x & 0x0000ff00) != 0) { result +=  8; x >>=  8; }
	if ((x & 0x000000f0) != 0) { result +=  4; x >>=  4; }
	if ((x & 0x0000000c) != 0) { result +=  2; x >>=  2; }
	if ((x & 0x00000002) != 0) { result +=  1; }
	reti32(args, x ? result : -1);
	return 0;
}
static int b32hi(nfcContext args) {
	uint32_t x = (uint32_t) argi32(args, 0);
	x |= x >> 1;
	x |= x >> 2;
	x |= x >> 4;
	x |= x >> 8;
	x |= x >> 16;
	reti32(args, x - (x >> 1));
	return 0;
}
static int b32lo(nfcContext args) {
	int32_t x = argi32(args, 0);
	reti32(args, x & -x);
	return 0;
}
static int b32swap(nfcContext args) {
	uint32_t x = (uint32_t) argi32(args, 0);
	x = ((x >> 1) & 0x55555555) | ((x & 0x55555555) << 1);
	x = ((x >> 2) & 0x33333333) | ((x & 0x33333333) << 2);
	x = ((x >> 4) & 0x0F0F0F0F) | ((x & 0x0F0F0F0F) << 4);
	x = ((x >> 8) & 0x00FF00FF) | ((x & 0x00FF00FF) << 8);
	reti64(args, (x >> 16) | (x << 16));
	return 0;
}
// */
//#}#endregion

//#{#region file operations
static inline size_t argpos(size_t *argp, size_t size) {
	size_t result = *argp;
	*argp += padded(size ? size : 1, vm_size);
	return result;
}

#define debugFILE(msg, ...) //do { prerr("debug:File", msg, ##__VA_ARGS__); } while(0)

static const char *const proto_file_open = "File Open(char path[*])";
static const char *const proto_file_create = "File Create(char path[*])";
static const char *const proto_file_append = "File Append(char path[*])";

static const char *const proto_file_peek_byte = "int Peek(File file)";
static const char *const proto_file_read_byte = "int Read(File file)";
static const char *const proto_file_read_buff = "int Read(File file, uint8 buff[])";
static const char *const proto_file_read_line = "int ReadLine(File file, uint8 buff[])";

static const char *const proto_file_write_byte = "int Write(File file, uint8 byte)";
static const char *const proto_file_write_buff = "int Write(File file, uint8 buff[])";

static const char *const proto_file_flush = "void Flush(File file)";
static const char *const proto_file_close = "void Close(File file)";

static const char *const proto_file_get_stdin = "File StdIn";
static const char *const proto_file_get_stdout = "File StdOut";
static const char *const proto_file_get_stderr = "File StdErr";
static const char *const proto_file_get_dbgout = "File DbgOut";

static vmError FILE_open(nfcContext args) {       // File Open(char filename[]);
	size_t argc = 0;
	char *name = argref(args, argpos(&argc, vm_size));
	char *mode = NULL;
	if (args->proto == proto_file_open) {
		mode = "r";
	}
	else if (args->proto == proto_file_create) {
		mode = "w";
	}
	else if (args->proto == proto_file_append) {
		mode = "a";
	}
	FILE *file = fopen(name, mode);
	rethnd(args, file);
	debugFILE("Name: %s, Mode: %s, File: %x", name, mode, file);
	return file == NULL ? noError : libCallAbort;
}
static vmError FILE_close(nfcContext args) {      // void close(File file);
	FILE *file = arghnd(args, 0);
	debugFILE("File: %x", file);
	fclose(file);
	return noError;
}
static vmError FILE_stream(nfcContext args) {     // File std[in, out, err];
	if (args->proto == proto_file_get_stdin) {
		rethnd(args, stdin);
		return noError;
	}
	if (args->proto == proto_file_get_stdout) {
		rethnd(args, stdout);
		return noError;
	}
	if (args->proto == proto_file_get_stderr) {
		rethnd(args, stderr);
		return noError;
	}
	if (args->proto == proto_file_get_dbgout) {
		rethnd(args, args->rt->logFile);
		return noError;
	}

	debugFILE("error opening stream: %x", args->proto);
	return executionAborted;
}

static vmError FILE_getc(nfcContext rt) {
	FILE *file = arghnd(rt, 0);
	reti32(rt, fgetc(file));
	return noError;
}
static vmError FILE_peek(nfcContext rt) {
	FILE *file = arghnd(rt, 0);
	int chr = ungetc(getc(file), file);
	reti32(rt, chr);
	return noError;
}
static vmError FILE_read(nfcContext rt) {         // int read(File &f, uint8 buff[])
	size_t argc = 0;
	FILE *file = arghnd(rt, argpos(&argc, sizeof(FILE *)));
	char *buff = argref(rt, argpos(&argc, vm_size));
	int len = argi32(rt, argpos(&argc, vm_size));
	reti32(rt, fread(buff, (size_t) len, 1, file));
	return noError;
}
static vmError FILE_gets(nfcContext args) {       // int fgets(File &f, uint8 buff[])
	size_t argc = 0;
	FILE *file = arghnd(args, argpos(&argc, sizeof(FILE *)));
	char *buff = argref(args, argpos(&argc, vm_size));
	int len = argi32(args, argpos(&argc, vm_size));
	debugFILE("Buff: %08x[%d], File: %x", buff, len, file);
	if (feof(file)) {
		reti32(args, -1);
	}
	else {
		long pos = ftell(file);
		char *unused = fgets(buff, len, file);
		reti32(args, ftell(file) - pos);
		(void)unused;
	}
	return noError;
}

static vmError FILE_putc(nfcContext args) {
	size_t argc = 0;
	FILE *file = arghnd(args, argpos(&argc, sizeof(FILE *)));
	int data = argi32(args, argpos(&argc, vm_size));
	debugFILE("Data: %c, File: %x", data, file);
	reti32(args, putc(data, file));
	return noError;
}
static vmError FILE_write(nfcContext args) {      // int write(File &f, uint8 buff[])
	size_t argc = 0;
	FILE *file = arghnd(args, argpos(&argc, sizeof(FILE *)));
	char *buff = argref(args, argpos(&argc, vm_size));
	int len = argi32(args, argpos(&argc, vm_size));
	debugFILE("Buff: %08x[%d], File: %x", buff, len, file);
	len = fwrite(buff, (size_t) len, 1, file);
	reti32(args, len);
	return noError;
}

static vmError FILE_flush(nfcContext args) {
	FILE *file = arghnd(args, 0);
	debugFILE("File: %x", file);
	fflush(file);
	return noError;
}
//#}#endregion

//#{#region system functions (exit, rand, clock, debug)

#if (defined __WATCOMC__) || (defined _MSC_VER) || (defined __WIN32)
#include <Windows.h>
static inline int64_t timeMillis() {
	static const int64_t kTimeEpoc = 116444736000000000LL;
	static const int64_t kTimeScaler = 10000;  // 100 ns to us.

	// Although win32 uses 64-bit integers for representing timestamps,
	// these are packed into a FILETIME structure. The FILETIME
	// structure is just a struct representing a 64-bit integer. The
	// TimeStamp union allows access to both a FILETIME and an integer
	// representation of the timestamp. The Windows timestamp is in
	// 100-nanosecond intervals since January 1, 1601.
	typedef union {
		FILETIME ftime;
		int64_t time;
	} TimeStamp;
	TimeStamp time;
	GetSystemTimeAsFileTime(&time.ftime);
	return (time.time - kTimeEpoc) / kTimeScaler;
}
static inline void sleepMillis(int64_t milliseconds) {
	Sleep(milliseconds);
}
#else
#include <sys/time.h>
static inline uint64_t timeMillis() {
	uint64_t now;
	struct timeval tv;
	gettimeofday(&tv, NULL);
	now = tv.tv_sec * (uint64_t)1000;
	now += tv.tv_usec / (uint64_t)1000;
	return now;
}
static inline void sleepMillis(int64_t milliseconds) {
	struct timespec ts;
	ts.tv_sec = milliseconds / 1000;
	ts.tv_nsec = (milliseconds % 1000) * 1000000;
	nanosleep(&ts, NULL);
}
#endif

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
	reti32(args, (int) time(NULL));
	return noError;
}
static vmError sysClock(nfcContext args) {
	reti32(args, clock());
	return noError;
}
static vmError sysMillis(nfcContext args) {
	reti64(args, timeMillis());
	return noError;
}
static vmError sysMSleep(nfcContext args) {
	sleepMillis(argi64(args, 0));
	return noError;
}

enum {
	raise_abort = -1,   // log and abort execution.
	raise_error = 0,    // log and continue execution.
	raise_warn = 1,     // log and continue execution.
	raise_info = 2,     // log and continue execution.
	raise_debug = 3,    // log and continue execution.
	raise_verbose = 4   // log and continue execution.
};
// void raise(int logLevel, string message, variant inspect, int logTrace);
static vmError sysRaise(nfcContext args) {
	rtContext rt = args->rt;
	char *file = argref(args, 0 * vm_size);
	int line = argi32(args, 1 * vm_size);
	int logLevel = argi32(args, 2 * vm_size);
	char *message = argref(args, 3 * vm_size);
	void *varRef = argref(args, 4 * vm_size);
	symn varType = argref(args, 5 * vm_size);
	int maxTrace = argi32(args, 6 * vm_size);
	int isOutput = 0;

	// logging disabled or log level not reached.
	if (rt->logFile == NULL || logLevel > rt->logLevel) {
		return noError;
	}

	// print valid code position (where the function was invoked).
	if (file != NULL && line > 0) {
		printFmt(rt->logFile, NULL, "%s:%u", file, line);
		isOutput = 1;
	}

	// print the message
	if (message != NULL) {
		if (isOutput) {
			printFmt(rt->logFile, NULL, ": ");
		}
		printFmt(rt->logFile, NULL, "%s", message);
		isOutput = 1;
	}

	// print the value of the object (handy to inspect values).
	if (varType != NULL && varRef != NULL) {
		if (isOutput) {
			printFmt(rt->logFile, NULL, ": ");
		}
		printVal(rt->logFile, NULL, rt, varType, varRef, prSymType, 0);
		isOutput = 1;
	}

	// add a line ending to the output.
	if (isOutput) {
		printFmt(rt->logFile, NULL, "\n");
	}

	// print stack trace skipping this function
	if (rt->dbg != NULL && maxTrace > 0) {
		logTrace(rt->dbg, NULL, 1, 0, maxTrace);
	}

	// abort the execution
	if (logLevel < 0) {
		return executionAborted;
	}

	return noError;
}

static vmError sysTryExec(nfcContext args) {
	rtContext rt = args->rt;
	dbgContext dbg = rt->dbg;
	int argval = argi32(args, 0 * vm_size);
	symn action = argsym(args, 1 * vm_size);
	if (action != NULL) {
		int result;
		#pragma pack(push, 4)
		struct { int32_t ptr; } cbArg = { .ptr = argval };
		#pragma pack(pop)
		if (dbg != NULL) {
			int oldValue = dbg->checked;
			dbg->checked = 1;
			result = invoke(rt, action, NULL, &cbArg, args->extra);
			dbg->checked = oldValue;
		}
		else {
			result = invoke(rt, action, NULL, &cbArg, NULL);
		}
		reti32(args, result);
	}
	return noError;
}

static vmError sysMemMgr(nfcContext rt) {
	void *old = argref(rt, 0);
	int size = argi32(rt, 4);
	void *res = rtAlloc(rt->rt, old, (size_t) size, NULL);
	retref(rt, vmOffset(rt->rt, res));
	return noError;
}
static vmError sysMemCpy(nfcContext rt) {
	void *dest = argref(rt, 0 * vm_size);
	void *src = argref(rt, 1 * vm_size);
	int size = argi32(rt, 2 * vm_size);
	void *res = memcpy(dest, src, (size_t) size);
	retref(rt, vmOffset(rt->rt, res));
	return noError;
}
static vmError sysMemSet(nfcContext rt) {
	void *dest = argref(rt, 0 * vm_size);
	int value = argi32(rt, 1 * vm_size);
	int size = argi32(rt, 2 * vm_size);
	void *res = memset(dest, value, (size_t) size);
	retref(rt, vmOffset(rt->rt, res));
	return noError;
}
//#}#endregion

int ccLibStdc(rtContext rt) {
	ccContext cc = rt->cc;
	symn nsp = NULL;		// namespace
	int err = 0;
	size_t i;

	struct {
		vmError (*fun)(nfcContext);
		char *def;
	}
	bit32[] = {			// 32 bit operations
		{b32zxt, "int32 zxt(int32 value, int32 offs, int32 count)"},
		{b32sxt, "int32 sxt(int32 value, int32 offs, int32 count)"},
	},
	bit64[] = {			// 64 bit operations
		{b64zxt, "int64 zxt(int64 value, int32 offs, int32 count)"},
		{b64sxt, "int64 sxt(int64 value, int32 offs, int32 count)"},
	},
	flt32[] = {		// sin, cos, sqrt, ...
		{f32sin,   "float32 sin(float32 x)"},
		{f32cos,   "float32 cos(float32 x)"},
		{f32tan,   "float32 tan(float32 x)"},
		{f32log,   "float32 log(float32 x)"},
		{f32exp,   "float32 exp(float32 x)"},
		{f32pow,   "float32 pow(float32 x, float32 y)"},
		{f32sqrt,  "float32 sqrt(float32 x)"},
		{f32atan2, "float32 atan2(float32 x, float32 y)"},
	},
	flt64[] = {		// sin, cos, sqrt, ...
		{f64sin,   "float64 sin(float64 x)"},
		{f64cos,   "float64 cos(float64 x)"},
		{f64tan,   "float64 tan(float64 x)"},
		{f64log,   "float64 log(float64 x)"},
		{f64exp,   "float64 exp(float64 x)"},
		{f64pow,   "float64 pow(float64 x, float64 y)"},
		{f64sqrt,  "float64 sqrt(float64 x)"},
		{f64atan2, "float64 atan2(float64 x, float64 y)"},
	},
	misc[] = {			// IO/MEM/EXIT
		{sysExit,		"void exit(int code)"},
		{sysSRand,		"void srand(int seed)"},
		{sysRand,		"int32 rand()"},

		{sysTime,		"int32 time()"},
		{sysClock,		"int32 clock()"},
		{sysMillis,		"int64 millis()"},
		{sysMSleep,		"void sleep(int64 millis)"},
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
		{raise_abort, "abort"},
		{raise_error, "error"},
		{raise_warn, "warn"},
		{raise_info, "info"},
		{raise_debug, "debug"},
		{raise_verbose, "verbose"},
		// trace levels
		{0, "noTrace"},
		{128, "defTrace"},
	};

	for (i = 0; i < lengthOf(constants); i += 1) {
		if (!ccDefInt(rt, constants[i].name, constants[i].value)) {
			err = 1;
			break;
		}
	}

	if (!err && cc->type_var != NULL) {		// debug, trace, assert, fatal, ...
		cc->libc_dbg = ccDefCall(rt, sysRaise, "void raise(int level, char message[*], variant inspect, int maxTrace)");
		cc->libc_dbg_idx = cc->libc_dbg->offs;
		if (cc->libc_dbg == NULL) {
			err = 2;
		}
		if (!err && cc->libc_dbg) {
			enter(cc);
			for (i = 0; i < lengthOf(logLevels); i += 1) {
				if (!ccDefInt(rt, logLevels[i].name, logLevels[i].value)) {
					err = 1;
					break;
				}
			}
			dieif(cc->libc_dbg->fields, ERR_INTERNAL_ERROR);
			cc->libc_dbg->fields = leave(cc, cc->libc_dbg, ATTR_stat | KIND_typ, 0, NULL);
		}
	}

	if (!err && cc->type_ptr != NULL) {		// tryExecute
		if(!ccDefCall(rt, sysTryExec, "int tryExec(pointer args, void action(pointer args))")) {
			err = 2;
		}
	}

	if (!err && cc->type_ptr != NULL) {		// realloc, malloc, free, memset, memcpy
		if(!ccDefCall(rt, sysMemMgr, "pointer memmgr(pointer ptr, int32 size)")) {
			err = 3;
		}
		if(!ccDefCall(rt, sysMemSet, "pointer memset(pointer dest, int value, int32 size)")) {
			err = 3;
		}
		if(!ccDefCall(rt, sysMemCpy, "pointer memcpy(pointer dest, pointer src, int32 size)")) {
			err = 3;
		}
	}

	// System.Exit(int code), ...
	if (!err && (nsp = install(cc, "System", ATTR_stat | ATTR_cnst | KIND_typ | CAST_vid, 0, cc->type_vid, NULL))) {
		enter(cc);
		for (i = 0; i < lengthOf(misc); i += 1) {
			if (!ccDefCall(rt, misc[i].fun, misc[i].def)) {
				err = 4;
				break;
			}
		}
		//~ install(cc, "Arguments", CAST_arr, 0, 0);	// string Args[];
		//~ install(cc, "Environment", KIND_def, 0, 0);	// string Env[string];
		nsp->fields = leave(cc, nsp, ATTR_stat | KIND_typ, 0, NULL);
	}

	// Add extra operations to int32
	if (!err && cc->type_u32 != NULL) {
		enter(cc);
		for (i = 0; i < lengthOf(bit32); i += 1) {
			if (!ccDefCall(rt, bit32[i].fun, bit32[i].def)) {
				err = 5;
				break;
			}
		}
		dieif(cc->type_u32->fields, ERR_INTERNAL_ERROR);
		cc->type_u32->fields = leave(cc, cc->type_u32, ATTR_stat | KIND_typ, 0, NULL);
	}
	// Add extra operations to int64
	if (!err && cc->type_u64 != NULL) {
		enter(cc);
		for (i = 0; i < lengthOf(bit64); i += 1) {
			if (!ccDefCall(rt, bit64[i].fun, bit64[i].def)) {
				err = 5;
				break;
			}
		}
		dieif(cc->type_u64->fields, ERR_INTERNAL_ERROR);
		cc->type_u64->fields = leave(cc, cc->type_u64, ATTR_stat | KIND_typ, 0, NULL);
	}
	// add math functions to float32
	if (!err && rt->cc->type_f32 != NULL) {
		enter(cc);
		for (i = 0; i < lengthOf(flt32); i += 1) {
			if (!ccDefCall(rt, flt32[i].fun, flt32[i].def)) {
				err = 7;
				break;
			}
		}
		dieif(cc->type_f32->fields, ERR_INTERNAL_ERROR);
		cc->type_f32->fields = leave(cc, cc->type_f32, ATTR_stat | KIND_typ, 0, NULL);
	}
	// add math functions to float64
	if (!err && cc->type_f64 != NULL) {
		enter(cc);
		for (i = 0; i < lengthOf(flt64); i += 1) {
			if (!ccDefCall(rt, flt64[i].fun, flt64[i].def)) {
				err = 6;
				break;
			}
		}
		dieif(cc->type_f64->fields, ERR_INTERNAL_ERROR);
		cc->type_f64->fields = leave(cc, cc->type_f64, ATTR_stat | KIND_typ, 0, NULL);
	}
	return err;
}

int ccLibFile(rtContext rt) {
	symn file_nsp = ccDefType(rt, "File", sizeof(FILE*), 0);
	int err = file_nsp == NULL;

	if (file_nsp != NULL) {
		enter(rt->cc);

		err = err || !ccDefCall(rt, FILE_open, proto_file_open);
		err = err || !ccDefCall(rt, FILE_open, proto_file_create);
		err = err || !ccDefCall(rt, FILE_open, proto_file_append);

		err = err || !ccDefCall(rt, FILE_peek, proto_file_peek_byte);
		err = err || !ccDefCall(rt, FILE_getc, proto_file_read_byte);
		err = err || !ccDefCall(rt, FILE_read, proto_file_read_buff);
		err = err || !ccDefCall(rt, FILE_gets, proto_file_read_line);

		err = err || !ccDefCall(rt, FILE_putc, proto_file_write_byte);
		err = err || !ccDefCall(rt, FILE_write, proto_file_write_buff);

		err = err || !ccDefCall(rt, FILE_flush, proto_file_flush);
		err = err || !ccDefCall(rt, FILE_close, proto_file_close);

		err = err || !ccDefCall(rt, FILE_stream, proto_file_get_stdin);
		err = err || !ccDefCall(rt, FILE_stream, proto_file_get_stdout);
		err = err || !ccDefCall(rt, FILE_stream, proto_file_get_stderr);
		err = err || !ccDefCall(rt, FILE_stream, proto_file_get_dbgout);

		file_nsp->fields = leave(rt->cc, file_nsp, ATTR_stat | KIND_typ, 0, NULL);
	}

	return err;
}

/*******************************************************************************
 *   File: file.c
 *   Date: 2017.02.17
 *   Desc: standard file library
 *******************************************************************************
 */

#include "cmpl.h"

static const char *const proto_file = "File";

static inline rtValue nextArg(nfcContext ctx) {
	return ctx->rt->api.nfcReadArg(ctx, ctx->rt->api.nfcNextArg(ctx));
}

static const char *const proto_file_open = "File open(const char path[*])";
static const char *const proto_file_create = "File create(const char path[*])";
static const char *const proto_file_append = "File append(const char path[*])";
static vmError FILE_open(nfcContext ctx) {
	char *name = nextArg(ctx).ref;
	const char *mode = NULL;
	if (ctx->proto == proto_file_open) {
		mode = "r";
	}
	else if (ctx->proto == proto_file_create) {
		mode = "w";
	}
	else if (ctx->proto == proto_file_append) {
		mode = "a";
	}
	FILE *file = fopen(name, mode);
	rethnd(ctx, file);
	return file != NULL ? noError : nativeCallError;
}

static const char *const proto_file_read_byte = "int read(File file)";
static vmError FILE_getc(nfcContext ctx) {
	FILE *file = (FILE *) arghnd(ctx, 0);
	reti32(ctx, fgetc(file));
	return noError;
}

static const char *const proto_file_peek_byte = "int peek(File file)";
static vmError FILE_peek(nfcContext ctx) {
	FILE *file = (FILE *) arghnd(ctx, 0);
	int chr = ungetc(getc(file), file);
	reti32(ctx, chr);
	return noError;
}

static const char *const proto_file_read_buff = "int read(File file, uint8 buff[])";
static vmError FILE_read(nfcContext ctx) {
	FILE *file = (FILE *) nextArg(ctx).ref;
	rtValue buff = nextArg(ctx);
	size_t n = fread(buff.ref, 1, buff.length, file);
	reti32(ctx, n);
	return noError;
}

static const char *const proto_file_read_line = "int readLine(File file, uint8 buff[])";
static vmError FILE_gets(nfcContext ctx) {
	FILE *file = (FILE *) nextArg(ctx).ref;
	rtValue buff = nextArg(ctx);
	if (feof(file)) {
		reti32(ctx, -1);
	}
	else {
		long pos = ftell(file);
		char *unused = fgets((char *) buff.ref, buff.length, file);
		reti32(ctx, ftell(file) - pos);
		(void)unused;
	}
	return noError;
}

static const char *const proto_file_write_byte = "int write(File file, uint8 byte)";
static const char *const proto_file_write_buff = "int write(File file, const uint8 buff[])";
static vmError FILE_write(nfcContext ctx) {
	FILE *file = (FILE *) nextArg(ctx).ref;
	if (ctx->proto == proto_file_write_byte) {
		int data = nextArg(ctx).i32;
		reti32(ctx, putc(data, file));
		return noError;
	}
	if (ctx->proto == proto_file_write_buff) {
		rtValue buff = nextArg(ctx);
		size_t len = fwrite(buff.ref, 1, buff.length, file);
		reti32(ctx, len);
		return noError;
	}
	return nativeCallError;
}

static const char *const proto_file_flush = "void flush(File file)";
static vmError FILE_flush(nfcContext ctx) {
	FILE *file = (FILE *) arghnd(ctx, 0);
	fflush(file);
	return noError;
}

static const char *const proto_file_tell = "int tell(File file)";
static vmError FILE_tell(nfcContext ctx) {
	FILE *file = (FILE *) arghnd(ctx, 0);
	size_t pos = ftell(file);
	reti32(ctx, pos);
	return noError;
}

static const char *const proto_file_close = "void close(File file)";
static vmError FILE_close(nfcContext ctx) {
	FILE *file = (FILE *) arghnd(ctx, 0);
	/* TODO: currently it is allowed to close any file
	if (file == stdin || file == stdout || file == stderr) {
		return nativeCallError;
	}
	if (file == ctx->rt->logFile) {
		return nativeCallError;
	}*/
	fclose(file);
	return noError;
}

static const char *const proto_file_get_stdIn = "File in";
static const char *const proto_file_get_stdOut = "File out";
static const char *const proto_file_get_stdErr = "File err";
static const char *const proto_file_get_logOut = "File log";
static vmError FILE_stream(nfcContext ctx) {
	if (ctx->proto == proto_file_get_stdIn) {
		rethnd(ctx, stdin);
		return noError;
	}
	if (ctx->proto == proto_file_get_stdOut) {
		rethnd(ctx, stdout);
		return noError;
	}
	if (ctx->proto == proto_file_get_stdErr) {
		rethnd(ctx, stderr);
		return noError;
	}
	if (ctx->proto == proto_file_get_logOut) {
		rethnd(ctx, ctx->rt->logFile);
		return noError;
	}

	return nativeCallError;
}

int cmplInit(rtContext rt) {
	ccContext cc = rt->cc;
	symn type = rt->api.ccAddType(cc, proto_file, sizeof(FILE*), 0);
	int err = type == NULL;

	if (rt->api.ccExtend(cc, type)) {

		err = err || !rt->api.ccAddCall(cc, FILE_open, proto_file_open);
		err = err || !rt->api.ccAddCall(cc, FILE_open, proto_file_create);
		err = err || !rt->api.ccAddCall(cc, FILE_open, proto_file_append);

		err = err || !rt->api.ccAddCall(cc, FILE_getc, proto_file_read_byte);
		err = err || !rt->api.ccAddCall(cc, FILE_peek, proto_file_peek_byte);
		err = err || !rt->api.ccAddCall(cc, FILE_read, proto_file_read_buff);
		err = err || !rt->api.ccAddCall(cc, FILE_gets, proto_file_read_line);

		err = err || !rt->api.ccAddCall(cc, FILE_write, proto_file_write_byte);
		err = err || !rt->api.ccAddCall(cc, FILE_write, proto_file_write_buff);

		err = err || !rt->api.ccAddCall(cc, FILE_flush, proto_file_flush);
		err = err || !rt->api.ccAddCall(cc, FILE_close, proto_file_close);
		err = err || !rt->api.ccAddCall(cc, FILE_tell, proto_file_tell);

		err = err || !rt->api.ccAddCall(cc, FILE_stream, proto_file_get_stdIn);
		err = err || !rt->api.ccAddCall(cc, FILE_stream, proto_file_get_stdOut);
		err = err || !rt->api.ccAddCall(cc, FILE_stream, proto_file_get_stdErr);
		err = err || !rt->api.ccAddCall(cc, FILE_stream, proto_file_get_logOut);

		rt->api.ccEnd(cc, type);
	}

	return err;
}

/*******************************************************************************
 *   File: file.c
 *   Date: 2017.02.17
 *   Desc: standard file library
********************************************************************************
gcc -shared -fPIC -Wall -g0 -Os -o file.so file.c -I ../src/
*******************************************************************************/

#include "cmpl.h"

static const char *const proto_file = "File";
static const char *const proto_file_open = "File open(char path[*])";
static const char *const proto_file_create = "File create(char path[*])";
static const char *const proto_file_append = "File append(char path[*])";

static const char *const proto_file_peek_byte = "int peek(File file)";
static const char *const proto_file_read_byte = "int read(File file)";
static const char *const proto_file_read_buff = "int read(File file, uint8 buff[])";
static const char *const proto_file_read_line = "int readLine(File file, uint8 buff[])";

static const char *const proto_file_write_byte = "int write(File file, uint8 byte)";
static const char *const proto_file_write_buff = "int write(File file, uint8 buff[])";

static const char *const proto_file_flush = "void flush(File file)";
static const char *const proto_file_close = "void close(File file)";

static const char *const proto_file_get_stdIn = "File in";
static const char *const proto_file_get_stdOut = "File out";
static const char *const proto_file_get_stdErr = "File err";
static const char *const proto_file_get_logOut = "File log";

#define debugFILE(msg, ...) do { /*prerr("debug", msg, ##__VA_ARGS__);*/ } while(0)

static inline rtValue nextArg(nfcContext ctx) {
	return ctx->rt->api.nfcReadArg(ctx, ctx->rt->api.nfcNextArg(ctx));
}

static vmError FILE_open(nfcContext ctx) {       // File open(char filename[]);
	char *name = nextArg(ctx).data;
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
	debugFILE("Name: '%s', Mode: '%s', File: %x", name, mode, file);
	return file != NULL ? noError : nativeCallError;
}
static vmError FILE_close(nfcContext ctx) {      // void close(File file);
	FILE *file = (FILE *) arghnd(ctx, 0);
	debugFILE("File: %x", file);
	fclose(file);
	return noError;
}
static vmError FILE_stream(nfcContext ctx) {     // File std[in, out, err];
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

	debugFILE("error opening stream: %x", ctx->proto);
	return executionAborted;
}

static vmError FILE_getc(nfcContext ctx) {
	FILE *file = (FILE *) arghnd(ctx, 0);
	reti32(ctx, fgetc(file));
	return noError;
}
static vmError FILE_peek(nfcContext ctx) {
	FILE *file = (FILE *) arghnd(ctx, 0);
	int chr = ungetc(getc(file), file);
	reti32(ctx, chr);
	return noError;
}
static vmError FILE_read(nfcContext ctx) {         // int read(File &f, uint8 buff[])
	FILE *file = (FILE *) nextArg(ctx).data;
	rtValue buff = nextArg(ctx);
	reti32(ctx, fread(buff.data, buff.length, 1, file));
	return noError;
}
static vmError FILE_gets(nfcContext ctx) {       // int fgets(File &f, uint8 buff[])
	FILE *file = (FILE *) nextArg(ctx).data;
	rtValue buff = nextArg(ctx);
	debugFILE("Buff: %08x[%d], File: %x", buff.data, buff.length, file);
	if (feof(file)) {
		reti32(ctx, -1);
	}
	else {
		long pos = ftell(file);
		char *unused = fgets((char *) buff.data, buff.length, file);
		reti32(ctx, ftell(file) - pos);
		(void)unused;
	}
	return noError;
}

static vmError FILE_putc(nfcContext ctx) {
	FILE *file = (FILE *) nextArg(ctx).data;
	int data = nextArg(ctx).i32;
	debugFILE("Data: %c, File: %x", data, file);
	reti32(ctx, putc(data, file));
	return noError;
}
static vmError FILE_write(nfcContext ctx) {      // int write(File &f, uint8 buff[])
	FILE *file = (FILE *) nextArg(ctx).data;
	rtValue buff = nextArg(ctx);
	debugFILE("Buff: %08x[%d], File: %x", buff.data, buff.length, file);
	int len = fwrite(buff.data, buff.length, 1, file);
	reti32(ctx, len);
	return noError;
}

static vmError FILE_flush(nfcContext ctx) {
	FILE *file = (FILE *) arghnd(ctx, 0);
	debugFILE("File: %x", file);
	fflush(file);
	return noError;
}

int cmplInit(rtContext rt) {
	ccContext cc = rt->cc;
	symn type = rt->api.ccDefType(cc, proto_file, sizeof(FILE*), 0);
	int err = type == NULL;

	if (rt->api.ccExtend(cc, type)) {

		err = err || !rt->api.ccDefCall(cc, FILE_open, proto_file_open);
		err = err || !rt->api.ccDefCall(cc, FILE_open, proto_file_create);
		err = err || !rt->api.ccDefCall(cc, FILE_open, proto_file_append);

		err = err || !rt->api.ccDefCall(cc, FILE_peek, proto_file_peek_byte);
		err = err || !rt->api.ccDefCall(cc, FILE_getc, proto_file_read_byte);
		err = err || !rt->api.ccDefCall(cc, FILE_read, proto_file_read_buff);
		err = err || !rt->api.ccDefCall(cc, FILE_gets, proto_file_read_line);

		err = err || !rt->api.ccDefCall(cc, FILE_putc, proto_file_write_byte);
		err = err || !rt->api.ccDefCall(cc, FILE_write, proto_file_write_buff);

		err = err || !rt->api.ccDefCall(cc, FILE_flush, proto_file_flush);
		err = err || !rt->api.ccDefCall(cc, FILE_close, proto_file_close);

		err = err || !rt->api.ccDefCall(cc, FILE_stream, proto_file_get_stdIn);
		err = err || !rt->api.ccDefCall(cc, FILE_stream, proto_file_get_stdOut);
		err = err || !rt->api.ccDefCall(cc, FILE_stream, proto_file_get_stdErr);
		err = err || !rt->api.ccDefCall(cc, FILE_stream, proto_file_get_logOut);

		rt->api.ccEnd(cc, type);
	}

	return err;
}

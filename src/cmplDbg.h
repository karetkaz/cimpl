/**
 * Debugger core functions.
 */

#ifndef CMPL_DEBUGGER_H
#define CMPL_DEBUGGER_H

/// Dynamic array
struct arrBuffer {
	char *ptr;         // data
	size_t esz;        // element size
	size_t cap;        // capacity
	size_t cnt;        // allocated size
};

int initBuff(struct arrBuffer *buff, size_t initCount, size_t elemSize);
void freeBuff(struct arrBuffer *buff);
static inline void *getBuff(struct arrBuffer *buff, size_t idx) {
	size_t pos = idx * buff->esz;
	if (pos >= buff->cap || buff->ptr == NULL) {
		return NULL;
	}
	return buff->ptr + pos;
}

/// Break point actions
typedef enum {
	brkSkip = 0x00,     // do nothing
	brkPrint = 0x01,    // print when hit
	brkTrace = 0x02,    // trace when hit
	brkPause = 0x04,    // pause when hit
	brkOnce = 0x10,     // one shoot breakpoint (disabled after first hit)
} brkMode;

/// Debug info node
struct dbgNode {
	// the statement tree
	astn stmt;
	// the declared function
	symn func;

	// position in source file
	char *file;
	int line;

	// position in byte code
	size_t start, end;

	// break point
	brkMode bp;

	// profile data
	int64_t total, self;  // time spent executing function / statement
	int64_t hits, exec;  // hit count and successful executions
};

/**
 * Check if the current stacktrace contains tryExec, if an error can terminate the execution of the vm.
 * @param ctx debug context
 * @return true or false
 */
int isChecked(dbgContext ctx);

/**
 * Convert an error code to message (string).
 * 
 * @param error the error code
 * @return the error message for the given code
 */
char* vmErrorMessage(vmError error);

/**
 * Request debug information of function
 * @param ctx runtime context
 * @param position 
 * @return 
 */
dbgn mapDbgFunction(rtContext ctx, size_t position);
dbgn addDbgFunction(rtContext ctx, symn fun);

dbgn mapDbgStatement(rtContext ctx, size_t position);
dbgn getDbgStatement(rtContext ctx, char *file, int line);
dbgn addDbgStatement(rtContext ctx, size_t start, size_t end, astn tag);

#endif

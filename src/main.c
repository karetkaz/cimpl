/*******************************************************************************
 *   File: lstd.c
 *   Date: 2011/06/23
 *   Desc: command line executable
 *******************************************************************************
 * command line argument parsing
 * console debugger
 * json / text dump
 */

#include "internal.h"
#include <time.h>

// default values
static const char *STDLIB = "lib/stdlib.ci";   // standard library

static inline int strEquals(const char *str, const char *with) {
	if (str == NULL || with == NULL) {
		return str == with;
	}
	return strcmp(str, with) == 0;
}
static inline int strBegins(const char *str, const char *with) {
	return strncmp(str, with, strlen(with)) == 0;
}

static const char *parseInt(const char *str, int32_t *res, int radix) {
	int64_t result = 0;
	int sign = 1;

	//~ ('+' | '-')?
	switch (*str) {
		case '-':
			sign = -1;
			// fall through
		case '+':
			str += 1;
			// fall through
		default:
			break;
	}

	if (radix == 0) {		// auto detect
		radix = 10;
		if (*str == '0') {
			str += 1;

			switch (*str) {

				default:
					radix = 8;
					break;

				case 'b':
				case 'B':
					radix = 2;
					str += 1;
					break;

				case 'o':
				case 'O':
					radix = 8;
					str += 1;
					break;

				case 'x':
				case 'X':
					radix = 16;
					str += 1;
					break;
			}
		}
	}

	//~ ([0-9])*
	while (*str != 0) {
		int value = *str;
		if (value >= '0' && value <= '9') {
			value -= '0';
		}
		else if (value >= 'a' && value <= 'z') {
			value -= 'a' - 10;
		}
		else if (value >= 'A' && value <= 'Z') {
			value -= 'A' - 10;
		}
		else {
			break;
		}

		if (value > radix) {
			break;
		}

		result *= radix;
		result += value;
		str += 1;
	}

	*res = (int32_t)(sign * result);

	return str;
}

static const char **escapeXML() {
	static const char *escape[256];
	static char initialized = 0;
	if (!initialized) {
		memset((void*)escape, 0, sizeof(escape));
		escape['\''] = "&apos;";
		escape['"'] = "&quot;";
		escape['&'] = "&amp;";
		escape['<'] = "&lt;";
		escape['>'] = "&gt;";
		initialized = 1;
	}
	return escape;
}
static const char **escapeJSON() {
	static const char *escape[256];
	static char initialized = 0;
	if (!initialized) {
		memset((void*)escape, 0, sizeof(escape));
		escape['\n'] = "\\n";
		escape['\r'] = "\\r";
		escape['\t'] = "\\t";
		//~ escape['\''] = "\\'";
		escape['\"'] = "\\\"";
		initialized = 1;
	}
	return escape;
}

// Debug commands
typedef enum {
	dbgAbort = 'q',		// stop debugging
	dbgResume = 'r',	// break on next breakpoint or error

	dbgStepNext = 'a',	// break on next instruction
	dbgStepLine = 'n',	// break on next statement
	dbgStepInto = 'i',	// break on next function
	dbgStepOut = 'o',	// break on next return

	dbgPrintStackTrace = 't',	// print stack trace
	dbgPrintStackValues = 's',	// print values on sack
	dbgPrintInstruction = 'p',	// print current instruction
} dbgMode;

// Profile commands
typedef enum {
	trcMethods = 0x0010,     // dump call tree
	trcOpcodes = 0x0020,     // dump executing instructions
	trcLocals  = 0x0040,     // dump the content of the stack
	trcTime    = 0x0080,     // dump timestamp
} runMode;

struct userContextRec {
	rtContext rt;
	FILE *in;           // debugger input

	FILE *out;          // dump file
	int indent;         // dump indentation
	const char **esc;   // escape characters

	int hasOut;

	dmpMode dmpApi;     // dump symbols
	dmpMode dmpAst;     // dump syntax tree
	dmpMode dmpAsm;     // dump operation codes
	//TODO: dmpDoc:     // dump documentation

	int dmpMain;        // include main initializer
	int dmpApiAll;      // include builtin symbols
	int dmpApiInfo;     // dump detailed info
	int dmpApiPrms;     // dump parameters and fields
	int dmpApiUsed;     // dump usages
	int dmpAsmStmt;     // print source code statements
	int hideOffsets;    // remove offsets from dumps
	int dmpTimes;      // show compilation and runtime
	char *compileSteps; // dump compilation steps

	int dmpGlobals;     // dump global variables after execution
	int dmpProfile;     // dump profile and coverage stats
	int dmpMemory;      // dump memory allocation free and used chunks

	runMode exec;
	clock_t ccStart;       // start time of compilation
	clock_t rtStart;       // start time of execution

	// debugger
	dbgMode dbgCommand; // last debugger command
	brkMode dbgOnError; // on uncaught exception
	brkMode dbgOnCaught;// on caught exception
	size_t dbgNextBreak;// offset of the next break (used for step in, out, over, instruction)
};

static void dumpAstXML(FILE *out, const char **esc, astn ast, dmpMode mode, int indent, const char *text) {
	if (ast == NULL) {
		return;
	}

	printFmt(out, esc, "%I<%s token=\"%k\"", indent, text, ast->kind);

	if ((mode & (prSymType|prAstType)) != 0) {
		printFmt(out, esc, " type=\"%T\"", ast->type);
		if (ast->type != NULL) {
			printFmt(out, esc, " kind=\"%K\"", ast->type->kind);
		}
	}

	if (ast->kind == STMT_beg && ast->file != NULL && ast->line > 0) {
		printFmt(out, esc, " file=\"%s:%d\"", ast->file, ast->line);
	}

	switch (ast->kind) {
		default:
			fatal(ERR_INTERNAL_ERROR);
			return;

		//#{ STATEMENTS
		case STMT_beg:
			printFmt(out, esc, ">\n");
			for (astn list = ast->stmt.stmt; list; list = list->next) {
				dumpAstXML(out, esc, list, mode, indent + 1, "stmt");
			}
			printFmt(out, esc, "%I</%s>\n", indent, text);
			break;

		case STMT_end:
			printFmt(out, esc, " stmt=\"%?t\">\n", ast);
			dumpAstXML(out, esc, ast->stmt.stmt, mode, indent + 1, "expr");
			printFmt(out, esc, "%I</%s>\n", indent, text);
			break;

		case STMT_if:
		case STMT_sif:
			printFmt(out, esc, " stmt=\"%?t\">\n", ast);
			dumpAstXML(out, esc, ast->stmt.test, mode, indent + 1, "test");
			dumpAstXML(out, esc, ast->stmt.stmt, mode, indent + 1, "then");
			dumpAstXML(out, esc, ast->stmt.step, mode, indent + 1, "else");
			printFmt(out, esc, "%I</%s>\n", indent, text);
			break;

		case STMT_for:
			printFmt(out, esc, " stmt=\"%?t\">\n", ast);
			dumpAstXML(out, esc, ast->stmt.init, mode, indent + 1, "init");
			dumpAstXML(out, esc, ast->stmt.test, mode, indent + 1, "test");
			dumpAstXML(out, esc, ast->stmt.step, mode, indent + 1, "step");
			dumpAstXML(out, esc, ast->stmt.stmt, mode, indent + 1, "stmt");
			printFmt(out, esc, "%I</%s>\n", indent, text);
			break;

		case STMT_con:
		case STMT_brk:
		case STMT_ret:
			if (ast->jmp.value != NULL) {
				printFmt(out, esc, " stmt=\"%?t\">\n", ast);
				dumpAstXML(out, esc, ast->jmp.value, mode & ~prSymInit, indent + 1, "expr");
				printFmt(out, esc, "%I</%s>\n", indent, text);
			}
			else {
				printFmt(out, esc, " stmt=\"%?t\" />\n", ast);
			}
			break;

		//#}
		//#{ OPERATORS
		case OPER_fnc:		// '()'
			printFmt(out, esc, " value=\"%?t\">\n", ast);
			for (astn list = chainArgs(ast->op.rhso); list != NULL; list = list->next) {
				dumpAstXML(out, esc, list, mode, indent + 1, "push");
			}
			dumpAstXML(out, esc, ast->op.lhso, mode & ~prSymInit, indent + 1, "call");
			printFmt(out, esc, "%I</%s>\n", indent, text);
			break;

		case OPER_dot:		// '.'
		case OPER_idx:		// '[]'

		case OPER_pls:		// '+'
		case OPER_mns:		// '-'
		case OPER_cmt:		// '~'
		case OPER_not:		// '!'

		case OPER_add:		// '+'
		case OPER_sub:		// '-'
		case OPER_mul:		// '*'
		case OPER_div:		// '/'
		case OPER_mod:		// '%'

		case OPER_shl:		// '>>'
		case OPER_shr:		// '<<'
		case OPER_and:		// '&'
		case OPER_ior:		// '|'
		case OPER_xor:		// '^'

		case OPER_ceq:		// '=='
		case OPER_cne:		// '!='
		case OPER_clt:		// '<'
		case OPER_cle:		// '<='
		case OPER_cgt:		// '>'
		case OPER_cge:		// '>='

		case OPER_all:		// '&&'
		case OPER_any:		// '||'
		case OPER_sel:		// '?:'

		case OPER_com:		// ','

		case INIT_set:		// '='
		case ASGN_set:		// '='
			printFmt(out, esc, " value=\"%?t\">\n", ast);
			dumpAstXML(out, esc, ast->op.test, mode & ~prSymInit, indent + 1, "test");
			dumpAstXML(out, esc, ast->op.lhso, mode & ~prSymInit, indent + 1, "left");
			dumpAstXML(out, esc, ast->op.rhso, mode & ~prSymInit, indent + 1, "right");
			printFmt(out, esc, "%I</%s>\n", indent, text);
			break;

		//#}
		//#{ VALUES
		case TOKEN_var: {
			symn link = ast->ref.link;
			// declaration
			if (link && link->init && (mode & prSymInit)) {
				printFmt(out, esc, " value=\"%?t\">\n", ast);
				dumpAstXML(out, esc, link->init, mode, indent + 1, "init");
				printFmt(out, esc, "%I</%s>\n", indent, text);
				break;
			}
		}
		// fall through

		case TOKEN_opc:
		case TOKEN_val:
			printFmt(out, esc, " value=\"%?t\" />\n", ast);
			break;

		//#}
	}
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ json output
static char *const JSON_KEY_VERSION = "version";
static char *const JSON_KEY_SYMBOLS = "symbols";
static char *const JSON_KEY_PROFILE = "profile";

static char *const JSON_OBJ_ARR_START = "%I, \"%s\": [{\n";
static char *const JSON_OBJ_START = "%I, \"%s\": {\n";
static char *const JSON_ARR_START = "%I, \"%s\": [\n";
static char *const JSON_OBJ_NEXT = "%I}, {\n";
static char *const JSON_OBJ_ARR_END = "%I}]\n";
static char *const JSON_OBJ_END = "%I}\n";
static char *const JSON_ARR_END = "%I]\n";

static char *const JSON_KEY_FILE = "file";
static char *const JSON_KEY_LINE = "line";
static char *const JSON_KEY_NAME = "name";
static char *const JSON_KEY_OFFS = "offs";
static char *const JSON_KEY_SIZE = "size";

static void jsonDumpSym(FILE *out, const char **esc, symn ptr, const char *kind, int indent) {
	static char *const JSON_KEY_PROTO = "";
	static char *const JSON_KEY_KIND = "kind";
	static char *const JSON_KEY_CAST = "cast";
	static char *const JSON_KEY_OWNER = "owner";
	static char *const JSON_KEY_TYPE = "type";
	static char *const JSON_KEY_ARGS = "args";
	static char *const JSON_KEY_CNST = "const";
	static char *const JSON_KEY_STAT = "static";

	static char *const JSON_VAL_TRUE = "true";
	static char *const JSON_VAL_FALSE = "false";

	if (ptr == NULL) {
		return;
	}
	if (kind != NULL) {
		printFmt(out, esc, JSON_OBJ_START, indent - 1, kind, ptr);
	}

	printFmt(out, esc, "%I\"%s\": \"%T\"\n", indent, JSON_KEY_PROTO, ptr);
	printFmt(out, esc, "%I, \"%s\": \"%K\"\n", indent, JSON_KEY_KIND, ptr->kind & MASK_kind);
	printFmt(out, esc, "%I, \"%s\": \"%K\"\n", indent, JSON_KEY_CAST, ptr->kind & MASK_cast);
	printFmt(out, esc, "%I, \"%s\": \"%.T\"\n", indent, JSON_KEY_NAME, ptr);
	if (ptr->owner != NULL) {
		printFmt(out, esc, "%I, \"%s\": \"%T\"\n", indent, JSON_KEY_OWNER, ptr->owner);
	}
	if (ptr->type != NULL) {
		printFmt(out, esc, "%I, \"%s\": \"%T\"\n", indent, JSON_KEY_TYPE, ptr->type);
	}
	if (ptr->file != NULL && ptr->line != 0) {
		printFmt(out, esc, "%I, \"%s\": \"%s\"\n", indent, JSON_KEY_FILE, ptr->file);
		printFmt(out, esc, "%I, \"%s\": %u\n", indent, JSON_KEY_LINE, ptr->line);
	}

	if (isInvokable(ptr)) {
		printFmt(out, NULL, JSON_OBJ_ARR_START, indent, JSON_KEY_ARGS);
		for (symn arg = ptr->params; arg; arg = arg->next) {
			if (arg != ptr->params) {
				printFmt(out, NULL, JSON_OBJ_NEXT, indent);
			}
			jsonDumpSym(out, esc, arg, NULL, indent + 1);
		}
		printFmt(out, NULL, JSON_OBJ_ARR_END, indent);
	}
	printFmt(out, esc, "%I, \"%s\": %u\n", indent, JSON_KEY_SIZE, ptr->size);
	printFmt(out, esc, "%I, \"%s\": %u\n", indent, JSON_KEY_OFFS, ptr->offs);
	printFmt(out, esc, "%I, \"%s\": %s\n", indent, JSON_KEY_STAT, isStatic(ptr) ? JSON_VAL_TRUE : JSON_VAL_FALSE);
	printFmt(out, esc, "%I, \"%s\": %s\n", indent, JSON_KEY_CNST, isConst(ptr) ? JSON_VAL_TRUE : JSON_VAL_FALSE);
	if (kind != NULL) {
		printFmt(out, esc, JSON_OBJ_END, indent - 1);
	}
}
static void jsonDumpAst(FILE *out, const char **esc, astn ast, const char *kind, int indent) {
	static char *const JSON_KEY_PROTO = "";
	static char *const JSON_KEY_KIND = "kind";
	static char *const JSON_KEY_TYPE = "type";
	static char *const JSON_KEY_STMT = "stmt";
	static char *const JSON_KEY_INIT = "init";
	static char *const JSON_KEY_TEST = "test";
	static char *const JSON_KEY_THEN = "then";
	static char *const JSON_KEY_STEP = "step";
	static char *const JSON_KEY_ELSE = "else";
	static char *const JSON_KEY_ARGS = "args";
	static char *const JSON_KEY_LHSO = "left";
	static char *const JSON_KEY_RHSO = "right";
	static char *const JSON_KEY_VALUE = "value";

	if (ast == NULL) {
		return;
	}
	if (kind != NULL) {
		printFmt(out, esc, JSON_OBJ_START, indent, kind);
	}

	printFmt(out, esc, "%I\"%s\": \"%t\"\n", indent + 1, JSON_KEY_PROTO, ast);

	printFmt(out, esc, "%I, \"%s\": \"%k\"\n", indent + 1, JSON_KEY_KIND, ast->kind);
	if (ast->type != NULL) {
		printFmt(out, esc, "%I, \"%s\": \"%T\"\n", indent + 1, JSON_KEY_TYPE, ast->type);
	}
	if (ast->file != NULL) {
		printFmt(out, esc, "%I, \"%s\": \"%s\"\n", indent + 1, JSON_KEY_FILE, ast->file);
	}
	if (ast->line != 0) {
		printFmt(out, esc, "%I, \"%s\": %u\n", indent + 1, JSON_KEY_LINE, ast->line);
	}
	switch (ast->kind) {
		default:
			fatal(ERR_INTERNAL_ERROR);
			return;

		//#{ STATEMENTS
		case STMT_beg: {
			astn list;
			printFmt(out, esc, JSON_OBJ_ARR_START, indent + 1, JSON_KEY_STMT);
			for (list = ast->stmt.stmt; list; list = list->next) {
				if (list != ast->stmt.stmt) {
					printFmt(out, esc, JSON_OBJ_NEXT, indent + 1, list);
				}
				jsonDumpAst(out, esc, list, NULL, indent + 1);
			}
			printFmt(out, esc, JSON_OBJ_ARR_END, indent + 1);
			break;
		}

		case STMT_if:
		case STMT_sif:
			jsonDumpAst(out, esc, ast->stmt.test, JSON_KEY_TEST, indent + 1);
			jsonDumpAst(out, esc, ast->stmt.stmt, JSON_KEY_THEN, indent + 1);
			jsonDumpAst(out, esc, ast->stmt.step, JSON_KEY_ELSE, indent + 1);
			break;

		case STMT_for:
			jsonDumpAst(out, esc, ast->stmt.init, JSON_KEY_INIT, indent + 1);
			jsonDumpAst(out, esc, ast->stmt.test, JSON_KEY_TEST, indent + 1);
			jsonDumpAst(out, esc, ast->stmt.step, JSON_KEY_STEP, indent + 1);
			jsonDumpAst(out, esc, ast->stmt.stmt, JSON_KEY_STMT, indent + 1);
			break;

		case STMT_con:
		case STMT_brk:
			break;

		case STMT_end:
		case STMT_ret:
			jsonDumpAst(out, esc, ast->jmp.value, JSON_KEY_STMT, indent + 1);
			break;
		//#}
		//#{ OPERATORS
		case OPER_fnc: {    // '()'
			astn args = chainArgs(ast->op.rhso);
			printFmt(out, esc, JSON_OBJ_ARR_START, indent + 1, JSON_KEY_ARGS);
			while (args != NULL) {
				if (args != ast->op.rhso) {
					printFmt(out, esc, JSON_OBJ_NEXT, indent + 1);
				}
				jsonDumpAst(out, esc, args, NULL, indent + 1);
				args = args->next;
			}
			printFmt(out, esc, JSON_OBJ_ARR_END, indent + 1);
			break;
		}

		case OPER_dot:		// '.'
		case OPER_idx:		// '[]'

		case OPER_pls:		// '+'
		case OPER_mns:		// '-'
		case OPER_cmt:		// '~'
		case OPER_not:		// '!'

		case OPER_add:		// '+'
		case OPER_sub:		// '-'
		case OPER_mul:		// '*'
		case OPER_div:		// '/'
		case OPER_mod:		// '%'

		case OPER_shl:		// '>>'
		case OPER_shr:		// '<<'
		case OPER_and:		// '&'
		case OPER_ior:		// '|'
		case OPER_xor:		// '^'

		case OPER_ceq:		// '=='
		case OPER_cne:		// '!='
		case OPER_clt:		// '<'
		case OPER_cle:		// '<='
		case OPER_cgt:		// '>'
		case OPER_cge:		// '>='

		case OPER_all:		// '&&'
		case OPER_any:		// '||'
		case OPER_sel:		// '?:'

		case OPER_com:		// ','

		case INIT_set:		// '='
		case ASGN_set:		// '='
			jsonDumpAst(out, esc, ast->op.test, JSON_KEY_TEST, indent + 1);
			jsonDumpAst(out, esc, ast->op.lhso, JSON_KEY_LHSO, indent + 1);
			jsonDumpAst(out, esc, ast->op.rhso, JSON_KEY_RHSO, indent + 1);
			break;
		//#}
		//#{ VALUES
		case TOKEN_opc:
		case TOKEN_val:
		case TOKEN_var:
			printFmt(out, esc, "%I, \"%s\": \"%t\"\n", indent + 1, JSON_KEY_VALUE, ast);
			break;
		//#}
	}

	if (kind != NULL) {
		printFmt(out, esc, JSON_OBJ_END, indent);
	}
}
static void jsonDumpAsm(FILE *out, const char **esc, symn sym, rtContext rt, int indent) {
	static char *const JSON_KEY_OPC = "instruction";
	static char *const JSON_KEY_CODE = "code";

	size_t end = sym->offs + sym->size;
	for (size_t pc = sym->offs; pc < end; ) {
		unsigned char *ip = vmPointer(rt, pc);
		if (pc != sym->offs) {
			printFmt(out, esc, JSON_OBJ_NEXT, indent, indent);
		}
		printFmt(out, esc, "%I\"%s\": \"%.A\"\n", indent + 1, JSON_KEY_OPC, ip);
		printFmt(out, esc, "%I, \"%s\": \"%s\"\n", indent + 1, JSON_KEY_NAME, opcode_tbl[*ip].name);
		printFmt(out, esc, "%I, \"%s\": \"0x%02x\"\n", indent + 1, JSON_KEY_CODE, opcode_tbl[*ip].code);
		printFmt(out, esc, "%I, \"%s\": %u\n", indent + 1, JSON_KEY_OFFS, pc);
		printFmt(out, esc, "%I, \"%s\": %u\n", indent + 1, JSON_KEY_SIZE, opcode_tbl[*ip].size);
		pc += opcode_tbl[*ip].size;
		if (ip == vmPointer(rt, pc)) {
			break;
		}
	}
}
static void dumpApiJSON(userContext ctx, symn sym) {
	static char *const JSON_KEY_ASM = "asm";
	static char *const JSON_KEY_AST = "ast";

	FILE *out = ctx->out;
	int indent = ctx->indent;
	const char **esc = ctx->esc;

	if (ctx->dmpApi == prSkip && ctx->dmpAsm == prSkip && ctx->dmpAst == prSkip) {
		return;
	}

	if (sym != ctx->rt->main->fields) {
		// not the first symbol
		printFmt(out, esc, JSON_OBJ_NEXT, indent);
	}

	jsonDumpSym(out, esc, sym, NULL, indent + 1);

	// export valid syntax tree if we still have compiler context
	if (ctx->dmpAst != prSkip && ctx->rt->cc && sym->init) {
		jsonDumpAst(out, esc, sym->init, JSON_KEY_AST, indent + 1);
	}

	// export assembly
	if (ctx->dmpAsm != prSkip && isFunction(sym)) {
		printFmt(out, esc, JSON_OBJ_ARR_START, indent + 1, JSON_KEY_ASM);
		jsonDumpAsm(out, esc, sym, ctx->rt, indent + 1);
		printFmt(out, esc, JSON_OBJ_ARR_END, indent + 1);
	}
}

static dbgn jsonProfile(dbgContext ctx, vmError error, size_t ss, void *stack, size_t caller, size_t callee) {
	if (callee != 0) {
		clock_t ticks = clock();
		userContext cc = ctx->extra;
		FILE *out = cc->out;
		const char **esc = cc->esc;
		if ((ssize_t)callee < 0) {
			printFmt(out, esc, "% I%d,%d,%d%s\n", ss, ticks, ctx->usedMem, -1, ss > 0 ? "," : "");
		} else {
			printFmt(out, esc, "% I%d,%d,%d%s\n", ss, ticks, ctx->usedMem, (vmOffs) callee, ",");
		}
	}
	(void)caller;
	(void)error;
	(void)stack;
	return NULL;
}
static void jsonPostProfile(dbgContext ctx) {
	static char *const JSON_KEY_FUNC = "functions";
	static char *const JSON_KEY_STMT = "statements";
	static char *const JSON_KEY_TIME = "time";
	static char *const JSON_KEY_TOTAL = "total";
	static char *const JSON_KEY_HITS = "hits";
	static char *const JSON_KEY_FAILS = "fails";

	userContext cc = ctx->extra;
	FILE *out = cc->out;
	const int indent = cc->indent;
	const char **esc = cc->esc;

	size_t covFunc = 0, nFunc = ctx->functions.cnt;
	size_t covStmt = 0, nStmt = ctx->statements.cnt;
	dbgn dbg = (dbgn) ctx->functions.ptr;

	printFmt(out, esc, JSON_ARR_END, indent + 1);

	printFmt(out, esc, JSON_OBJ_ARR_START, indent + 1, JSON_KEY_FUNC);
	for (size_t i = 0; i < nFunc; ++i, dbg++) {
		symn sym = dbg->func;
		if (dbg->hits == 0) {
			// skip functions not invoked
			continue;
		}
		covFunc += 1;
		if (sym == NULL) {
			sym = rtLookup(ctx->rt, dbg->start, 0);
		}
		if (covFunc > 1) {
			printFmt(out, esc, JSON_OBJ_NEXT, indent + 1);
		}
		jsonDumpSym(out, esc, sym, NULL, indent + 2);
		printFmt(out, esc, "%I, \"%s\": %d\n", indent + 2, JSON_KEY_TIME, dbg->self);
		printFmt(out, esc, "%I, \"%s\": %d\n", indent + 2, JSON_KEY_TOTAL, dbg->total);
		printFmt(out, esc, "%I, \"%s\": %d\n", indent + 2, JSON_KEY_HITS, dbg->hits);
		printFmt(out, esc, "%I, \"%s\": %d\n", indent + 2, JSON_KEY_FAILS, dbg->exec - dbg->hits);
	}
	printFmt(out, esc, JSON_OBJ_ARR_END, indent + 1);

	printFmt(out, esc, JSON_OBJ_ARR_START, indent + 1, JSON_KEY_STMT);
	dbg = (dbgn) ctx->statements.ptr;
	for (size_t i = 0; i < nStmt; ++i, dbg++) {
		size_t symOffs = 0;
		symn sym = dbg->func;
		if (dbg->hits == 0) {
			continue;
		}
		covStmt += 1;
		if (sym == NULL) {
			sym = rtLookup(ctx->rt, dbg->start, 0);
		}
		if (sym != NULL) {
			symOffs = dbg->start - sym->offs;
		}
		if (covStmt > 1) {
			printFmt(out, esc, JSON_OBJ_NEXT, indent + 1);
		}
		printFmt(out, esc, "%I\"%s\": \"%?T+%d\"\n", indent + 2, "", sym, symOffs);
		printFmt(out, esc, "%I, \"%s\": %d\n", indent + 2, JSON_KEY_OFFS, dbg->start);
		printFmt(out, esc, "%I, \"%s\": %d\n", indent + 2, JSON_KEY_TIME, dbg->self);
		printFmt(out, esc, "%I, \"%s\": %d\n", indent + 2, JSON_KEY_TOTAL, dbg->total);
		printFmt(out, esc, "%I, \"%s\": %d\n", indent + 2, JSON_KEY_HITS, dbg->hits);
		printFmt(out, esc, "%I, \"%s\": %d\n", indent + 2, JSON_KEY_FAILS, dbg->exec - dbg->hits);
		if (dbg->file != NULL && dbg->line > 0) {
			printFmt(out, esc, "%I, \"%s\": \"%s\"\n", indent + 2, JSON_KEY_FILE, dbg->file);
			printFmt(out, esc, "%I, \"%s\": %d\n", indent + 2, JSON_KEY_LINE, dbg->line);
		}
	}
	printFmt(out, esc, JSON_OBJ_ARR_END, indent + 1);

	printFmt(out, esc, "%I, \"%s\": %d\n", indent + 1, "ticksPerSec", CLOCKS_PER_SEC);
	printFmt(out, esc, "%I, \"%s\": %d\n", indent + 1, "functionCount", ctx->functions.cnt);
	printFmt(out, esc, "%I, \"%s\": %d\n", indent + 1, "statementCount", ctx->statements.cnt);
	printFmt(out, esc, JSON_OBJ_END, indent);
}
static void jsonPreProfile(dbgContext ctx) {
	userContext cc = ctx->extra;
	FILE *out = cc->out;
	const int indent = cc->indent;
	const char **esc = cc->esc;

	// TODO: use JSON_*_START and JSON_*_END
	printFmt(out, esc, "%I%s\"%s\": {\n", indent, cc->hasOut ? ", " : "", JSON_KEY_PROFILE);
	printFmt(out, esc, "%I\"%s\": [", indent + 1, "callTreeData");
	printFmt(out, esc, "\"%s\", ", "ctTickIndex");
	printFmt(out, esc, "\"%s\", ", "ctHeapIndex");
	printFmt(out, esc, "\"%s\"", "ctFunIndex");
	printFmt(out, esc, "]\n");

	printFmt(out, esc, JSON_ARR_START, indent + 1, "callTree");
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ text output
static void textDumpDBG(FILE *out, const char **esc, dbgn dbg, userContext ctx, int indent) {
	dmpMode mode = ctx->dmpAsm;
	printFmt(out, esc, "%I%?s:%?u", indent, dbg->file, dbg->line);
	printFmt(out, esc, ": (%d bytes", dbg->end - dbg->start);
	if (mode & prAsmAddr && !ctx->hideOffsets) {
		printFmt(out, esc, " @.%06x-.%06x", dbg->start, dbg->end);
	}
	printFmt(out, esc, ")");
	if (dbg->stmt != NULL && ctx->rt->cc != NULL) {
		printFmt(out, esc, ": %.*t", mode | prFull | prOneLine, dbg->stmt);
	}
	printFmt(out, esc, "\n");
}

static void textDumpAsm(FILE *out, const char **esc, size_t offs, userContext ctx, int indent) {
	dmpMode mode = ctx->dmpAsm;
	if (ctx->hideOffsets) {
		mode = prNoOffs | (mode & ~prAsmCode);
	}
	if (ctx->dmpAsmStmt && ctx->rt->cc != NULL) {
		dbgn dbg = mapDbgStatement(ctx->rt, offs);
		if (dbg != NULL && dbg->start == offs) {
			textDumpDBG(out, esc, dbg, ctx, indent);
		}
	}
	printFmt(out, esc, "%I", indent);
	printAsm(out, esc, ctx->rt, vmPointer(ctx->rt, offs), mode);
	printFmt(out, esc, "\n");
}
static void textDumpMem(dbgContext dbg, void *ptr, size_t size, char *kind) {
	userContext ctx = dbg->extra;
	const char **esc = ctx->esc;
	FILE *out = ctx->out;

	char *unit = "bytes";
	double value = (double)size;

	if (size > (1 << 30)) {
		unit = "Gb";
		value /= 1 << 30;
	}
	else if (size > (1 << 20)) {
		unit = "Mb";
		value /= 1 << 20;
	}
	else if (size > (1 << 10)) {
		unit = "Kb";
		value /= 1 << 10;
	}

	printFmt(out, esc, "memory[%s] @%06x; size: %d(%?.1F %s)\n", kind, vmOffset(ctx->rt, ptr), size, value, unit);
}

void printFields(rtContext rt, FILE *out, const char **esc, symn sym, int all) {
	for (symn var = sym->fields; var; var = var->next) {
		if (var == rt->main || isInline(var)) {
			// alias and type names.
			continue;
		}
		if (isTypename(var)) {
			dieif(!isStatic(var), ERR_INTERNAL_ERROR);
			printFields(rt, out, esc, var, all);
			if (!all) {
				continue;
			}
		}
		if (isFunction(var)) {
			dieif(!isStatic(var), ERR_INTERNAL_ERROR);
			printFields(rt, out, esc, var, all);
			if (!all) {
				continue;
			}
		}
		if (!isStatic(var)) {
			// members and locals.
			continue;
		}

		if (var->file != NULL && var->line > 0) {
			printFmt(out, esc, "%s:%u: ", var->file, var->line);
		}
		else if (!all) {
			// exclude internal symbols
			continue;
		}

		char *ofs = (char *) rt->_mem + var->offs;
		printVal(out, esc, rt, var, (vmValue *) ofs, prSymQual | prSymType, 0);
		printFmt(out, esc, "\n");
	}
}
void printGlobals(rtContext rt, FILE *out, const char **esc, int all) {
	for (symn var = rt->main->fields; var; var = var->next) {
		char *ofs = NULL;

		if (var == rt->main || isInline(var)) {
			continue;
		}
		if (isTypename(var)) {
			dieif(!isStatic(var), ERR_INTERNAL_ERROR);
			printFields(rt, out, esc, var, all);
			if (!all) {
				continue;
			}
		}
		if (isFunction(var)) {
			dieif(!isStatic(var), ERR_INTERNAL_ERROR);
			printFields(rt, out, esc, var, all);
			if (!all) {
				continue;
			}
		}

		if (var->file && var->line) {
			printFmt(out, esc, "%s:%u: ", var->file, var->line);
		}
		else if (!all) {
			// exclude internals
			continue;
		}

		if (isStatic(var)) {
			// static variable.
			ofs = (char *) rt->_mem + var->offs;
		}
		else {
			// local variable (or argument).
			ofs = (char *) rt->vm.cell - var->offs;
		}

		printVal(out, esc, rt, var, (vmValue *) ofs, prSymQual | prSymType, 0);
		printFmt(out, esc, "\n");
	}
}

static void textPostProfile(userContext usr) {
	static const double CLOCKS_PER_MILLI = CLOCKS_PER_SEC / 1000.;
	const char **esc = usr->esc;
	FILE *out = usr->out;
	rtContext rt = usr->rt;
	const char *prefix = usr->compileSteps ? usr->compileSteps : "\n---------- ";

	if (usr->dmpTimes != 0) {
		clock_t now = clock();
		printFmt(out, esc, "%?sTime:\nCompile: %.3F millis\nExecute: %.3F millis\n", prefix,
			(usr->rtStart - usr->ccStart) / CLOCKS_PER_MILLI,
			(now - usr->rtStart) / CLOCKS_PER_MILLI
		);
	}

	if (usr->dmpGlobals != 0) {
		printFmt(out, esc, "%?sGlobals:\n", prefix);
		printGlobals(rt, out, esc, usr->dmpGlobals > 1);
	}

	dbgContext dbg = rt->dbg;
	if (dbg == NULL) {
		return;
	}

	if (dbg->extra == NULL) {
		return;
	}

	if (dbg->extra != usr) {
		logif("warn", ERR_INTERNAL_ERROR);
		dbg->extra = usr;
	}

	if (usr->dmpProfile > 0) {
		int dmpCoverage = usr->dmpProfile > 0;
		int dmpFunctions = usr->dmpProfile > 0;
		int dmpStatements = usr->dmpProfile > 1;
		int dmpNotExecuted = usr->dmpProfile > 2;

		if (dmpCoverage != 0) {
			size_t covFunc = 0, nFunc = dbg->functions.cnt;
			dbgn ptrFunc = (dbgn) dbg->functions.ptr;
			for (size_t i = 0; i < nFunc; ++i, ptrFunc++) {
				covFunc += ptrFunc->hits > 0;
			}

			size_t covStmt = 0, nStmt = dbg->statements.cnt;
			dbgn ptrStmt = (dbgn) dbg->statements.ptr;
			for (size_t i = 0; i < nStmt; ++i, ptrStmt++) {
				covStmt += ptrStmt->hits > 0;
			}

			printFmt(out, esc, "%?sProfile: coverage\nfunctions: %.2f%% (%d/%d)\nstatements: %.2f%% (%d/%d)\n", prefix,
				covFunc * 100. / (nFunc ? nFunc : 1), covFunc, nFunc, covStmt * 100. / (nStmt ? nStmt : 1), covStmt, nStmt
			);
		}

		if (dmpFunctions != 0) {
			printFmt(out, esc, "%?sProfile: functions\n", prefix);
			size_t cnt = dbg->functions.cnt;
			for (size_t i = 0; i < cnt; ++i) {
				dbgn ptr = (dbgn) dbg->functions.ptr + i;
				if (ptr->hits == 0) {
					continue;
				}
				symn sym = ptr->func;
				if (sym == NULL) {
					sym = rtLookup(rt, ptr->start, 0);
				}
				printFmt(out, esc,
					"%?s:%?u:[.%06x, .%06x): %?T, hits(%D/%D), time(%D%?+D / %.3F%?+.3F ms)\n", ptr->file,
					ptr->line, ptr->start, ptr->end, sym, (int64_t) ptr->hits, (int64_t) ptr->exec,
					(int64_t) ptr->total, (int64_t) -(ptr->total - ptr->self),
					ptr->total / CLOCKS_PER_MILLI, -(ptr->total - ptr->self) / CLOCKS_PER_MILLI
				);
			}

			if (dmpNotExecuted != 0) {
				printFmt(out, esc, "%?sProfile: functions not executed\n", prefix);
				for (size_t i = 0; i < cnt; ++i) {
					dbgn ptr = (dbgn) dbg->functions.ptr + i;
					symn sym = ptr->func;
					if (ptr->hits != 0) {
						continue;
					}
					if (sym == NULL) {
						sym = rtLookup(rt, ptr->start, 0);
					}
					printFmt(out, esc, "%?s:%?u:[.%06x, .%06x): %?T\n", ptr->file, ptr->line, ptr->start
						, ptr->end, sym
					);
				}
			}
		}

		if (dmpStatements != 0) {
			size_t cnt = dbg->statements.cnt;
			printFmt(out, esc, "%?sProfile: statements\n", prefix);
			for (size_t i = 0; i < cnt; ++i) {
				dbgn ptr = (dbgn) dbg->statements.ptr + i;
				size_t symOffs = 0;
				symn sym = ptr->func;
				if (ptr->hits == 0) {
					continue;
				}
				if (sym == NULL) {
					sym = rtLookup(rt, ptr->start, 0);
				}
				if (sym != NULL) {
					symOffs = ptr->start - sym->offs;
				}
				if (dmpStatements) {
					printFmt(out, esc,
						"%?s:%?u:[.%06x, .%06x): <%?.T+%d> hits(%D/%D), time(%D%?+D / %.3F%?+.3F ms)\n", ptr->file,
						ptr->line, ptr->start, ptr->end, sym, symOffs, (int64_t) ptr->hits, (int64_t) ptr->exec,
						(int64_t) ptr->total, (int64_t) -(ptr->total - ptr->self),
						ptr->total / CLOCKS_PER_MILLI, -(ptr->total - ptr->self) / CLOCKS_PER_MILLI
					);
				}
			}

			if (dmpNotExecuted != 0) {
				printFmt(out, esc, "%?sProfile: statements not executed\n", prefix);
				for (size_t i = 0; i < cnt; ++i) {
					dbgn ptr = (dbgn) dbg->statements.ptr + i;
					size_t symOffs = 0;
					symn sym = ptr->func;
					if (ptr->hits != 0) {
						continue;
					}
					if (sym == NULL) {
						sym = rtLookup(rt, ptr->start, 0);
					}
					if (sym != NULL) {
						symOffs = ptr->start - sym->offs;
					}
					printFmt(out, esc, "%?s:%?u:[.%06x, .%06x): <%?.T+%d>\n", ptr->file, ptr->line, ptr->start, ptr->end, sym, symOffs);
				}
			}		}
	}

	if (usr->dmpMemory > 0) {
		printFmt(out, esc, "%?sMemory layout:\n", prefix);
		textDumpMem(dbg, rt->_mem, rt->_end - (unsigned char*)rt, "all");
		textDumpMem(dbg, rt->_mem, rt->vm.ro, "meta");
		textDumpMem(dbg, rt->_mem + rt->vm.pc, rt->vm.px + px_size - rt->vm.pc, "code");
		textDumpMem(dbg, rt->_beg, rt->_end - rt->_beg, "heap");
		textDumpMem(dbg, rt->_end - rt->vm.ss, rt->vm.ss, "stack");
		if (usr->dmpMemory > 1) {
			// show allocated memory chunks.
			printFmt(out, esc, "%?sMemory allocations:\n", prefix);
			rtAlloc(rt, NULL, 0, textDumpMem);
		}
	}
}

static void dumpApiText(userContext extra, symn sym) {
	int dmpAsm = 0;
	int dmpAst = 0;
	int dumpExtraData = 0;
	const char **esc = extra->esc;
	FILE *out = extra->out;
	int indent = extra->indent;

	if (extra->dmpAst != prSkip && extra->rt->cc && sym->init != NULL) {
		// dump valid syntax tree only
		dmpAst = 1;
	}

	if (extra->dmpAsm != prSkip && isFunction(sym)) {
		dmpAsm = 1;
	}

	if (extra->dmpApi == prSkip && !dmpAsm && !dmpAst) {
		// nothing to dump
		return;
	}
	else if (extra->dmpMain && sym == extra->rt->main) {
		// do not skip main symbol
	}
	else if (extra->dmpApiAll && sym->file == NULL && sym->line == 0) {
		// do not skip internal symbols
	}
	else if (sym->file == NULL || sym->line == 0) {
		// skip generated or builtin symbols
		return;
	}

	// print qualified name with arguments and type
	printFmt(out, esc, "%I%T: %T", extra->indent, sym, sym->type);

	// print symbol info (kind, size, offset, ...)
	if (extra->dmpApiInfo) {
		if (!dumpExtraData) {
			printFmt(out, esc, " {\n");
			dumpExtraData = 1;
		}
		printFmt(out, esc, "%I.kind: %K\n", indent, sym->kind);
		printFmt(out, esc, "%I.base: `%T`\n", indent, sym->type);
		printFmt(out, esc, "%I.size: %d\n", indent, sym->size);
		if (!extra->hideOffsets) {
			printFmt(out, esc, "%I.offset: %06x\n", indent, sym->offs);
		}
		printFmt(out, esc, "%I.name: '%s'\n", indent, sym->name);
		if (sym->fmt != NULL) {
			printFmt(out, esc, "%I.print: '%s'\n", indent, sym->fmt);
		}
		if (sym->file != NULL && sym->line > 0) {
			printFmt(out, esc, "%I.file: '%s:%u'\n", indent, sym->file, sym->line);
		}
		if (sym->owner != NULL) {
			printFmt(out, esc, "%I.owner: %?T\n", indent, sym->owner);
		}
	}

	// dump params of the function
	if (extra->dmpApiPrms) {
		symn param;
		if (!dumpExtraData) {
			printFmt(out, esc, " {\n");
			dumpExtraData = 1;
		}
		if (extra->dmpApi != prSkip && sym->fields != sym->params) {
			for (param = sym->fields; param; param = param->next) {
				if (extra->hideOffsets) {
					printFmt(out, esc, "%I.field %.T: %?T (size: %d -> %K)\n", indent, param, param->type, param->size, param->kind);
				}
				else {
					printFmt(out, esc, "%I.field %.T: %?T (size: %d @%d -> %K)\n", indent, param, param->type, param->size, param->offs, param->kind);
				}
			}
		}
		for (param = sym->params; param; param = param->next) {
			if (extra->hideOffsets) {
				printFmt(out, esc, "%I.param %.T: %?T (size: %d -> %K)\n", indent, param, param->type, param->size, param->kind);
			}
			else {
				printFmt(out, esc, "%I.param %.T: %?T (size: %d @%d -> %K)\n", indent, param, param->type, param->size, param->offs, param->kind);
			}
		}
	}

	if (dmpAst != 0) {
		dmpMode mode = extra->dmpAst;
		if (!dumpExtraData) {
			printFmt(out, esc, " {\n");
			dumpExtraData = 1;
		}
		printFmt(out, esc, "%I.value: ", indent);
		printAst(out, esc, sym->init, mode, -indent);
		printFmt(out, esc, "\n");
	}

	// print function disassembled instructions
	if (dmpAsm != 0) {
		size_t end = sym->offs + sym->size;

		if (!dumpExtraData) {
			printFmt(out, esc, " {\n");
			dumpExtraData = 1;
		}
		printFmt(out, esc, "%I.instructions: (%d bytes", indent, sym->size);
		if (!extra->hideOffsets) {
			printFmt(out, esc, " @.%06x-.%06x", sym->offs, sym->offs + sym->size);
		}
		printFmt(out, esc, ")\n");

		for (size_t pc = sym->offs; pc < end; ) {
			unsigned char *ip = vmPointer(extra->rt, pc);
			size_t ppc = pc;

			if (ip == NULL) {
				break;
			}
			textDumpAsm(out, esc, pc, extra, indent + 1);
			pc += opcode_tbl[*ip].size;
			if (pc == ppc) {
				break;
			}
		}
	}

	// print usages of symbol
	if (extra->dmpApiUsed) {
		int extUsages = 0;
		if (!dumpExtraData) {
			printFmt(out, esc, " {\n");
			dumpExtraData = 1;
		}
		printFmt(out, esc, "%I.references:\n", indent);
		for (astn usage = sym->use; usage; usage = usage->ref.used) {
			if (usage->file && usage->line) {
				int referenced = usage != sym->tag;
				printFmt(out, esc, "%I%s:%u: %s as `%t`\n", indent + 1, usage->file, usage->line, referenced ? "referenced" : "defined", usage);
			}
			else {
				extUsages += 1;
			}
		}
		if (extUsages > 0) {
			printFmt(out, esc, "%Iinternal references: %d\n", indent + 1, extUsages);
		}
	}

	if (dumpExtraData) {
		printFmt(out, esc, "%I}", extra->indent);
	}

	printFmt(out, esc, "\n");
}
static void dumpApiSciTE(userContext extra, symn sym) {
	const char **esc = extra->esc;
	FILE *out = extra->out;

	if (sym->name == NULL) {
		return;
	}

	// skip hidden symbols
	switch (*sym->name) {
		default:
			break;

		case '.':
		case 0:
			return;
	}

	if (isInvokable(sym)) {
		printSym(out, NULL, sym, prSymQual|prSymArgs|prSymType, 0);
	}
	else {
		printSym(out, NULL, sym, prSymQual|prSymArgs, 0);
	}
	printFmt(out, esc, "\n");
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ console debugger and profiler
static dbgn conDebug(dbgContext ctx, vmError error, size_t ss, void *stack, size_t caller, size_t callee) {
	char buff[1024];
	rtContext rt = ctx->rt;
	userContext usr = ctx->extra;
	const char **esc = usr->esc;
	FILE *out = usr->out;
	FILE *con = stderr;

	brkMode breakMode = brkSkip;
	char *breakCause = NULL;
	dbgn dbg = mapDbgStatement(rt, caller);

	// enter or leave
	if (callee != 0) {
		// TODO: enter will break after executing call?
		// TODO: leave will break after executing return?
		if (usr->dbgCommand == dbgStepInto && (ssize_t) callee > 0) {
			usr->dbgNextBreak = (size_t) -1;
		}
		else if (usr->dbgCommand == dbgStepOut && (ssize_t) callee < 0) {
			usr->dbgNextBreak = (size_t) -1;
		}

		if (usr->exec & trcMethods) {
			if (usr->exec & trcTime) {
				double now = clock() * (1000. / CLOCKS_PER_SEC);
				printFmt(out, esc, "[% 3.2F]", now);
			}
			if ((ssize_t) callee > 0) {
				printFmt(out, esc, ">% I %d, %T\n", ss, ctx->usedMem, rtLookup(rt, callee, 0));
			}
			else {
				printFmt(out, esc, "<% I %d, return\n", ss, ctx->usedMem);
			}
		}
		return NULL;
	}

	if (error != noError) {
		breakCause = vmErrorMessage(error);
		if (isChecked(ctx)) {
			breakMode = usr->dbgOnCaught;
		}
		else {
			breakMode = usr->dbgOnError;
		}
	}
	else if (usr->dbgNextBreak == (size_t) -1) {
		breakMode = brkPause;
		breakCause = "Break";
	}
	else if (usr->dbgNextBreak == caller) {
		breakMode = brkPause;
		breakCause = "Break";
	}
	else if (dbg != NULL) {
		if (caller == dbg->start) {
			breakMode = dbg->bp;
			if (breakMode & (brkPause | brkPrint | brkTrace)) {
				breakCause = "Break point";
			}
			if (breakMode & brkOnce) {
				// remove breakpoint
				dbg->bp = brkSkip;
			}
		}
	}

	// print executing instruction.
	if (error == noError && (usr->exec & trcOpcodes)) {
		if (usr->exec & trcLocals) {
			size_t i = 0;
			printFmt(out, esc, "     sp: <%d> [", ss);
			if (ss > maxLogItems) {
				printFmt(out, esc, " …");
				i = ss - maxLogItems;
			}
			for (; i < ss; i++) {
				if (i > 0) {
					printFmt(out, esc, ",");
				}
				symn sym = NULL;
				uint32_t val = ((uint32_t *) stack)[ss - i - 1];
				if (val > 0 && val <= rt->vm.px) {
					sym = rtLookup(rt, val, 0);
					if (sym && !isFunction(sym)) {
						if (sym->offs != val) {
							sym = NULL;
						}
					}
				}
				if (sym != NULL) {
					size_t symOffs = val - sym->offs;
					printFmt(out, esc, "<%?.T%?+d>", sym, symOffs);
				} else {
					printFmt(out, esc, " %d", val);
				}
			}
			printFmt(out, esc, "\n");
		}
		if (dbg != NULL && dbg->start == caller) {
			// print the position of the current statement
			textDumpDBG(out, esc, dbg, usr, 0);
		}
		if (usr->exec & trcOpcodes) {
			if (usr->exec & trcTime) {
				double now = clock() * (1000. / CLOCKS_PER_SEC);
				printFmt(out, esc, "[% 3.2F]", now);
			}
			textDumpAsm(out, esc, caller, usr, 0);
		}
	}

	// print error type
	if (breakMode & brkPrint) {
		symn fun = rtLookup(rt, caller, 0); 
		size_t funOffs = caller;
		if (fun != NULL) {
			funOffs -= fun->offs;
		}
		// print current file position
		if (dbg != NULL && dbg->file != NULL && dbg->line > 0) {
			printFmt(out, esc, "%s:%u: ", dbg->file, dbg->line);
		}
		printFmt(out, esc, ERR_EXEC_INSTRUCTION"\n", breakCause, caller, fun, funOffs, vmPointer(rt, caller));
		if (out != stdout && out != stderr) {
			// print the error message also to the console
			if (dbg != NULL && dbg->file != NULL && dbg->line > 0) {
				printFmt(con, esc, "%s:%u: ", dbg->file, dbg->line);
			}
			printFmt(con, esc, ERR_EXEC_INSTRUCTION"\n", breakCause, caller, fun, funOffs, vmPointer(rt, caller));
		}
	}
	if (breakMode & brkTrace) {
		traceCalls(ctx, out, 1, 20, 0);
	}

	// pause execution in debugger
	for ( ; breakMode & brkPause; ) {
		dbgMode cmd = usr->dbgCommand;
		char *arg = NULL;
		printFmt(con, esc, ">dbg[%?c] %.A: ", cmd, vmPointer(rt, caller));
		if (usr->in == NULL || fgets(buff, sizeof(buff), usr->in) == NULL) {
			printFmt(con, esc, "can not read from standard input, aborting\n");
			return ctx->abort;
		}
		if ((arg = strchr(buff, '\n'))) {
			*arg = 0;		// remove new line char
		}

		if (*buff != 0) {
			// update last command
			cmd = (dbgMode) buff[0];
			arg = buff + 1;
			usr->dbgCommand = cmd;
		}

		switch (cmd) {
			default:
				printFmt(con, esc, "invalid command '%s'\n", buff);
				break;

			case dbgAbort:
				return ctx->abort;

			case dbgResume:
			case dbgStepInto:
			case dbgStepOut:
				usr->dbgNextBreak = 0;
				return dbg;

			case dbgStepNext:
				usr->dbgNextBreak = (size_t)-1;
				return dbg;

			case dbgStepLine:
				usr->dbgNextBreak = dbg ? dbg->end : 0;
				return dbg;

			case dbgPrintStackTrace:
				traceCalls(ctx, con, 1, 20, 0);
				break;

			case dbgPrintInstruction:
				textDumpAsm(con, esc, caller, usr, 0);
				break;

			case dbgPrintStackValues:
				if (*arg == 0) {
					size_t offs;
					// print top of stack
					for (offs = 0; offs < ss; offs++) {
						vmValue *v = (vmValue*)&((long*)stack)[offs];
						printFmt(con, esc, "\tsp(%d): {0x%08x, i32(%d), f32(%f), i64(%D), f64(%F)}\n", offs, v->i32, v->i32, v->f32, v->i64, v->f64);
					}
				}
				else {
					symn sym = ccLookup(rt, NULL, arg);
					printFmt(con, esc, "arg:%T", sym);
					if (sym && isVariable(sym) && !isStatic(sym)) {
						printVal(con, esc, rt, sym, (vmValue *) stack, prSymType, 0);
					}
				}
				break;
		}
	}
	return dbg;
}

static void dumpVmOpc(const char *error, const struct opcodeRec *info) {
	FILE *out = stdout;
	printFmt(out, NULL, "\n## Instruction %s\n", info->name);
	printFmt(out, NULL, "Perform [TODO]\n");
	printFmt(out, NULL, "\n### Description\n");
	printFmt(out, NULL, "Instruction code: 0x%02x  \n", info->code);
	printFmt(out, NULL, "Instruction length: %d byte%?c  \n", info->size, info->size == 1 ? 0 : 's');
	printFmt(out, NULL, "\n### Stack change\n");

	printFmt(out, NULL, "Requires %d operand%?c: […", info->stack_in, info->stack_in == 1 ? 0 : 's');
	for (unsigned i = 0; i < info->stack_in; ++i) {
		printFmt(out, NULL, ", %c", 'a' + i);
	}
	printFmt(out, NULL, "  \nReturns %d value%?c: […", info->stack_out, info->stack_in == 1 ? 0 : 's');
	for (unsigned i = 0; i < info->stack_out; ++i) {
		printFmt(out, NULL, ", %c", 'a' + i);
	}
	printFmt(out, NULL, ", [TODO]  \n");

	if (error != NULL) {
		printFmt(out, NULL, "\n### Note\n%s\n", error);
	}
}

static void testVmOpc(const char *error, const struct opcodeRec *info) {
	if (error == NULL) {
		return;
	}
	printFmt(stdout, NULL, "%s: %s\n", error, info->name);
}

static int usage() {
	const char *USAGE = \
		"cmpl [global options] [files with options]..."
		"\n"
		"\n<global options>:"
		"\n"
		"\n  -run[*]               run at full speed, but without: debug information, stacktrace, bounds checking, ..."
		"\n    /g /G               dump global variable values (/G includes types and functions)"
		"\n"
		"\n  -debug[*]             run with attached debugger, pausing on uncaught errors and break points"
		"\n    /g /G               dump global variable values (/G includes types and functions)"
		"\n    /h /H               dump memory usage (/H includes heap allocations)"
		"\n    /p /P               print caught errors (/P includes stack trace)"
		"\n    /t /T               trace the execution of instructions"
		"\n    /a                  pause on all(caught) errors"
		"\n    /s                  pause on startup"
		"\n"
		"\n  -profile[*]           run code with profiler: coverage, method tracing"
		"\n    /g /G               dump global variable values (/G includes types and functions)"
		"\n    /h /H               dump memory usage (/H includes heap allocations)"
		"\n    /p /P               show statement execution times (/P include not executed code)"
		"\n    /t /T               trace the execution of methods"
		"\n"
		"\n  -std<file>            specify custom standard library file (empty file name disables std library compilation)."
		"\n"
		"\n  -mem<int>[kKmMgG]     override memory usage for compiler and runtime(heap)"
		"\n"
		"\n  -log[*]<int> <file>   set logger for: compilation errors and warnings, runtime debug messages"
		"\n    <int>               set the default log(warning) level 0 - 15"
		"\n    /a                  append to the log file"
		"\n"
		"\n  -dump[?] <file>       set output for: dump(symbols, assembly, abstract syntax tree, coverage, call tree)"
		"\n    .scite              dump api for SciTE text editor"
		"\n    .json               dump api and profile data in javascript object notation format"
		"\n"
		"\n  -api[*]               dump symbols"
		"\n    /a                  include all builtin symbols"
		"\n    /m                  include main builtin symbol"
		"\n    /d                  dump details of symbol"
		"\n    /p                  dump params and fields"
		"\n    /u                  dump usages"
		"\n"
		"\n  -asm[*]<int>          dump assembled code: jmp +80"
		"\n    /a                  use global address: jmp @0x003d8c"
		"\n    /n                  prefer names over addresses: jmp <main+80>"
		"\n    /s                  print source code statements"
		"\n    /m                  include main builtin symbol"
		"\n    /d                  dump details of symbol"
		"\n    /p                  dump params and fields"
		"\n    /u                  dump usages"
		"\n"
		"\n  -ast[*]               dump syntax tree"
		"\n    /t                  dump sub-expression type information"
		"\n    /l                  do not expand statements (print on single line)"
		"\n    /b                  don't keep braces ('{') on the same line"
		"\n    /e                  don't keep `else if` constructs on the same line"
		"\n    /m                  include main builtin symbol"
		"\n    /d                  dump details of symbol"
		"\n    /p                  dump params and fields"
		"\n    /u                  dump usages"
		"\n"
		"\n<files with options>: filename followed by switches"
		"\n  <file>                if file extension is (.so|.dll) load as library else compile"
		"\n  -w[a|x|<int>]         set or disable warning level for current file"
		"\n  -b[*]<int>            break point on <int> line in current file"
		"\n    /[l|L]              print only, do not pause (/L includes stack trace)"
		"\n    /o                  one shot breakpoint, disable after first hit"
		"\n"
		"\nexamples:"
		"\n"
		"\n>app -dump.json api.json"
		"\n    dump builtin symbols to `api.json` file (types, functions, variables, aliases)"
		"\n"
		"\n>app -run test.tracing.ci"
		"\n    compile and execute the file `test.tracing.ci`"
		"\n"
		"\n>app -debug gl.so -w0 gl.ci -w0 test.ci -wx -b12 -b15 -b/t/19"
		"\n    execute in debug mode"
		"\n    import `gl.so`"
		"\n        with no warnings"
		"\n    compile `gl.ci`"
		"\n        with no warnings"
		"\n    compile `test.ci`"
		"\n        treating all warnings as errors"
		"\n        break execution on lines 12 and 15"
		"\n        print message when line 19 is hit"
		"\n";
	fputs(USAGE, stdout);
	return 0;
}

int main(int argc, char *argv[]) {
	setbuf(stdout, NULL);
	setbuf(stderr, NULL);

	if (argc < 2) {
		return usage();
	}
	if (argc == 2) {
		if (strcmp(argv[1], "--help") == 0) {
			return usage();
		}
		if (strcmp(argv[1], "-vmTest") == 0) {
			return vmSelfTest(testVmOpc);
		}
	}
	struct userContextRec extra = {
		.dbgCommand = dbgResume,	// last command: resume
		.dbgOnError = brkPause | brkPrint,
		.dbgOnCaught = brkSkip,
		.dbgNextBreak = 0,

		.dmpApi = prSkip,
		.dmpMain = 0,
		.dmpApiAll = 0,
		.dmpApiInfo = 0,
		.dmpApiPrms = 0,
		.dmpApiUsed = 0,
		.hideOffsets = 0,
		.dmpTimes = 0,
		.compileSteps = NULL,

		.dmpAsm = prSkip,
		.dmpAsmStmt = 0,

		.dmpAst = prSkip,
		.dmpGlobals = 0,
		.dmpProfile = 0,
		.dmpMemory = 0,

		.exec = (runMode) 0,

		.in = stdin,
		.out = stdout,
		.esc = NULL,
		.hasOut = 0,
		.indent = 0,

		.rt = NULL
	};

	struct {
		// optimizations
		int foldConst;
		int fastInstr;
		int fastAssign;
		int genGlobals;
		int warnLevel;

		size_t memory;
	} settings = {
		// use all optimizations by default
		.foldConst = 1,
		.fastInstr = 1,
		.fastAssign = 1,
		.genGlobals = 0,
		.warnLevel = 5,

		// 2 Mb memory compiler + runtime
		.memory = 2 << 20
	};

	rtContext rt = NULL;

	ccInstall install = install_def;
	char *stdLib = (char*)STDLIB;	// standard library
	char *ccFile = NULL;			// compiled filename

	char *logFileName = NULL;		// logger filename
	int logAppend = 0;				// do not clear the log file

	FILE *dumpFile = NULL;			// dump file
	char *dumpFileName = NULL;		// dump file
	char *pathDumpXml = NULL;		// dump file path
	void (*dumpFun)(userContext, symn) = NULL;
	enum { run, debug, profile, compile } run_code = compile;

	int i;

	// TODO: max 32 break points ?
	brkMode bp_type[32];
	char *bp_file[32];
	int bp_line[32];
	int bp_size = 0;

	// process global options
	for (i = 1; i < argc; ++i) {
		char *arg = argv[i];

		// not an option
		if (*arg != '-') {
			break;
		}

		// run, debug or profile
		else if (strncmp(arg, "-run", 4) == 0) {
			char *arg2 = arg + 4;
			if (run_code != compile) {
				fatal("argument specified multiple times: %s", arg);
				return -1;
			}
			run_code = run;
			while (*arg2 == '/') {
				switch (arg2[1]) {
					default:
						break;

					case 'G':
						extra.dmpGlobals = 2;
						arg2 += 2;
						break;
					case 'g':
						extra.dmpGlobals = 1;
						arg2 += 2;
						break;
				}
			}
			if (*arg2) {
				fatal("invalid argument '%s'", arg);
				return -1;
			}
		}
		else if (strncmp(arg, "-debug", 6) == 0) {
			char *arg2 = arg + 6;
			if (run_code != compile) {
				fatal("argument specified multiple times: %s", arg);
				return -1;
			}
			run_code = debug;
			while (*arg2 == '/') {
				switch (arg2[1]) {
					default:
						break;

					case 'G':
						extra.dmpGlobals = 2;
						arg2 += 2;
						break;
					case 'g':
						extra.dmpGlobals = 1;
						arg2 += 2;
						break;

					case 'H':
						extra.dmpMemory = 2;
						arg2 += 2;
						break;
					case 'h':
						extra.dmpMemory = 1;
						arg2 += 2;
						break;

					case 'P':
						extra.dbgOnCaught |= brkTrace;
						// fall through
					case 'p':
						extra.dbgOnCaught |= brkPrint;
						arg2 += 2;
						break;

					case 'a':
						extra.dbgOnCaught |= brkPause;
						arg2 += 2;
						break;
					case 's':
						extra.dbgNextBreak = (size_t)-1;
						arg2 += 2;
						break;

					case 'T':
						extra.exec |= trcLocals;
						// fall through
					case 't':
						extra.exec |= trcOpcodes;
						arg2 += 2;
						break;

					// dump stats
				}
			}
			if (*arg2) {
				fatal("invalid argument '%s'", arg);
				return -1;
			}
		}
		else if (strncmp(arg, "-profile", 8) == 0) {
			char *arg2 = arg + 8;
			if (run_code != compile) {
				fatal("argument specified multiple times: %s", arg);
				return -1;
			}
			run_code = profile;
			extra.dmpProfile = 1;
			while (*arg2 == '/') {
				switch (arg2[1]) {
					default:
						break;

					case 'G':
						extra.dmpGlobals = 2;
						arg2 += 2;
						break;
					case 'g':
						extra.dmpGlobals = 1;
						arg2 += 2;
						break;

					case 'H':
						extra.dmpMemory = 2;
						arg2 += 2;
						break;
					case 'h':
						extra.dmpMemory = 1;
						arg2 += 2;
						break;

					case 'P':
						extra.dmpProfile = 3;
						arg2 += 2;
						break;
					case 'p':
						extra.dmpProfile = 2;
						arg2 += 2;
						break;

					case 'T':
						extra.exec |= trcOpcodes;
						// fall through
					case 't':
						extra.exec |= trcTime | trcMethods;
						arg2 += 2;
						break;
				}
			}
			if (*arg2) {
				fatal("invalid argument '%s'", arg);
				return -1;
			}
		}

		// override stdlib file
		else if (strncmp(arg, "-std", 4) == 0) {
			if (stdLib != (char*)STDLIB) {
				fatal("argument specified multiple times: %s", arg);
				return -1;
			}
			if (arg[4] != 0) {
				stdLib = arg + 4;
			}
			else {
				// disable standard library
				stdLib = NULL;
			}
		}
		// override heap size
		else if (strncmp(arg, "-mem", 4) == 0) {
			int value = -1;
			const char *arg2 = arg + 4;
			if (*arg2) {
				arg2 = parseInt(arg2, &value, 10);
				settings.memory = (size_t) value;
			}
			switch (*arg2) {
				default:
					break;

				case 'k':
				case 'K':
					settings.memory <<= 10;
					if (settings.memory >> 10 != (size_t)value) {
						break;
					}
					arg2 += 1;
					break;

				case 'm':
				case 'M':
					settings.memory <<= 20;
					if (settings.memory >> 20 != (size_t)value) {
						break;
					}
					arg2 += 1;
					break;

				case 'g':
				case 'G':
					settings.memory <<= 30;
					if (settings.memory >> 30 != (size_t)value) {
						break;
					}
					arg2 += 1;
					break;
			}
			if (*arg2) {
				fatal("invalid argument '%s'", arg);
				return -1;
			}
		}

		// logger filename and level
		else if (strncmp(arg, "-log", 4) == 0) {
			char *arg2 = arg + 4;
			if (++i >= argc || logFileName) {
				fatal("log file not or double specified");
				return -1;
			}
			logFileName = argv[i];
			while (*arg2 == '/') {
				switch (arg2[1]) {
					default:
						// allow `-log/a/12`
						arg2 += 1;
						break;

					case 'a':
						logAppend = 1;
						arg2 += 2;
						break;
				}
			}
			if (*parseInt(arg2, &settings.warnLevel, 10)) {
				fatal("invalid argument '%s'", arg);
				return -1;
			}
			if (settings.warnLevel > 15) {
				settings.warnLevel = 15;
			}
		}

		// dump output format and filename
		else if (strncmp(arg, "-dump", 5) == 0) {
			if (strEquals(arg, "-dump")) {
				dumpFun = dumpApiText;
			}
			else if (strEquals(arg, "-dump.ast.xml")) {
				if (++i >= argc || pathDumpXml) {
					fatal("dump file not or double specified");
					return -1;
				}
				pathDumpXml = argv[i];
				continue;
			}
			else if (strEquals(arg, "-dump.json")) {
				dumpFun = dumpApiJSON;
			}
			else if (strEquals(arg, "-dump.scite")) {
				dumpFun = dumpApiSciTE;
			}
			else {
				fatal("invalid argument '%s'", arg);
				return -1;
			}
			if (++i >= argc || dumpFile) {
				fatal("dump file not or double specified");
				return -1;
			}
			dumpFileName = argv[i];
			dumpFile = fopen(dumpFileName, "w");
			if (dumpFile == NULL) {
				fatal("error opening dump file: %s", argv[i]);
				return -1;
			}
		}
		else if (strncmp(arg, "-api", 4) == 0) {
			char *arg2 = arg + 4;
			if (extra.dmpApi != prSkip) {
				fatal("argument specified multiple times: %s", arg);
				return -1;
			}
			extra.dmpApi = prName;
			while (*arg2 == '/') {
				switch (arg2[1]) {
					default:
						break;

					case 'a':
						extra.dmpApiAll = 1;
						arg2 += 2;
						break;

					// include extras in dump
					case 'm':
						extra.dmpMain = 1;
						arg2 += 2;
						break;
					case 'd':
						extra.dmpApiInfo = 1;
						arg2 += 2;
						break;
					case 'p':
						extra.dmpApiPrms = 1;
						arg2 += 2;
						break;
					case 'u':
						extra.dmpApiUsed = 1;
						arg2 += 2;
						break;
				}
			}
			if (*arg2) {
				fatal("invalid argument '%s'", arg);
				return -1;
			}
		}
		else if (strncmp(arg, "-asm", 4) == 0) {
			char *arg2 = arg + 4;
			if (extra.dmpAsm != prSkip) {
				fatal("argument specified multiple times: %s", arg);
				return -1;
			}
			extra.dmpAsm = 0;
			while (*arg2 == '/') {
				switch (arg2[1]) {
					default:
						// allow `-asm/a/9`
						arg2 += 1;
						break;

					case 'a':
						extra.dmpAsm |= prAsmAddr;
						arg2 += 2;
						break;
					case 'n':
						extra.dmpAsm |= prAsmName;
						arg2 += 2;
						break;
					case 's':
						extra.dmpAsmStmt = 1;
						arg2 += 2;
						break;

					// include extras in dump
					case 'm':
						extra.dmpMain = 1;
						arg2 += 2;
						break;
					case 'd':
						extra.dmpApiInfo = 1;
						arg2 += 2;
						break;
					case 'p':
						extra.dmpApiPrms = 1;
						arg2 += 2;
						break;
					case 'u':
						extra.dmpApiUsed = 1;
						arg2 += 2;
						break;
				}
			}
			int level = 9;
			if (*arg2 && *parseInt(arg2, &level, 10)) {
				fatal("invalid argument '%s'", arg);
				return -1;
			}
			if (level > 15) {
				level = 15;
			}
			extra.dmpAsm |= prAsmCode & level;
		}
		else if (strncmp(arg, "-ast", 4) == 0) {
			char *arg2 = arg + 4;
			if (extra.dmpAst != prSkip) {
				fatal("argument specified multiple times: %s", arg);
				return -1;
			}
			extra.dmpAst = prFull;
			while (*arg2 == '/') {
				switch (arg2[1]) {
					default:
						break;

					case 't':
						extra.dmpAst |= prAstType;
						arg2 += 2;
						break;

					// output format options
					case 'l':
						extra.dmpAst |= prOneLine;
						arg2 += 2;
						break;
					case 'b':
						extra.dmpAst |= nlAstBody;
						arg2 += 2;
						break;
					case 'e':
						extra.dmpAst |= nlAstElIf;
						arg2 += 2;
						break;

					// include extras in dump
					case 'm':
						extra.dmpMain = 1;
						arg2 += 2;
						break;
					case 'd':
						extra.dmpApiInfo = 1;
						arg2 += 2;
						break;
					case 'p':
						extra.dmpApiPrms = 1;
						arg2 += 2;
						break;
					case 'u':
						extra.dmpApiUsed = 1;
						arg2 += 2;
						break;
				}
			}
			if (*arg2) {
				fatal("invalid argument '%s'", arg);
				return -1;
			}
		}

		// enable or disable hidden settings
		else if (strBegins(arg, "-X")) {
			char *arg2 = arg + 2;
			while (*arg2 == '-' || *arg2 == '+') {
				int on = *arg2 == '+';
				if (strBegins(arg2 + 1, "fold")) {
					settings.foldConst = on;
					arg2 += 5;
				}
				else if (strBegins(arg2 + 1, "fast")) {
					settings.fastInstr = on;
					arg2 += 5;
				}
				else if (strBegins(arg2 + 1, "asgn")) {
					settings.fastAssign = on;
					arg2 += 5;
				}
				else if (strBegins(arg2 + 1, "glob")) {
					settings.genGlobals = on;
					arg2 += 5;
				}
				else if (strBegins(arg2 + 1, "emit")) {
					install = (install & ~installEmit) | on * installEmit;
					arg2 += 5;
				}
				else if (strBegins(arg2 + 1, "ptr")) {
					install = (install & ~install_ptr) | on * install_ptr;
					arg2 += 4;
				}
				else if (strBegins(arg2 + 1, "var")) {
					install = (install & ~install_var) | on * install_var;
					arg2 += 4;
				}
				else if (strBegins(arg2 + 1, "obj")) {
					install = (install & ~install_obj) | on * install_obj;
					arg2 += 4;
				}
				else if (strBegins(arg2 + 1, "stdin")) {
					extra.in = on ? stdin : NULL;
					arg2 += 6;
				}
				else if (strBegins(arg2 + 1, "std")) {
					install = (install & ~installLibs) | on * installLibs;
					arg2 += 4;
				}
				else if (strBegins(arg2 + 1, "offsets")) {
					extra.hideOffsets = !on;
					arg2 += 8;
				}
				else if (strBegins(arg2 + 1, "times")) {
					extra.dmpTimes = on;
					arg2 += 6;
				}
				else if (strBegins(arg2 + 1, "steps")) {
					extra.compileSteps = on ? "\n---------- " : NULL;
					arg2 += 6;
				}
				else {
					break;
				}
			}
			if (*arg2) {
				fatal("invalid argument '%s' at %d", arg, arg2 - arg);
				return -1;
			}
		}

		// no more global options
		else break;
	}

	extra.ccStart = clock();
	// initialize runtime context
	rt = rtInit(NULL, settings.memory);

	if (rt == NULL) {
		fatal("initializing runtime context");
		return -1;
	}

	rt->foldCasts = settings.foldConst != 0;
	rt->foldConst = settings.foldConst != 0;
	rt->foldInstr = settings.fastInstr != 0;
	rt->fastMemory = settings.fastInstr != 0;
	rt->fastAssign = settings.fastAssign != 0;
	rt->genGlobals = settings.genGlobals != 0;
	rt->warnLevel = settings.warnLevel;

	// open log file (global option)
	if (dumpFile && strEquals(dumpFileName, logFileName)) {
		logFILE(rt, dumpFile);
	}
	else if (logFileName && !logFile(rt, logFileName, logAppend)) {
		info(rt, NULL, 0, ERR_OPENING_FILE, logFileName);
	}

	// install base type system.
	if (!ccInit(rt, install, NULL)) {
		fatal("error registering base types");
		logFile(rt, NULL, 0);
		return -6;
	}

	extra.rt = rt;
	if (dumpFile != NULL) {
		extra.out = dumpFile;
	}

	if (install & installLibs) {
		// install standard library.
		if (extra.compileSteps != NULL) {printFmt(extra.out, extra.esc, "%sCompile: `%?s`\n", extra.compileSteps, stdLib);}
		if (ccAddLib(rt->cc, ccLibStd, stdLib) != 0) {
			fatal("error registering standard library");
		}
	}

	// compile and import files / modules
	for (; i <= argc; ++i) {
		char *arg = argv[i];
		if (i == argc || *arg != '-') {
			if (extra.compileSteps != NULL && ccFile != NULL) {printFmt(extra.out, extra.esc, "%sCompile: `%?s`\n", extra.compileSteps, ccFile);}
			if (ccFile != NULL) {
				char *ext = strrchr(ccFile, '.');
				if (ext && (strEquals(ext, ".so") || strEquals(ext, ".dll"))) {
					int resultCode = importLib(rt, ccFile);
					if (resultCode != 0) {
						fatal("error(%d) importing library `%s`", resultCode, ccFile);
					}
				}
				else if (!ccAddUnit(rt->cc, ccFile, 1, NULL)) {
					error(rt, ccFile, 1, "error compiling source `%s`", ccFile);
				}
			}
			ccFile = arg;
			rt->warnLevel = (unsigned) settings.warnLevel;
		}
		else {
			if (ccFile == NULL) {
				fatal("argument `%s` must be preceded by a file", arg);
			}
			if (arg[1] == 'w') {		// warning level for file (TODO: parser warnings only)
				int level = 0;
				if (strcmp(arg, "-wa") == 0) {
					level = 15;
				}
				else if (*parseInt(arg + 2, &level, 10)) {
					fatal("invalid warning level '%s'", arg + 2);
				}
				rt->warnLevel = (unsigned) level;
			}
			else if (arg[1] == 'b') {
				int line = 0;
				brkMode type = brkPause;
				char *arg2 = arg + 2;
				if (bp_size > (int)lengthOf(bp_file)) {
					info(rt, NULL, 0, "can not add more than %d breakpoints.", lengthOf(bp_file));
				}
				while (*arg2 == '/') {
					switch (arg2[1]) {
						default:
							// allow `-b/p/10`
							arg2 += 1;
							break;

						case 'p': // print only
							type &= ~brkPause;
							type |= brkPrint;
							arg2 += 2;
							break;

						case 't': // trace only
							type &= ~brkPause;
							type |= brkTrace;
							arg2 += 2;
							break;

						case 'o': // remove after first hit
							type |= brkOnce;
							arg2 += 2;
							break;
					}
				}

				if (*parseInt(arg2, &line, 10)) {
					fatal("invalid line number `%s`", arg + 2);
				}
				bp_type[bp_size] = type;
				bp_file[bp_size] = ccFile;
				bp_line[bp_size] = line;
				bp_size += 1;
			}
			else {
				fatal("invalid option: `%s`", arg);
			}
		}
	}

	if (pathDumpXml != NULL) {
		FILE *xmlFile = fopen(pathDumpXml, "w");
		if (xmlFile != NULL) {
			dumpAstXML(xmlFile, escapeXML(), rt->cc->root, prDbg, 0, "main");
			fclose(xmlFile);
		}
		else {
			fatal(ERR_OPENING_FILE, pathDumpXml);
		}
	}

	int ccErrors = rt->errors;
	// generate code only if there are no compilation errors
	if (rt->errors == 0) {
		if (extra.compileSteps != NULL) {printFmt(extra.out, extra.esc, "%sGenerate: byte-code\n", extra.compileSteps);}
		if (ccGenCode(rt->cc, run_code != run) != 0) {
			trace("error generating code");
		}
		// set breakpoints
		for (i = 0; i < bp_size; ++i) {
			char *file = bp_file[i];
			int line = bp_line[i];
			brkMode type = bp_type[i];
			dbgn dbg = getDbgStatement(rt, file, line);
			if (dbg != NULL) {
				dbg->bp = type;
			}
			else {
				info(rt, file, line, "invalid breakpoint");
			}
		}
	}

	if (dumpFun == NULL) {
		if (extra.dmpApi != prSkip || extra.dmpAsm != prSkip || extra.dmpAst != prSkip) {
			dumpFun = dumpApiText;
		}
	}

	if (ccErrors == 0) {
		if (dumpFun == dumpApiJSON) {
			extra.esc = escapeJSON();
			printFmt(extra.out, NULL, "{\n");
			printFmt(extra.out, extra.esc, "%I\"%s\": \"%d\"\n", extra.indent, JSON_KEY_VERSION, CMPL_API_H);
			printFmt(extra.out, extra.esc, JSON_OBJ_ARR_START, extra.indent, JSON_KEY_SYMBOLS);
			dumpApi(rt, &extra, dumpApiJSON);
			printFmt(extra.out, extra.esc, JSON_OBJ_ARR_END, extra.indent);
			extra.hasOut = 1;
		}
		else if (dumpFun != NULL) {
			extra.esc = NULL;
			if (extra.compileSteps != NULL) {printFmt(extra.out, extra.esc, "%sSymbols:\n", extra.compileSteps);}
			dumpApi(rt, &extra, dumpFun);
		}
	}

	// run code if there are no compilation errors.
	if (rt->errors == 0 && run_code != compile) {
		if (rt->dbg != NULL) {
			rt->dbg->extra = &extra;

			// set debugger or profiler
			if (run_code == debug) {
				rt->dbg->debug = &conDebug;
				if (extra.dbgNextBreak != 0) {
					// break on first instruction
					extra.dbgNextBreak = rt->vm.pc;
				}
			}
			else if (run_code == profile) {
				// set call tree dump method
				if (dumpFun == dumpApiJSON) {
					rt->dbg->debug = &jsonProfile;
					jsonPreProfile(rt->dbg);
				}
				else {
					rt->dbg->debug = &conDebug;
				}
			}
		}
		if (extra.dmpAsm == prSkip) {
			if (extra.exec & trcOpcodes) {
				extra.dmpAsmStmt = 1;
				extra.dmpAsm = prAsmAddr | 9;
			}
		}
		if (extra.compileSteps != NULL) {printFmt(extra.out, extra.esc, "%sExecute:\n", extra.compileSteps);}
		extra.rtStart = clock();
		rt->errors = execute(rt, 0, NULL, NULL);
	}

	if (rt->errors == 0) {
		if (dumpFun == dumpApiJSON) {
			if (rt->dbg != NULL) {
				jsonPostProfile(rt->dbg);
			}
			printFmt(extra.out, NULL, "}\n");
		}
		else if (run_code != compile) {
			// dump results: global variables, memory allocations
			textPostProfile(&extra);
		}
	}

	if (dumpFile != NULL) {
		fclose(dumpFile);
	}

	// release resources
	closeLibs();

	return rtClose(rt) != 0;
	(void)dumpVmOpc;
}

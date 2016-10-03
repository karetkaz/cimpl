#include <time.h>
#include <stddef.h>
#include "internal.h"

// default values
static const int wl = 15;           // default warning level
static char mem[8 << 20];           // 8MB runtime memory
const char *STDLIB = "stdlib.cvx";  // standard library
const char *USAGE = "application [global options] [files with options]..."
"\n"
"\n<global options>:"
"\n"
"\n  -std<file>            specify custom standard library file (empty file name disables std library compilation)."
"\n"
"\n  -mem<int>[kKmMgG]     override memory usage for compiler and runtime(heap)"
"\n"
"\n  -log[*] <file>        set logger for: compilation errors and warnings, runtime debug messages"
"\n    /a                  append to the log file"
"\n"
"\n  -dump[?] <file>       set output for: dump(symbols, assembly, abstract syntax tree, coverage, call tree)"
"\n    .scite              dump api for SciTE text editor"
"\n    .json               dump api in javascript object notation format"
"\n"
"\n  -api[*]               dump symbols"
"\n    /a                  include builtin symbols"
"\n    /d                  dump details"
"\n    /p                  dump params and fields"
"\n    /u                  dump usages"
"\n"
"\n  -asm[*]               dump assembled code"
"\n    /a                  use global address: (@0x003d8c)"
"\n    /n                  prefer names over addresses: <main+80>"
"\n    /s                  print source code statements"
"\n"
"\n  -ast[*]               dump syntax tree"
"\n    /l                  do not expand statements (print on single line)"
"\n    /b                  don't keep braces ('{') on the same line"
"\n    /e                  don't keep `else if` constructs on the same line"
"\n"
"\n  -run                  run without: debug information, stacktrace, bounds checking, ..."
"\n"
"\n  -debug[*]             run with attached debugger, pausing on uncaught errors and break points"
"\n    /s                  pause on startup"
"\n    /a                  pause on caught errors"
"\n    /l                  print cause of caught errors"
"\n    /L                  print stack trace of caught errors"
"\n                        profiler options ?"
"\n    /g                  dump global variable values"
"\n    /h                  dump allocated heap memory chunks"
"\n    /p                  dump function statistics"
"\n"
"\n  -profile[*]           run code with profiler: coverage, method tracing"
"\n    /a                  dump all collected profile data"
"\n    /g                  dump global variable values (/a: include functions)"
"\n    /h                  dump allocated heap memory chunks"
"\n    /p                  dump function statistics (/a: include statements)"
"\n    /t                  dump call tree"
"\n"
"\n<files with options>: filename followed by switches"
"\n  <file>                if file extension is (.so|.dll) load as library else compile"
"\n  -w[a|x|<int>]         set or disable warning level for current file"
"\n  -b[*]<int>            break point on <int> line in current file"
"\n    /l                  print only, do not pause"
"\n    /L                  print stack trace, do not pause"
"\n    /o                  one shot breakpoint, disable after first hit"
"\n"
"\nexamples:"
"\n"
"\n>app -dump.json api.json"
"\n    dump builtin symbols to `api.json` file (types, functions, variables, aliases)"
"\n"
"\n>app -run test.tracing.ccc"
"\n    compile and execute the file `test.tracing.ccc`"
"\n"
"\n>app -debug gl.so -w0 gl.ccc -w0 test.ccc -wx -b12 -b15 -b/t19"
"\n    execute in debug mode"
"\n    import `gl.so`"
"\n        with no warnings"
"\n    compile `gl.ccc`"
"\n        with no warnings"
"\n    compile `test.ccc`"
"\n        treating all warnings as errors"
"\n        break execution on lines 12 and 15"
"\n        print message when line 19 is hit"
"\n";

static inline int strEquals(const char *str, const char *with) {
	return strcmp(str, with) == 0;
}
static inline int strBegins(const char *str, const char *with) {
	return strncmp(str, with, strlen(with)) == 0;
}

static char *parseInt(const char *str, int32_t *res, int radix) {
	int64_t result = 0;
	int sign = 1;

	//~ ('+' | '-')?
	switch (*str) {
		case '-':
			sign = -1;
		case '+':
			str += 1;
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

	return (char*)str;
}

enum {
	// break point flags
	dbg_pause = 0x01,
	dbg_print = 0x02,
	dbg_trace = 0x04,
	dbg_1shot = 0x08,
};
struct userContextRec {
	rtContext rt;
	FILE *out;
	int indent;

	unsigned hasOut:1;

	unsigned dmpApi:1;      // dump symbols
	unsigned dmpApiAll:1;   // include builtin symbols
	unsigned dmpApiInfo:1;  // dump detailed info
	unsigned dmpApiPrms:1;  // dump parameters
	unsigned dmpApiUsed:1;  // dump usages

	unsigned dmpAsm:1;      // dump disassembled code
	unsigned dmpAsmAddr:1;  // use global address: (@0x003d8c)
	unsigned dmpAsmName:1;  // prefer names over addresses: <main+80>
	unsigned dmpAsmStmt:1;  // print source code statements
	unsigned dmpAsmCode:4;  // print code bytes (0-15)

	unsigned dmpAst:1;      // dump syntax tree
	unsigned dmpAstLine:1;  // dump syntax tree collapsed on a single line
	unsigned dmpAstBody:1;  // dump '{' on new line
	unsigned dmpAstElIf:1;  // dump 'else if' on separate lines

	// execution statistics
	unsigned dmpRunStat:2;  // dump execution statistics
	unsigned dmpVarStat:2;  // dump global variables
	unsigned dmpMemStat:2;  // dump runtime heap

	unsigned dmpRunCall:1;  // dump call tree
	unsigned dmpRunCode:2;  // dump executing instructions

	// debugging
	size_t breakNext;   // break on executing instruction
	int dbgCommand;     // last debugger command
	int dbgOnError;     // on uncaught exception: dbg_xxx
	int dbgOnCaught;    // on caught exception: dbg_xxx
};

// json output
static const int JSON_PRETTY_PRINT = 0;
static const char *JSON_KEY_API = "symbols";
static const char *JSON_KEY_RUN = "profile";

static void jsonDumpSym(FILE *out, const char **esc, symn ptr, const char *kind, int indent) {
	static const char *FMT_START = "%I, \"%s\": {\n";
	static const char *FMT_NEXT = "%I}, {\n";
	static const char *FMT_END = "%I}\n";
	static const char *KEY_PROTO = "proto";
	static const char *KEY_KIND = "kind";
	static const char *KEY_FILE = "file";
	static const char *KEY_LINE = "line";
	static const char *KEY_NAME = "name";
	static const char *KEY_OWNER = "owner";
	static const char *KEY_TYPE = "type";
	static const char *KEY_ARGS = "args";
	static const char *KEY_CNST = "const";
	static const char *KEY_STAT = "static";
	static const char *KEY_MEMB = "member";
	static const char *KEY_SIZE = "size";
	static const char *KEY_OFFS = "offs";
	static const char *VAL_TRUE = "true";
	static const char *VAL_FALSE = "false";

	if (ptr == NULL) {
		return;
	}
	if (kind != NULL) {
		printFmt(out, esc, FMT_START, indent - 1, kind, ptr);
	}

	printFmt(out, esc, "%I\"%s\": \"%T\"\n", indent, KEY_PROTO, ptr);
	printFmt(out, esc, "%I, \"%s\": \"%K\"\n", indent, KEY_KIND, ptr->kind);
	printFmt(out, esc, "%I, \"%s\": \"%.T\"\n", indent, KEY_NAME, ptr);
	if (ptr->owner != NULL) {
		printFmt(out, esc, "%I, \"%s\": \"%?T\"\n", indent, KEY_OWNER, ptr->owner);
	}

	if (ptr->type != NULL) {
		printFmt(out, esc, "%I, \"%s\": \"%T\"\n", indent, KEY_TYPE, ptr->type);
	}
	if (ptr->file != NULL) {
		printFmt(out, esc, "%I, \"%s\": \"%s\"\n", indent, KEY_FILE, ptr->file);
	}
	if (ptr->line != 0) {
		printFmt(out, esc, "%I, \"%s\": %u\n", indent, KEY_LINE, ptr->line);
	}

	if (ptr->params != NULL) {
		symn arg;
		printFmt(out, NULL, "%I, \"%s\": [{\n", indent, KEY_ARGS);
		for (arg = ptr->params; arg; arg = arg->next) {
			if (arg != ptr->params) {
				printFmt(out, NULL, FMT_NEXT, indent);
			}
			jsonDumpSym(out, esc, arg, NULL, indent + 1);
		}
		printFmt(out, NULL, "%I}]\n", indent);
	}
	printFmt(out, esc, "%I, \"%s\": %u\n", indent, KEY_SIZE, ptr->size);
	printFmt(out, esc, "%I, \"%s\": %u\n", indent, KEY_OFFS, ptr->offs);
	printFmt(out, esc, "%I, \"%s\": %s\n", indent, KEY_CNST, ptr->kind & ATTR_cnst ? VAL_TRUE : VAL_FALSE);
	printFmt(out, esc, "%I, \"%s\": %s\n", indent, KEY_STAT, ptr->kind & ATTR_stat ? VAL_TRUE : VAL_FALSE);
	printFmt(out, esc, "%I, \"%s\": %s\n", indent, KEY_MEMB, ptr->kind & ATTR_memb ? VAL_TRUE : VAL_FALSE);
	if (kind != NULL) {
		printFmt(out, esc, FMT_END, indent - 1);
	}
}
static void jsonDumpAst(FILE *out, const char **esc, astn ast, const char *kind, int indent) {
	static const char *KEY_PROTO = "proto";
	static const char *KEY_KIND = "kind";
	static const char *KEY_FILE = "file";
	static const char *KEY_LINE = "line";
	static const char *KEY_TYPE = "type";
	static const char *KEY_STMT = "stmt";
	static const char *KEY_INIT = "init";
	static const char *KEY_TEST = "test";
	static const char *KEY_THEN = "then";
	static const char *KEY_STEP = "step";
	static const char *KEY_ELSE = "else";
	static const char *KEY_ARGS = "args";
	static const char *KEY_LHSO = "lval";
	static const char *KEY_RHSO = "rval";
	static const char *KEY_VALUE = "value";

	if (ast == NULL) {
		return;
	}
	if (kind != NULL) {
		printFmt(out, esc, "%I, \"%s\": {\n", indent, kind);
	}

	printFmt(out, esc, "%I\"%s\": \"%t\"\n", indent + 1, KEY_PROTO, ast);

	printFmt(out, esc, "%I, \"%s\": \"%K\"\n", indent + 1, KEY_KIND, ast->kind);
	if (ast->type != NULL) {
		printFmt(out, esc, "%I, \"%s\": \"%T\"\n", indent + 1, KEY_TYPE, ast->type);
	}
	if (ast->file != NULL) {
		printFmt(out, esc, "%I, \"%s\": \"%s\"\n", indent + 1, KEY_FILE, ast->file);
	}
	if (ast->line != 0) {
		printFmt(out, esc, "%I, \"%s\": %u\n", indent + 1, KEY_LINE, ast->line);
	}
	switch (ast->kind) {
		default:
			fatal(ERR_INTERNAL_ERROR);
			return;
		//#{ STATEMENT
		case STMT_beg: {
			astn list;
			printFmt(out, esc, "%I, \"%s\": [{\n", indent + 1, KEY_STMT);
			for (list = ast->stmt.stmt; list; list = list->next) {
				if (list != ast->stmt.stmt) {
					printFmt(out, esc, "%I}, {\n", indent + 1, list);
				}
				jsonDumpAst(out, esc, list, NULL, indent + 1);
			}
			printFmt(out, esc, "%I}]\n", indent + 1);
			break;
		}

		case STMT_if:
		case STMT_sif:
			jsonDumpAst(out, esc, ast->stmt.test, KEY_TEST, indent + 1);
			jsonDumpAst(out, esc, ast->stmt.stmt, KEY_THEN, indent + 1);
			jsonDumpAst(out, esc, ast->stmt.step, KEY_ELSE, indent + 1);
			break;

		case STMT_for:
			jsonDumpAst(out, esc, ast->stmt.init, KEY_INIT, indent + 1);
			jsonDumpAst(out, esc, ast->stmt.test, KEY_TEST, indent + 1);
			jsonDumpAst(out, esc, ast->stmt.step, KEY_STEP, indent + 1);
			jsonDumpAst(out, esc, ast->stmt.stmt, KEY_STMT, indent + 1);
			break;

		case STMT_con:
		case STMT_brk:
			break;

		case STMT_end:
		case STMT_ret:
			jsonDumpAst(out, esc, ast->stmt.stmt, KEY_STMT, indent + 1);
			break;
		//#}
		//#{ OPERATOR
		case OPER_fnc: {	// '()'
			astn args = ast->op.rhso;
			printFmt(out, esc, "%I, \"%s\": [{\n", indent + 1, KEY_ARGS);
			if (args != NULL) {
				while (args && args->kind == OPER_com) {
					if (args != ast->stmt.stmt) {
						printFmt(out, esc, "%I}, {\n", indent + 1);
					}
					jsonDumpAst(out, esc, args->op.rhso, NULL, indent + 1);
					args = args->op.lhso;
				}
				if (args != ast->stmt.stmt) {
					printFmt(out, esc, "%I}, {\n", indent + 1, args);
				}
				jsonDumpAst(out, esc, args, NULL, indent + 1);
			}
			printFmt(out, esc, "%I}]\n", indent + 1);
			break;
		}

		case OPER_dot:		// '.'
		case OPER_idx:		// '[]'

		case OPER_adr:		// '&'
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

		case OPER_equ:		// '=='
		case OPER_neq:		// '!='
		case OPER_lte:		// '<'
		case OPER_leq:		// '<='
		case OPER_gte:		// '>'
		case OPER_geq:		// '>='

		case OPER_all:		// '&&'
		case OPER_any:		// '||'
		case OPER_sel:		// '?:'

		case OPER_com:		// ','

		case ASGN_set:		// '='
			jsonDumpAst(out, esc, ast->op.test, KEY_TEST, indent + 1);
			jsonDumpAst(out, esc, ast->op.lhso, KEY_LHSO, indent + 1);
			jsonDumpAst(out, esc, ast->op.rhso, KEY_RHSO, indent + 1);
			break;
		//#}
		//#{ VALUE
		case TOKEN_val:
		case TOKEN_var:
			printFmt(out, esc, "%I, \"%s\": \"%t\"\n", indent + 1, KEY_VALUE, ast);
			break;
		//#}
	}

	if (kind != NULL) {
		printFmt(out, esc, "%I}\n", indent);
	}
}
static void jsonDumpAsm(FILE *out, const char **esc, symn sym, rtContext rt, int indent) {
	static const char *FMT_START = "{\n";
	static const char *FMT_NEXT = "%I}, {\n";
	static const char *FMT_END = "%I}";
	static const char *KEY_INSN = "instruction";
	static const char *KEY_OFFS = "offset";

	printFmt(out, esc, FMT_START, indent);

	size_t pc, end = sym->offs + sym->size;
	for (pc = sym->offs; pc < end; ) {
		unsigned char *ip = getip(rt, pc);
		if (pc != sym->offs) {
			printFmt(out, esc, FMT_NEXT, indent, indent);
		}
		printFmt(out, esc, "%I\"%s\": \"%.A\"\n", indent + 1, KEY_INSN, ip);
		printFmt(out, esc, "%I, \"%s\": %u\n", indent + 1, KEY_OFFS, pc);
		pc += opcode_tbl[*ip].size;
	}
	printFmt(out, esc, FMT_END, indent);
}
static void dumpApiJSON(userContext ctx, symn sym) {
	FILE *out = ctx->out;
	int indent = ctx->indent;

	// lazy initialization of escape characters
	static const char **esc = NULL;
	if (esc == NULL) {
		static const char *esc_json[256];
		memset(esc_json, 0, sizeof(esc_json));
		esc_json['\n'] = "\\n";
		esc_json['\r'] = "\\r";
		esc_json['\t'] = "\\t";
		//~ esc_json['\''] = "\\'";
		esc_json['\"'] = "\\\"";
		esc = esc_json;
	}

	if (!ctx->dmpApi && !ctx->dmpAsm && !ctx->dmpAst) {
		return;
	}

	if (sym != ctx->rt->main->fields) {
		// not the first symbol
		printFmt(out, esc, "%I}, {\n", indent);
	}

	jsonDumpSym(out, esc, sym, NULL, indent + 1);

	// export valid syntax tree if we still have compiler context
	if (ctx->dmpAst && sym->init && ctx->rt->cc) {
		jsonDumpAst(out, esc, sym->init, "ast", indent + 1);
	}

	// export assembly
	if (ctx->dmpAsm && isFunction(sym)) {
		printFmt(out, esc, "%I, \"%s\": [", indent + 1, "instructions");
		jsonDumpAsm(out, esc, sym, ctx->rt, indent + 1);
		printFmt(out, esc, "]\n", indent + 1);
	}
}

static dbgn jsonProfile(dbgContext ctx, vmError error, size_t ss, void *stack, void *caller, void *callee) {
	if (callee != NULL) {
		clock_t ticks = clock();
		userContext cc = ctx->extra;
		FILE *out = cc->out;
		const int indent = cc->indent;
		const char **esc = NULL;
		if ((ptrdiff_t) callee < 0) {
			if (JSON_PRETTY_PRINT) {
				printFmt(out, esc, "\n% I%d,%d,-1%s", ss, ticks, ctx->usedMem, ss ? "," : "");
			}
			else {
				printFmt(out, esc, "%d,%d,-1%s", ticks, ctx->usedMem, ss ? "," : "");
			}
			if (ss == 0) {
				int i;
				int covFunc = 0, nFunc = ctx->functions.cnt;
				int covStmt = 0, nStmt = ctx->statements.cnt;
				dbgn dbg = (dbgn) ctx->functions.ptr;

				printFmt(out, esc, "]\n");

				printFmt(out, esc, "%I, \"%s\": [{\n", indent + 1, "functions");
				for (i = 0; i < nFunc; ++i, dbg++) {
					symn sym = dbg->decl;
					if (dbg->hits == 0) {
						// skip functions not invoked
						continue;
					}
					covFunc += 1;
					if (sym == NULL) {
						sym = rtFindSym(ctx->rt, dbg->start, 1);
					}
					if (covFunc > 1) {
						printFmt(out, esc, "%I}, {\n", indent + 1);
					}
					jsonDumpSym(out, esc, sym, NULL, indent + 2);
					printFmt(out, esc, "%I, \"%s\": %d\n", indent + 2, "time", dbg->self);
					printFmt(out, esc, "%I, \"%s\": %d\n", indent + 2, "total", dbg->total);
					printFmt(out, esc, "%I, \"%s\": %d\n", indent + 2, "hits", dbg->hits);
					printFmt(out, esc, "%I, \"%s\": %d\n", indent + 2, "fails", dbg->exec - dbg->hits);
					if (dbg->file != NULL && dbg->line > 0) {
						printFmt(out, esc, "%I, \"%s\": \"%s\"\n", indent + 2, "file", dbg->file);
						printFmt(out, esc, "%I, \"%s\": %d\n", indent + 2, "line", dbg->line);
					}
				}
				printFmt(out, esc, "%I}]\n", indent + 1);

				printFmt(out, esc, "%I, \"%s\": [{\n", indent + 1, "statements");
				dbg = (dbgn) ctx->statements.ptr;
				for (i = 0; i < nStmt; ++i, dbg++) {
					size_t symOffs = 0;
					symn sym = dbg->decl;
					if (dbg->hits == 0) {
						continue;
					}
					covStmt += 1;
					if (sym == NULL) {
						sym = rtFindSym(ctx->rt, dbg->start, 1);
					}
					if (sym != NULL) {
						symOffs = dbg->start - sym->offs;
					}
					if (covStmt > 1) {
						printFmt(out, esc, "%I}, {\n", indent + 1);
					}
					printFmt(out, esc, "%I\"%s\": \"%?T+%d\"\n", indent + 2, "", sym, symOffs);
					printFmt(out, esc, "%I, \"%s\": %d\n", indent + 2, "offs", dbg->start);
					printFmt(out, esc, "%I, \"%s\": %d\n", indent + 2, "time", dbg->self);
					printFmt(out, esc, "%I, \"%s\": %d\n", indent + 2, "total", dbg->total);
					printFmt(out, esc, "%I, \"%s\": %d\n", indent + 2, "hits", dbg->hits);
					printFmt(out, esc, "%I, \"%s\": %d\n", indent + 2, "fails", dbg->exec - dbg->hits);
					if (dbg->file != NULL && dbg->line > 0) {
						printFmt(out, esc, "%I, \"%s\": \"%s\"\n", indent + 2, "file", dbg->file);
						printFmt(out, esc, "%I, \"%s\": %d\n", indent + 2, "line", dbg->line);
					}
				}
				printFmt(out, esc, "%I}]\n", indent + 1);

				printFmt(out, esc, "%I, \"%s\": %d\n", indent + 1, "ticksPerSec", CLOCKS_PER_SEC);
				printFmt(out, esc, "%I, \"%s\": %d\n", indent + 1, "functionCount", ctx->functions.cnt);
				printFmt(out, esc, "%I, \"%s\": %d\n", indent + 1, "statementCount", ctx->statements.cnt);
				printFmt(out, esc, "%I}", indent);
			}
		}
		else {
			size_t offs = vmOffset(ctx->rt, callee);
			if (ss == 0) {
				printFmt(out, esc, "\n%I%s\"%s\": {\n", indent, cc->hasOut ? ", " : "", JSON_KEY_RUN);
				printFmt(out, esc, "%I\"%s\": [", indent + 1, "callTreeData");
				printFmt(out, esc, "\"%s\", ", "ctTickIndex");
				printFmt(out, esc, "\"%s\", ", "ctHeapIndex");
				printFmt(out, esc, "\"%s\"", "ctFunIndex");
				printFmt(out, esc, "]\n");

				printFmt(out, esc, "%I, \"%s\": [", indent + 1, "callTree");
			}
			if (JSON_PRETTY_PRINT) {
				printFmt(out, esc, "\n% I%d,%d,%d,", ss, ticks, ctx->usedMem, offs);
			}
			else {
				printFmt(out, esc, "%d,%d,%d,", ticks, ctx->usedMem, offs);
			}
		}
	}
	(void)caller;
	(void)error;
	(void)stack;
	return NULL;
}

// text output
static void conDumpAsm(FILE *out, const char **esc, void *ip, userContext ctx, int indent) {
	size_t offs = vmOffset(ctx->rt, ip);
	int mode = ctx->dmpAsmCode;

	if (ctx->dmpAsmAddr) {
		mode |= prAsmAddr;
	}
	if (ctx->dmpAsmName) {
		mode |= prAsmName;
	}
	mode |= prFull | prOneLine;

	if (ctx->dmpAsmStmt && ctx->rt->cc != NULL) {
		dbgn dbg = mapDbgStatement(ctx->rt, offs);
		if (dbg != NULL && dbg->stmt != NULL && dbg->start == offs) {
			printFmt(out, esc, "%I%?s:%?u: [%06x-%06x): %.*t\n", indent, dbg->file, dbg->line, dbg->start, dbg->end, mode, dbg->stmt);
		}
	}

	printFmt(out, esc, "%I", indent);
	printAsm(out, esc, ctx->rt, ip, mode);
	printFmt(out, esc, "\n");
}
static void conDumpMem(rtContext rt, void *ptr, size_t size, char *kind) {
	userContext ctx = rt->dbg->extra;
	const char **esc = NULL;
	FILE *out = ctx->out;

	char *unit = "bytes";
	float value = 0;

	if (size > (1 << 30)) {
		unit = "Gb";
		value = size;
		value /= (1 << 30);
	}
	else if (size > (1 << 20)) {
		unit = "Mb";
		value = size;
		value /= (1 << 20);
	}
	else if (size > (1 << 10)) {
		unit = "Kb";
		value = size;
		value /= (1 << 10);
	}

	printFmt(out, esc, "memory[%s] @%06x; size: %d(%?.1f %s)\n", kind, vmOffset(rt, ptr), size, value, unit);
}
static void conDumpProf(userContext usr) {
	const char **esc = NULL;
	FILE *out = usr->out;
	rtContext rt = usr->rt;
	dbgContext dbg = rt->dbg;

	if (usr->dmpRunStat) {
		int i, all = usr->dmpRunStat > 1;
		int covFunc = 0, nFunc = dbg->functions.cnt;
		int covStmt = 0, nStmt = dbg->statements.cnt;
		dbgn fun = (dbgn) dbg->functions.ptr;

		printFmt(out, esc, "\n/*-- Profile:\n");

		for (i = 0; i < nFunc; ++i, fun++) {
			symn sym = fun->decl;
			if (fun->hits == 0) {
				continue;
			}
			covFunc += 1;
			if (sym == NULL) {
				sym = rtFindSym(rt, fun->start, 1);
			}
			printFmt(out, esc,
				"%?s:%?u:[.%06x, .%06x): <%?T> hits(%D/%D), time(%D%?+D / %.3F%?+.3F ms)\n", fun->file,
				fun->line, fun->start, fun->end, sym, (int64_t) fun->hits, (int64_t) fun->exec,
				(int64_t) fun->total, (int64_t) -(fun->total - fun->self),
				fun->total / (double) CLOCKS_PER_SEC, -(fun->total - fun->self) / (double) CLOCKS_PER_SEC
			);
		}

		if (all) {
			printFmt(out, esc, "\n//-- statements:\n");
		}
		fun = (dbgn) rt->dbg->statements.ptr;
		for (i = 0; i < nStmt; ++i, fun++) {
			size_t symOffs = 0;
			symn sym = fun->decl;
			if (fun->hits == 0) {
				continue;
			}
			covStmt += 1;
			if (sym == NULL) {
				sym = rtFindSym(rt, fun->start, 1);
			}
			if (sym != NULL) {
				symOffs = fun->start - sym->offs;
			}
			if (all) {
				printFmt(out, esc,
					"%?s:%?u:[.%06x, .%06x): <%?T+%d> hits(%D/%D), time(%D%?+D / %.3F%?+.3F ms)\n", fun->file,
					fun->line, fun->start, fun->end, sym, symOffs, (int64_t) fun->hits, (int64_t) fun->exec,
					(int64_t) fun->total, (int64_t) -(fun->total - fun->self),
					fun->total / (double) CLOCKS_PER_SEC, -(fun->total - fun->self) / (double) CLOCKS_PER_SEC
				);
			}
		}

		printFmt(out, esc, "\n//-- coverage(functions: %.2f%%(%d/%d), statements: %.2f%%(%d/%d))\n",
			covFunc * 100. / (nFunc ?: 1), covFunc, nFunc, covStmt * 100. / (nStmt ?: 1), covStmt, nStmt
		);

		printFmt(out, esc, "// */\n");
	}

	if (usr->dmpVarStat != 0) {
		int all = usr->dmpVarStat > 1;
		symn var;
		printFmt(out, esc, "\n/*-- Globals:\n");
		for (var = rt->main->fields; var; var = var->next) {
			char *ofs = NULL;

			// exclude types and aliases
			switch (var->kind & MASK_kind) {
				case KIND_def:
				case KIND_typ:
					continue;

				default:
					break;
			}

			// exclude functions
			if (var->params != NULL && !all)
				continue;

			// exclude null
			if (var->offs == 0)
				continue;

			if (var->file && var->line) {
				printFmt(out, esc, "%s:%u: ", var->file, var->line);
			} else if (!all) {
				// exclude internals
				continue;
			}

			if (var->kind & ATTR_stat) {
				// static variable.
				ofs = (char *) rt->_mem + var->offs;
			} else {
				// argument or local variable.
				ofs = (char *) rt->vm.cell - var->offs;
			}

			printVal(out, esc, rt, var, (stkval *) ofs, prSymQual | prSymType, 0);
			printFmt(out, esc, "\n");
		}
		printFmt(out, esc, "// */\n");
	}

	if (usr->dmpMemStat != 0) {
		int all = usr->dmpMemStat > 1;
		// show allocated memory chunks.
		printFmt(out, esc, "\n/*-- Allocations:\n");
		if (all) {
			conDumpMem(rt, rt->_mem, rt->vm.ro, "meta");
			conDumpMem(rt, rt->_mem, rt->vm.px + px_size, "code");
			conDumpMem(rt, NULL, rt->vm.ss, "stack");
			conDumpMem(rt, rt->_beg, rt->_end - rt->_beg, "heap");
		}
		rtAlloc(rt, NULL, 0, conDumpMem);
		printFmt(out, esc, "// */\n");
	}
}

static void dumpApiText(userContext extra, symn sym) {
	int dmpAsm = 0;
	int dmpAst = 0;
	int dumpExtraData = 0;
	const char **esc = NULL;
	FILE *out = extra->out;
	int indent = extra->indent;

	if (extra->dmpAst && extra->rt->cc && sym->init != NULL) {
		// dump valid syntax trees only
		dmpAst = 1;
	}

	if (extra->dmpAsm && isFunction(sym)) {
		dmpAsm = 1;
	}

	if (sym->file == NULL && sym->line == 0 && !extra->dmpApiAll) {
		return;
	}
	if (!extra->dmpApi && !dmpAsm && !dmpAst) {
		return;
	}

	// print qualified name with arguments and type
	printFmt(out, esc, "%I%T: %T", extra->indent, sym, sym->type);

	// print symbol info (kind, size, offset, ...)
	if (extra->dmpApi && extra->dmpApiInfo) {
		if (!dumpExtraData) {
			printFmt(out, esc, " {\n");
			dumpExtraData = 1;
		}
		printFmt(out, esc, "%I.kind: %K\n", indent, sym->kind);
		printFmt(out, esc, "%I.offset: %06x\n", indent, sym->offs);
		printFmt(out, esc, "%I.size: %d\n", indent, sym->size);
		printFmt(out, esc, "%I.owner: %?T\n", indent, sym->owner);
	}

	// explain params of the function
	if (extra->dmpApi && extra->dmpApiPrms) {
		symn param;
		if (!dumpExtraData) {
			printFmt(out, esc, " {\n");
			dumpExtraData = 1;
		}
		for (param = sym->fields; param; param = param->next) {
			printFmt(out, esc, "%I.field %.T: %?T (size: %d @%d -> %K)\n", indent, param, param->type, param->size, param->offs, param->kind);
		}
		for (param = sym->params; param; param = param->next) {
			printFmt(out, esc, "%I.param %.T: %?T (@%+d->%K)\n", indent, param, param->type, param->offs, param->kind);
		}
	}

	if (dmpAst) {
		int mode = prFull;
		if (!dumpExtraData) {
			printFmt(out, esc, " {\n");
			dumpExtraData = 1;
		}
		printFmt(out, esc, "%I.value: ", indent);
		if (extra->dmpAstBody) {
			mode |= nlAstBody;
		}
		if (extra->dmpAstElIf) {
			mode |= nlAstElIf;
		}
		if (extra->dmpAstLine) {
			mode |= prOneLine;
		}
		printAst(out, esc, sym->init, mode, -indent);

		if (extra->dmpAstLine || sym->init->kind != STMT_beg) {
			printFmt(out, esc, "\n");
		}
	}

	// print function disassembled instructions
	if (dmpAsm != 0) {
		size_t pc, end = sym->offs + sym->size;

		if (!dumpExtraData) {
			printFmt(out, esc, " {\n");
			dumpExtraData = 1;
		}
		printFmt(out, esc, "%I.instructions: [%d bytes @.%06x]\n", indent, sym->size, sym->offs);
		for (pc = sym->offs; pc < end; ) {
			unsigned char *ip = getip(extra->rt, pc);
			size_t ppc = pc;

			if (ip == NULL) {
				break;
			}
			conDumpAsm(out, esc, ip, extra, indent + 1);
			pc += opcode_tbl[*ip].size;
			if (pc == ppc) {
				break;
			}
		}
	}

	// print usages of symbol
	if (extra->dmpApi && extra->dmpApiUsed) {
		astn usage;
		int extUsages = 0;
		if (!dumpExtraData) {
			printFmt(out, esc, " {\n");
			dumpExtraData = 1;
		}
		printFmt(out, esc, "%I.references:\n", indent);
		for (usage = sym->use; usage; usage = usage->ref.used) {
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
	const char **esc = NULL;
	FILE *out = extra->out;
	if (sym->params != NULL) {
		printFmt(out, esc, "%T: %T", sym, sym->params->type);
	}
	else {
		printFmt(out, esc, "%T", sym);
	}
	printFmt(out, esc, "\n");
}

// console debugger and profiler
static dbgn conDebug(dbgContext ctx, vmError error, size_t ss, void *stack, void *caller, void *callee) {
	enum {
		dbgAbort = 'q',		// stop debugging
		dbgResume = 'r',	// break on next breakpoint or error

		dbgStepNext = 'a',	// break on next instruction
		dbgStepLine = 'n',	// break on next statement
		dbgStepInto = 'i',	// break on next function
		dbgStepOut = 'o',	// break on next return

		dbgPrintStackTrace = 't',	// print stack trace
		dbgPrintStackValues = 's',	// print values on sack
		dbgPrintInstruction = 'p',	// print current instruction
	};

	char buff[1024];
	rtContext rt = ctx->rt;
	userContext usr = ctx->extra;

	const char **esc = NULL;
	FILE *out = stdout;

	void *ip = caller;
	int breakMode = 0;
	char *breakCause = NULL;
	size_t offs = vmOffset(rt, ip);
	symn fun = rtFindSym(rt, offs, 0);
	dbgn dbg = mapDbgStatement(rt, offs);

	// enter or leave
	if (callee != NULL) {
		// TODO: enter will break after executing call?
		// TODO: leave will break after executing return?
		if (usr->dbgCommand == dbgStepInto && callee > 0) {
			usr->breakNext = (size_t) -1;
		}
		else if (usr->dbgCommand == dbgStepOut && callee < 0) {
			usr->breakNext = (size_t) -1;
		}

		if (usr->dmpRunCall) {
			clock_t ticks = clock();
			if ((ptrdiff_t) callee > 0) {
				size_t offs = vmOffset(rt, callee);
				printFmt(out, esc, " % I> %d, %d, 0x%06x %?T\n", ss, ticks, ctx->usedMem, offs, rtFindSym(rt, offs, 1));
			}
			else {
				printFmt(out, esc, " % I< %d, %d\n", ss, ticks, ctx->usedMem);
			}
		}
		return NULL;
	}

	if (error != noError) {
		switch (error) {
			default:
				breakCause = "Unknown error";
				break;

			case noError:
				breakCause = "Breakpoint";
				break;

			case invalidIP:
				breakCause = "Invalid instruction pointer";
				break;

			case invalidSP:
				breakCause = "Invalid stack pointer";
				break;

			case illegalInstruction:
				breakCause = "Invalid instruction";
				break;

			case stackOverflow:
				breakCause = "Stack Overflow";
				break;

			case divisionByZero:
				breakCause = "Division by Zero";
				break;

			case libCallAbort:
				breakCause = "External call abort";
				break;

			case memReadError:
				breakCause = "Access violation reading memory";
				break;

			case memWriteError:
				breakCause = "Access violation writing memory";
				break;
		}
		if (ctx->checked) {
			breakMode = usr->dbgOnCaught;
		}
		else {
			breakMode = usr->dbgOnError;
		}
	}
	else if (usr->breakNext == (size_t)-1) {
		breakMode = dbg_pause;
		breakCause = "Break";
	}
	else if (usr->breakNext == offs) {
		breakMode = dbg_pause;
		breakCause = "Break";
	}
	else if (dbg != NULL) {
		if (offs == dbg->start) {
			breakMode = dbg->bp;
			if (breakMode & dbg_pause) {
				breakCause = "Break point";
			}
			if (breakMode & dbg_print) {
				breakCause = "Break point";
			}
			if (breakMode & dbg_trace) {
				breakCause = "Trace point";
			}
			if (breakMode & dbg_1shot) {
				// remove breakpoint
				dbg->bp = 0;
			}
		}
	}

	// print executing instruction.
	if (error == noError && usr->dmpRunCode) {
		if (usr->dmpRunCode > 1) {
			const int MAX_ELEMENTS = 5;
			size_t i, sm = ss;
			printFmt(out, esc, "\tstack(%d): [", ss);
			if (sm > MAX_ELEMENTS) {
				printFmt(out, esc, " …,");
				sm = MAX_ELEMENTS;
			}
			for (i = 0; i < sm; i++) {
				if (i > 0) {
					printFmt(out, esc, ",");
				}
				printFmt(out, esc, " 0x%08x", ((int32_t *) stack)[ss - i - 1]);
			}
			printFmt(out, esc, "\n");
		}
		conDumpAsm(out, esc, ip, usr, 0);
	}

	// print error type
	if (breakMode & (dbg_print | dbg_trace)) {
		size_t funOffs = offs;
		if (fun != NULL) {
			funOffs = offs - fun->offs;
		}
		// print current file position
		if (dbg != NULL && dbg->file != NULL && dbg->line > 0) {
			printFmt(out, esc, "%s:%u: ", dbg->file, dbg->line);
		}
		printFmt(out, esc, "%s in function: <%.T%?+d>\n", breakCause, fun, funOffs);
		if (breakMode & dbg_trace) {
			logTrace(ctx, out, 1, 0, 20);
		}
	}

	// pause execution in debugger
	for ( ; breakMode & dbg_pause; ) {
		int cmd = usr->dbgCommand;
		char *arg = NULL;
		printFmt(out, esc, ">dbg[%?c] %.A: ", cmd, ip);
		if (fgets(buff, sizeof(buff), stdin) == NULL) {
			printFmt(out, esc, "can not read from standard input, resuming\n");
		}
		if ((arg = strchr(buff, '\n'))) {
			*arg = 0;		// remove new line char
		}

		if (*buff != 0) {
			// update last command
			cmd = buff[0];
			arg = buff + 1;
			usr->dbgCommand = cmd;
		}

		switch (cmd) {
			default:
				printFmt(out, esc, "invalid command '%s'\n", buff);
				break;

			case dbgAbort:
				return ctx->abort;

			case dbgResume:
			case dbgStepInto:
			case dbgStepOut:
				usr->breakNext = 0;
				return 0;

			case dbgStepNext:
				usr->breakNext = (size_t)-1;
				return 0;

			case dbgStepLine:
				usr->breakNext = dbg ? dbg->end : 0;
				return 0;

			case dbgPrintStackTrace:
				logTrace(ctx, out, 1, 0, 20);
				break;

			case dbgPrintInstruction:
				conDumpAsm(out, esc, ip, usr, 0);
				break;

			case dbgPrintStackValues:
				if (*arg == 0) {
					// print top of stack
					for (offs = 0; offs < ss; offs++) {
						stkval *v = (stkval*)&((long*)stack)[offs];
						printFmt(out, esc, "\tsp(%d): {0x%08x, i32(%d), f32(%f), i64(%D), f64(%F)}\n", offs, v->i4, v->i4, v->f4, v->i8, v->f8);
					}
				}
				else {
					symn sym = ccLookupSym(rt->cc, NULL, arg);
					printFmt(out, esc, "arg:%T", sym);
					if (sym && sym->kind == KIND_var && !(sym->kind & ATTR_stat)) {
						printVal(out, esc, rt, sym, (stkval *) stack, prSymType, 0);
					}
				}
				break;
		}
	}
	return dbg;
}

static void dumpAstXML(FILE *out, const char **esc, astn ast, int mode, int indent, const char *text) {
	astn list;

	if (ast == NULL) {
		return;
	}

	printFmt(out, esc, "%I<%s token=\"%k\"", indent, text, ast->kind);

	if ((mode & (prSymType|prAstType)) != 0) {
		printFmt(out, esc, " type=\"%T\"", ast->type);
	}

	if (ast->kind == STMT_beg && ast->file != NULL && ast->line > 0) {
		printFmt(out, esc, " file=\"%s:%d\"", ast->file, ast->line);
	}

	switch (ast->kind) {
		default:
			fatal(ERR_INTERNAL_ERROR);
			return;

		//#{ STMT
		case STMT_beg:
			printFmt(out, esc, ">\n");
			for (list = ast->stmt.stmt; list; list = list->next) {
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
			printFmt(out, esc, " />\n");
			break;

		case STMT_ret:
			printFmt(out, esc, " stmt=\"%?t\">\n", ast);
			dumpAstXML(out, esc, ast->stmt.stmt, mode, indent + 1, "expr");
			printFmt(out, esc, "%I</%s>\n", indent, text);
			break;

		//#}
		//#{ OPER
		case OPER_fnc:		// '()'
			//printFmt(out, esc, ">\n");
			printFmt(out, esc, " value=\"%?t\">\n", ast);
			list = ast->op.rhso;
			while (list != NULL) {
				astn arg = list;
				if (list->kind == OPER_com) {
					list = list->op.rhso;
					arg = arg->op.lhso;
				}
				else {
					list = NULL;
				}
				dumpAstXML(out, esc, arg, mode, indent + 1, "push");
			}
			dumpAstXML(out, esc, ast->op.lhso, mode, indent + 1, "call");
			printFmt(out, esc, "%I</%s>\n", indent, text);
			break;

		case OPER_dot:		// '.'
		case OPER_idx:		// '[]'

		case OPER_adr:		// '&'
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

		case OPER_equ:		// '=='
		case OPER_neq:		// '!='
		case OPER_lte:		// '<'
		case OPER_leq:		// '<='
		case OPER_gte:		// '>'
		case OPER_geq:		// '>='

		case OPER_all:		// '&&'
		case OPER_any:		// '||'
		case OPER_sel:		// '?:'

		case OPER_com:		// ','

		case ASGN_set:		// '='
			printFmt(out, esc, " value=\"%?t\">\n", ast);
			dumpAstXML(out, esc, ast->op.test, mode, indent + 1, "test");
			dumpAstXML(out, esc, ast->op.lhso, mode, indent + 1, "lval");
			dumpAstXML(out, esc, ast->op.rhso, mode, indent + 1, "rval");
			printFmt(out, esc, "%I</%s>\n", indent, text);
			break;

		//#}
		//#{ VAL
		case TOKEN_val:
		case TOKEN_var: {
			symn link = ast->ref.link;
			// declaration
			if (link != NULL && link->tag == ast) {
				if (link->init != NULL) {
					printFmt(out, esc, " value=\"%?t\">\n", ast);
					dumpAstXML(out, esc, link->init, mode, indent + 1, "init");
					printFmt(out, esc, "%I</%s>\n", indent, text);
				}
				else {
					printFmt(out, esc, " value=\"%?t\"/>\n", ast);
				}
			}
			// usage
			else {
				printFmt(out, esc, " oper=\"%?t\"/>\n", ast);
			}
			break;
		}
		//#}
	}
}

static int program(int argc, char *argv[]) {
	struct {

		int32_t logAppend: 1;

		int32_t foldConst: 1;
		int32_t foldInstr: 1;
		int32_t fastAssign: 1;
		int32_t genGlobals: 1;

		int32_t addPointer: 1;
		int32_t addVariant: 1;
		int32_t addObject: 1;
		int32_t addEmit: 1;
		int32_t stdLibs: 1;

		ccInstall install;
		size_t memory;
	} settings = {
		.logAppend = 0,

		.foldConst = 1,
		.foldInstr = 1,
		.fastAssign = 1,
		.genGlobals = 1,

		.addPointer = (install_def & install_ptr) != 0,
		.addVariant = (install_def & install_var) != 0,
		.addObject = (install_def & install_obj) != 0,
		.addEmit = (install_def & installEmit) != 0,
		.stdLibs = 1,

		.install = install_def,
		.memory = sizeof(mem)
	};

	struct userContextRec extra = {
		.breakNext = 0,
		.dbgCommand = 'r',	// last command: resume
		.dbgOnError = dbg_pause | dbg_print,
		.dbgOnCaught = 0,

		.dmpApi = 0,
		.dmpApiAll = 0,
		.dmpApiInfo = 0,
		.dmpApiPrms = 0,
		.dmpApiUsed = 0,

		.dmpAsm = 0,
		.dmpAsmAddr = 0,
		.dmpAsmName = 0,
		.dmpAsmStmt = 0,
		.dmpAsmCode = 9,

		.dmpAst = 0,
		.dmpAstLine = 0,
		.dmpAstBody = 0,
		.dmpAstElIf = 0,

		.out = stdout,
		.hasOut = 0,
		.indent = 0,

		.rt = NULL
	};

	rtContext rt = NULL;

	char *stdLib = (char*)STDLIB;	// standard library
	char *ccFile = NULL;			// compiled filename
	char *logFile = NULL;			// logger filename
	FILE *dumpFile = NULL;			// dump file
	void (*dumpFun)(userContext, symn) = NULL;
	enum {run, debug, profile, skipExecution} run_code = skipExecution;

	int i, warn, errors = 0;

	// TODO: max 32 break points ?
	char *bp_file[32];
	int bp_line[32];
	int bp_type[32];
	int bp_size = 0;

	// process global options
	for (i = 1; i < argc; ++i) {
		char *arg = argv[i];

		// override stdlib file
		if (strncmp(arg, "-std", 4) == 0) {
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
		// enable or disable settings
		else if (strBegins(arg, "-X")) {
			char *arg2 = arg + 2;
			while (*arg2 == '-' || *arg2 == '+') {
				int on = *arg2 == '+';
				if (strBegins(arg2 + 1, "fold")) {
					settings.foldConst = on;
					arg2 += 5;
				}
				else if (strBegins(arg2 + 1, "fast")) {
					settings.foldInstr = on;
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
					settings.addEmit = on;
					arg2 += 5;
				}
				else if (strBegins(arg2 + 1, "ptr")) {
					settings.addPointer = on;
					arg2 += 4;
				}
				else if (strBegins(arg2 + 1, "var")) {
					settings.addVariant = on;
					arg2 += 4;
				}
				else if (strBegins(arg2 + 1, "obj")) {
					settings.addObject = on;
					arg2 += 4;
				}
				else if (strBegins(arg2 + 1, "std")) {
					settings.stdLibs = on;
					arg2 += 4;
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
		// override heap size
		else if (strncmp(arg, "-mem", 4) == 0) {
			int value = -1;
			char *arg2 = arg + 4;
			if (settings.memory != sizeof(mem)) {
				fatal("argument specified multiple times: %s", arg);
				return -1;
			}
			settings.memory = 1;
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

		// logger filename
		else if (strncmp(arg, "-log", 4) == 0) {
			char *arg2 = arg + 4;
			if (++i >= argc || logFile) {
				fatal("log file not or double specified");
				return -1;
			}
			logFile = argv[i];
			while (*arg2 == '/') {
				switch (arg2[1]) {
					default:
						arg2 += 1;
						break;

					case 'a':
						settings.logAppend = 1;
						arg2 += 2;
						break;
				}
			}
			if (*arg2) {
				fatal("invalid argument '%s'", arg);
				return -1;
			}
		}

		// output api, ast, asm
		else if (strncmp(arg, "-api", 4) == 0) {
			char *arg2 = arg + 4;
			if (extra.dmpApi != 0) {
				fatal("argument specified multiple times: %s", arg);
				return -1;
			}
			extra.dmpApi = 1;
			while (*arg2 == '/') {
				switch (arg2[1]) {
					default:
						arg2 += 1;
						break;
					case 'a':
						extra.dmpApiAll = 1;
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
			if (extra.dmpAsm) {
				fatal("argument specified multiple times: %s", arg);
				return -1;
			}
			extra.dmpAsm = 1;
			extra.dmpAsmCode = 9;
			while (*arg2 == '/') {
				switch (arg2[1]) {
					default:
						arg2 += 1;
						break;
					case 's':
						extra.dmpAsmStmt = 1;
						arg2 += 2;
						break;
					case 'a':
						extra.dmpAsmAddr = 1;
						arg2 += 2;
						break;
					case 'n':
						extra.dmpAsmName = 1;
						arg2 += 2;
						break;
				}
			}
			if (*arg2) {
				fatal("invalid argument '%s'", arg);
				return -1;
			}
		}
		else if (strncmp(arg, "-ast", 4) == 0) {
			char *arg2 = arg + 4;
			if (extra.dmpAst) {
				fatal("argument specified multiple times: %s", arg);
				return -1;
			}
			extra.dmpAst = 1;
			while (*arg2 == '/') {
				switch (arg2[1]) {
					default:
						arg2 += 1;
						break;
					case 'l':
						extra.dmpAstLine = 1;
						arg2 += 2;
						break;
					case 'b':
						extra.dmpAstBody = 1;
						arg2 += 2;
						break;
					case 'e':
						extra.dmpAstElIf = 1;
						arg2 += 2;
						break;
				}
			}
			if (*arg2) {
				fatal("invalid argument '%s'", arg);
				return -1;
			}
		}
		// output format and filename
		else if (strncmp(arg, "-dump", 5) == 0) {
			if (++i >= argc || dumpFile) {
				fatal("dump file not or double specified");
				return -1;
			}
			dumpFile = fopen(argv[i], "w");
			if (dumpFile == NULL) {
				fatal("error opening dump file: %s", argv[i]);
				return -1;
			}
			if (strEquals(arg, "-dump.scite")) {
				dumpFun = dumpApiSciTE;
			}
			else if (strEquals(arg, "-dump.json")) {
				dumpFun = dumpApiJSON;
			}
			else if (strEquals(arg, "-dump")) {
				dumpFun = dumpApiText;
			}
			else {
				fatal("invalid argument '%s'", arg);
				return -1;
			}
		}

		// run, debug or profile
		else if (strncmp(arg, "-run", 4) == 0) {		// execute code in release mode
			char *arg2 = arg + 4;
			if (run_code != skipExecution) {
				fatal("argument specified multiple times: %s", arg);
				return -1;
			}
			if (*arg2) {
				fatal("invalid argument '%s'", arg);
				return -1;
			}
			run_code = run;
		}
		else if (strncmp(arg, "-debug", 6) == 0) {		// execute code in debug mode
			char *arg2 = arg + 6;
			if (run_code != skipExecution) {
				fatal("argument specified multiple times: %s", arg);
				return -1;
			}
			run_code = debug;
			while (*arg2 == '/') {
				switch (arg2[1]) {
					default:
						arg2 += 1;
						break;

					// break, print, trace ...
					case 's':
						extra.breakNext = (size_t)-1;
						arg2 += 2;
						break;
					case 'a':
						extra.dbgOnCaught |= dbg_pause;
						arg2 += 2;
						break;
					case 'l':
						extra.dbgOnCaught |= dbg_print;
						arg2 += 2;
						break;
					case 'L':
						extra.dbgOnCaught |= dbg_trace;
						arg2 += 2;
						break;

					// dump stats
					case 'p':
						extra.dmpRunStat = 1;
						arg2 += 2;
						break;
					case 'g':
						extra.dmpVarStat = 1;
						arg2 += 2;
						break;
					case 'h':
						extra.dmpMemStat = 1;
						arg2 += 2;
						break;
				}
			}
			if (*arg2) {
				fatal("invalid argument '%s'", arg);
				return -1;
			}
		}
		else if (strncmp(arg, "-profile", 8) == 0) {		// execute code in profile mode
			char *arg2 = arg + 8;
			if (run_code != skipExecution) {
				fatal("argument specified multiple times: %s", arg);
				return -1;
			}
			run_code = profile;
			while (*arg2 == '/') {
				switch (arg2[1]) {
					default:
						arg2 += 1;
						break;

					// dump stats
					case 't':
						extra.dmpRunCall = 1;
						arg2 += 2;
						break;
					case 'p':
					case 'P':
						extra.dmpRunStat = arg2[1] == 'p' ? 1 : 2;
						arg2 += 2;
						break;
					case 'g':
					case 'G':
						extra.dmpVarStat = arg2[1] == 'g' ? 1 : 2;
						arg2 += 2;
						break;
					case 'h':
					case 'H':
						extra.dmpMemStat = arg2[1] == 'h' ? 1 : 2;
						arg2 += 2;
						break;
				}
			}
			if (*arg2) {
				fatal("invalid argument '%s'", arg);
				return -1;
			}
		}

		// no more global options
		else break;
	}

	// initialize runtime context
	if (settings.memory > sizeof(mem)) {
		rt = rtInit(NULL, settings.memory);
	}
	else {
		rt = rtInit(mem, settings.memory);
	}

	if (rt == NULL) {
		fatal("initializing runtime context");
		return -1;
	}
	else {
		settings.install &= ~(install_ptr | install_var | install_obj | installEmit);
		settings.install |= install_ptr * settings.addPointer;
		settings.install |= install_var * settings.addVariant;
		settings.install |= install_obj * settings.addObject;
		settings.install |= installEmit * settings.addEmit;

		rt->foldConst = settings.foldConst;
		rt->foldInstr = settings.foldInstr;
		rt->fastAssign = settings.fastAssign;
		rt->genGlobals = settings.genGlobals;
	}

	// open log file (global option)
	if (logFile && !logfile(rt, logFile, settings.logAppend)) {
		info(rt, NULL, 0, ERR_OPENING_FILE, logFile);
	}

	// install base type system.
	if (!ccInit(rt, settings.install, NULL)) {
		error(rt, NULL, 0, "error registering base types");
		logfile(rt, NULL, 0);
		return -6;
	}

	if (settings.stdLibs) {
		// install standard library.
		if (!ccAddLib(rt, wl, ccLibStdc, stdLib)) {
			error(rt, NULL, 0, "error registering standard library");
			logfile(rt, NULL, 0);
			return -6;
		}

		// install file operations.
		if (!ccAddLib(rt, wl, ccLibFile, NULL)) {
			error(rt, NULL, 0, "error registering file library");
			logfile(rt, NULL, 0);
			return -6;
		}
	}

	// compile and import files / modules
	for (warn = -1; i <= argc; ++i) {
		char *arg = argv[i];
		if (i == argc || *arg != '-') {
			if (ccFile != NULL) {
				char *ext = strrchr(ccFile, '.');
				if (ext && (strEquals(ext, ".so") || strEquals(ext, ".dll"))) {
					int resultCode = importLib(rt, ccFile);
					if (resultCode != 0) {
						error(rt, NULL, 0, "error(%d) importing library `%s`", resultCode, ccFile);
					}
				}
				else if (!ccAddUnit(rt, warn == -1 ? wl : warn, ccFile, 1, NULL)) {
					error(rt, NULL, 0, "error compiling source `%s`", arg);
				}
			}
			ccFile = arg;
			warn = -1;
		}
		else {
			if (ccFile == NULL) {
				error(rt, NULL, 0, "argument `%s` must be preceded by a file", arg);
			}
			if (arg[1] == 'w') {		// warning level for file
				if (warn != -1) {
					info(rt, NULL, 0, "argument overwrites previous value: %d", warn);
				}
				if (strcmp(arg, "-wx") == 0) {
					warn = -2;
				}
				else if (strcmp(arg, "-wa") == 0) {
					warn = 32;
				}
				else if (*parseInt(arg + 2, &warn, 10)) {
					error(rt, NULL, 0, "invalid warning level '%s'", arg + 2);
				}
			}
			else if (arg[1] == 'b') {
				int line = 0;
				int type = dbg_pause;
				char *arg2 = arg + 2;
				if (bp_size > (int)lengthOf(bp_file)) {
					info(rt, NULL, 0, "can not add more than %d breakpoints.", lengthOf(bp_file));
				}
				while (*arg2 == '/') {
					switch (arg2[1]) {
						default:
							arg2 += 1;
							break;

						case 'p': // print only
							type &= ~dbg_pause;
							type |= dbg_print;
							arg2 += 2;
							break;

						case 't': // trace only
							type &= ~dbg_pause;
							type |= dbg_trace;
							arg2 += 2;
							break;

						case 'o': // remove after first hit
							type |= dbg_1shot;
							arg2 += 2;
							break;
					}
				}

				if (*parseInt(arg2, &line, 10)) {
					error(rt, NULL, 0, "invalid line number `%s`", arg + 2);
				}
				bp_file[bp_size] = ccFile;
				bp_line[bp_size] = line;
				bp_type[bp_size] = type;
				bp_size += 1;
			}
			else {
				error(rt, NULL, 0, "invalid option: `%s`", arg);
			}
		}
	}

	errors = rt->errors;

	extra.rt = rt;
	if (dumpFile != NULL) {
		extra.out = dumpFile;
	}

	// generate code only if there are no compilation errors
	if (errors == 0) {
		if (!gencode(rt, run_code != run)) {
			trace("error generating code");
		}
		// set breakpoints
		for (i = 0; i < bp_size; ++i) {
			char *file = bp_file[i];
			int line = bp_line[i];
			int type = bp_type[i];
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
		if (extra.dmpApi != 0 || extra.dmpAsm != 0 || extra.dmpAst != 0) {
			dumpFun = dumpApiText;
		}
	}

	if (dumpFun == dumpApiJSON) {
		// TODO: use json escaping
		printFmt(extra.out, NULL, "{\n%I\"%s\": [{\n", extra.indent, JSON_KEY_API);
		dumpApi(rt, &extra, dumpFun);
		printFmt(extra.out, NULL, "%I}]", extra.indent);
		extra.hasOut = 1;
	}
	else if (dumpFun != NULL) {
		dumpApi(rt, &extra, dumpFun);
	}

	// run code if there are no compilation errors.
	if (errors == 0 && run_code != skipExecution) {
		if (rt->dbg != NULL) {
			rt->dbg->extra = &extra;
			// set debugger of profiler
			if (run_code == debug) {
				rt->dbg->debug = &conDebug;
				if (extra.breakNext != 0) {
					// break on first instruction
					extra.breakNext = rt->vm.pc;
				}
			}
			else if (run_code == profile) {
				// set call tree dump method
				if (dumpFun == dumpApiJSON) {
					rt->dbg->debug = &jsonProfile;
				}
				else {
					rt->dbg->debug = &conDebug;
				}
			}
		}
		errors = execute(rt, 0, NULL, NULL);
	}

	if (dumpFun == dumpApiJSON) {
		// TODO: use json escaping
		printFmt(extra.out, NULL, "\n}\n");
	}
	if (dumpFile != NULL) {
		fclose(dumpFile);
	}

	// release resources
	closeLibs();
	rtClose(rt);

	return errors;
}

static void testCC_initLocalStatic(rtContext rt, int warn) {
//	ccAddUnit(rt, warn, __FILE__, __LINE__, "inline myI32 = 9d;");
//	ccAddUnit(rt, warn, __FILE__, __LINE__, "inline myI64 = 9D;");
//	ccAddUnit(rt, warn, __FILE__, __LINE__, "inline myU32 = 9u;");
//	ccAddUnit(rt, warn, __FILE__, __LINE__, "inline myU64 = 9U;");
//	ccAddUnit(rt, warn, __FILE__, __LINE__, "inline myF32 = 9f;");
//	ccAddUnit(rt, warn, __FILE__, __LINE__, "inline myF64 = 9F;");

	ccAddUnit(rt, warn, __FILE__, __LINE__, "inline myVal = 1024;");
	ccAddUnit(rt, warn, __FILE__, __LINE__, "inline myInt = int64;");
	ccAddUnit(rt, warn, __FILE__, __LINE__, "myInt i;");
	ccAddUnit(rt, warn, __FILE__, __LINE__, "static myInt I;");
	ccAddUnit(rt, warn, __FILE__, __LINE__, "myInt j = myVal;");
	ccAddUnit(rt, warn, __FILE__, __LINE__, "static myInt J = myVal;");
	ccAddUnit(rt, warn, __FILE__, __LINE__, "myInt k = j;");
	ccAddUnit(rt, warn, __FILE__, __LINE__, "static myInt K = j;");
	ccAddUnit(rt, warn, __FILE__, __LINE__, "myInt l = J;");
	ccAddUnit(rt, warn, __FILE__, __LINE__, "static myInt L = J;");
}
static void testCC_initRefVarPtr(rtContext rt, int warn) {
	ccAddUnit(rt, warn, __FILE__, __LINE__ + 1,
		"struct complex {float64 re; float64 im; /*bool b;*/}\n"
		"inline integer = int8;\n"
		"\n"
		"integer value = 33;\n"
		"integer valueRef& = value;\n"
		"pointer valuePtr = value;\n"
		"variant valueVar = value;\n"
		"\n"
		"integer copyVal = value;\n"
		"integer copyRef& = valueRef;\n"
		"pointer copyPtr = valuePtr;\n"
		"variant copyVar = valueVar;\n"
		"integer fromRef = valueRef;\n"
		"integer fromPtr& = valuePtr;\n"
		"integer fromVar& = valueVar;\n"
		"\n"
		"integer nullRef& = null;\n"
		"pointer nullPtr = null;\n"
		"variant nullVar = null;\n"
		"pointer typePtr = integer;\n"
		"variant typeVar = integer;\n"
		"\n"// Assing typenames to pointer
		"pointer ptrVoid = void;\n"
		"pointer ptrBool = bool;\n"
		"pointer ptrChar = char;\n"
		"pointer ptrInt8 = int8;\n"
		"pointer ptrInt16 = int16;\n"
		"pointer ptrInt32 = int32;\n"
		"pointer ptrInt64 = int64;\n"
		"pointer ptrUint8 = uint8;\n"
		"pointer ptrUint16 = uint16;\n"
		"pointer ptrUint32 = uint32;\n"
		"pointer ptrUint64 = uint64;\n"
		"pointer ptrFloat32 = float32;\n"
		"pointer ptrFloat64 = float64;\n"
		"pointer ptrTypename = typename;\n"
		"pointer ptrFunction = function;\n"
		"pointer ptrPointer = pointer;\n"
		"pointer ptrVariant = variant;\n"
		"pointer ptrObject = object;\n"
		"\n"// Assing typenames to variant
		"variant varVoid = void;\n"
		"variant varBool = bool;\n"
		"variant varChar = char;\n"
		"variant varInt8 = int8;\n"
		"variant varInt16 = int16;\n"
		"variant varInt32 = int32;\n"
		"variant varInt64 = int64;\n"
		"variant varUint8 = uint8;\n"
		"variant varUint16 = uint16;\n"
		"variant varUint32 = uint32;\n"
		"variant varUint64 = uint64;\n"
		"variant varFloat32 = float32;\n"
		"variant varFloat64 = float64;\n"
		"variant varTypename = typename;\n"
		"variant varFunction = function;\n"
		"variant varPointer = pointer;\n"
		"variant varVariant = variant;\n"
		"variant varObject = object;\n"
		// */
	);

//	ccAddUnit(rt, warn, __FILE__, __LINE__, "ref = value;\n");
//	ccAddUnit(rt, warn, __FILE__, __LINE__, "value = valueRef;\n");
//	ccAddUnit(rt, warn, __FILE__, __LINE__, "value = valueRef = 12;\n"); // TODO

//	ccAddUnit(rt, warn, __FILE__, __LINE__, "integer slice[];\n");
//	ccAddUnit(rt, warn, __FILE__, __LINE__, "integer array[3];\n");
//	ccAddUnit(rt, warn, __FILE__, __LINE__, "integer pointer[*] = null;\n");
}
static void testCC_nfc_math(rtContext rt, int warn) {
	ccAddUnit(rt, warn, __FILE__, __LINE__, "double halfPi = 3.14 / 2;");
	ccAddUnit(rt, warn, __FILE__, __LINE__, "double spi1 = double.sin(halfPi);");
	ccAddUnit(rt, warn, __FILE__, __LINE__, "double spi2 = double.sin(3.14 / 2);");
}
static void testCC_nfc_memmgr(rtContext rt, int warn) {
	// TODO: ccAddUnit(rt, warn, __FILE__, __LINE__, "inline malloc(int size) = memmgr(null, size);");
	// TODO: ccAddUnit(rt, warn, __FILE__, __LINE__, "inline free(pointer data) = memmgr(data, 0);");
	// TODO: fix code generation
//	ccAddUnit(rt, warn, __FILE__, __LINE__, "pointer p1 = malloc(1024);");
	ccAddUnit(rt, warn, __FILE__, __LINE__, "pointer p2 = memmgr(null, 80);");
	ccAddUnit(rt, warn, __FILE__, __LINE__, "pointer p3 = memmgr(null, 160);");
	ccAddUnit(rt, warn, __FILE__, __LINE__, "pointer p4 = memmgr(null, 820);");
	// TODO: free memory
//	ccAddUnit(rt, warn, __FILE__, __LINE__, "memmgr(p1, 0);");
//	ccAddUnit(rt, warn, __FILE__, __LINE__, "memmgr(p2, 0);");
//	ccAddUnit(rt, warn, __FILE__, __LINE__, "memmgr(p3, 0);");
//	ccAddUnit(rt, warn, __FILE__, __LINE__, "memmgr(p4, 0);");
}
static void testCC_def_types(rtContext rt, int warn) {
	ccAddUnit(rt, warn, __FILE__, __LINE__, "struct rec:1 { int32 a; int8 b; int64 c; int8 d; }");
//	ccAddUnit(rt, warn, __FILE__, __LINE__, "int fun(int32 a, int8 b, int64 c, byte d, long e) { static int A = 9; return a * b; }");
	ccAddUnit(rt, warn, __FILE__, __LINE__, "struct type: object { int a; type b; }");
//	ccAddUnit(rt, warn, __FILE__, __LINE__, "const type var = { b: null; a: 2; };");
	ccAddUnit(rt, warn, __FILE__, __LINE__, "object o = null;");
}
static void testCC_inline(rtContext rt, int warn) {
	ccAddUnit(rt, warn, __FILE__, __LINE__, "inline min(int a, int b) = a < b ? a : b;");
	ccAddUnit(rt, warn, __FILE__, __LINE__, "inline max(int a, int b) = a > b ? a : b;");
	ccAddUnit(rt, warn, __FILE__, __LINE__, "inline mxa(int a, int b) = a ? a : b;");
	ccAddUnit(rt, warn, __FILE__, __LINE__, "inline mx0(int a, int b) = 0;");
	ccAddUnit(rt, warn, __FILE__, __LINE__, "int a = 3;");
	ccAddUnit(rt, warn, __FILE__, __LINE__, "int b = 6;");
	ccAddUnit(rt, warn, __FILE__, __LINE__, "int m1 = max(a, b);");
	ccAddUnit(rt, warn, __FILE__, __LINE__, "int m2 = mxa(0, b);");
	ccAddUnit(rt, warn, __FILE__, __LINE__, "int m3 = mx0(a, b);");
}
static void testCC_statement_if(rtContext rt, int warn) {
	ccAddUnit(rt, warn, __FILE__, __LINE__, "static if (0 == 0) { 1; }");
	ccAddUnit(rt, warn, __FILE__, __LINE__, "static if (0 != 0) { 2; }");
	ccAddUnit(rt, warn, __FILE__, __LINE__, "static if (0 == 0) { 3; } else { 4; }");
	ccAddUnit(rt, warn, __FILE__, __LINE__, "static if (0 != 0) { 5; } else { 6; }");

	enter(rt->cc);
	ccAddUnit(rt, warn, __FILE__, __LINE__, "int i = 0;");
	ccAddUnit(rt, warn, __FILE__, __LINE__, "if (i == 0) { 1; }");
	ccAddUnit(rt, warn, __FILE__, __LINE__, "if (i != 0) { 2; }");
	ccAddUnit(rt, warn, __FILE__, __LINE__, "if (i != 0) { 3; } else { 4; }");
	ccAddUnit(rt, warn, __FILE__, __LINE__, "if (i != 0) { 5; } else { 6; }");
	leave(rt->cc, NULL, TYPE_any, 0, NULL);
}

static vmError nfcAdd42(nfcContext args) {
	int32_t arg = argi32(args, 0);
	reti32(args, arg + 42);
	return noError;
}
static void testCC_todo(rtContext rt, int warn) {
	// TODO: wrong debug statement: ccAddUnit(rt, warn, __FILE__, __LINE__, "if (2 > 1) { 3 * 2; }");
	// TODO: unsigned and cast: ccAddUnit(rt, warn, __FILE__, __LINE__, "uint32 x = 0b1000; int32 y = int32(x, 3) >> 1;");

	// valid expressions, but should fail type-checking
//	ccAddUnit(rt, 32, __FILE__, __LINE__, "() + (1) + 2() + 3(4) + 5(6 + 7);");
//	ccAddUnit(rt, 32, __FILE__, __LINE__, "[] + [1] + 2[] + 3[4] + 5[6 + 7];");

//	ccAddUnit(rt, 32, __FILE__, __LINE__, "3 *= 4 + 5;");				// 3 := 3 * (4 + 5)
//	ccAddUnit(rt, 32, __FILE__, __LINE__, "3 += 4 * 5;");				// 3 := 3 + 4 * 5

//	ccAddUnit(rt, warn, __FILE__, __LINE__, "raise(1, \"test\", var, 2);");
//
//
//	ccDefCall(rt, nfcAdd42, "int add42(int x0)");
//	ccAddUnit(rt, warn, __FILE__, __LINE__, "int z1 = add42(22);");

	// TODO: update returned value
//	ccAddUnit(rt, warn, __FILE__, __LINE__, "inline add42Ext(int x1) = add42(x1);");
//	ccAddUnit(rt, warn, __FILE__, __LINE__, "int z2 = add42Ext(22);");

	// TODO: allow inline arguments
//	ccAddUnit(rt, warn, __FILE__, __LINE__, "inline add42Ext1(inline int x2) = add42(x2);");
//	ccAddUnit(rt, warn, __FILE__, __LINE__, "int z3 = add42Ext1(22);");
}

static int testCC() {
	int libs = 1;
	int warn = 32;
	rtContext rt = rtInit(mem, sizeof(mem));
	struct userContextRec extra = {
		.breakNext = 0,	// break on all instructions: -1
		.dbgCommand = 'r',
		.dbgOnError = dbg_trace,
		.dbgOnCaught = dbg_print,

		.dmpApi = 1,
		.dmpApiAll = 0,
		.dmpApiInfo = 1,
		.dmpApiPrms = 1,
		.dmpApiUsed = 0,

		.dmpAst = 0,
		.dmpAstLine = 0,
		.dmpAstBody = 0,
		.dmpAstElIf = 0,

		.dmpAsm = 1,
		.dmpAsmAddr = 1,
		.dmpAsmName = 1,
		.dmpAsmStmt = 1,
		.dmpAsmCode = 9,

		.dmpRunStat = 2,
		.dmpVarStat = 2,
		.dmpMemStat = 2,

		.dmpRunCall = 1,
		.dmpRunCode = 0,

		.hasOut = 0,
		.indent = 0,

		.out = stdout,
		.rt = rt
	};
	struct userContextRec extraMain = extra;

	// initialize runtime and compiler
	if (rt == NULL) {
		fatal("initializing runtime context");
		return -1;
	}

	// install base type system.
	if (!ccInit(rt, install_def, NULL)) {
		error(rt, NULL, 0, "error registering base types");
		logfile(rt, NULL, 0);
		return -6;
	}

	// install standard library.
	if (libs && !ccAddLib(rt, wl, ccLibStdc, NULL)) {
		error(rt, NULL, 0, "error registering standard library");
		logfile(rt, NULL, 0);
		return -6;
	}

	// install file operations.
	if (libs && !ccAddLib(rt, wl, ccLibFile, NULL)) {
		error(rt, NULL, 0, "error registering file library");
		logfile(rt, NULL, 0);
		return -6;
	}

	testCC_initRefVarPtr(rt, warn);
	testCC_initLocalStatic(rt, warn);
	testCC_def_types(rt, warn);
	testCC_nfc_math(rt, warn);
	testCC_nfc_memmgr(rt, warn);
	testCC_inline(rt, warn);
	testCC_statement_if(rt, warn);
	//testCC_todo(rt, warn);

//	ccAddUnit(rt, warn, "tests/test.Lang.inline.cc", 0, NULL);

//	disable optimizations
	rt->foldConst = 0;
	rt->foldInstr = 0;
//	rt->fastAssign = 0;
//	rt->genGlobals = 0;

//	printAst(stdout, NULL, rt->cc->root, prDbg, 0);
//	dumpAstXML(stdout, NULL, rt->cc->root, -1, 0, "source");
	if (!gencode(rt, 1)) {
		error(rt, NULL, 0, "error generating instructions");
	}
//	dumpApi(rt, &extra, dumpApiText);
	if (rt->main != NULL) {
		rt->main->file = __FILE__;
		rt->main->line = __LINE__;
		//extra.breakNext = rt->main->offs;
		extraMain.dmpApi = 0;
		extraMain.dmpAst = 0;
		extraMain.dmpAsm = 1;
		dumpApiText(&extraMain, rt->main);
	}

	if (!rt->errors) {
		FILE *oldOut = extra.out;
		rt->dbg->extra = &extra;
		rt->dbg->debug = conDebug;
		rt->dbg->debug = jsonProfile;
		printFmt(extra.out, NULL, "\n/*-- Execute:\n");

		if (rt->dbg->debug == jsonProfile) {
			extra.out = fopen("dump.json", "w");
			if (extra.out == NULL) {
				extra.out = oldOut;
			}
			printFmt(extra.out, NULL, "{\n");
		}

		if (execute(rt, 0, NULL, NULL) != noError) {
			error(rt, NULL, 0, "error executing instructions");
		}
		/*
		if (invoke(rt, rt->main, NULL, NULL, NULL) != noError) {
			error(rt, NULL, 0, "error executing instructions");
		}// */
		if (rt->dbg->debug == jsonProfile) {
			printFmt(extra.out, NULL, "\n}");
			if (extra.out != oldOut) {
				fclose(extra.out);
				extra.out = oldOut;
			}
		}
		printFmt(extra.out, NULL, "// */\n");
		conDumpProf(&extra);
	}

	rtClose(rt);

	(void)dumpAstXML;

	return rt->errors;
}

int main(int argc, char *argv[]) {
	setbuf(stdout, NULL);
	setbuf(stderr, NULL);
	// vmSelfTest();
	return testCC();
	if (argc < 2) {
		fputs(USAGE, stdout);
		return 0;
	}
	if (argc == 2) {
		if (strcmp(argv[1], "-vmTest") == 0) {
			return vmSelfTest();
		}
		if (strcmp(argv[1], "--help") == 0) {
			fputs(USAGE, stdout);
			return 0;
		}
	}
	return program(argc, argv);
}

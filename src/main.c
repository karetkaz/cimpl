#include <time.h>
#include <stddef.h>
#include "internal.h"

// default values
static const int wl = 15;           // default warning level
static char mem[512 << 10];         // 512 Kb memory compiler + runtime
const char *STDLIB = "stdlib.cvx";  // standard library

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
	FILE *in;

	FILE *out;
	int indent;

	int hasOut;

	int dmpApi;         // dump symbols
	int dmpApiAll;      // include builtin symbols
	int dmpApiMain;     // include main initializer
	int dmpApiInfo;     // dump detailed info
	int dmpApiPrms;     // dump parameters and fields
	int dmpApiUsed;     // dump usages

	int dmpAsm;         // dump disassembled code
	int dmpAsmAddr;     // use global address: (@0x003d8c)
	int dmpAsmName;     // prefer names over addresses: <main+80>
	int dmpAsmStmt;     // print source code statements
	int dmpAsmCode;     // print code bytes (0-15)

	int dmpAst;         // dump syntax tree
	int dmpAstType;     // dump syntax tree collapsed on a single line
	int dmpAstLine;     // dump syntax tree collapsed on a single line
	int dmpAstBody;     // dump '{' on new line
	int dmpAstElIf;     // dump 'else if' on separate lines
	int dmpAstXML;      // dump ast in xml format to console

	// execution statistics
	int dmpRunStat;     // dump execution statistics
	int dmpVarStat;     // dump global variables
	int dmpMemStat;     // dump runtime heap

	int dmpRunTime;     // dump call tree
	int dmpRunCode;     // dump executing instructions
	int dmpRunStack;    // dump the stack, dmpRunCode must be enabled

	// debugging
	int dbgCommand;     // last debugger command
	int dbgOnError;     // on uncaught exception: dbg_xxx
	int dbgOnCaught;    // on caught exception: dbg_xxx
	size_t dbgNextBreak;// offset of the next break (used for step in, out, over, instruction)
};

static void dumpAstXML(FILE *out, const char **esc, astn ast, int mode, int indent, const char *text) {
	astn list;

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
			/*list = chainArgs(ast->op.rhso);
			while (list != NULL) {
				dumpAstXML(out, esc, list, mode, indent + 1, "push");
				list = list->next;
			}*/
			dumpAstXML(out, esc, ast->op.rhso, mode, indent + 1, "push");
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

		case ASGN_set:		// '='
			printFmt(out, esc, " value=\"%?t\">\n", ast);
			dumpAstXML(out, esc, ast->op.test, mode, indent + 1, "test");
			dumpAstXML(out, esc, ast->op.lhso, mode, indent + 1, "lval");
			dumpAstXML(out, esc, ast->op.rhso, mode, indent + 1, "rval");
			printFmt(out, esc, "%I</%s>\n", indent, text);
			break;

		//#}
		//#{ VAL
		case TOKEN_var: {
			symn link = ast->ref.link;
			// declaration
			if (link && link->init /*&& link->tag == ast*/) {
				printFmt(out, esc, " value=\"%?t\">\n", ast);
				dumpAstXML(out, esc, link->init, mode, indent + 1, "init");
				printFmt(out, esc, "%I</%s>\n", indent, text);
				break;
			}
		}

		// fall trough
		case TOKEN_opc:
		case TOKEN_val:
			printFmt(out, esc, " value=\"%?t\"/>\n", ast);
			break;

		//#}
	}
}

// json output
static const int JSON_PRETTY_PRINT = 1;
static const char *JSON_KEY_API = "symbols";
static const char *JSON_KEY_RUN = "profile";

static void jsonDumpSym(FILE *out, const char **esc, symn ptr, const char *kind, int indent) {
	static const char *FMT_START = "%I, \"%s\": {\n";
	static const char *FMT_NEXT = "%I}, {\n";
	static const char *FMT_END = "%I}\n";
	static const char *KEY_PROTO = "proto";
	static const char *KEY_KIND = "kind";
	static const char *KEY_CAST = "cast";
	static const char *KEY_FILE = "file";
	static const char *KEY_LINE = "line";
	static const char *KEY_NAME = "name";
	static const char *KEY_OWNER = "owner";
	static const char *KEY_TYPE = "type";
	static const char *KEY_ARGS = "args";
	static const char *KEY_CNST = "const";
	static const char *KEY_STAT = "static";
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
	printFmt(out, esc, "%I, \"%s\": \"%K\"\n", indent, KEY_KIND, ptr->kind & MASK_kind);
	printFmt(out, esc, "%I, \"%s\": \"%K\"\n", indent, KEY_CAST, ptr->kind & MASK_cast);
	printFmt(out, esc, "%I, \"%s\": \"%.T\"\n", indent, KEY_NAME, ptr);
	if (ptr->owner != NULL) {
		printFmt(out, esc, "%I, \"%s\": \"%T\"\n", indent, KEY_OWNER, ptr->owner);
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
	printFmt(out, esc, "%I, \"%s\": %s\n", indent, KEY_STAT, isStatic(ptr) ? VAL_TRUE : VAL_FALSE);
	printFmt(out, esc, "%I, \"%s\": %s\n", indent, KEY_CNST, isConst(ptr) ? VAL_TRUE : VAL_FALSE);
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
		case OPER_fnc: {    // '()'
			astn args = chainArgs(ast->op.rhso);
			printFmt(out, esc, "%I, \"%s\": [{\n", indent + 1, KEY_ARGS);
			while (args != NULL) {
				if (args != ast->op.rhso) {
					printFmt(out, esc, "%I}, {\n", indent + 1);
				}
				jsonDumpAst(out, esc, args, NULL, indent + 1);
				args = args->next;
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

		case ASGN_set:		// '='
			jsonDumpAst(out, esc, ast->op.test, KEY_TEST, indent + 1);
			jsonDumpAst(out, esc, ast->op.lhso, KEY_LHSO, indent + 1);
			jsonDumpAst(out, esc, ast->op.rhso, KEY_RHSO, indent + 1);
			break;
		//#}
		//#{ VALUE
		case TOKEN_opc:
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

	size_t end = sym->offs + sym->size;
	printFmt(out, esc, FMT_START, indent);
	for (size_t pc = sym->offs; pc < end; ) {
		unsigned char *ip = vmPointer(rt, pc);
		if (pc != sym->offs) {
			printFmt(out, esc, FMT_NEXT, indent, indent);
		}
		printFmt(out, esc, "%I\"%s\": \"%.A\"\n", indent + 1, KEY_INSN, ip);
		printFmt(out, esc, "%I, \"%s\": %u\n", indent + 1, KEY_OFFS, pc);
		pc += opcode_tbl[*ip].size;
		if (ip == vmPointer(rt, pc)) {
			break;
		}
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

static void jsonPreProfile(dbgContext ctx) {
    userContext cc = ctx->extra;
    FILE *out = cc->out;
    const int indent = cc->indent;
    const char **esc = NULL;

    printFmt(out, esc, "\n%I%s\"%s\": {\n", indent, cc->hasOut ? ", " : "", JSON_KEY_RUN);
    printFmt(out, esc, "%I\"%s\": [", indent + 1, "callTreeData");
    printFmt(out, esc, "\"%s\", ", "ctTickIndex");
    printFmt(out, esc, "\"%s\", ", "ctHeapIndex");
    printFmt(out, esc, "\"%s\"", "ctFunIndex");
    printFmt(out, esc, "]\n");

    printFmt(out, esc, "%I, \"%s\": [", indent + 1, "callTree");

}
static void jsonPostProfile(dbgContext ctx) {
	userContext cc = ctx->extra;
	FILE *out = cc->out;
	const int indent = cc->indent;
	const char **esc = NULL;

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
static dbgn jsonProfile(dbgContext ctx, vmError error, size_t ss, void *stack, size_t caller, size_t callee) {
	if (callee != 0) {
		clock_t ticks = clock();
		userContext cc = ctx->extra;
		FILE *out = cc->out;
		const char **esc = NULL;
		if ((ptrdiff_t) callee < 0) {
			if (JSON_PRETTY_PRINT) {
				printFmt(out, esc, "\n% I%d,%d,-1%s", ss, ticks, ctx->usedMem, ss ? "," : "");
			}
			else {
				printFmt(out, esc, "%d,%d,-1%s", ticks, ctx->usedMem, ss ? "," : "");
			}
			if (ss == 0) {
				jsonPostProfile(ctx);
			}
		}
		else {
			if (ss == 0) {
				jsonPreProfile(ctx);
			}
			if (JSON_PRETTY_PRINT) {
				printFmt(out, esc, "\n% I%d,%d,%d,", ss, ticks, ctx->usedMem, callee);
			}
			else {
				printFmt(out, esc, "%d,%d,%d,", ticks, ctx->usedMem, callee);
			}
		}
	}
	(void)caller;
	(void)error;
	(void)stack;
	return NULL;
}

// text output
static void conDumpAsm(FILE *out, const char **esc, size_t offs, userContext ctx, int indent) {
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
			if (ctx->dmpAsmAddr) {
				printFmt(out, esc, "%I%?s:%?u: [%06x-%06x): %.*t\n", indent, dbg->file, dbg->line, dbg->start, dbg->end, mode, dbg->stmt);
			}
			else {
				printFmt(out, esc, "%I%?s:%?u: (%d bytes): %.*t\n", indent, dbg->file, dbg->line, dbg->end - dbg->start, mode, dbg->stmt);
			}
		}
	}

	printFmt(out, esc, "%I", indent);
	printAsm(out, esc, ctx->rt, vmPointer(ctx->rt, offs), mode);
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

	if (dbg == NULL) {
		return;
	}

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
				"%?s:%?u:[.%06x, .%06x): %?T, hits(%D/%D), time(%D%?+D / %.3F%?+.3F ms)\n", fun->file,
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
					"%?s:%?u:[.%06x, .%06x): <%?.T+%d> hits(%D/%D), time(%D%?+D / %.3F%?+.3F ms)\n", fun->file,
					fun->line, fun->start, fun->end, sym, symOffs, (int64_t) fun->hits, (int64_t) fun->exec,
					(int64_t) fun->total, (int64_t) -(fun->total - fun->self),
					fun->total / (double) CLOCKS_PER_SEC, -(fun->total - fun->self) / (double) CLOCKS_PER_SEC
				);
			}
		}

		printFmt(out, esc, "\n//-- coverage(functions: %.2f%%(%d/%d), statements: %.2f%%(%d/%d))\n",
			covFunc * 100. / (nFunc ? nFunc : 1), covFunc, nFunc, covStmt * 100. / (nStmt ? nStmt : 1), covStmt, nStmt
		);

		if (all) {
			printFmt(out, esc, "\n//-- functions not executed:\n");
			fun = (dbgn) dbg->functions.ptr;
			for (i = 0; i < nFunc; ++i, fun++) {
				symn sym = fun->decl;
				if (fun->hits != 0) {
					continue;
				}
				if (sym == NULL) {
					sym = rtFindSym(rt, fun->start, 1);
				}
				printFmt(out, esc, "%?s:%?u:[.%06x, .%06x): %?T\n", fun->file, fun->line, fun->start, fun->end, sym);
			}
			printFmt(out, esc, "\n//-- statements not executed:\n");
			fun = (dbgn) rt->dbg->statements.ptr;
			for (i = 0; i < nStmt; ++i, fun++) {
				size_t symOffs = 0;
				symn sym = fun->decl;
				if (fun->hits != 0) {
					continue;
				}
				if (sym == NULL) {
					sym = rtFindSym(rt, fun->start, 1);
				}
				if (sym != NULL) {
					symOffs = fun->start - sym->offs;
				}
				printFmt(out, esc, "%?s:%?u:[.%06x, .%06x): <%?.T+%d>\n", fun->file, fun->line, fun->start, fun->end, sym, symOffs);
			}
		}

		printFmt(out, esc, "// */\n");
	}

	if (usr->dmpVarStat != 0) {
		int all = usr->dmpVarStat > 1;
		symn var;
		printFmt(out, esc, "\n/*-- Globals:\n");
		for (var = rt->main->fields; var; var = var->next) {
			char *ofs = NULL;

			if (isInline(var)) {
				continue;
			}

			if (isTypename(var)) {
				continue;
			}

			if (isFunction(var) && !all) {
				continue;
			}

			// exclude null
			if (var->offs == 0) {
				continue;
			}

			if (var->file && var->line) {
				printFmt(out, esc, "%s:%u: ", var->file, var->line);
			} else if (!all) {
				// exclude internals
				continue;
			}

			if (isStatic(var)) {
				// static variable.
				ofs = (char *) rt->_mem + var->offs;
			} else {
				// local variable (or argument).
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

	if (!extra->dmpApi && !dmpAsm && !dmpAst) {
		// nothing to dump
		return;
	}
	else if (extra->dmpApiMain && sym == extra->rt->main) {
		// do not skip main symbol
	}
	else if (extra->dmpApiAll && sym->file == NULL && sym->line == 0) {
		// do not skip internal symbols
	}
	else {
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
		if (extra->dmpAstType) {
			mode |= prAstType;
		}
		printAst(out, esc, sym->init, mode, -indent);

		if (extra->dmpAstLine || sym->init->kind != STMT_beg) {
			printFmt(out, esc, "\n");
		}
		if (extra->dmpAstXML) {
			dumpAstXML(out, esc, sym->init, mode, -indent, "main");
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
			unsigned char *ip = vmPointer(extra->rt, pc);
			size_t ppc = pc;

			if (ip == NULL) {
				break;
			}
			conDumpAsm(out, esc, pc, extra, indent + 1);
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

	if (sym->params != NULL) {
		printSym(out, NULL, sym, prSymQual|prSymArgs|prSymType, 0);
	}
	else {
		printSym(out, NULL, sym, prSymQual|prSymArgs, 0);
	}
	printFmt(out, esc, "\n");
}

// console debugger and profiler
static dbgn conDebug(dbgContext ctx, vmError error, size_t ss, void *stack, size_t caller, size_t callee) {
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
	double now = clock() * (1000. / CLOCKS_PER_SEC);
	const char **esc = NULL;
	FILE *out = stdout;

	int breakMode = 0;
	char *breakCause = NULL;
	symn fun = rtFindSym(rt, caller, 0);
	dbgn dbg = mapDbgStatement(rt, caller);

	// enter or leave
	if (callee != 0) {
		// TODO: enter will break after executing call?
		// TODO: leave will break after executing return?
		if (usr->dbgCommand == dbgStepInto && (ptrdiff_t) callee > 0) {
			usr->dbgNextBreak = (size_t) -1;
		}
		else if (usr->dbgCommand == dbgStepOut && (ptrdiff_t) callee < 0) {
			usr->dbgNextBreak = (size_t) -1;
		}

		if (usr->dmpRunTime) {
			if ((ptrdiff_t) callee > 0) {
				printFmt(out, esc, "[% 3.2F]>% I %d, %T\n", now, ss, ctx->usedMem, rtFindSym(rt, callee, 1));
			}
			else {
				printFmt(out, esc, "[% 3.2F]<% I %d\n", now, ss, ctx->usedMem);
			}
		}
		return NULL;
	}

	if (error != noError) {
		breakCause = vmErrorMessage(error);
		if (ctx->checked) {
			breakMode = usr->dbgOnCaught;
		}
		else {
			breakMode = usr->dbgOnError;
		}
	}
	else if (usr->dbgNextBreak == (size_t) -1) {
		breakMode = dbg_pause;
		breakCause = "Break";
	}
	else if (usr->dbgNextBreak == caller) {
		breakMode = dbg_pause;
		breakCause = "Break";
	}
	else if (dbg != NULL) {
		if (caller == dbg->start) {
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
		if (usr->dmpRunStack) {
			size_t i = 0;
			printFmt(out, esc, "\tstack(%d): [", ss);
			if (ss > LOG_MAX_ITEMS) {
				printFmt(out, esc, " …");
				i = ss - LOG_MAX_ITEMS;
			}
			for (; i < ss; i++) {
				if (i > 0) {
					printFmt(out, esc, ",");
				}
				printFmt(out, esc, " %d", ((int32_t *) stack)[ss - i - 1]);
			}
			printFmt(out, esc, "\n");
		}
		if (usr->dmpRunTime) {
			printFmt(out, esc, "[% 3.2F]\t", now);
		}
		if (usr->dmpRunCode) {
			conDumpAsm(out, esc, caller, usr, 0);
		}
	}

	// print error type
	if (breakMode & dbg_print) {
		size_t funOffs = caller;
		if (fun != NULL) {
			funOffs -= fun->offs;
		}
		// print current file position
		if (dbg != NULL && dbg->file != NULL && dbg->line > 0) {
			printFmt(out, esc, "%s:%u: ", dbg->file, dbg->line);
		}
		printFmt(out, esc, ERR_EXEC_INSTRUCTION"\n", breakCause, caller, fun, funOffs, vmPointer(rt, caller));
	}
	if (breakMode & dbg_trace) {
		traceCalls(ctx, out, 1, 0, 20);
	}

	// pause execution in debugger
	for ( ; breakMode & dbg_pause; ) {
		int cmd = usr->dbgCommand;
		char *arg = NULL;
		printFmt(out, esc, ">dbg[%?c] %.A: ", cmd, vmPointer(rt, caller));
		if (usr->in == NULL || fgets(buff, sizeof(buff), usr->in) == NULL) {
			printFmt(out, esc, "can not read from standard input, aborting\n");
			return ctx->abort;
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
				usr->dbgNextBreak = 0;
				return dbg;

			case dbgStepNext:
				usr->dbgNextBreak = (size_t)-1;
				return dbg;

			case dbgStepLine:
				usr->dbgNextBreak = dbg ? dbg->end : 0;
				return dbg;

			case dbgPrintStackTrace:
				traceCalls(ctx, out, 1, 0, 20);
				break;

			case dbgPrintInstruction:
				conDumpAsm(out, esc, caller, usr, 0);
				break;

			case dbgPrintStackValues:
				if (*arg == 0) {
					size_t offs;
					// print top of stack
					for (offs = 0; offs < ss; offs++) {
						stkval *v = (stkval*)&((long*)stack)[offs];
						printFmt(out, esc, "\tsp(%d): {0x%08x, i32(%d), f32(%f), i64(%D), f64(%F)}\n", offs, v->i4, v->i4, v->f4, v->i8, v->f8);
					}
				}
				else {
					symn sym = ccLookupSym(rt->cc, NULL, arg);
					printFmt(out, esc, "arg:%T", sym);
					if (sym && isVariable(sym) && !isStatic(sym)) {
						printVal(out, esc, rt, sym, (stkval *) stack, prSymType, 0);
					}
				}
				break;
		}
	}
	return dbg;
}

static int program(int argc, char *argv[]) {
	struct {
		int logAppend;
		int foldConst;
		int foldInstr;
		int fastAssign;
		int genGlobals;
		int stdLibs;
		ccInstall install;
		size_t memory;
	} settings = {
		.logAppend = 0,

		// enable all optimizations by default
		.foldConst = 1,
		.foldInstr = 1,
		.fastAssign = 1,
		.genGlobals = 1,

		.stdLibs = 1,

		.install = install_def,
		.memory = sizeof(mem)
	};

	struct userContextRec extra = {
		.dbgCommand = 'r',	// last command: resume
		.dbgOnError = dbg_pause | dbg_print,
		.dbgOnCaught = 0,
		.dbgNextBreak = 0,

		.dmpApi = 0,
		.dmpApiAll = 0,
		.dmpApiMain = 0,
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

		.dmpRunStat = 0,
		.dmpVarStat = 0,
		.dmpMemStat = 0,

		.dmpRunTime = 0,
		.dmpRunCode = 0,
		.dmpRunStack = 0,

		.in = stdin,
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

	int i, warn;

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
				unsigned on = *arg2 == '+';
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
					settings.install = (settings.install & ~installEmit) | on * installEmit;
					arg2 += 5;
				}
				else if (strBegins(arg2 + 1, "ptr")) {
					settings.install = (settings.install & ~install_ptr) | on * install_ptr;
					arg2 += 4;
				}
				else if (strBegins(arg2 + 1, "var")) {
					settings.install = (settings.install & ~install_var) | on * install_var;
					arg2 += 4;
				}
				else if (strBegins(arg2 + 1, "obj")) {
					settings.install = (settings.install & ~install_obj) | on * install_obj;
					arg2 += 4;
				}
				else if (strBegins(arg2 + 1, "stdin")) {
					extra.in = on ? stdin : NULL;
					arg2 += 6;
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
					case 'm':
						extra.dmpApiMain = 1;
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
					case 'a':
						extra.dmpAsmAddr = 1;
						arg2 += 2;
						break;
					case 'n':
						extra.dmpAsmName = 1;
						arg2 += 2;
						break;
					case 's':
						extra.dmpAsmStmt = 1;
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
					case 'x':
						extra.dmpAstXML = 1;
						arg2 += 2;
						break;
					case 't':
						extra.dmpAstType = 1;
						arg2 += 2;
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
			if (strEquals(arg, "-dump")) {
				dumpFun = dumpApiText;
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
			dumpFile = fopen(argv[i], "w");
			if (dumpFile == NULL) {
				fatal("error opening dump file: %s", argv[i]);
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
						extra.dbgNextBreak = (size_t)-1;
						arg2 += 2;
						break;
					case 'a':
						extra.dbgOnCaught |= dbg_pause;
						arg2 += 2;
						break;
					case 'L':
						extra.dbgOnCaught |= dbg_trace;
					case 'l':
						extra.dbgOnCaught |= dbg_print;
						arg2 += 2;
						break;

					case 'E':
						extra.dmpRunStack |= 1;
					case 'e':
						extra.dmpRunCode |= 1;
						arg2 += 2;
						break;

					// dump stats
					case 'P':
						extra.dmpRunStat |= 2;
					case 'p':
						extra.dmpRunStat |= 1;
						arg2 += 2;
						break;
					case 'G':
						extra.dmpVarStat |= 2;
					case 'g':
						extra.dmpVarStat |= 1;
						arg2 += 2;
						break;
					case 'H':
						extra.dmpMemStat |= 2;
					case 'h':
						extra.dmpMemStat |= 1;
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
			extra.dmpRunStat = 1;
			while (*arg2 == '/') {
				switch (arg2[1]) {
					default:
						arg2 += 1;
						break;

					// dump call tree
					case 't':
						extra.dmpRunTime = 1;
						arg2 += 2;
						break;
					// dump stats
					case 'P':
						extra.dmpRunStat |= 2;
					case 'p':
						extra.dmpRunStat |= 1;
						arg2 += 2;
						break;
					case 'G':
						extra.dmpVarStat |= 2;
					case 'g':
						extra.dmpVarStat |= 1;
						arg2 += 2;
						break;
					case 'H':
						extra.dmpMemStat |= 2;
					case 'h':
						extra.dmpMemStat |= 1;
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
		if (!ccAddLib(rt->cc, wl, ccLibStdc, stdLib)) {
			error(rt, NULL, 0, "error registering standard library");
			logfile(rt, NULL, 0);
			return -6;
		}

		// install file operations.
		if (!ccAddLib(rt->cc, wl, ccLibFile, NULL)) {
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
				else if (!ccAddUnit(rt->cc, warn == -1 ? wl : warn, ccFile, 1, NULL)) {
					error(rt, NULL, 0, "error compiling source `%s`", ccFile);
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

	extra.rt = rt;
	if (dumpFile != NULL) {
		extra.out = dumpFile;
	}

	// generate code only if there are no compilation errors
	if (rt->errors == 0) {
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
	if (rt->errors == 0 && run_code != skipExecution) {
		if (rt->dbg != NULL) {
			rt->dbg->extra = &extra;
			// set debugger of profiler
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
				}
				else {
					rt->dbg->debug = &conDebug;
				}
			}
		}
		rt->errors = execute(rt, 0, NULL, NULL);
	}

	if (dumpFun == dumpApiJSON) {
		printFmt(extra.out, NULL, "\n}\n");
	}
	else if (rt->errors == 0) {
		conDumpProf(&extra);
	}
	if (dumpFile != NULL) {
		fclose(dumpFile);
	}

	// release resources
	closeLibs();
	rtClose(rt);

	return rt->errors != 0;
}

static int usage() {
    const char *USAGE = "cmpl [global options] [files with options]..."
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
            "\n    /m                  dump main"
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
            "\n    /x                  dump syntax tree also in xml form"
            "\n    /t                  dump sub-expression type information"
            "\n    /l                  do not expand statements (print on single line)"
            "\n    /b                  don't keep braces ('{') on the same line"
            "\n    /e                  don't keep `else if` constructs on the same line"
            "\n"
            "\n  -run                  run without: debug information, stacktrace, bounds checking, ..."
            "\n"
            "\n  -debug[*]             run with attached debugger, pausing on uncaught errors and break points"
            "\n    /s                  pause on startup"
            "\n    /a                  pause on caught errors"
            "\n    /[e|E]              print executing instructions (/E includes stack values)"
            "\n    /[l|L]              print caught errors (/L includes stack trace)"
            "\n    /[g|G]              dump global variable values (/G includes function values)"
            "\n    /[h|H]              dump allocated heap memory chunks (/H includes static chunks)"
            "\n    /[p|P]              dump function statistics (/P includes statement statistics)"
            "\n"
            "\n  -profile[*]           run code with profiler: coverage, method tracing"
            "\n    /t                  dump call tree"
            "\n    /[g|G]              dump global variable values (/G includes function values)"
            "\n    /[h|H]              dump allocated heap memory chunks (/H includes static chunks)"
            "\n    /[p|P]              dump function statistics (/P includes statement statistics)"
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
		if (strcmp(argv[1], "-vmTest") == 0) {
			return vmSelfTest();
		}
		if (strcmp(argv[1], "--help") == 0) {
			return usage();
		}
	}
	return program(argc, argv);
}

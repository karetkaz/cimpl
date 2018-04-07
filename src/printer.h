/**
 * Formatted printing:
 * Symbol
 * Syntax tree
 * Instruction
 */

#ifndef CMPL_PRINTER_H
#define CMPL_PRINTER_H

#include "cmplVm.h"
#include <stdlib.h>

const char **escapeStr();

typedef enum {
	prSkip = -1,
	prAsmCode = 0x00000f,   // print code bytes (0-15)
	prAsmAddr = 0x000010,   // print global address: (@0x003d8c)
	prAsmName = 0x000020,   // use symbol names instead of addresses: <main+80>
	//prAsmRefs = 0x000040,   // print memory reference as comments: load.ref .000000 ;null

	prOneLine = 0x000080,   // force printing on a single line (skip: function body, typename fields, statement blocks, ...)

	prSymQual = 0x000100,   // print qualified symbol names.
	prSymArgs = 0x000200,   // print functions parameters.
	prSymType = 0x000400,   // print variable type, function return type, typename base type.
	prSymInit = 0x000800,   // print variable initializer, function body, typename fields.

	prAttr    = 0x001000,   // print attributes: const, static, member
	prAstType = 0x002000,   // print type cast of each subexpression
	nlAstBody = 0x004000,   // print compound statements on new line (like in cs)
	nlAstElIf = 0x008000,   // don't keep `else if` constructs on the same line.

	prNoOffs = 0x800000,

	prName = 0,		// print operator or symbol name only.
	prValue = prOneLine,
	prShort = prSymQual | prSymArgs | prOneLine ,	// %t, %T
	prFull = prAttr | prSymQual | prSymArgs | prSymType | prSymInit,		// %±t, %±T
	prDbg = prAttr | prAstType | prSymQual | prSymArgs | prSymType | prSymInit | prAsmAddr | prAsmName | 9 | prOneLine
} dmpMode;

/**
 * Print a symbol to the output stream..
 * 
 * @param out Output stream.
 * @param esc Escape translation (format string will be not escaped).
 * @param sym the symbol to be printed.
 * @param mode How to print.
 * @param indent Indentation level. (Used for struct members)
 */
void printSym(FILE *out, const char **esc, symn sym, dmpMode mode, int indent);

/**
 * Print abstract syntax tree to the output stream.
 * 
 * @param out Output stream.
 * @param esc Escape translation (format string will be not escaped).
 * @param ast the abstract syntax tree to be printed.
 * @param mode How to print.
 * @param indent Indentation level. (Used for statements)
 */
void printAst(FILE *out, const char **esc, astn ast, dmpMode mode, int indent);

/**
 * Print an instruction to the output stream.
 * 
 * @param out Output stream.
 * @param esc Escape translation (format string will be not escaped).
 * @param rt (optional) Runtime context.
 * @param ptr Instruction pointer.
 * @param mode How to print.
 */
void printAsm(FILE *out, const char **esc, rtContext rt, void *ptr, dmpMode mode);

/**
 * Print an instruction to the output stream.
 * 
 * @param out Output stream.
 * @param esc Escape translation (format string will be not escaped).
 * @param opc Operation code of the instruction
 * @param args Argument of the instruction
 */
void printOpc(FILE *out, const char **esc, vmOpcode opc, int64_t args);

/**
 * Print the value of a variable at runtime.
 * 
 * @param rt Runtime context.
 * @param out Output stream.
 * @param var Variable to be printed. (May be a typename also)
 * @param ref Base offset of variable.
 * @param level Indentation level. (Used for members and arrays)
 */
void printVal(FILE *out, const char **esc, rtContext ctx, symn var, vmValue *ref, dmpMode mode, int indent);

/**
 * Print formatted text to the output stream.
 * 
 * @param out Output stream.
 * @param esc Escape translation (format string will be not escaped).
 * @param fmt Format text.
 * @param ... Format variables.
 * @note %[?]?[+-]?[0 ]?[0-9]*(\.([*]|[0-9]*))?[tTKAIbBoOxXuUdDfFeEsScC]
 *    skip: [?]? skip printing `(null)` or `0` (may print pad character if specified)
 *    sign: [+-]? sign flag / alignment.
 *    pad:  [0 ]? padding character.
 *    size: [0-9]* length / indent.
 *    mode: (\.[*]|[0-9]*)? precision / mode.
 *
 *    T: symbol (inline, typename, function or variable)
 *      %T: print qualified symbol name with arguments
 *      %.T: print simple symbol name
 *      %+T: expands fields on multiple lines
 *      %-T: same as '%+T', but skips first indent
 *
 *    t: token (abstract syntax tree)
 *      %t: print statement or expression
 *      %.t: print only the token
 *      %+t: expand statements on multiple lines
 *      %-t: same as '%+t', but skip first indent
 *
 *    K: symbol kind
 *    k: token kind
 *    A: instruction (asm)
 *    I: indent
 *    b: 32 bit bin value
 *    B: 64 bit bin value
 *    o: 32 bit oct value
 *    O: 64 bit oct value
 *    x: 32 bit hex value
 *    X: 64 bit hex value
 *    u: 32 bit unsigned value
 *    U: 64 bit unsigned value
 *    d: 32 bit signed value (decimal)
 *    D: 64 bit signed value (decimal)
 *    f: 32 bit floating point value
 *    F: 64 bit floating point value
 *    e: 32 bit floating point value (Scientific notation (mantissa/exponent))
 *    E: 64 bit floating point value (Scientific notation (mantissa/exponent))
 *    s: ansi string
 *    c: ansi character
 *    S: ?wide string
 *    C: ?wide character
 * 
 * flags
 *    +-
 *        oOuU: ignored
 *        bBxX: lowerCase / upperCase
 *        dDfFeE: forceSign /
 */
void printFmt(FILE *out, const char **esc, const char *fmt, ...);

/**
 * Print compilation and runtime errors.
 */
void printErr(rtContext rt, int level, const char *file, int line, const char *msg, ...);

/**
 * Dump all accessible symbols.
 */
void dumpApi(rtContext rt, userContext ctx, void action(userContext, symn));

/**
 * Print the current stacktrace.
 * 
 * @param ctx Debugger context
 * @param out The file to be used as output
 * @param indent indentation
 * @param maxCalls limit the size of the stacktrace 
 */
void traceCalls(dbgContext ctx, FILE *out, int indent, size_t maxCalls);

#endif

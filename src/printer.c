/*******************************************************************************
 *   File: printer.c
 *   Date: 2011/06/23
 *   Desc: Print formatted text to the output stream.
 *******************************************************************************
 */

#include "internal.h"
#include <math.h>

/// print a character
static inline int printChr(FILE *out, int chr) { return fputc(chr, out); }

/// print a string
static void printStr(FILE *out, const char **esc, const char *str) {
	if (str == NULL) {
		str = "(null)";
	}
	if (esc == NULL) {
		fputs(str, out);
	}
	else {
		int c;
		while ((c = *str++)) {
			if (esc[c & 0xff] != NULL) {
				fputs(esc[c & 0xff], out);
			}
			else {
				fputc(c & 0xff, out);
			}
		}
	}
}

/// print qualified name
static void printQualified(FILE *out, const char **esc, symn sym) {
	if (sym == NULL) {
		return;
	}
	if (sym->owner != NULL && sym->owner != sym) {
		printQualified(out, esc, sym->owner);
		printChr(out, '.');
	}
	if (sym->name != NULL) {
		printStr(out, esc, sym->name);
	}
	else {
		printStr(out, esc, "?");
	}
}

/// print array type
static void printArray(FILE *out, const char **esc, symn sym, dmpMode mode) {
	if (sym != NULL && isArrayType(sym)) {
		symn length = sym->fields;
		printArray(out, esc, sym->type, mode);
		if (length != NULL && isStatic(length)) {
			printFmt(out, esc, "[%d]", sym->size / sym->type->size);
		}
		else if (length != NULL) {
			printFmt(out, esc, "[]");
		}
		else {
			printFmt(out, esc, "[*]");
		}
		return;
	}
	printSym(out, esc, sym, mode, 0);
}

/// format an integer number
static char *formatNum(char *dst, int max, int prc, int radix, uint64_t num) {
	char *ptr = dst + max;
	int p = 0, plc = ',';
	*--ptr = 0;
	do {
		if (prc > 0 && ++p > prc) {
			*--ptr = (char) plc;
			p = 1;
		}
		*--ptr = "0123456789abcdef"[num % radix];
	} while (num /= radix);
	return ptr;
}

static void print_fmt(FILE *out, const char **esc, const char *msg, va_list ap) {
	char buff[1024], chr;

	while ((chr = *msg++)) {
		if (chr == '%') {
			char nil = 0;    // [?]? skip on null || zero value (may print pad char once)
			char sgn = 0;    // [+-]? sign flag / alignment.
			char pad = 0;    // [0 ]? padding character.
			int len = 0;     // [0-9]* length / mode
			int prc = 0;     // (\.\*|[0-9]*)? precision / indent
			int noPrc = 1;   // format string has no precision

			const char *fmt = msg - 1;
			char *str = NULL;

			if (*msg == '?') {
				nil = *msg++;
			}
			if (*msg == '+' || *msg == '-') {
				sgn = *msg++;
			}
			if (*msg == '0' || *msg == ' ') {
				pad = *msg++;
			}

			while (*msg >= '0' && *msg <= '9') {
				len = (len * 10) + (*msg - '0');
				msg++;
			}

			if (*msg == '.') {
				msg++;
				noPrc = 0;
				if (*msg == '*') {
					prc = va_arg(ap, int32_t);
					msg++;
				}
				else {
					prc = 0;
					while (*msg >= '0' && *msg <= '9') {
						prc = (prc * 10) + (*msg - '0');
						msg++;
					}
				}
			}

			switch (chr = *msg++) {
				case 0:
					msg -= 1;
					continue;

				default:
					printChr(out, chr);
					continue;

				case 'T': {		// type name
					symn sym = va_arg(ap, symn);

					if (sym == NULL && nil) {
						if (pad != 0) {
							printChr(out, pad);
						}
						continue;
					}

					switch (sgn) {
						default:
							if (noPrc) {
								prc = prShort;
							}
							break;

						case '-':
							len = -len;
							// fall through
						case '+':
							if (noPrc) {
								prc = prFull;
							}
							break;
					}
					printSym(out, esc, sym, (dmpMode) prc, len);
					continue;
				}

				case 't': {		// tree node
					astn ast = va_arg(ap, astn);

					if (ast == NULL && nil) {
						if (pad != 0) {
							printChr(out, pad);
						}
						continue;
					}

					switch (sgn) {
						default:
							if (noPrc) {
								prc = prShort;
							}
							break;

						case '-':
							len = -len;
							// fall through
						case '+':
							if (noPrc) {
								prc = prFull;
							}
							break;
					}
					printAst(out, esc, ast, (dmpMode) prc, len);
					continue;
				}

				case 'K': {		// symbol kind
					ccKind arg = va_arg(ap, ccKind);
					char *_stat = "";
					char *_const = "";
					char *_paral = "";
					char *_kind = "";
					char *_cast = "ERR";

					if (nil && arg == 0) {
						if (pad != 0) {
							printChr(out, pad);
						}
						continue;
					}

					switch (arg & MASK_cast) {
						default:
							break;

						case CAST_any:
							_cast = NULL;
							break;

						case CAST_vid:
							_cast = "void";
							break;

						case CAST_bit:
							_cast = "bool";
							break;

						case CAST_i32:
							_cast = "i32";
							break;

						case CAST_u32:
							_cast = "u32";
							break;

						case CAST_i64:
							_cast = "i64";
							break;

						case CAST_u64:
							_cast = "u64";
							break;

						case CAST_f32:
							_cast = "f32";
							break;

						case CAST_f64:
							_cast = "f64";
							break;

						case CAST_val:
							_cast = "val";
							break;

						case CAST_ref:
							_cast = "ref";
							break;

						case CAST_arr:
							_cast = "arr";
							break;

						case CAST_var:
							_cast = "var";
							break;
					}

					switch (arg & MASK_kind) {
						default:
							break;

						case KIND_def:
							if (_cast != NULL) {
								_kind = _cast;
								_cast = NULL;
							}
							else if (!nil) {
								_kind = "inline";
							}
							break;

						case KIND_typ:
							_kind = "typename";
							break;

						case KIND_fun:
							_kind = "function";
							break;

						case KIND_var:
							_kind = "variable";
							break;
					}

					if (arg & ATTR_stat) {
						_stat = "static ";
					}
					if (arg & ATTR_cnst) {
						_const = "const ";
					}
					if (arg & ATTR_paral) {
						_const = "parallel ";
					}
					if (_cast != NULL) {
						snprintf(buff, sizeof(buff), "%s%s%s%s(%s)", _stat, _const, _paral, _kind, _cast);
					}
					else {
						snprintf(buff, sizeof(buff), "%s%s%s%s", _stat, _const, _paral, _kind);
					}
					str = buff;
					break;
				}

				case 'k': {		// token kind
					ccToken arg = va_arg(ap, ccToken);

					if (arg == 0 && nil) {
						if (pad != 0) {
							printChr(out, pad);
						}
						continue;
					}

					if (arg < tok_last) {
						str = token_tbl[arg].name;
					}
					else {
						snprintf(buff, sizeof(buff), "TOKEN_%02x", arg);
						str = buff;
					}
					break;
				}

				case 'A': {		// instruction
					void *opc = va_arg(ap, void*);

					if (opc == NULL && nil) {
						str = "";
						len = 1;
						break;
					}

					switch (sgn) {
						default:
							if (noPrc) {
								prc = prAbsOffs;
							}
							break;

						case '-':
						case '+':
							if (noPrc) {
								// print using relative offset
								prc = prRelOffs;
							}
							break;
					}

					// length is the max number of bytes to be printed
					prc |= (prc & prAsmCode) | (len & prAsmCode);

					printAsm(out, esc, NULL, opc, (dmpMode) prc);
					continue;
				}

				case 'a': {		// offset
					size_t offs = va_arg(ap, size_t);

					if (offs == 0 && nil) {
						str = "";
						len = 1;
						break;
					}

					switch (sgn) {
						default:
							if (noPrc) {
								prc = prAbsOffs;
							}
							break;

						case '-':
						case '+':
							if (noPrc) {
								// print using relative offset
								prc = prRelOffs;
							}
							break;
					}
					printOfs(out, esc, NULL, NULL, offs, (dmpMode) prc);
					continue;
				}

				case 'I': {		// indent
					unsigned arg = va_arg(ap, unsigned);
					len = len ? len * arg : arg;
					if (pad == 0) {
						pad = '\t';
					}
					str = (char*)"";
					break;
				}

				case 'b': {		// bin32
					uint32_t num = va_arg(ap, uint32_t);
					if (num == 0 && nil) {
						len = pad != 0;
						str = "";
						break;
					}
					str = formatNum(buff, sizeof(buff), prc, 2, num);
					break;
				}

				case 'B': {		// bin64
					uint64_t num = va_arg(ap, uint64_t);
					if (num == 0 && nil) {
						len = pad != 0;
						str = "";
						break;
					}
					str = formatNum(buff, sizeof(buff), prc, 2, num);
					break;
				}

				case 'o': {		// oct32
					uint32_t num = va_arg(ap, uint32_t);
					if (num == 0 && nil) {
						len = pad != 0;
						str = "";
						break;
					}
					str = formatNum(buff, sizeof(buff), prc, 8, num);
					break;
				}

				case 'O': {		// oct64
					uint64_t num = va_arg(ap, uint64_t);
					if (num == 0 && nil) {
						len = pad != 0;
						str = "";
						break;
					}
					str = formatNum(buff, sizeof(buff), prc, 8, num);
					break;
				}

				case 'x': {		// hex32
					uint32_t num = va_arg(ap, uint32_t);
					if (num == 0 && nil) {
						len = pad != 0;
						str = "";
						break;
					}
					str = formatNum(buff, sizeof(buff), prc, 16, num);
					break;
				}

				case 'X': {		// hex64
					uint64_t num = va_arg(ap, uint64_t);
					if (num == 0 && nil) {
						len = pad != 0;
						str = "";
						break;
					}
					str = formatNum(buff, sizeof(buff), prc, 16, num);
					break;
				}

				case 'u':		// uint32
				case 'd': {		// dec32
					int neg = 0;
					uint32_t num = va_arg(ap, uint32_t);

					if (num == 0 && nil) {
						len = pad != 0;
						str = "";
						break;
					}

					if (chr == 'd' && (int32_t)num < 0) {
						num = (uint32_t) 0 -num;
						neg = -1;
					}

					str = formatNum(buff, sizeof(buff), prc, 10, num);
					if (neg) {
						*--str = '-';
					}
					else if (sgn == '+') {
						*--str = '+';
					}
					break;
				}

				case 'U':		// uint64
				case 'D': {		// dec64
					int neg = 0;
					uint64_t num = va_arg(ap, uint64_t);

					if (num == 0 && nil) {
						len = pad != 0;
						str = "";
						break;
					}

					if (chr == 'D' && (int64_t)num < 0) {
						num = (uint64_t) 0 -num;
						neg = -1;
					}
					str = formatNum(buff, sizeof(buff), prc, 10, num);
					if (neg) {
						*--str = '-';
					}
					else if (sgn == '+') {
						*--str = '+';
					}
					break;
				}

				case 'E':		// float64
				case 'F':		// float64
					chr -= 'A' - 'a';
					// fall through
				case 'e':		// float32
				case 'f': {		// float32
					float64_t num = va_arg(ap, float64_t);
					if (num == 0 && nil) {
						len = pad != 0;
						str = "";
						break;
					}

					if ((len = msg - fmt - 1) < 1020) {
						memcpy(buff, fmt, (size_t) len);
						if (len > 1 && buff[1] == '?') {
							len -= 1;
							memcpy(buff+1, buff+2, (size_t) len);
						}
						if (len > 1 && (buff[1] == '-' || buff[1] == '+')) {
							len -= 1;
							memcpy(buff+1, buff+2, (size_t) len);
							if (signbit(num) == 0) {
								fprintf(out, "+");
							}
						}
						buff[len++] = chr;
						buff[len] = 0;
						fprintf(out, buff, num);
					}
					continue;
				}

				case 's':		// string
					str = va_arg(ap, char*);
					if (str == NULL && nil) {
						len = pad != 0;
						str = "";
						break;
					}
					break;

				case 'c':		// char
					buff[0] = va_arg(ap, int);
					buff[1] = 0;
					str = buff;
					break;
			}

			if (str != NULL) {
				len -= strlen(str);

				if (pad == 0) {
					pad = ' ';
				}

				while (len > 0) {
					printChr(out, pad);
					len -= 1;
				}

				printStr(out, esc, str);
			}
		}
		else {
			printChr(out, chr);
		}
	}
	fflush(out);
}

void printAst(FILE *out, const char **esc, astn ast, dmpMode mode, int indent) {
	static const int exprLevel = 0;
	const int nlBody = mode & nlAstBody;
	const int nlElIf = mode & nlAstElIf;
	const int oneLine = mode & prOneLine;

	if (ast == NULL) {
		printStr(out, esc, NULL);
		return;
	}

	if (indent > 0) {
		if (!oneLine) {
			printFmt(out, esc, "%I", indent);
		}
	}
	else {
		indent = -indent;
	}

	ccToken kind = ast->kind;
	switch (kind) {
		default:
			printStr(out, esc, token_tbl[kind].name);
			break;

		//#{ STATEMENTS
		case STMT_end:
			if (mode == prName) {
				printStr(out, esc, token_tbl[kind].name);
				break;
			}

			printAst(out, esc, ast->stmt.stmt, mode, exprLevel);
			printStr(out, esc, ";");
			if (oneLine) {
				break;
			}
			break;

		case STMT_pbeg:
		case STMT_beg:
			if (mode == prName) {
				printStr(out, esc, token_tbl[kind].name);
				break;
			}

			if (oneLine) {
				printStr(out, esc, "{...}");
				break;
			}

			printStr(out, esc, "{");
			for (astn list = ast->stmt.stmt; list; list = list->next) {
				int indent2 = indent + 1;
				int exprStatement = 0;
				printStr(out, esc, "\n");
				if (list->kind < STMT_beg || list->kind > STMT_end) {
					switch (list->kind) {
						default:
							// not valid expression statement
							exprStatement = -1;
							indent2 = exprLevel;
							printFmt(out, esc, "%I", indent + 1);
							break;

						case OPER_fnc:
						case INIT_set:
						case ASGN_set:
							// valid expression statement
							exprStatement = 1;
							indent2 = exprLevel;
							printFmt(out, esc, "%I", indent + 1);
							break;

						case TOKEN_var:
							if (list->ref.link && list->ref.link->tag == list) {
								// valid declaration statement
								exprStatement = 1;
							}
							break;
					}
				}
				printAst(out, esc, list, mode, indent2);
				if (exprStatement != 0) {
					if (exprStatement < 0) {
						printFmt(out, esc, "; /* `%k` is not a valid statement */", list->kind);
					}
					else {
						printStr(out, esc, ";");
					}
				}
			}
			printFmt(out, esc, "\n%I%s", indent, "}");
			break;

		case STMT_sif:
		case STMT_if:
			printStr(out, esc, token_tbl[kind].name);
			if (mode == prName) {
				break;
			}

			printStr(out, esc, " (");
			printAst(out, esc, ast->stmt.test, mode, exprLevel);
			printStr(out, esc, ")");

			if (oneLine) {
				break;
			}

			// if then part
			if (ast->stmt.stmt != NULL) {
				kind = ast->stmt.stmt->kind;
				if (kind == STMT_beg && !nlBody) {
					printStr(out, esc, " ");
					printAst(out, esc, ast->stmt.stmt, mode, -indent);
				}
				else {
					printStr(out, esc, "\n");
					printAst(out, esc, ast->stmt.stmt, mode, indent + (kind != STMT_beg));
				}
			}
			else {
				printStr(out, esc, " ;");
			}

			// if else part
			if (ast->stmt.step != NULL) {
				kind = ast->stmt.step->kind;
				if ((kind == STMT_if || kind == STMT_sif) && !nlElIf) {
					printFmt(out, esc, "\n%Ielse ", indent);
					printAst(out, esc, ast->stmt.step, mode, -indent);
				}
				else if (kind == STMT_beg && !nlBody) {
					printFmt(out, esc, "\n%Ielse ", indent);
					printAst(out, esc, ast->stmt.step, mode, -indent);
				}
				else {
					printFmt(out, esc, "\n%Ielse\n", indent);
					printAst(out, esc, ast->stmt.step, mode, indent + (kind != STMT_beg));
				}
			}
			break;

		case STMT_pfor:
		case STMT_sfor:
		case STMT_for:
			printStr(out, esc, token_tbl[kind].name);
			if (mode == prName) {
				break;
			}

			printStr(out, esc, " (");
			if (ast->stmt.init) {
				printAst(out, esc, ast->stmt.init, mode | oneLine, exprLevel);
			}
			else {
				printStr(out, esc, " ");
			}
			printStr(out, esc, "; ");
			if (ast->stmt.test) {
				printAst(out, esc, ast->stmt.test, mode, exprLevel);
			}
			printStr(out, esc, "; ");
			if (ast->stmt.step) {
				printAst(out, esc, ast->stmt.step, mode, exprLevel);
			}
			printStr(out, esc, ")");

			if (oneLine) {
				break;
			}

			if (ast->stmt.stmt != NULL) {
				kind = ast->stmt.stmt->kind;
				if (kind == STMT_beg && !nlBody) {
					printStr(out, esc, " ");
					printAst(out, esc, ast->stmt.stmt, mode, -indent);
				}
				else {
					printStr(out, esc, "\n");
					printAst(out, esc, ast->stmt.stmt, mode, indent + (kind != STMT_beg));
				}
			}
			else {
				printStr(out, esc, " ;");
			}
			break;

		case STMT_ret:
		case STMT_con:
		case STMT_brk:
			printStr(out, esc, token_tbl[kind].name);
			if (mode == prName) {
				break;
			}
			if (ast->jmp.value != NULL) {
				printStr(out, esc, " ");
				printAst(out, esc, ast->jmp.value, mode, exprLevel);
			}
			printStr(out, esc, ";");
			break;

		//#}
		//#{ OPERATORS
		case OPER_fnc:		// '()'
			if (mode == prName) {
				printStr(out, esc, token_tbl[kind].name);
				break;
			}

			if (ast->op.lhso != NULL) {
				printAst(out, esc, ast->op.lhso, mode, exprLevel);
			}
			printChr(out, '(');
			if (ast->op.rhso != NULL) {
				printAst(out, esc, ast->op.rhso, mode, exprLevel);
			}
			printChr(out, ')');
			break;

		case OPER_idx:		// '[]'
			if (mode == prName) {
				printStr(out, esc, token_tbl[kind].name);
				break;
			}

			if (ast->op.lhso != NULL) {
				printAst(out, esc, ast->op.lhso, mode, exprLevel);
			}
			printChr(out, '[');
			if (ast->op.rhso != NULL) {
				printAst(out, esc, ast->op.rhso, mode, exprLevel);
			}
			printChr(out, ']');
			break;

		case OPER_dot:		// '.'
			if (mode == prName) {
				printStr(out, esc, token_tbl[kind].name);
				break;
			}
			printAst(out, esc, ast->op.lhso, mode, exprLevel);
			printChr(out, '.');
			printAst(out, esc, ast->op.rhso, mode, exprLevel);
			break;

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

		case OPER_com:		// ','

		case INIT_set:		// '='
		case ASGN_set:		// '='
			if (mode == prName) {
				printStr(out, esc, token_tbl[kind].name);
				break;
			}
			int precedence = token_tbl[kind].type & 0x0f;
			int putParen = indent > precedence;

			if ((mode & prAstCast) && ast->type) {
				printSym(out, esc, ast->type, prSymQual, 0);
				putParen = 1;
			}

			if (putParen) {
				printChr(out, '(');
			}

			if (ast->op.lhso != NULL) {
				printAst(out, esc, ast->op.lhso, mode, -precedence);
				if (kind != OPER_com) {
					printChr(out, ' ');
				}
			}

			printStr(out, esc, token_tbl[kind].name);

			// in case of unary operators: '-a' not '- a'
			if (ast->op.lhso != NULL) {
				printChr(out, ' ');
			}

			printAst(out, esc, ast->op.rhso, mode, -precedence);

			if (putParen) {
				printChr(out, ')');
			}
			break;

		case OPER_sel:		// '?:'
			if (mode == prName) {
				printStr(out, esc, token_tbl[kind].name);
				break;
			}
			precedence = token_tbl[kind].type & 0x0f;
			printAst(out, esc, ast->op.test, mode, -precedence);
			printStr(out, esc, " ? ");
			printAst(out, esc, ast->op.lhso, mode, -precedence);
			printStr(out, esc, " : ");
			printAst(out, esc, ast->op.rhso, mode, -precedence);
			break;
		//#}
		//#{ VALUES
		case TOKEN_opc:
			printOpc(out, esc, ast->opc.code, ast->opc.args);
			break;

		case TOKEN_val:
			if (ast->type->fmt == NULL) {
				printFmt(out, esc, "{%.T @%D}", ast->type, ast->cInt);
			}
			else if (ast->type->fmt == type_fmt_string) {
				printFmt(out, esc, "%c", type_fmt_string_chr);
				printFmt(out, esc ? esc : escapeStr(), type_fmt_string, ast->ref.name);
				printFmt(out, esc, "%c", type_fmt_string_chr);
			}
			else if (ast->type->fmt == type_fmt_character) {
				printFmt(out, esc, "%c", type_fmt_character_chr);
				printFmt(out, esc ? esc : escapeStr(), type_fmt_character, ast->cInt);
				printFmt(out, esc, "%c", type_fmt_character_chr);
			}
			else if (ast->type->fmt == type_fmt_signed32) {
				printFmt(out, esc, type_fmt_signed32, (int32_t) ast->cInt);
			}
			else if (ast->type->fmt == type_fmt_signed64) {
				printFmt(out, esc, type_fmt_signed64, (int64_t) ast->cInt);
			}
			else if (ast->type->fmt == type_fmt_unsigned32) {
				printFmt(out, esc, type_fmt_unsigned32, (uint32_t) ast->cInt);
			}
			else if (ast->type->fmt == type_fmt_unsigned64) {
				printFmt(out, esc, type_fmt_unsigned64, (uint64_t) ast->cInt);
			}
			else if (ast->type->fmt == type_fmt_float32) {
				printFmt(out, esc, type_fmt_float32, (float32_t) ast->cFlt);
			}
			else if (ast->type->fmt == type_fmt_float64) {
				printFmt(out, esc, type_fmt_float64, (float64_t) ast->cFlt);
			}
			else if (ast->type->fmt == type_fmt_typename) {
				printFmt(out, esc, type_fmt_typename, ast->type);
			}
			else {
				printFmt(out, esc, ast->type->fmt, ast->cInt);
			}
			break;

		case TOKEN_var:
			if (ast->ref.link != NULL) {
				if (ast->ref.link->tag == ast) {
					printSym(out, esc, ast->ref.link, mode & ~prSymQual, -indent);
				}
				else {
					printSym(out, esc, ast->ref.link, prName, 0);
				}
			}
			else {
				printStr(out, esc, ast->ref.name);
			}
			break;
		//#}
	}
}

void printSym(FILE *out, const char **esc, symn sym, dmpMode mode, int indent) {
	int oneLine = mode & prOneLine;
	int pr_attr = mode & prAttr;

	int pr_qual = mode & prSymQual;
	int pr_args = mode & prSymArgs;
	int pr_result = 0;
	int pr_type = mode & prSymType;
	int pr_init = mode & prSymInit;
	int pr_body = !oneLine && pr_init;

	if (sym == NULL) {
		printStr(out, esc, NULL);
		return;
	}

	if (indent > 0) {
		printFmt(out, esc, "%I", indent);
	}
	else {
		indent = -indent;
	}

	if (pr_attr) {
		if (isStatic(sym)) {
			printStr(out, esc, "static ");
		}
		if (isConst(sym) != 0) {
			printStr(out, esc, "const ");
		}
	}

	if (sym->name && *sym->name) {
		if (pr_qual) {
			printQualified(out, esc, sym);
		}
		else {
			printStr(out, esc, sym->name);
		}
	}

	switch (sym->kind & MASK_kind) {
		case KIND_typ:
			if (castOf(sym) == CAST_arr) {
				if (sym->name == NULL) {
					printArray(out, esc, sym, mode);
					return;
				}
			}
			else if (pr_body) {
				symn arg;
				printFmt(out, esc, ": struct %s", "{\n");

				for (arg = sym->fields; arg; arg = arg->next) {
					printSym(out, esc, arg, mode & ~prSymQual, indent + 1);
					if (arg->kind != KIND_typ) {		// nested struct
						printStr(out, esc, ";\n");
					}
				}
				printFmt(out, esc, "%I%s", indent, "}");
			}
			break;

		case KIND_def:
		case KIND_fun:
		case KIND_var: {
			symn type = sym->type;
			if (pr_args && sym->params != NULL) {
				symn arg = sym->params;
				int first = 1;
				if (!pr_result) {
					type = arg->type;
					arg = arg->next;
					pr_type = 1;
				}
				printChr(out, '(');
				for ( ; arg; arg = arg->next) {
					if (!first) {
						printStr(out, esc, ", ");
					}
					else {
						first = 0;
					}
					printSym(out, esc, arg, (mode | prSymType) & ~prSymQual, 0);
				}
				printChr(out, ')');
			}
			if (pr_type && type) {
				printStr(out, esc, ": ");
				printSym(out, esc, type, prName, -indent);
			}
			if (pr_init && sym->init) {
				printStr(out, esc, " := ");
				printAst(out, esc, sym->init, mode, -indent);
			}
			break;
		}

		default:
			fatal(ERR_INTERNAL_ERROR);
			return;
	}
}

void printFmt(FILE *out, const char **esc, const char *fmt, ...) {
	va_list ap;
	va_start(ap, fmt);
	print_fmt(out, esc, fmt, ap);
	va_end(ap);
}

void print_log(rtContext rt, raiseLevel level, const char *file, int line, rtValue *inspect, const char *msg, va_list vaList) {
	FILE *out = rt->logFile;
	const char **esc = NULL;
	const char *logType = "UNKNOWN";

	if (rt->cc && rt->cc->siff && level < raiseWarn) {
		// convert errors to warnings inside static if (false) { ... }
		level = raiseWarn;
	}
	if (level > (int)rt->logLevel) {
		// no need to show the log on the current level
		return;
	}
	switch (level) {
		default:
			if (level > raiseWarn && level < raiseInfo) {
				logType = "warn";
				break;
			}
			break;

		case raiseFatal:
			logType = "fatal";
			rt->errors += 1;
			break;

		case raiseError:
			logType = "error";
			rt->errors += 1;
			break;

		case raisePrint:
			logType = NULL;
			break;

		case raiseWarn:
			logType = "warn";
			break;

		case raiseInfo:
			logType = "info";
			break;

		case raiseDebug:
			logType = "debug";
			break;

		case raiseVerbose:
			logType = "verbose";
			break;
	}
	if (out == NULL) {
		return;
	}

	int wasOutput = 0;
	if (file != NULL && line > 0) {
		printFmt(out, esc, "%?s:%?u", file, line);
		wasOutput = 1;
	}

	if (logType != NULL) {
		if (wasOutput) {
			printStr(rt->logFile, NULL, ": ");
		}
		printStr(out, esc, logType);
		wasOutput = 1;
	}

	if (msg != NULL) {
		if (wasOutput) {
			printStr(rt->logFile, NULL, ": ");
		}
		print_fmt(out, NULL, msg, vaList);
		wasOutput = 1;
	}

	if (inspect && inspect->type != NULL && inspect->ref != NULL) {
		if (wasOutput) {
			printStr(rt->logFile, NULL, ": ");
		}
		printVal(rt->logFile, NULL, rt, inspect->type, inspect->ref, prGlobal, 0);
		wasOutput = 1;
	}

	if (wasOutput) {
		printChr(out, '\n');
		fflush(out);
	}
}

void printLog(rtContext rt, raiseLevel level, const char *file, int line, rtValue *inspect, const char *msg, ...) {
	va_list vaList;
	va_start(vaList, msg);
	print_log(rt, level, file, line, inspect, msg, vaList);
	va_end(vaList);
}

void dumpApi(rtContext rt, userContext ctx, void dumpSym(userContext, symn)) {
	symn sym, bp[maxTokenCount], *sp = bp;

	if (rt == NULL || dumpSym == NULL) {
		dieif(rt == NULL, ERR_INTERNAL_ERROR);
		dieif(dumpSym == NULL, ERR_INTERNAL_ERROR);
		return;
	}

	// compilation errors or compiler was not initialized.
	if (rt->cc == NULL || rt->main == NULL) {
		dieif(rt->cc == NULL, ERR_INTERNAL_ERROR);
		dieif(rt->main == NULL, ERR_INTERNAL_ERROR);
		return;
	}
	if (rt->cc->scope != rt->main->fields) {
		dieif(rt->cc->scope != rt->main->fields, ERR_INTERNAL_ERROR);
		return;
	}
	for (*sp = rt->main->fields; sp >= bp;) {
		if (!(sym = *sp)) {
			--sp;
			continue;
		}
		*sp = sym->next;

		if (sym->fields != NULL) {
			if (sym == rt->main) {
				continue;
			}
			*++sp = sym->fields;
		}
		dumpSym(ctx, sym);
	}
	dumpSym(ctx, rt->main);
}

void logFILE(rtContext rt, FILE *file) {
	// close previous opened file
	if (rt->closeLog != 0) {
		fclose(rt->logFile);
		rt->closeLog = 0;
	}
	rt->logFile = file;
}

FILE *logFile(rtContext rt, char *file, int append) {

	// close previous file and set stderr
	logFILE(rt, stdout);

	if (file != NULL) {
		rt->logFile = fopen(file, append ? "ab" : "wb");
		if (rt->logFile != NULL) {
			rt->closeLog = 1;
		}
	}
	return rt->logFile;
}

const char **escapeStr() {
	static const char *escape[256];
	static char initialized = 0;
	if (!initialized) {
		memset((void*)escape, 0, sizeof(escape));
		escape['\n'] = "\\n";
		escape['\r'] = "\\r";
		escape['\t'] = "\\t";
		escape['\''] = "\\'";
		escape['\"'] = "\\\"";
		initialized = 1;
	}
	return escape;
}

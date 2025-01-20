#include <stdint.h>

#include "zcode.h"

// rtroutine: ident number, local count, array of instructions
// zinstr: opcode, array of operands, store, branch, string

struct rtroutine rtroutines[] = {
	{
		R_ENTRY,
		0,
		(struct zinstr []) {
			{Z_CALL1N, {ROUTINE(R_TERPTEST)}},

			{Z_STORE, {SMALL(REG_R_SPA), ROUTINE(R_SPACE_PRINT_AUTO)}},
			{Z_STORE, {SMALL(REG_R_USIMPLE), ROUTINE(R_UNIFY_SIMPLE)}},
			{Z_STORE, {SMALL(REG_AUXBASE), REF(G_AUXBASE)}},

			{Z_STORE, {SMALL(REG_A+1), REF(G_PROGRAM_ENTRY)}},

			{OP_LABEL(1)},
			{Z_STORE, {SMALL(REG_TRAIL), REF(G_AUXSIZE)}},
			{Z_STORE, {SMALL(REG_TOP), REF(G_HEAPBASE)}},
			{Z_STORE, {SMALL(REG_CONT), ROUTINE(R_QUIT_PRED)}},
			{Z_STORE, {SMALL(REG_ENV), REF(G_HEAPEND)}},
			{Z_STORE, {SMALL(REG_CHOICE), VALUE(REG_ENV)}},
			{Z_STORE, {SMALL(REG_SPACE), SMALL(4)}},
			{Z_STORE, {SMALL(REG_COLL), SMALL(0)}},
			{Z_STORE, {SMALL(REG_UPPER), SMALL(0)}},
			{Z_STORE, {SMALL(REG_FORWORDS), SMALL(0)}},
			{Z_STORE, {SMALL(REG_STATUSBAR), SMALL(0)}},
			{Z_STORE, {SMALL(REG_STYLE), SMALL(0)}},
			{Z_STORE, {SMALL(REG_NSPAN), SMALL(0)}},
			{Z_STORE, {SMALL(REG_FGCOLOR), LARGE(0xFFFE)}},
			{Z_STORE, {SMALL(REG_BGCOLOR), LARGE(0xFFFE)}},
			{Z_SET_WINDOW, {SMALL(0)}},
			{Z_CALL1N, {ROUTINE(R_RESET_STYLE)}},

			{Z_CALL1S, {ROUTINE(R_OUTERLOOP)}, REG_TEMP},
			{Z_ADD, {VALUE(REG_TEMP), VALUE(REG_4000)}, REG_A+0},
			{Z_CALL1N, {ROUTINE(R_LINE)}},
			{Z_STORE, {SMALL(REG_A+1), REF(G_ERROR_ENTRY)}},
			{Z_JUMP, {REL_LABEL(1)}},
			{Z_END},
		}
	},
	{
		R_OUTERLOOP,
		0,
			// arg 0: optional argument to entry point
			// arg 1: entry point
		(struct zinstr []) {
			{Z_CATCH, {}, REG_FATALJMP},

			{Z_CALL2N, {ROUTINE(R_TRY_ME_ELSE_0), VALUE(REG_CONT)}}, // which is R_QUIT_PRED
			{Z_STORE, {SMALL(REG_STOPCHOICE), VALUE(REG_CHOICE)}},

			{Z_CALL2N, {ROUTINE(R_INNERLOOP), VALUE(REG_A+1)}},

			{OP_LABEL(1)},
			{Z_LOADW, {VALUE(REG_CHOICE), SMALL(CHOICE_NEXTPC)}, REG_TEMP},
			{Z_CALL2N, {ROUTINE(R_INNERLOOP), VALUE(REG_TEMP)}},
			{Z_JUMP, {REL_LABEL(1)}},
			{Z_END},
		}
	},
	{
		R_UNICODE,
		2,
			// 0 (param): Unicode char
			// 1: temp
		(struct zinstr []) {
			{Z_JG, {VALUE(REG_STATUSBAR), SMALL(1)}, 0, RFALSE},

			{Z_LOADB, {SMALL(0), SMALL(0x32)}, REG_LOCAL+1},
			{Z_JL, {VALUE(REG_LOCAL+1), SMALL(1)}, 0, 1},

			{Z_CHECK_UNICODE, {VALUE(REG_LOCAL+0)}, REG_LOCAL+1},
			{Z_TESTN, {VALUE(REG_LOCAL+1), SMALL(1)}, 0, 1},

			{Z_PRINT_UNICODE, {VALUE(REG_LOCAL+0)}},
			{Z_RFALSE},

			{OP_LABEL(1)},
			{Z_JE, {VALUE(REG_LOCAL+0), LARGE(0x2013)}, 0, 2},	// en dash
			{Z_JE, {VALUE(REG_LOCAL+0), LARGE(0x2014)}, 0, 3},	// em dash
			{Z_JE, {VALUE(REG_LOCAL+0), LARGE(0x201c)}, 0, 4},	// upper 66 quote
			{Z_JE, {VALUE(REG_LOCAL+0), LARGE(0x201d)}, 0, 4},	// upper 99 quote
			{Z_JE, {VALUE(REG_LOCAL+0), LARGE(0x201e)}, 0, 4},	// lower 99 quote
			{Z_JE, {VALUE(REG_LOCAL+0), LARGE(0x2022)}, 0, 5},	// bullet

			{Z_PRINTLIT, {}, 0, 0, "?"},
			{Z_RFALSE},

			{OP_LABEL(2)},
			{Z_PRINTLIT, {}, 0, 0, "-"},
			{Z_RFALSE},

			{OP_LABEL(3)},
			{Z_PRINTLIT, {}, 0, 0, "--"},
			{Z_RFALSE},

			{OP_LABEL(4)},
			{Z_PRINTLIT, {}, 0, 0, "\""},
			{Z_RFALSE},

			{OP_LABEL(5)},
			{Z_PRINTLIT, {}, 0, 0, "*"},
			{Z_RFALSE},

			{Z_END},
		}
	},
	{
		R_INNERLOOP,
		1,
			// 0 (param): first routine to invoke
		(struct zinstr []) {
			{Z_CATCH, {}, REG_FAILJMP},
			{OP_LABEL(1)},
#if 0
			{Z_PRINTLIT, {}, 0, 0, "<"},
			{Z_PRINTNUM, {VALUE(REG_LOCAL+0)}},
			{Z_PRINTLIT, {}, 0, 0, ">\r"},
#endif
			{Z_CALL1S, {VALUE(REG_LOCAL+0)}, REG_LOCAL+0},
			{Z_JNZ, {VALUE(REG_LOCAL+0)}, 0, 1},

			{Z_LOADW, {VALUE(REG_CHOICE), SMALL(CHOICE_NEXTPC)}, REG_LOCAL+0},
			{Z_JUMP, {REL_LABEL(1)}},

			{Z_END},
		}
	},
	{
		R_NOSPACE_PRINT_NOSPACE,
		1,
			// 0 (param): what to print, packed address
		(struct zinstr []) {
			{Z_JG, {VALUE(REG_STATUSBAR), SMALL(1)}, 0, RFALSE},
			{Z_JNE, {VALUE(REG_SPACE), SMALL(2)}, 0, 1},
			{Z_PRINTLIT, {}, 0, 0, " "},

			{OP_LABEL(1)},
			{Z_CALL2N, {ROUTINE(R_PRINT_UPPER), VALUE(REG_LOCAL+0)}},
			{Z_STORE, {SMALL(REG_SPACE), SMALL(1)}},
			{Z_RFALSE},
			{Z_END},
		}
	},
	{
		R_NOSPACE_PRINT_AUTO,
		1,
			// 0 (param): what to print, packed address
		(struct zinstr []) {
			{Z_JG, {VALUE(REG_STATUSBAR), SMALL(1)}, 0, RFALSE},
			{Z_JNE, {VALUE(REG_SPACE), SMALL(2)}, 0, 1},
			{Z_PRINTLIT, {}, 0, 0, " "},

			{OP_LABEL(1)},
			{Z_CALL2N, {ROUTINE(R_PRINT_UPPER), VALUE(REG_LOCAL+0)}},
			{Z_STORE, {SMALL(REG_SPACE), SMALL(0)}},
			{Z_RFALSE},
			{Z_END},
		}
	},
	{
		R_SPACE_PRINT_NOSPACE,
		1,
			// 0 (param): what to print, packed address
		(struct zinstr []) {
			{Z_JG, {VALUE(REG_STATUSBAR), SMALL(1)}, 0, RFALSE},
			{Z_CALL1N, {ROUTINE(R_SYNC_SPACE)}},
			{Z_CALL2N, {ROUTINE(R_PRINT_UPPER), VALUE(REG_LOCAL+0)}},
			{Z_STORE, {SMALL(REG_SPACE), SMALL(1)}},
			{Z_RFALSE},
			{Z_END},
		}
	},
	{
		R_SPACE_PRINT_AUTO,
		1,
			// 0 (param): what to print, packed address
		(struct zinstr []) {
			{Z_JG, {VALUE(REG_STATUSBAR), SMALL(1)}, 0, RFALSE},
			{Z_CALL1N, {ROUTINE(R_SYNC_SPACE)}},
			{Z_CALL2N, {ROUTINE(R_PRINT_UPPER), VALUE(REG_LOCAL+0)}},
			{Z_STORE, {SMALL(REG_SPACE), SMALL(0)}},
			{Z_RFALSE},
			{Z_END},
		}
	},
	{
		R_PRINT_UPPER,
		4,
			// 0 (param): what to print, packed address (and temp)
			// 1: memory area
			// 2: saved REG_COLL
			// 3: word count
		(struct zinstr []) {
			{Z_JNZ, {VALUE(REG_UPPER)}, 0, 1},
			{Z_PRINTPADDR, {VALUE(REG_LOCAL+0)}},
			{Z_RFALSE},

			{OP_LABEL(1)},
			{Z_STORE, {SMALL(REG_LOCAL+2), VALUE(REG_COLL)}},
			/* One word for storing the length. +1 to round up. */
			{Z_CALL2S, {ROUTINE(R_AUX_ALLOC), LARGE(1 + (MAXSTRING + 1) / 2)}, REG_LOCAL+1},

			{Z_OUTPUT_STREAM, {SMALL(3), VALUE(REG_LOCAL+1)}},
			{Z_PRINTPADDR, {VALUE(REG_LOCAL+0)}},
			{Z_OUTPUT_STREAM, {LARGE(0x10000-3)}},

			{Z_LOADW, {VALUE(REG_LOCAL+1), SMALL(0)}, REG_LOCAL+3},
			{Z_JZ, {VALUE(REG_LOCAL+3)}, 0, 3},

			{Z_ADD, {VALUE(REG_LOCAL+1), SMALL(2)}, REG_LOCAL+1},
			{Z_LOADB, {VALUE(REG_LOCAL+1), SMALL(0)}, REG_LOCAL+0},

			{Z_JL, {VALUE(REG_LOCAL+0), SMALL(0x61)}, 0, 4},
			{Z_JGE, {VALUE(REG_LOCAL+0), SMALL(0x7b)}, 0, 4},

			// convert to uppercase
			{Z_AND, {VALUE(REG_LOCAL+0), SMALL(0xdf)}, REG_LOCAL+0},
			{Z_PRINTCHAR, {VALUE(REG_LOCAL+0)}},
			{Z_INC, {SMALL(REG_LOCAL+1)}},
			{Z_DEC, {SMALL(REG_LOCAL+3)}},

			{OP_LABEL(4)},
			{Z_CALLVN, {ROUTINE(R_PRINT_N_ZSCII), VALUE(REG_LOCAL+3), VALUE(REG_LOCAL+1)}},
			{Z_STORE, {SMALL(REG_UPPER), SMALL(0)}},

			{OP_LABEL(3)},
			{Z_STORE, {SMALL(REG_COLL), VALUE(REG_LOCAL+2)}},
			{Z_RFALSE},
			{Z_END},
		}
	},
	{
		R_NOSPACE,
		0,
		(struct zinstr []) {
			{Z_JNZ, {VALUE(REG_FORWORDS)}, 0, RFALSE},
			{Z_JNZ, {VALUE(REG_SPACE)}, 0, RFALSE},
			{Z_STORE, {SMALL(REG_SPACE), SMALL(1)}},
			{Z_RFALSE},
			{Z_END},
		}
	},
	{
		R_SPACE,
		0,
		(struct zinstr []) {
			{Z_JNZ, {VALUE(REG_FORWORDS)}, 0, RFALSE},
			{Z_JG, {VALUE(REG_SPACE), SMALL(1)}, 0, RFALSE},
			{Z_STORE, {SMALL(REG_SPACE), SMALL(2)}},
			{Z_RFALSE},
			{Z_END},
		}
	},
	{
		R_SPACE_N,
		1,
			// 0 (param): number of spaces to print, as tagged value
		(struct zinstr []) {
			{Z_JNZ, {VALUE(REG_FORWORDS)}, 0, RFALSE},
			{Z_JG, {VALUE(REG_STATUSBAR), SMALL(1)}, 0, RFALSE},
			{Z_CALL2S, {ROUTINE(R_DEREF), VALUE(REG_LOCAL+0)}, REG_LOCAL+0},
			{Z_STORE, {SMALL(REG_SPACE), SMALL(1)}},
			{Z_JLE, {VALUE(REG_LOCAL+0), VALUE(REG_4000)}, 0, RFALSE},
			{OP_LABEL(1)},
			{Z_PRINTLIT, {}, 0, 0, " "},
			{Z_DEC_JGE, {SMALL(REG_LOCAL+0), LARGE(0x4001)}, 0, 1},
			{Z_RFALSE},
			{Z_END},
		}
	},
	{
		R_LINE,
		0,
		(struct zinstr []) {
			{Z_JNZ, {VALUE(REG_FORWORDS)}, 0, RFALSE},
			{Z_JG, {VALUE(REG_SPACE), SMALL(3)}, 0, RFALSE},
			{Z_JNZ, {VALUE(REG_STATUSBAR)}, 0, 1},

			{Z_NEW_LINE},
			{Z_STORE, {SMALL(REG_SPACE), SMALL(4)}},
			{Z_RFALSE},

			{OP_LABEL(1)},
			{Z_JNE, {VALUE(REG_STATUSBAR), SMALL(1)}, 0, RFALSE},
			{Z_INC, {SMALL(REG_YPOS)}},
			{Z_STORE, {SMALL(REG_SPACE), SMALL(4)}},
			{Z_JG, {VALUE(REG_YPOS), VALUE(REG_CURRSPLIT)}, 0, RFALSE},
			{Z_SET_CURSOR, {VALUE(REG_YPOS), VALUE(REG_XOFFSET)}},
			{Z_RFALSE},

			{Z_END},
		}
	},
	{
		R_PAR,
		0,
		(struct zinstr []) {
			{Z_JNZ, {VALUE(REG_FORWORDS)}, 0, RFALSE},
			{Z_JG, {VALUE(REG_SPACE), SMALL(4)}, 0, RFALSE},
			{Z_JNZ, {VALUE(REG_STATUSBAR)}, 0, 2},

			{Z_JE, {VALUE(REG_SPACE), SMALL(4)}, 0, 1},
			{Z_NEW_LINE},
			{OP_LABEL(1)},
			{Z_NEW_LINE},
			{Z_STORE, {SMALL(REG_SPACE), SMALL(5)}},
			{Z_RFALSE},

			{OP_LABEL(2)},
			{Z_JNE, {VALUE(REG_STATUSBAR), SMALL(1)}, 0, RFALSE},
			{Z_JE, {VALUE(REG_SPACE), SMALL(4)}, 0, 3},
			{Z_INC, {SMALL(REG_YPOS)}},
			{OP_LABEL(3)},
			{Z_INC, {SMALL(REG_YPOS)}},

			{Z_JLE, {VALUE(REG_YPOS), VALUE(REG_CURRSPLIT)}, 0, 4},
			{Z_STORE, {SMALL(REG_YPOS), VALUE(REG_CURRSPLIT)}},
			{OP_LABEL(4)},

			{Z_SET_CURSOR, {VALUE(REG_YPOS), VALUE(REG_XOFFSET)}},
			{Z_STORE, {SMALL(REG_SPACE), SMALL(5)}},
			{Z_RFALSE},
			{Z_END},
		}
	},
	{
		R_PAR_N,
		1,
			// 0 (param): raw number of blank lines to produce
		(struct zinstr []) {
			{Z_JNZ, {VALUE(REG_FORWORDS)}, 0, RFALSE},

			{Z_CALL1N, {ROUTINE(R_LINE)}},
			{Z_ADD, {VALUE(REG_LOCAL+0), SMALL(3)}, REG_LOCAL+0},
			{Z_JG, {VALUE(REG_SPACE), VALUE(REG_LOCAL+0)}, 0, RFALSE},

			{Z_JNZ, {VALUE(REG_STATUSBAR)}, 0, 2},

			{OP_LABEL(1)},
			{Z_NEW_LINE},
			{Z_INC_JLE, {SMALL(REG_SPACE), VALUE(REG_LOCAL+0)}, 0, 1},
			{Z_RFALSE},

			{OP_LABEL(2)},
			{Z_JNE, {VALUE(REG_STATUSBAR), SMALL(1)}, 0, RFALSE},
			{Z_INC, {SMALL(REG_YPOS)}},
			{Z_INC_JLE, {SMALL(REG_SPACE), VALUE(REG_LOCAL+0)}, 0, 2},

			{Z_JLE, {VALUE(REG_YPOS), VALUE(REG_CURRSPLIT)}, 0, 4},
			{Z_STORE, {SMALL(REG_YPOS), VALUE(REG_CURRSPLIT)}},
			{OP_LABEL(4)},

			{Z_SET_CURSOR, {VALUE(REG_YPOS), VALUE(REG_XOFFSET)}},
			{Z_RFALSE},
			{Z_END},
		}
	},
	{
		R_SYNC_SPACE,
		0,
		(struct zinstr []) {
			{Z_JZ, {VALUE(REG_SPACE)}, 0, 1},
			{Z_JNE, {VALUE(REG_SPACE), SMALL(2)}, 0, RFALSE},
			{OP_LABEL(1)},
			{Z_STORE, {SMALL(REG_SPACE), SMALL(3)}},
			{Z_JG, {VALUE(REG_STATUSBAR), SMALL(1)}, 0, RFALSE},
			{Z_PRINTLIT, {}, 0, 0, " "},
			{Z_RFALSE},
			{Z_END},
		}
	},
	{
		R_PRINT_OR_PUSH,
		1,
			// 0 (param): what to print, reference
		(struct zinstr []) {
			{Z_JZ, {VALUE(REG_FORWORDS)}, 0, 1},
			{Z_CALL2N, {ROUTINE(R_COLLECT_PUSH), VALUE(REG_LOCAL+0)}},
			{Z_RFALSE},
			{OP_LABEL(1)},
			{Z_CALL2N, {ROUTINE(R_PRINT_VALUE), VALUE(REG_LOCAL+0)}},
			{Z_RFALSE},
			{Z_END},
		}
	},
	{
		R_TRACE_VALUE,
		1,
			// 0 (param): what to print, reference
		(struct zinstr []) {
			{Z_CALLVN, {ROUTINE(R_PRINT_VALUE), VALUE(REG_LOCAL+0), SMALL(3)}},
			{Z_RFALSE},
			{Z_END},
		}
	},
	{
		R_PRINT_VALUE,
		5,
			// 0 (param): what to print, reference
			// 1 (param): flags, 2 = @, 1 = +
			// 2: saved REG_COLL
			// 3: string length, list iterator
			// 4: temp
		(struct zinstr []) {
			{Z_JG, {VALUE(REG_STATUSBAR), SMALL(1)}, 0, RFALSE},
			{Z_CALL2S, {ROUTINE(R_DEREF), VALUE(REG_LOCAL+0)}, REG_LOCAL+0},
			{Z_AND, {VALUE(REG_LOCAL+0), LARGE(0xff00)}, REG_LOCAL+4},
			{Z_JNE, {VALUE(REG_LOCAL+4), LARGE(0x3e00)}, 0, 1},

			// single-char dictionary word
			{Z_TESTN, {VALUE(REG_LOCAL+1), SMALL(2)}, 0, 23},
			{Z_CALL1N, {ROUTINE(R_SYNC_SPACE)}},
			{Z_PRINTLIT, {}, 0, 0, "@"},
			{Z_STORE, {SMALL(REG_SPACE), SMALL(0)}},

			{OP_LABEL(23)},
			{Z_AND, {VALUE(REG_LOCAL+0), SMALL(0xff)}, REG_LOCAL+4},
			{Z_JNZ, {VALUE(REG_SPACE)}, 0, 3},

			{Z_TEST, {VALUE(REG_LOCAL+1), SMALL(1)}, 0, 3},
			{Z_JE, {VALUE(REG_LOCAL+4), SMALL('.'), SMALL(',')}, 0, 11},
			{Z_JE, {VALUE(REG_LOCAL+4), SMALL(';'), SMALL(')')}, 0, 11},

			{OP_LABEL(3)},
			{Z_CALL1N, {ROUTINE(R_SYNC_SPACE)}},

			{OP_LABEL(11)},
			{Z_JZ, {VALUE(REG_UPPER)}, 0, 14},

			// convert to uppercase
			{Z_STORE, {SMALL(REG_UPPER), SMALL(0)}},
			{Z_JL, {VALUE(REG_LOCAL+4), SMALL(0x61)}, 0, 14},
			{Z_JGE, {VALUE(REG_LOCAL+4), SMALL(0x7b)}, 0, 14},
			{Z_AND, {VALUE(REG_LOCAL+4), SMALL(0xdf)}, REG_LOCAL+4},

			{OP_LABEL(14)},
			{Z_JLE, {VALUE(REG_LOCAL+4), SMALL(32)}, 0, 25},
			{Z_JL, {VALUE(REG_LOCAL+4), SMALL(129)}, 0, 24},
			{Z_JG, {VALUE(REG_LOCAL+4), SMALL(132)}, 0, 24},

			{OP_LABEL(25)},
			{Z_PRINTCHAR, {SMALL('\\')}},
			{Z_STORE, {SMALL(REG_LOCAL+4), SMALL('?')}},

			{OP_LABEL(24)},
			{Z_PRINTCHAR, {VALUE(REG_LOCAL+4)}},

			{Z_STORE, {SMALL(REG_SPACE), SMALL(0)}},
			{Z_JNE, {VALUE(REG_LOCAL+4), SMALL('(')}, 0, RFALSE},
			{Z_TEST, {VALUE(REG_LOCAL+1), SMALL(1)}, 0, RFALSE},

			{Z_STORE, {SMALL(REG_SPACE), SMALL(1)}},
			{Z_RFALSE},

			{OP_LABEL(1)},

			{Z_CALL1N, {ROUTINE(R_SYNC_SPACE)}},
			{Z_STORE, {SMALL(REG_SPACE), SMALL(0)}},

			{Z_JLE, {VALUE(REG_LOCAL+0), VALUE(REG_C000)}, 0, 2},

			// bound value
			{Z_JL, {VALUE(REG_LOCAL+0), VALUE(REG_4000)}, 0, 4},

			// integer
			{Z_AND, {VALUE(REG_LOCAL+0), VALUE(REG_3FFF)}, REG_LOCAL+0},
			{Z_PRINTNUM, {VALUE(REG_LOCAL+0)}},
			{Z_STORE, {SMALL(REG_UPPER), SMALL(0)}},
			{Z_RFALSE},

			{OP_LABEL(2)},
			// unbound variable
			{Z_PRINTLIT, {}, 0, 0, "$"},
			{Z_STORE, {SMALL(REG_UPPER), SMALL(0)}},
			{Z_RFALSE},

			{OP_LABEL(4)},
			{Z_JGE, {VALUE(REG_LOCAL+0), SMALL(0)}, 0, 5},

			{Z_JL, {VALUE(REG_LOCAL+0), VALUE(REG_E000)}, 0, 15},

			// extended dictionary word
			{Z_AND, {VALUE(REG_LOCAL+0), VALUE(REG_NIL)}, REG_LOCAL+0},
			{Z_LOADW, {SMALL(0), VALUE(REG_LOCAL+0)}, REG_LOCAL+3},
			{Z_JL, {VALUE(REG_LOCAL+3), SMALL(0)}, 0, 17},

			{Z_STORE, {SMALL(REG_SPACE), SMALL(1)}},
			{Z_CALLVN, {ROUTINE(R_PRINT_VALUE), VALUE(REG_LOCAL+3), VALUE(REG_LOCAL+1)}},
			{Z_JUMP, {REL_LABEL(16)}},

			{OP_LABEL(17)},
			{Z_AND, {VALUE(REG_LOCAL+3), VALUE(REG_NIL)}, REG_LOCAL+3},
			{Z_LOADW, {SMALL(0), VALUE(REG_LOCAL+3)}, REG_LOCAL+4},
			{Z_STORE, {SMALL(REG_SPACE), SMALL(1)}},
			{Z_CALLVN, {ROUTINE(R_PRINT_VALUE), VALUE(REG_LOCAL+4), VALUE(REG_LOCAL+1)}},
			{Z_AND, {VALUE(REG_LOCAL+1), SMALL(1)}, REG_LOCAL+1},
			{Z_LOADW, {SMALL(2), VALUE(REG_LOCAL+3)}, REG_LOCAL+3},
			{Z_JNE, {VALUE(REG_LOCAL+3), VALUE(REG_NIL)}, 0, 17},

			{OP_LABEL(16)},
			{Z_AND, {VALUE(REG_LOCAL+1), SMALL(1)}, REG_LOCAL+1},
			{Z_JZ, {VALUE(REG_LOCAL+1)}, 0, 20},
			{Z_PRINTLIT, {}, 0, 0, "+"},

			{OP_LABEL(20)},
			{Z_LOADW, {SMALL(2), VALUE(REG_LOCAL+0)}, REG_LOCAL+3},
			{Z_JE, {VALUE(REG_LOCAL+3), VALUE(REG_NIL)}, 0, RFALSE},

			{OP_LABEL(18)},
			{Z_AND, {VALUE(REG_LOCAL+3), VALUE(REG_NIL)}, REG_LOCAL+3},
			{Z_LOADW, {SMALL(0), VALUE(REG_LOCAL+3)}, REG_LOCAL+4},
			{Z_STORE, {SMALL(REG_SPACE), SMALL(1)}},
			{Z_CALLVN, {ROUTINE(R_PRINT_VALUE), VALUE(REG_LOCAL+4), VALUE(REG_LOCAL+1)}},
			{Z_LOADW, {SMALL(2), VALUE(REG_LOCAL+3)}, REG_LOCAL+3},
			{Z_JNE, {VALUE(REG_LOCAL+3), VALUE(REG_NIL)}, 0, 18},

			{Z_STORE, {SMALL(REG_SPACE), SMALL(0)}},
			{Z_RFALSE},

			{OP_LABEL(15)},
			// list
			{Z_PRINTLIT, {}, 0, 0, "["},
			{Z_STORE, {SMALL(REG_UPPER), SMALL(0)}},
			{Z_STORE, {SMALL(REG_SPACE), SMALL(1)}},
			{Z_AND, {VALUE(REG_LOCAL+1), SMALL(1)}, REG_LOCAL+4},

			{OP_LABEL(19)},
			{Z_SUB, {VALUE(REG_LOCAL+0), VALUE(REG_4000)}, REG_LOCAL+0},
			{Z_CALLVN, {ROUTINE(R_PRINT_VALUE), VALUE(REG_LOCAL+0), VALUE(REG_LOCAL+4)}},
			{Z_INC, {SMALL(REG_LOCAL+0)}},
			{Z_CALL2S, {ROUTINE(R_DEREF), VALUE(REG_LOCAL+0)}, REG_LOCAL+0},
			{Z_JE, {VALUE(REG_LOCAL+0), VALUE(REG_NIL)}, 0, 21},
			{Z_AND, {VALUE(REG_LOCAL+0), VALUE(REG_E000)}, REG_LOCAL+2},
			{Z_JE, {VALUE(REG_LOCAL+2), VALUE(REG_C000)}, 0, 19},
			{Z_PRINTLIT, {}, 0, 0, " | "},
			{Z_STORE, {SMALL(REG_SPACE), SMALL(1)}},
			{Z_CALLVN, {ROUTINE(R_PRINT_VALUE), VALUE(REG_LOCAL+0), VALUE(REG_LOCAL+4)}},

			{OP_LABEL(21)},
			{Z_PRINTLIT, {}, 0, 0, "]"},
			{Z_STORE, {SMALL(REG_SPACE), SMALL(0)}},
			{Z_RFALSE},

			{OP_LABEL(5)},
			{Z_JL, {VALUE(REG_LOCAL+0), VALUE(REG_2000)}, 0, 6},

			// dictionary word
			{Z_TESTN, {VALUE(REG_LOCAL+1), SMALL(2)}, 0, 22},
			{Z_PRINTLIT, {}, 0, 0, "@"},

			{OP_LABEL(22)},
			{Z_SUB, {VALUE(REG_LOCAL+0), VALUE(REG_2000)}, REG_LOCAL+0},
			{Z_MUL, {VALUE(REG_LOCAL+0), SMALL(6)}, REG_LOCAL+0},
			{Z_ADD, {VALUE(REG_LOCAL+0), REF(G_DICT_TABLE)}, REG_LOCAL+0},

			{Z_JNZ, {VALUE(REG_UPPER)}, 0, 10},

			{Z_PRINTADDR, {VALUE(REG_LOCAL+0)}},
			{Z_RFALSE},

			{OP_LABEL(6)},
			{Z_JE, {VALUE(REG_LOCAL+0), VALUE(REG_NIL)}, 0, 7},

			// object id
			{Z_PRINTLIT, {}, 0, 0, "#"},
			{Z_PRINTOBJ, {VALUE(REG_LOCAL+0)}},
			{Z_STORE, {SMALL(REG_UPPER), SMALL(0)}},
			{Z_RFALSE},

			{OP_LABEL(7)},
			// empty list
			{Z_PRINTLIT, {}, 0, 0, "[]"},
			{Z_STORE, {SMALL(REG_UPPER), SMALL(0)}},
			{Z_RFALSE},

			{OP_LABEL(10)},
			// capitalized dictionary word
			{Z_STORE, {SMALL(REG_LOCAL+4), REF(G_SCRATCH)}},
			{Z_OUTPUT_STREAM, {SMALL(3), VALUE(REG_LOCAL+4)}},
			{Z_PRINTADDR, {VALUE(REG_LOCAL+0)}},
			{Z_OUTPUT_STREAM, {LARGE(0x10000-3)}},

			{Z_LOADW, {VALUE(REG_LOCAL+4), SMALL(0)}, REG_LOCAL+3},
			{Z_LOADB, {VALUE(REG_LOCAL+4), SMALL(2)}, REG_LOCAL+0},

			{Z_JL, {VALUE(REG_LOCAL+0), SMALL(0x61)}, 0, 13},
			{Z_JGE, {VALUE(REG_LOCAL+0), SMALL(0x7b)}, 0, 13},
			{Z_AND, {VALUE(REG_LOCAL+0), SMALL(0xdf)}, REG_LOCAL+0},
			{Z_PRINTCHAR, {VALUE(REG_LOCAL+0)}},
			{Z_ADD, {VALUE(REG_LOCAL+4), SMALL(3)}, REG_LOCAL+4},
			{Z_DEC, {SMALL(REG_LOCAL+3)}},

			{OP_LABEL(13)},
			{Z_CALLVN, {ROUTINE(R_PRINT_N_ZSCII), VALUE(REG_LOCAL+3), VALUE(REG_LOCAL+4)}},

			{OP_LABEL(12)},
			{Z_STORE, {SMALL(REG_UPPER), SMALL(0)}},
			{Z_RFALSE},
			{Z_END},
		}
	},
	{
		R_ENABLE_STYLE,
		1,
			// 0 (param): style to enable
		(struct zinstr []) {
			{Z_JNZ, {VALUE(REG_FORWORDS)}, 0, RFALSE},
			{Z_JNZ, {VALUE(REG_STATUSBAR)}, 0, RFALSE},
			{Z_CALL1N, {ROUTINE(R_SYNC_SPACE)}},
			{Z_TEXTSTYLE, {VALUE(REG_LOCAL+0)}},
			{Z_RFALSE},
			{Z_END},
		}
	},
	{
		R_RESET_STYLE,
		1,
			// 0: temporary variable
		(struct zinstr []) {
			{Z_CALL2N, {ROUTINE(R_SET_STYLE), VALUE(REG_STYLE)}},
			// Now for the new stuff: use REG_FGCOLOR and REG_BGCOLOR to reset the colors
			
			{Z_JL, {VALUE(REG_FGCOLOR), SMALL(0)}, 0, 1}, // Is the foreground color negative? If so, we should do a "standard" setting instead of a "true" setting
			{Z_TRUECOLOR, {VALUE(REG_FGCOLOR), LARGE(0xFFFE)}}, // Pass -2 as the background color to not change it (0 means black with this opcode)
			{Z_JUMP, {REL_LABEL(2)}}, // And move on to the background color
			
			{OP_LABEL(1)}, // Foreground color is negative, we need to do some math
			{Z_SUB, {VALUE(REG_FFFF), VALUE(REG_FGCOLOR)}, REG_LOCAL+0}, // Subtract the color code from -1 to convert it into a non-negative number
			{Z_COLOR, {VALUE(REG_LOCAL+0), SMALL(0)}}, // Pass 0 as the background color to not change it
	//		{Z_PRINTLIT, {}, 0, 0, "FG: "},
	//		{Z_PRINTNUM, {VALUE(REG_LOCAL+0)}},
			
			{OP_LABEL(2)}, // Now for the background
			{Z_JL, {VALUE(REG_BGCOLOR), SMALL(0)}, 0, 3}, // Second verse, same as the first
			{Z_TRUECOLOR, {LARGE(0xFFFE), VALUE(REG_BGCOLOR)}},
			{Z_RFALSE}, // No need to do any more work after this, we can just return false
			
			{OP_LABEL(3)}, // Background is negative
			{Z_SUB, {VALUE(REG_FFFF), VALUE(REG_BGCOLOR)}, REG_LOCAL+0},
			{Z_COLOR, {SMALL(0), VALUE(REG_LOCAL+0)}},
	//		{Z_PRINTLIT, {}, 0, 0, "BG: "},
	//		{Z_PRINTNUM, {VALUE(REG_LOCAL+0)}},
			
			// End new stuff
			{Z_RFALSE},
			{Z_END},
		}
	},
	{
		R_SET_STYLE,
		1,
			// 0 (param): style(s) to set
		(struct zinstr []) {
			{Z_JNZ, {VALUE(REG_FORWORDS)}, 0, RFALSE},
			{Z_JNZ, {VALUE(REG_STATUSBAR)}, 0, RFALSE},
			{Z_TEXTSTYLE, {SMALL(0)}},
			{Z_TEXTSTYLE, {VALUE(REG_LOCAL+0)}},
			{Z_RFALSE},
			{Z_END},
		}
	},
	{
		R_SET_COLORS,
		3,
			// 0 (param): foreground color to use
			// 1 (param): background color to use
		(struct zinstr []) {
			// Push BGCOLOR, then FGCOLOR
			{Z_CALL2N, {ROUTINE(R_AUX_PUSH1), VALUE(REG_BGCOLOR)}},
			{Z_CALL2N, {ROUTINE(R_AUX_PUSH1), VALUE(REG_FGCOLOR)}},
			
			{OP_LABEL(1)},
			// If either of them is INHERIT (-1), don't update the register, just leave it as it is
			{Z_JE, {VALUE(REG_LOCAL+0), VALUE(REG_FFFF)}, 0, 2},
			{Z_STORE, {SMALL(REG_FGCOLOR), VALUE(REG_LOCAL+0)}},
			
			{OP_LABEL(2)},
			{Z_JE, {VALUE(REG_LOCAL+1), VALUE(REG_FFFF)}, 0, 3},
			{Z_STORE, {SMALL(REG_BGCOLOR), VALUE(REG_LOCAL+1)}},
			
			{OP_LABEL(3)},
			{Z_CALL1N, {ROUTINE(R_RESET_STYLE)}},
			{Z_RFALSE},
			{Z_END},
		}
	},
	{
		R_RESET_COLORS, // Pull saved color values off the stack and restore them
						// We don't have to do this for styles because that's handled within BEGIN_ and END_ for the boxes and spans, but the Dialog assembler can't pass more than three arguments to a routine (it doesn't support Z_CALLVN2), so we just assemble a separate routine call instead
		0,
		(struct zinstr []) {
			// Pull FGCOLOR, then BGCOLOR
			{Z_DEC, {SMALL(REG_COLL)}},
			{Z_LOADW, {VALUE(REG_AUXBASE), VALUE(REG_COLL)}, REG_FGCOLOR},
			{Z_DEC, {SMALL(REG_COLL)}},
			{Z_LOADW, {VALUE(REG_AUXBASE), VALUE(REG_COLL)}, REG_BGCOLOR},
			{Z_CALL1N, {ROUTINE(R_RESET_STYLE)}},
			{Z_RFALSE},
			{Z_END},
		}
	},
	{
		R_IS_WORD,
		1,
			// 0 (param): tagged reference
		(struct zinstr []) {
			{Z_CALL2S, {ROUTINE(R_DEREF), VALUE(REG_LOCAL+0)}, REG_LOCAL+0},
			{Z_AND, {VALUE(REG_LOCAL+0), VALUE(REG_E000)}, REG_LOCAL+0},
			{Z_JE, {VALUE(REG_LOCAL+0), LARGE(0x2000)}, 0, RTRUE},
			{Z_JE, {VALUE(REG_LOCAL+0), VALUE(REG_E000)}, 0, RTRUE},
			{Z_RFALSE},
			{Z_END},
		}
	},
	{
		R_IS_UNKNOWN_WORD,
		1,
			// 0 (param): tagged reference
		(struct zinstr []) {
			{Z_CALL2S, {ROUTINE(R_DEREF), VALUE(REG_LOCAL+0)}, REG_LOCAL+0},
			{Z_TESTN, {VALUE(REG_LOCAL+0), VALUE(REG_E000)}, 0, RFALSE},
			{Z_AND, {VALUE(REG_LOCAL+0), VALUE(REG_NIL)}, REG_LOCAL+0},
			{Z_LOADW, {SMALL(0), VALUE(REG_LOCAL+0)}, REG_LOCAL+0},
			{Z_JL, {VALUE(REG_LOCAL+0), SMALL(0)}, 0, RTRUE},
			{Z_RFALSE},
			{Z_END},
		}
	},
	{
		R_UNIFY,
		5,
			// 0 (param): first value
			// 1 (param): second value
			// 2: pointer to first heap cell
			// 3: pointer to second heap cell
			// 4: temp
		(struct zinstr []) {
			{OP_LABEL(10)},

			// dereference first
			{Z_JGE, {VALUE(REG_LOCAL+0), VALUE(REG_C000)}, 0, 2},
			{OP_LABEL(1)},
			{Z_ADD, {VALUE(REG_LOCAL+0), VALUE(REG_LOCAL+0)}, REG_LOCAL+2},
			{Z_LOADW, {VALUE(REG_LOCAL+2), SMALL(0)}, REG_LOCAL+4},
			{Z_JZ, {VALUE(REG_LOCAL+4)}, 0, 2},
			{Z_STORE, {SMALL(REG_LOCAL+0), VALUE(REG_LOCAL+4)}},
			{Z_JL, {VALUE(REG_LOCAL+0), VALUE(REG_C000)}, 0, 1},
			{OP_LABEL(2)},

			// dereference second
			{Z_JGE, {VALUE(REG_LOCAL+1), VALUE(REG_C000)}, 0, 3},
			{OP_LABEL(8)},
			{Z_ADD, {VALUE(REG_LOCAL+1), VALUE(REG_LOCAL+1)}, REG_LOCAL+3},
			{Z_LOADW, {VALUE(REG_LOCAL+3), SMALL(0)}, REG_LOCAL+4},
			{Z_JZ, {VALUE(REG_LOCAL+4)}, 0, 3},
			{Z_STORE, {SMALL(REG_LOCAL+1), VALUE(REG_LOCAL+4)}},
			{Z_JL, {VALUE(REG_LOCAL+1), VALUE(REG_C000)}, 0, 8},
			{OP_LABEL(3)},

			// same variable/pair/dictext or identical object/integer? then succeed
			{Z_JE, {VALUE(REG_LOCAL+0), VALUE(REG_LOCAL+1)}, 0, RFALSE},

			{Z_JL, {VALUE(REG_LOCAL+0), VALUE(REG_C000)}, 0, 4},
			{Z_JL, {VALUE(REG_LOCAL+1), VALUE(REG_C000)}, 0, 5},

			{Z_TESTN, {VALUE(REG_LOCAL+0), VALUE(REG_E000)}, 0, 12},
			// first is an extended dictionary word
			{Z_AND, {VALUE(REG_LOCAL+0), VALUE(REG_NIL)}, REG_LOCAL+4},
			{Z_LOADW, {SMALL(0), VALUE(REG_LOCAL+4)}, REG_LOCAL+4},
			{Z_JL, {VALUE(REG_LOCAL+4), SMALL(0)}, 0, 13},

			// with a regular dictionary word as its mandatory part
			{Z_TESTN, {VALUE(REG_LOCAL+1), VALUE(REG_E000)}, 0, 14},
			{Z_AND, {VALUE(REG_LOCAL+1), VALUE(REG_NIL)}, REG_LOCAL+1},
			{Z_LOADW, {SMALL(0), VALUE(REG_LOCAL+1)}, REG_LOCAL+1},
			{OP_LABEL(14)},
			{Z_JE, {VALUE(REG_LOCAL+4), VALUE(REG_LOCAL+1)}, 0, RFALSE},

			{Z_THROW, {SMALL(0), VALUE(REG_FAILJMP)}},

			{OP_LABEL(13)},
			// first is an extended dictionary word with a mandatory character list
			// fail if second is not, otherwise unbox and retry
			{Z_TESTN, {VALUE(REG_LOCAL+1), VALUE(REG_E000)}, 0, 11},
			{Z_STORE, {SMALL(REG_LOCAL+0), VALUE(REG_LOCAL+4)}},
			{Z_AND, {VALUE(REG_LOCAL+1), VALUE(REG_NIL)}, REG_LOCAL+1},
			{Z_LOADW, {SMALL(0), VALUE(REG_LOCAL+1)}, REG_LOCAL+1},
			{Z_JUMP, {REL_LABEL(10)}},

			{OP_LABEL(12)},
			{Z_TESTN, {VALUE(REG_LOCAL+1), VALUE(REG_E000)}, 0, 15},
			// second is an extended dictionary word
			{Z_AND, {VALUE(REG_LOCAL+1), VALUE(REG_NIL)}, REG_LOCAL+4},
			{Z_LOADW, {SMALL(0), VALUE(REG_LOCAL+4)}, REG_LOCAL+4},
			{Z_JE, {VALUE(REG_LOCAL+0), VALUE(REG_LOCAL+4)}, 0, RFALSE},

			{OP_LABEL(11)},
			{Z_THROW, {SMALL(0), VALUE(REG_FAILJMP)}},

			{OP_LABEL(15)},

			// different types or mismatching object/integer? then fail
			{Z_JGE, {VALUE(REG_LOCAL+0), SMALL(0)}, 0, 11},
			{Z_JGE, {VALUE(REG_LOCAL+1), SMALL(0)}, 0, 11},

			// both are pairs
			{Z_SUB, {VALUE(REG_LOCAL+0), VALUE(REG_4000)}, REG_LOCAL+0},	// pair ref -> first var ref
			{Z_SUB, {VALUE(REG_LOCAL+1), VALUE(REG_4000)}, REG_LOCAL+1},	// pair ref -> first var ref
			{Z_CALLVN, {ROUTINE(R_UNIFY), VALUE(REG_LOCAL+0), VALUE(REG_LOCAL+1)}},
			{Z_ADD, {VALUE(REG_LOCAL+0), SMALL(1)}, REG_LOCAL+0},		// second var ref
			{Z_ADD, {VALUE(REG_LOCAL+1), SMALL(1)}, REG_LOCAL+1},		// second var ref
			{Z_JUMP, {REL_LABEL(10)}},

			{OP_LABEL(6)},
			{Z_THROW, {SMALL(FATAL_AUX), VALUE(REG_FATALJMP)}},

			{OP_LABEL(4)},
			// first is a variable, second might be
			{Z_JGE, {VALUE(REG_LOCAL+1), VALUE(REG_C000)}, 0, 7},

			// both are variables. unify by pretending that the older one is not.
			{Z_JL, {VALUE(REG_LOCAL+3), VALUE(REG_LOCAL+2)}, 0, 7},

			{OP_LABEL(5)},
			// second is a variable, first is not
			{Z_STOREW, {VALUE(REG_LOCAL+3), SMALL(0), VALUE(REG_LOCAL+0)}},
			{Z_LOADW, {VALUE(REG_CHOICE), SMALL(CHOICE_TOP)}, REG_TEMP},
			{Z_JGE, {VALUE(REG_LOCAL+3), VALUE(REG_TEMP)}, 0, RFALSE},
			{Z_DEC_JL, {SMALL(REG_TRAIL), VALUE(REG_COLL)}, 0, 6},
			{Z_STOREW, {VALUE(REG_AUXBASE), VALUE(REG_TRAIL), VALUE(REG_LOCAL+3)}},
			{Z_RFALSE},

			{OP_LABEL(7)},
			// first is a variable, second is not
			{Z_STOREW, {VALUE(REG_LOCAL+2), SMALL(0), VALUE(REG_LOCAL+1)}},
			{Z_LOADW, {VALUE(REG_CHOICE), SMALL(CHOICE_TOP)}, REG_TEMP},
			{Z_JGE, {VALUE(REG_LOCAL+2), VALUE(REG_TEMP)}, 0, RFALSE},
			{Z_DEC_JL, {SMALL(REG_TRAIL), VALUE(REG_COLL)}, 0, 6},
			{Z_STOREW, {VALUE(REG_AUXBASE), VALUE(REG_TRAIL), VALUE(REG_LOCAL+2)}},
			{Z_RFALSE},
			{Z_END},
		}
	},
	{
		R_UNIFY_SIMPLE,
		4,
			// 0 (param): first value, must be simple
			// 1 (param): second value
			// 2: temp
			// 3: pointer to second heap cell
		(struct zinstr []) {
			// dereference second
			{OP_LABEL(2)},
			{Z_JGE, {VALUE(REG_LOCAL+1), VALUE(REG_C000)}, 0, 4},
			{Z_ADD, {VALUE(REG_LOCAL+1), VALUE(REG_LOCAL+1)}, REG_LOCAL+3},
			{Z_LOADW, {VALUE(REG_LOCAL+3), SMALL(0)}, REG_LOCAL+2},
			{Z_JZ, {VALUE(REG_LOCAL+2)}, 0, 3},
			{Z_STORE, {SMALL(REG_LOCAL+1), VALUE(REG_LOCAL+2)}},
			{Z_JL, {VALUE(REG_LOCAL+1), VALUE(REG_C000)}, 0, 2},

			{OP_LABEL(4)},
			// second value is a bound value
			{Z_JE, {VALUE(REG_LOCAL+0), VALUE(REG_LOCAL+1)}, 0, RFALSE},
			{Z_TESTN, {VALUE(REG_LOCAL+1), VALUE(REG_E000)}, 0, 1},
			// its an extended dictionary word, so unbox it
			{Z_AND, {VALUE(REG_LOCAL+1), VALUE(REG_NIL)}, REG_LOCAL+1},
			{Z_LOADW, {SMALL(0), VALUE(REG_LOCAL+1)}, REG_LOCAL+1},
			{Z_JE, {VALUE(REG_LOCAL+0), VALUE(REG_LOCAL+1)}, 0, RFALSE},
			{OP_LABEL(1)},
			{Z_THROW, {SMALL(0), VALUE(REG_FAILJMP)}},

			{OP_LABEL(6)},
			{Z_THROW, {SMALL(FATAL_AUX), VALUE(REG_FATALJMP)}},

			{OP_LABEL(3)},
			// second value is an unbound variable
			{Z_STOREW, {VALUE(REG_LOCAL+3), SMALL(0), VALUE(REG_LOCAL+0)}},
			{Z_LOADW, {VALUE(REG_CHOICE), SMALL(CHOICE_TOP)}, REG_TEMP},
			{Z_JGE, {VALUE(REG_LOCAL+3), VALUE(REG_TEMP)}, 0, RFALSE},
			{Z_DEC_JL, {SMALL(REG_TRAIL), VALUE(REG_COLL)}, 0, 6},
			{Z_STOREW, {VALUE(REG_AUXBASE), VALUE(REG_TRAIL), VALUE(REG_LOCAL+3)}},
			{Z_RFALSE},
			{Z_END},
		}
	},
	{
		R_PUSH_VAR,
		1,
			// 0: temp
			// returns tagged reference
		(struct zinstr []) {
			{Z_JGE, {VALUE(REG_TOP), VALUE(REG_ENV)}, 0, 1},
			{Z_JL, {VALUE(REG_TOP), VALUE(REG_CHOICE)}, 0, 2},

			{OP_LABEL(1)},
			{Z_THROW, {SMALL(FATAL_HEAP), VALUE(REG_FATALJMP)}},

			{OP_LABEL(2)},
			{Z_LSHIFT, {VALUE(REG_TOP), VALUE(REG_FFFF)}, REG_LOCAL+0},
			{Z_OR, {VALUE(REG_LOCAL+0), VALUE(REG_8000)}, REG_LOCAL+0},
			{Z_STOREW, {VALUE(REG_TOP), SMALL(0), SMALL(0)}},
			{Z_ADD, {VALUE(REG_TOP), SMALL(2)}, REG_TOP},
			{Z_RET, {VALUE(REG_LOCAL+0)}},
			{Z_END},
		}
	},
	{
		R_PUSH_VAR_SETENV,
		2,
			// 0 (param): env slot
			// 1: temp
			// returns tagged reference
		(struct zinstr []) {
			{Z_JGE, {VALUE(REG_TOP), VALUE(REG_ENV)}, 0, 1},
			{Z_JL, {VALUE(REG_TOP), VALUE(REG_CHOICE)}, 0, 2},

			{OP_LABEL(1)},
			{Z_THROW, {SMALL(FATAL_HEAP), VALUE(REG_FATALJMP)}},

			{OP_LABEL(2)},
			{Z_LSHIFT, {VALUE(REG_TOP), VALUE(REG_FFFF)}, REG_LOCAL+1},
			{Z_OR, {VALUE(REG_LOCAL+1), VALUE(REG_8000)}, REG_LOCAL+1},
			{Z_STOREW, {VALUE(REG_TOP), SMALL(0), SMALL(0)}},
			{Z_ADD, {VALUE(REG_TOP), SMALL(2)}, REG_TOP},
			{Z_STOREW, {VALUE(REG_ENV), VALUE(REG_LOCAL+0), VALUE(REG_LOCAL+1)}},
			{Z_RET, {VALUE(REG_LOCAL+1)}},
			{Z_END},
		}
	},
	{
		R_PUSH_VARS_SETENV,
		4,
			// 0 (param): first env slot
			// 1 (param): last env slot
			// 2: temp and var ref
			// 3: pointer to end of heap area
		(struct zinstr []) {
			{Z_ADD, {VALUE(REG_LOCAL+1), VALUE(REG_LOCAL+1)}, REG_LOCAL+2},
			{Z_ADD, {VALUE(REG_TOP), VALUE(REG_LOCAL+2)}, REG_LOCAL+3},
			
			{Z_JG, {VALUE(REG_LOCAL+3), VALUE(REG_ENV)}, 0, 1},
			{Z_JLE, {VALUE(REG_LOCAL+3), VALUE(REG_CHOICE)}, 0, 2},

			{OP_LABEL(1)},
			{Z_THROW, {SMALL(FATAL_HEAP), VALUE(REG_FATALJMP)}},

			{OP_LABEL(2)},
			{Z_LSHIFT, {VALUE(REG_TOP), VALUE(REG_FFFF)}, REG_LOCAL+2},
			{Z_OR, {VALUE(REG_LOCAL+2), VALUE(REG_8000)}, REG_LOCAL+2},

			{OP_LABEL(3)},
			{Z_STOREW, {VALUE(REG_TOP), SMALL(0), SMALL(0)}},
			{Z_STOREW, {VALUE(REG_ENV), VALUE(REG_LOCAL+0), VALUE(REG_LOCAL+2)}},
			{Z_ADD, {VALUE(REG_TOP), SMALL(2)}, REG_TOP},
			{Z_INC, {SMALL(REG_LOCAL+2)}},
			{Z_INC_JLE, {SMALL(REG_LOCAL+0), VALUE(REG_LOCAL+1)}, 0, 3},

			{Z_RFALSE},
			{Z_END},
		}
	},
	{
		R_PUSH_LIST_V,
		1,
			// 0 (param): first value (and temp)
			// returns tagged reference
		(struct zinstr []) {
			{Z_CALLVS, {ROUTINE(R_PUSH_PAIR_VV), VALUE(REG_LOCAL+0), VALUE(REG_NIL)}, REG_LOCAL+0},
			{Z_RET, {VALUE(REG_LOCAL+0)}},
			{Z_END},
		}
	},
	{
		R_PUSH_PAIR_VV,
		2,
			// 0 (param): head value (and temp)
			// 1 (param): tail value
			// returns tagged reference
		(struct zinstr []) {
			{Z_STORE, {SMALL(REG_PAIR), VALUE(REG_TOP)}},
			{Z_ADD, {VALUE(REG_TOP), SMALL(4)}, REG_TOP},
			{Z_JG, {VALUE(REG_TOP), VALUE(REG_ENV)}, 0, 1},
			{Z_JLE, {VALUE(REG_TOP), VALUE(REG_CHOICE)}, 0, 2},

			{OP_LABEL(1)},
			{Z_THROW, {SMALL(FATAL_HEAP), VALUE(REG_FATALJMP)}},

			{OP_LABEL(2)},
			{Z_STOREW, {VALUE(REG_PAIR), SMALL(0), VALUE(REG_LOCAL+0)}},
			{Z_STOREW, {VALUE(REG_PAIR), SMALL(1), VALUE(REG_LOCAL+1)}},
			{Z_LSHIFT, {VALUE(REG_PAIR), VALUE(REG_FFFF)}, REG_LOCAL+0},
			{Z_OR, {VALUE(REG_LOCAL+0), VALUE(REG_C000)}, REG_LOCAL+0},
			{Z_RET, {VALUE(REG_LOCAL+0)}},
			{Z_END},
		}
	},
	{
		R_PUSH_PAIR_VR,
		3,
			// 0 (param): head value (and temp)
			// 1 (param): ref to register to receive tail var
			// 2: temp
			// returns tagged reference
		(struct zinstr []) {
			{Z_STORE, {SMALL(REG_PAIR), VALUE(REG_TOP)}},
			{Z_ADD, {VALUE(REG_TOP), SMALL(4)}, REG_TOP},
			{Z_JG, {VALUE(REG_TOP), VALUE(REG_ENV)}, 0, 1},
			{Z_JLE, {VALUE(REG_TOP), VALUE(REG_CHOICE)}, 0, 2},

			{OP_LABEL(1)},
			{Z_THROW, {SMALL(FATAL_HEAP), VALUE(REG_FATALJMP)}},

			{OP_LABEL(2)},
			{Z_STOREW, {VALUE(REG_PAIR), SMALL(0), VALUE(REG_LOCAL+0)}},
			{Z_LSHIFT, {VALUE(REG_PAIR), VALUE(REG_FFFF)}, REG_LOCAL+2},
			{Z_OR, {VALUE(REG_LOCAL+2), VALUE(REG_C000)}, REG_LOCAL+2},
			{Z_SUB, {VALUE(REG_LOCAL+2), VALUE(REG_3FFF)}, REG_LOCAL+0}, // pair ref -> second cell var ref
			{Z_STOREW, {VALUE(REG_PAIR), SMALL(1), SMALL(0)}},
			{Z_STORE, {VALUE(REG_LOCAL+1), VALUE(REG_LOCAL+0)}},
			{Z_RET, {VALUE(REG_LOCAL+2)}},
			{Z_END},
		}
	},
	{
		R_PUSH_PAIR_RV,
		3,
			// 0 (param): ref to register to receive head var
			// 1 (param): tail value (and temp)
			// 2: temp
			// returns tagged reference
		(struct zinstr []) {
			{Z_STORE, {SMALL(REG_PAIR), VALUE(REG_TOP)}},
			{Z_ADD, {VALUE(REG_TOP), SMALL(4)}, REG_TOP},
			{Z_JG, {VALUE(REG_TOP), VALUE(REG_ENV)}, 0, 1},
			{Z_JLE, {VALUE(REG_TOP), VALUE(REG_CHOICE)}, 0, 2},

			{OP_LABEL(1)},
			{Z_THROW, {SMALL(FATAL_HEAP), VALUE(REG_FATALJMP)}},

			{OP_LABEL(2)},
			{Z_STOREW, {VALUE(REG_PAIR), SMALL(1), VALUE(REG_LOCAL+1)}},
			{Z_LSHIFT, {VALUE(REG_PAIR), VALUE(REG_FFFF)}, REG_LOCAL+2},
			{Z_OR, {VALUE(REG_LOCAL+2), VALUE(REG_C000)}, REG_LOCAL+2},
			{Z_SUB, {VALUE(REG_LOCAL+2), VALUE(REG_4000)}, REG_LOCAL+1}, // pair ref -> first cell var ref
			{Z_STOREW, {VALUE(REG_PAIR), SMALL(0), SMALL(0)}},
			{Z_STORE, {VALUE(REG_LOCAL+0), VALUE(REG_LOCAL+1)}},
			{Z_RET, {VALUE(REG_LOCAL+2)}},
			{Z_END},
		}
	},
	{
		R_PUSH_PAIR_RR,
		4,
			// 0 (param): ref to register to receive head var
			// 1 (param): ref to register to receive tail var
			// 2: temp
			// 3: temp
			// returns tagged reference
		(struct zinstr []) {
			{Z_STORE, {SMALL(REG_PAIR), VALUE(REG_TOP)}},
			{Z_ADD, {VALUE(REG_TOP), SMALL(4)}, REG_TOP},
			{Z_JG, {VALUE(REG_TOP), VALUE(REG_ENV)}, 0, 1},
			{Z_JLE, {VALUE(REG_TOP), VALUE(REG_CHOICE)}, 0, 2},

			{OP_LABEL(1)},
			{Z_THROW, {SMALL(FATAL_HEAP), VALUE(REG_FATALJMP)}},

			{OP_LABEL(2)},
			{Z_LSHIFT, {VALUE(REG_PAIR), VALUE(REG_FFFF)}, REG_LOCAL+2},
			{Z_OR, {VALUE(REG_LOCAL+2), VALUE(REG_C000)}, REG_LOCAL+2},
			{Z_SUB, {VALUE(REG_LOCAL+2), VALUE(REG_4000)}, REG_LOCAL+3}, // pair ref -> first cell var ref
			{Z_STOREW, {VALUE(REG_PAIR), SMALL(0), SMALL(0)}},
			{Z_STORE, {VALUE(REG_LOCAL+0), VALUE(REG_LOCAL+3)}},
			{Z_INC, {SMALL(REG_LOCAL+3)}},
			{Z_STOREW, {VALUE(REG_PAIR), SMALL(1), SMALL(0)}},
			{Z_STORE, {VALUE(REG_LOCAL+1), VALUE(REG_LOCAL+3)}},
			{Z_RET, {VALUE(REG_LOCAL+2)}},
			{Z_END},
		}
	},
	{
		R_PUSH_LIST_VV,
		2,
			// 0 (param): first value (and temp)
			// 1 (param): second value
			// returns tagged reference
		(struct zinstr []) {
			{Z_STORE, {SMALL(REG_PAIR), VALUE(REG_TOP)}},
			{Z_ADD, {VALUE(REG_TOP), SMALL(8)}, REG_TOP},
			{Z_JG, {VALUE(REG_TOP), VALUE(REG_ENV)}, 0, 1},
			{Z_JLE, {VALUE(REG_TOP), VALUE(REG_CHOICE)}, 0, 2},

			{OP_LABEL(1)},
			{Z_THROW, {SMALL(FATAL_HEAP), VALUE(REG_FATALJMP)}},

			{OP_LABEL(2)},
			{Z_STOREW, {VALUE(REG_PAIR), SMALL(0), VALUE(REG_LOCAL+1)}},
			{Z_STOREW, {VALUE(REG_PAIR), SMALL(1), VALUE(REG_NIL)}},
			{Z_STOREW, {VALUE(REG_PAIR), SMALL(2), VALUE(REG_LOCAL+0)}},
			{Z_LSHIFT, {VALUE(REG_PAIR), VALUE(REG_FFFF)}, REG_LOCAL+0},
			{Z_OR, {VALUE(REG_LOCAL+0), VALUE(REG_C000)}, REG_LOCAL+0},
			{Z_STOREW, {VALUE(REG_PAIR), SMALL(3), VALUE(REG_LOCAL+0)}},
			{Z_ADD, {VALUE(REG_LOCAL+0), SMALL(2)}, REG_LOCAL+0},
			{Z_RET, {VALUE(REG_LOCAL+0)}},
			{Z_END},
		}
	},
	{
		R_PUSH_LIST_VVV,
		3,
			// 0 (param): first value
			// 1 (param): second value (and temp)
			// 2 (param): third value
			// returns tagged reference
		(struct zinstr []) {
			{Z_STORE, {SMALL(REG_PAIR), VALUE(REG_TOP)}},
			{Z_ADD, {VALUE(REG_TOP), SMALL(12)}, REG_TOP},
			{Z_JG, {VALUE(REG_TOP), VALUE(REG_ENV)}, 0, 1},
			{Z_JLE, {VALUE(REG_TOP), VALUE(REG_CHOICE)}, 0, 2},

			{OP_LABEL(1)},
			{Z_THROW, {SMALL(FATAL_HEAP), VALUE(REG_FATALJMP)}},

			{OP_LABEL(2)},
			{Z_STOREW, {VALUE(REG_PAIR), SMALL(0), VALUE(REG_LOCAL+2)}},
			{Z_STOREW, {VALUE(REG_PAIR), SMALL(1), VALUE(REG_NIL)}},
			{Z_STOREW, {VALUE(REG_PAIR), SMALL(2), VALUE(REG_LOCAL+1)}},
			{Z_LSHIFT, {VALUE(REG_PAIR), VALUE(REG_FFFF)}, REG_LOCAL+1},
			{Z_OR, {VALUE(REG_LOCAL+1), VALUE(REG_C000)}, REG_LOCAL+1},
			{Z_STOREW, {VALUE(REG_PAIR), SMALL(3), VALUE(REG_LOCAL+1)}},
			{Z_STOREW, {VALUE(REG_PAIR), SMALL(4), VALUE(REG_LOCAL+0)}},
			{Z_ADD, {VALUE(REG_LOCAL+1), SMALL(2)}, REG_LOCAL+1},
			{Z_STOREW, {VALUE(REG_PAIR), SMALL(5), VALUE(REG_LOCAL+1)}},
			{Z_ADD, {VALUE(REG_LOCAL+1), SMALL(2)}, REG_LOCAL+1},
			{Z_RET, {VALUE(REG_LOCAL+1)}},
			{Z_END},
		}
	},
	{
		R_GET_LIST_V,
		4,
			// 0 (param): incoming argument to unify with list of one element
			// 1 (param): first element
			// 2: pointer to variable cell
			// 3: temp
		(struct zinstr []) {
			{Z_JGE, {VALUE(REG_LOCAL+0), VALUE(REG_C000)}, 0, 3},
			{OP_LABEL(1)},
			{Z_ADD, {VALUE(REG_LOCAL+0), VALUE(REG_LOCAL+0)}, REG_LOCAL+2},
			{Z_LOADW, {VALUE(REG_LOCAL+2), SMALL(0)}, REG_LOCAL+3},
			{Z_JZ, {VALUE(REG_LOCAL+3)}, 0, 2},
			{Z_STORE, {SMALL(REG_LOCAL+0), VALUE(REG_LOCAL+3)}},
			{Z_JL, {VALUE(REG_LOCAL+0), VALUE(REG_C000)}, 0, 1},

			{OP_LABEL(3)},
			// incoming argument was bound. make sure it is a pair.
			{Z_JGE, {VALUE(REG_LOCAL+0), VALUE(REG_E000)}, 0, 5},

			{Z_SUB, {VALUE(REG_LOCAL+0), VALUE(REG_3FFF)}, REG_LOCAL+3}, // pair ref -> second cell var ref
			{Z_CALLVN, {VALUE(REG_R_USIMPLE), VALUE(REG_NIL), VALUE(REG_LOCAL+3)}},
			{Z_SUB, {VALUE(REG_LOCAL+0), VALUE(REG_4000)}, REG_LOCAL+3}, // pair ref -> first cell var ref
			{Z_CALLVN, {ROUTINE(R_UNIFY), VALUE(REG_LOCAL+3), VALUE(REG_LOCAL+1)}},
			{Z_RFALSE},

			{OP_LABEL(5)},
			{Z_THROW, {SMALL(0), VALUE(REG_FAILJMP)}},

			{OP_LABEL(4)},
			{Z_THROW, {SMALL(FATAL_AUX), VALUE(REG_FATALJMP)}},
			{Z_RFALSE},

			{OP_LABEL(2)},
			// incoming argument was unbound, so we create a new pair
			{Z_CALLVS, {ROUTINE(R_PUSH_PAIR_VV), VALUE(REG_LOCAL+1), VALUE(REG_NIL)}, REG_LOCAL+3},
			{Z_STOREW, {VALUE(REG_LOCAL+2), SMALL(0), VALUE(REG_LOCAL+3)}},

			{Z_LOADW, {VALUE(REG_CHOICE), SMALL(CHOICE_TOP)}, REG_TEMP},
			{Z_JGE, {VALUE(REG_LOCAL+2), VALUE(REG_TEMP)}, 0, RFALSE},
			{Z_DEC_JL, {SMALL(REG_TRAIL), VALUE(REG_COLL)}, 0, 4},
			{Z_STOREW, {VALUE(REG_AUXBASE), VALUE(REG_TRAIL), VALUE(REG_LOCAL+2)}},
			{Z_RFALSE},
			{Z_END},
		}
	},
	{
		R_GET_LIST_R,
		4,
			// 0 (param): incoming argument to unify with pair
			// 1 (param): ref to register to receive head value
			// 2: pointer to variable cell
			// 3: temp
		(struct zinstr []) {
			{Z_JGE, {VALUE(REG_LOCAL+0), VALUE(REG_C000)}, 0, 3},
			{OP_LABEL(1)},
			{Z_ADD, {VALUE(REG_LOCAL+0), VALUE(REG_LOCAL+0)}, REG_LOCAL+2},
			{Z_LOADW, {VALUE(REG_LOCAL+2), SMALL(0)}, REG_LOCAL+3},
			{Z_JZ, {VALUE(REG_LOCAL+3)}, 0, 2},
			{Z_STORE, {SMALL(REG_LOCAL+0), VALUE(REG_LOCAL+3)}},
			{Z_JL, {VALUE(REG_LOCAL+0), VALUE(REG_C000)}, 0, 1},

			{OP_LABEL(3)},
			// incoming argument was bound. make sure it is a pair.
			{Z_JGE, {VALUE(REG_LOCAL+0), VALUE(REG_E000)}, 0, 5},

			{Z_SUB, {VALUE(REG_LOCAL+0), VALUE(REG_3FFF)}, REG_LOCAL+3}, // pair ref -> second cell var ref
			{Z_CALLVN, {VALUE(REG_R_USIMPLE), VALUE(REG_NIL), VALUE(REG_LOCAL+3)}},
			{Z_SUB, {VALUE(REG_LOCAL+0), VALUE(REG_4000)}, REG_LOCAL+3}, // pair ref -> first cell var ref
			{Z_STORE, {VALUE(REG_LOCAL+1), VALUE(REG_LOCAL+3)}},
			{Z_RFALSE},

			{OP_LABEL(5)},
			{Z_THROW, {SMALL(0), VALUE(REG_FAILJMP)}},

			{OP_LABEL(4)},
			{Z_THROW, {SMALL(FATAL_AUX), VALUE(REG_FATALJMP)}},

			{OP_LABEL(2)},
			// incoming argument was unbound, so we create a new pair
			{Z_CALLVS, {ROUTINE(R_PUSH_PAIR_RV), VALUE(REG_LOCAL+1), VALUE(REG_NIL)}, REG_LOCAL+3},
			{Z_STOREW, {VALUE(REG_LOCAL+2), SMALL(0), VALUE(REG_LOCAL+3)}},

			{Z_LOADW, {VALUE(REG_CHOICE), SMALL(CHOICE_TOP)}, REG_TEMP},
			{Z_JGE, {VALUE(REG_LOCAL+2), VALUE(REG_TEMP)}, 0, RFALSE},
			{Z_DEC_JL, {SMALL(REG_TRAIL), VALUE(REG_COLL)}, 0, 4},
			{Z_STOREW, {VALUE(REG_AUXBASE), VALUE(REG_TRAIL), VALUE(REG_LOCAL+2)}},
			{Z_RFALSE},
			{Z_END},
		}
	},
	{
		R_GET_PAIR_VV,
		5,
			// 0 (param): incoming argument to unify with pair
			// 1 (param): head value to unify with
			// 2 (param): tail value to unify with
			// 3: temp
			// 4: pointer to variable cell
		(struct zinstr []) {
			{Z_JGE, {VALUE(REG_LOCAL+0), VALUE(REG_C000)}, 0, 3},
			{OP_LABEL(1)},
			{Z_ADD, {VALUE(REG_LOCAL+0), VALUE(REG_LOCAL+0)}, REG_LOCAL+4},
			{Z_LOADW, {VALUE(REG_LOCAL+4), SMALL(0)}, REG_LOCAL+3},
			{Z_JZ, {VALUE(REG_LOCAL+3)}, 0, 2},
			{Z_STORE, {SMALL(REG_LOCAL+0), VALUE(REG_LOCAL+3)}},
			{Z_JL, {VALUE(REG_LOCAL+0), VALUE(REG_C000)}, 0, 1},

			{OP_LABEL(3)},
			// incoming argument was bound. make sure it is a pair.
			{Z_JGE, {VALUE(REG_LOCAL+0), VALUE(REG_E000)}, 0, 5},

			{Z_SUB, {VALUE(REG_LOCAL+0), VALUE(REG_4000)}, REG_LOCAL+3}, // pair ref -> first cell var ref
			{Z_CALLVN, {ROUTINE(R_UNIFY), VALUE(REG_LOCAL+3), VALUE(REG_LOCAL+1)}},
			{Z_SUB, {VALUE(REG_LOCAL+0), VALUE(REG_3FFF)}, REG_LOCAL+3}, // pair ref -> second cell var ref
			{Z_CALLVN, {ROUTINE(R_UNIFY), VALUE(REG_LOCAL+3), VALUE(REG_LOCAL+2)}},
			{Z_RFALSE},

			{OP_LABEL(5)},
			{Z_THROW, {SMALL(0), VALUE(REG_FAILJMP)}},

			{OP_LABEL(4)},
			{Z_THROW, {SMALL(FATAL_AUX), VALUE(REG_FATALJMP)}},

			{OP_LABEL(2)},
			// incoming argument was unbound, so we create a new pair
			{Z_CALLVS, {ROUTINE(R_PUSH_PAIR_VV), VALUE(REG_LOCAL+1), VALUE(REG_LOCAL+2)}, REG_LOCAL+3},
			{Z_STOREW, {VALUE(REG_LOCAL+4), SMALL(0), VALUE(REG_LOCAL+3)}},

			{Z_LOADW, {VALUE(REG_CHOICE), SMALL(CHOICE_TOP)}, REG_TEMP},
			{Z_JGE, {VALUE(REG_LOCAL+4), VALUE(REG_TEMP)}, 0, RFALSE},
			{Z_DEC_JL, {SMALL(REG_TRAIL), VALUE(REG_COLL)}, 0, 4},
			{Z_STOREW, {VALUE(REG_AUXBASE), VALUE(REG_TRAIL), VALUE(REG_LOCAL+4)}},
			{Z_RFALSE},
			{Z_END},
		}
	},
	{
		R_GET_PAIR_VR,
		5,
			// 0 (param): incoming argument to unify with pair
			// 1 (param): head value to unify with
			// 2 (param): ref to register to receive tail value
			// 3: temp
			// 4: pointer to variable cell
		(struct zinstr []) {
			{Z_JGE, {VALUE(REG_LOCAL+0), VALUE(REG_C000)}, 0, 3},
			{OP_LABEL(1)},
			{Z_ADD, {VALUE(REG_LOCAL+0), VALUE(REG_LOCAL+0)}, REG_LOCAL+4},
			{Z_LOADW, {VALUE(REG_LOCAL+4), SMALL(0)}, REG_LOCAL+3},
			{Z_JZ, {VALUE(REG_LOCAL+3)}, 0, 2},
			{Z_STORE, {SMALL(REG_LOCAL+0), VALUE(REG_LOCAL+3)}},
			{Z_JL, {VALUE(REG_LOCAL+0), VALUE(REG_C000)}, 0, 1},

			{OP_LABEL(3)},
			// incoming argument was bound. make sure it is a pair.
			{Z_JGE, {VALUE(REG_LOCAL+0), VALUE(REG_E000)}, 0, 5},

			{Z_SUB, {VALUE(REG_LOCAL+0), VALUE(REG_4000)}, REG_LOCAL+3}, // pair ref -> first cell var ref
			{Z_CALLVN, {ROUTINE(R_UNIFY), VALUE(REG_LOCAL+3), VALUE(REG_LOCAL+1)}},
			{Z_SUB, {VALUE(REG_LOCAL+0), VALUE(REG_3FFF)}, REG_LOCAL+3}, // pair ref -> second cell var ref
			{Z_STORE, {VALUE(REG_LOCAL+2), VALUE(REG_LOCAL+3)}},
			{Z_RFALSE},

			{OP_LABEL(5)},
			{Z_THROW, {SMALL(0), VALUE(REG_FAILJMP)}},

			{OP_LABEL(4)},
			{Z_THROW, {SMALL(FATAL_AUX), VALUE(REG_FATALJMP)}},

			{OP_LABEL(2)},
			// incoming argument was unbound, so we create a new pair
			{Z_CALLVS, {ROUTINE(R_PUSH_PAIR_VR), VALUE(REG_LOCAL+1), VALUE(REG_LOCAL+2)}, REG_LOCAL+3},
			{Z_STOREW, {VALUE(REG_LOCAL+4), SMALL(0), VALUE(REG_LOCAL+3)}},

			{Z_LOADW, {VALUE(REG_CHOICE), SMALL(CHOICE_TOP)}, REG_TEMP},
			{Z_JGE, {VALUE(REG_LOCAL+4), VALUE(REG_TEMP)}, 0, RFALSE},
			{Z_DEC_JL, {SMALL(REG_TRAIL), VALUE(REG_COLL)}, 0, 4},
			{Z_STOREW, {VALUE(REG_AUXBASE), VALUE(REG_TRAIL), VALUE(REG_LOCAL+4)}},
			{Z_RFALSE},
			{Z_END},
		}
	},
	{
		R_GET_PAIR_RV,
		5,
			// 0 (param): incoming argument to unify with pair
			// 1 (param): ref to register to receive head value
			// 2 (param): tail value to unify with
			// 3: temp
			// 4: pointer to variable cell
		(struct zinstr []) {
			{Z_JGE, {VALUE(REG_LOCAL+0), VALUE(REG_C000)}, 0, 3},
			{OP_LABEL(1)},
			{Z_ADD, {VALUE(REG_LOCAL+0), VALUE(REG_LOCAL+0)}, REG_LOCAL+4},
			{Z_LOADW, {VALUE(REG_LOCAL+4), SMALL(0)}, REG_LOCAL+3},
			{Z_JZ, {VALUE(REG_LOCAL+3)}, 0, 2},
			{Z_STORE, {SMALL(REG_LOCAL+0), VALUE(REG_LOCAL+3)}},
			{Z_JL, {VALUE(REG_LOCAL+0), VALUE(REG_C000)}, 0, 1},

			{OP_LABEL(3)},
			// incoming argument was bound. make sure it is a pair.
			{Z_JGE, {VALUE(REG_LOCAL+0), VALUE(REG_E000)}, 0, 5},

			{Z_SUB, {VALUE(REG_LOCAL+0), VALUE(REG_3FFF)}, REG_LOCAL+3}, // pair ref -> second cell var ref
			{Z_CALLVN, {ROUTINE(R_UNIFY), VALUE(REG_LOCAL+3), VALUE(REG_LOCAL+2)}},
			{Z_SUB, {VALUE(REG_LOCAL+0), VALUE(REG_4000)}, REG_LOCAL+3}, // pair ref -> first cell var ref
			{Z_STORE, {VALUE(REG_LOCAL+1), VALUE(REG_LOCAL+3)}},
			{Z_RFALSE},

			{OP_LABEL(5)},
			{Z_THROW, {SMALL(0), VALUE(REG_FAILJMP)}},

			{OP_LABEL(4)},
			{Z_THROW, {SMALL(FATAL_AUX), VALUE(REG_FATALJMP)}},

			{OP_LABEL(2)},
			// incoming argument was unbound, so we create a new pair
			{Z_CALLVS, {ROUTINE(R_PUSH_PAIR_RV), VALUE(REG_LOCAL+1), VALUE(REG_LOCAL+2)}, REG_LOCAL+3},
			{Z_STOREW, {VALUE(REG_LOCAL+4), SMALL(0), VALUE(REG_LOCAL+3)}},

			{Z_LOADW, {VALUE(REG_CHOICE), SMALL(CHOICE_TOP)}, REG_TEMP},
			{Z_JGE, {VALUE(REG_LOCAL+4), VALUE(REG_TEMP)}, 0, RFALSE},
			{Z_DEC_JL, {SMALL(REG_TRAIL), VALUE(REG_COLL)}, 0, 4},
			{Z_STOREW, {VALUE(REG_AUXBASE), VALUE(REG_TRAIL), VALUE(REG_LOCAL+4)}},
			{Z_RFALSE},
			{Z_END},
		}
	},
	{
		R_GET_PAIR_RR,
		5,
			// 0 (param): incoming argument to unify with pair
			// 1 (param): ref to register to receive head value
			// 2 (param): ref to register to receive tail value
			// 3: temp
			// 4: pointer to variable cell
		(struct zinstr []) {
			{Z_JGE, {VALUE(REG_LOCAL+0), VALUE(REG_C000)}, 0, 3},
			{OP_LABEL(1)},
			{Z_ADD, {VALUE(REG_LOCAL+0), VALUE(REG_LOCAL+0)}, REG_LOCAL+4},
			{Z_LOADW, {VALUE(REG_LOCAL+4), SMALL(0)}, REG_LOCAL+3},
			{Z_JZ, {VALUE(REG_LOCAL+3)}, 0, 2},
			{Z_STORE, {SMALL(REG_LOCAL+0), VALUE(REG_LOCAL+3)}},
			{Z_JL, {VALUE(REG_LOCAL+0), VALUE(REG_C000)}, 0, 1},

			{OP_LABEL(3)},
			// incoming argument was bound. make sure it is a pair.
			{Z_JGE, {VALUE(REG_LOCAL+0), VALUE(REG_E000)}, 0, 5},

			{Z_SUB, {VALUE(REG_LOCAL+0), VALUE(REG_4000)}, REG_LOCAL+3},	// pair ref -> first cell var ref
			{Z_STORE, {VALUE(REG_LOCAL+1), VALUE(REG_LOCAL+3)}},
			{Z_ADD, {VALUE(REG_LOCAL+3), SMALL(1)}, REG_LOCAL+3},		// second cell var ref
			{Z_STORE, {VALUE(REG_LOCAL+2), VALUE(REG_LOCAL+3)}},
			{Z_RFALSE},

			{OP_LABEL(5)},
			{Z_THROW, {SMALL(0), VALUE(REG_FAILJMP)}},

			{OP_LABEL(4)},
			{Z_THROW, {SMALL(FATAL_AUX), VALUE(REG_FATALJMP)}},

			{OP_LABEL(2)},
			// incoming argument was unbound, so we create a new pair
			{Z_CALLVS, {ROUTINE(R_PUSH_PAIR_RR), VALUE(REG_LOCAL+1), VALUE(REG_LOCAL+2)}, REG_LOCAL+3},
			{Z_STOREW, {VALUE(REG_LOCAL+4), SMALL(0), VALUE(REG_LOCAL+3)}},

			{Z_LOADW, {VALUE(REG_CHOICE), SMALL(CHOICE_TOP)}, REG_TEMP},
			{Z_JGE, {VALUE(REG_LOCAL+4), VALUE(REG_TEMP)}, 0, RFALSE},
			{Z_DEC_JL, {SMALL(REG_TRAIL), VALUE(REG_COLL)}, 0, 4},
			{Z_STOREW, {VALUE(REG_AUXBASE), VALUE(REG_TRAIL), VALUE(REG_LOCAL+4)}},
			{Z_RFALSE},
			{Z_END},
		}
	},
	{
		R_ALLOCATE,
		1,
			// 0 (param): number of bytes needed for new environment frame
		(struct zinstr []) {
			{Z_JL, {VALUE(REG_ENV), VALUE(REG_CHOICE)}, 0, 1},

			{Z_SUB, {VALUE(REG_CHOICE), VALUE(REG_LOCAL+0)}, REG_TEMP},
			{Z_JGE, {VALUE(REG_TEMP), VALUE(REG_TOP)}, 0, 3},
			{Z_THROW, {SMALL(FATAL_HEAP), VALUE(REG_FATALJMP)}},

			{OP_LABEL(1)},
			{Z_SUB, {VALUE(REG_ENV), VALUE(REG_LOCAL+0)}, REG_TEMP},
			{Z_JGE, {VALUE(REG_TEMP), VALUE(REG_TOP)}, 0, 3},
			{Z_THROW, {SMALL(FATAL_HEAP), VALUE(REG_FATALJMP)}},

			{OP_LABEL(3)},
			{Z_STOREW, {VALUE(REG_TEMP), SMALL(ENV_ENV), VALUE(REG_ENV)}},
			{Z_STOREW, {VALUE(REG_TEMP), SMALL(ENV_CONT), VALUE(REG_CONT)}},
			{Z_STORE, {SMALL(REG_ENV), VALUE(REG_TEMP)}},
			{Z_RFALSE},
			{Z_END},
		}
	},
	{
		R_ALLOCATE_S,
		1,
			// 0 (param): number of bytes needed for new environment frame
		(struct zinstr []) {
			{Z_JL, {VALUE(REG_ENV), VALUE(REG_CHOICE)}, 0, 1},

			{Z_SUB, {VALUE(REG_CHOICE), VALUE(REG_LOCAL+0)}, REG_TEMP},
			{Z_JGE, {VALUE(REG_TEMP), VALUE(REG_TOP)}, 0, 3},
			{Z_THROW, {SMALL(FATAL_HEAP), VALUE(REG_FATALJMP)}},

			{OP_LABEL(1)},
			{Z_SUB, {VALUE(REG_ENV), VALUE(REG_LOCAL+0)}, REG_TEMP},
			{Z_JGE, {VALUE(REG_TEMP), VALUE(REG_TOP)}, 0, 3},
			{Z_THROW, {SMALL(FATAL_HEAP), VALUE(REG_FATALJMP)}},

			{OP_LABEL(3)},
			{Z_STOREW, {VALUE(REG_TEMP), SMALL(ENV_ENV), VALUE(REG_ENV)}},
			{Z_STOREW, {VALUE(REG_TEMP), SMALL(ENV_CONT), VALUE(REG_CONT)}},
			{Z_STOREW, {VALUE(REG_TEMP), SMALL(2), VALUE(REG_SIMPLE)}},
			{Z_STORE, {SMALL(REG_ENV), VALUE(REG_TEMP)}},
			{Z_RFALSE},
			{Z_END},
		}
	},
	{
		R_DEALLOCATE,
		0,
		(struct zinstr []) {
			{Z_LOADW, {VALUE(REG_ENV), SMALL(ENV_CONT)}, REG_CONT},
			{Z_LOADW, {VALUE(REG_ENV), SMALL(ENV_ENV)}, REG_ENV},
			{Z_RFALSE},
			{Z_END},
		}
	},
	{
		R_DEALLOCATE_S,
		0,
		(struct zinstr []) {
			{Z_LOADW, {VALUE(REG_ENV), SMALL(2)}, REG_SIMPLE},
			{Z_LOADW, {VALUE(REG_ENV), SMALL(ENV_CONT)}, REG_CONT},
			{Z_LOADW, {VALUE(REG_ENV), SMALL(ENV_ENV)}, REG_ENV},
			{Z_RFALSE},
			{Z_END},
		}
	},
	{
		R_DEALLOCATE_CS,
		0,
		(struct zinstr []) {
			{Z_LOADW, {VALUE(REG_ENV), SMALL(3)}, REG_SIMPLE},
			{Z_LOADW, {VALUE(REG_ENV), SMALL(ENV_CONT)}, REG_CONT},
			{Z_LOADW, {VALUE(REG_ENV), SMALL(ENV_ENV)}, REG_ENV},
			{Z_RFALSE},
			{Z_END},
		}
	},
	{
		R_TRY_ME_ELSE,
		2,
			// 0 (param): size of area to allocate (number of argument registers + CHOICE_SIZEOF) * 2
			// 1 (param): routine pointer for next choice
		(struct zinstr []) {
			{Z_JL, {VALUE(REG_ENV), VALUE(REG_CHOICE)}, 0, 1},
			{Z_SUB, {VALUE(REG_CHOICE), VALUE(REG_LOCAL+0)}, REG_TEMP},
			{Z_JGE, {VALUE(REG_TEMP), VALUE(REG_TOP)}, 0, 2},

			{OP_LABEL(3)},
			{Z_THROW, {SMALL(FATAL_HEAP), VALUE(REG_FATALJMP)}},

			{OP_LABEL(1)},
			{Z_SUB, {VALUE(REG_ENV), VALUE(REG_LOCAL+0)}, REG_TEMP},
			{Z_JL, {VALUE(REG_TEMP), VALUE(REG_TOP)}, 0, 3},

			{OP_LABEL(2)},

			{Z_STOREW, {VALUE(REG_TEMP), SMALL(CHOICE_CHOICE), VALUE(REG_CHOICE)}},
			{Z_STOREW, {VALUE(REG_TEMP), SMALL(CHOICE_NEXTPC), VALUE(REG_LOCAL+1)}},
			{Z_STOREW, {VALUE(REG_TEMP), SMALL(CHOICE_ENV), VALUE(REG_ENV)}},
			{Z_STOREW, {VALUE(REG_TEMP), SMALL(CHOICE_CONT), VALUE(REG_CONT)}},
			{Z_STOREW, {VALUE(REG_TEMP), SMALL(CHOICE_TRAIL), VALUE(REG_TRAIL)}},
			{Z_STOREW, {VALUE(REG_TEMP), SMALL(CHOICE_TOP), VALUE(REG_TOP)}},
			{Z_STOREW, {VALUE(REG_TEMP), SMALL(CHOICE_SIMPLE), VALUE(REG_SIMPLE)}},
			{Z_STORE, {SMALL(REG_CHOICE), VALUE(REG_TEMP)}},

			{Z_JE, {VALUE(REG_LOCAL+0), SMALL((0 + CHOICE_SIZEOF) * 2)}, 0, RFALSE},
			{Z_JE, {VALUE(REG_LOCAL+0), SMALL((1 + CHOICE_SIZEOF) * 2)}, 0, 11},
			{Z_JE, {VALUE(REG_LOCAL+0), SMALL((2 + CHOICE_SIZEOF) * 2)}, 0, 12},
			{Z_JE, {VALUE(REG_LOCAL+0), SMALL((3 + CHOICE_SIZEOF) * 2)}, 0, 13},
			{Z_JE, {VALUE(REG_LOCAL+0), SMALL((4 + CHOICE_SIZEOF) * 2)}, 0, 14},
			{Z_JE, {VALUE(REG_LOCAL+0), SMALL((5 + CHOICE_SIZEOF) * 2)}, 0, 15},
			{Z_JE, {VALUE(REG_LOCAL+0), SMALL((6 + CHOICE_SIZEOF) * 2)}, 0, 16},
			{Z_JE, {VALUE(REG_LOCAL+0), SMALL((7 + CHOICE_SIZEOF) * 2)}, 0, 17},
			{Z_JE, {VALUE(REG_LOCAL+0), SMALL((8 + CHOICE_SIZEOF) * 2)}, 0, 18},
			{Z_JE, {VALUE(REG_LOCAL+0), SMALL((9 + CHOICE_SIZEOF) * 2)}, 0, 19},
			{Z_JE, {VALUE(REG_LOCAL+0), SMALL((10 + CHOICE_SIZEOF) * 2)}, 0, 20},
			{Z_JE, {VALUE(REG_LOCAL+0), SMALL((11 + CHOICE_SIZEOF) * 2)}, 0, 21},
			{Z_JE, {VALUE(REG_LOCAL+0), SMALL((12 + CHOICE_SIZEOF) * 2)}, 0, 22},
			{Z_STOREW, {VALUE(REG_CHOICE), SMALL(CHOICE_SIZEOF + 12), VALUE(REG_A+12)}},
			{OP_LABEL(22)},
			{Z_STOREW, {VALUE(REG_CHOICE), SMALL(CHOICE_SIZEOF + 11), VALUE(REG_A+11)}},
			{OP_LABEL(21)},
			{Z_STOREW, {VALUE(REG_CHOICE), SMALL(CHOICE_SIZEOF + 10), VALUE(REG_A+10)}},
			{OP_LABEL(20)},
			{Z_STOREW, {VALUE(REG_CHOICE), SMALL(CHOICE_SIZEOF + 9), VALUE(REG_A+9)}},
			{OP_LABEL(19)},
			{Z_STOREW, {VALUE(REG_CHOICE), SMALL(CHOICE_SIZEOF + 8), VALUE(REG_A+8)}},
			{OP_LABEL(18)},
			{Z_STOREW, {VALUE(REG_CHOICE), SMALL(CHOICE_SIZEOF + 7), VALUE(REG_A+7)}},
			{OP_LABEL(17)},
			{Z_STOREW, {VALUE(REG_CHOICE), SMALL(CHOICE_SIZEOF + 6), VALUE(REG_A+6)}},
			{OP_LABEL(16)},
			{Z_STOREW, {VALUE(REG_CHOICE), SMALL(CHOICE_SIZEOF + 5), VALUE(REG_A+5)}},
			{OP_LABEL(15)},
			{Z_STOREW, {VALUE(REG_CHOICE), SMALL(CHOICE_SIZEOF + 4), VALUE(REG_A+4)}},
			{OP_LABEL(14)},
			{Z_STOREW, {VALUE(REG_CHOICE), SMALL(CHOICE_SIZEOF + 3), VALUE(REG_A+3)}},
			{OP_LABEL(13)},
			{Z_STOREW, {VALUE(REG_CHOICE), SMALL(CHOICE_SIZEOF + 2), VALUE(REG_A+2)}},
			{OP_LABEL(12)},
			{Z_STOREW, {VALUE(REG_CHOICE), SMALL(CHOICE_SIZEOF + 1), VALUE(REG_A+1)}},
			{OP_LABEL(11)},
			{Z_STOREW, {VALUE(REG_CHOICE), SMALL(CHOICE_SIZEOF + 0), VALUE(REG_A+0)}},
			{Z_RFALSE},
			{Z_END},
		}
	},
	{
		R_RETRY_ME_ELSE,
		2,
			// 0 (param): number of argument registers to restore (and temp)
			// 1 (param): routine pointer for next choice (and temp)
		(struct zinstr []) {
			{Z_JZ, {VALUE(REG_LOCAL+0)}, 0, 10},
			{Z_JE, {VALUE(REG_LOCAL+0), SMALL(1)}, 0, 11},
			{Z_JE, {VALUE(REG_LOCAL+0), SMALL(2)}, 0, 12},
			{Z_JE, {VALUE(REG_LOCAL+0), SMALL(3)}, 0, 13},
			{Z_JE, {VALUE(REG_LOCAL+0), SMALL(4)}, 0, 14},
			{Z_JE, {VALUE(REG_LOCAL+0), SMALL(5)}, 0, 15},
			{Z_JE, {VALUE(REG_LOCAL+0), SMALL(6)}, 0, 16},
			{Z_JE, {VALUE(REG_LOCAL+0), SMALL(7)}, 0, 17},
			{Z_JE, {VALUE(REG_LOCAL+0), SMALL(8)}, 0, 18},
			{Z_JE, {VALUE(REG_LOCAL+0), SMALL(9)}, 0, 19},
			{Z_JE, {VALUE(REG_LOCAL+0), SMALL(10)}, 0, 20},
			{Z_JE, {VALUE(REG_LOCAL+0), SMALL(11)}, 0, 21},
			{Z_JE, {VALUE(REG_LOCAL+0), SMALL(12)}, 0, 22},

			{Z_LOADW, {VALUE(REG_CHOICE), SMALL(CHOICE_SIZEOF + 12)}, REG_A+12},
			{OP_LABEL(22)},
			{Z_LOADW, {VALUE(REG_CHOICE), SMALL(CHOICE_SIZEOF + 11)}, REG_A+11},
			{OP_LABEL(21)},
			{Z_LOADW, {VALUE(REG_CHOICE), SMALL(CHOICE_SIZEOF + 10)}, REG_A+10},
			{OP_LABEL(20)},
			{Z_LOADW, {VALUE(REG_CHOICE), SMALL(CHOICE_SIZEOF + 9)}, REG_A+9},
			{OP_LABEL(19)},
			{Z_LOADW, {VALUE(REG_CHOICE), SMALL(CHOICE_SIZEOF + 8)}, REG_A+8},
			{OP_LABEL(18)},
			{Z_LOADW, {VALUE(REG_CHOICE), SMALL(CHOICE_SIZEOF + 7)}, REG_A+7},
			{OP_LABEL(17)},
			{Z_LOADW, {VALUE(REG_CHOICE), SMALL(CHOICE_SIZEOF + 6)}, REG_A+6},
			{OP_LABEL(16)},
			{Z_LOADW, {VALUE(REG_CHOICE), SMALL(CHOICE_SIZEOF + 5)}, REG_A+5},
			{OP_LABEL(15)},
			{Z_LOADW, {VALUE(REG_CHOICE), SMALL(CHOICE_SIZEOF + 4)}, REG_A+4},
			{OP_LABEL(14)},
			{Z_LOADW, {VALUE(REG_CHOICE), SMALL(CHOICE_SIZEOF + 3)}, REG_A+3},
			{OP_LABEL(13)},
			{Z_LOADW, {VALUE(REG_CHOICE), SMALL(CHOICE_SIZEOF + 2)}, REG_A+2},
			{OP_LABEL(12)},
			{Z_LOADW, {VALUE(REG_CHOICE), SMALL(CHOICE_SIZEOF + 1)}, REG_A+1},
			{OP_LABEL(11)},
			{Z_LOADW, {VALUE(REG_CHOICE), SMALL(CHOICE_SIZEOF + 0)}, REG_A+0},
			{OP_LABEL(10)},

			{Z_STOREW, {VALUE(REG_CHOICE), SMALL(CHOICE_NEXTPC), VALUE(REG_LOCAL+1)}},
			{Z_LOADW, {VALUE(REG_CHOICE), SMALL(CHOICE_ENV)}, REG_ENV},
			{Z_LOADW, {VALUE(REG_CHOICE), SMALL(CHOICE_CONT)}, REG_CONT},
			{Z_LOADW, {VALUE(REG_CHOICE), SMALL(CHOICE_TOP)}, REG_TOP},
			{Z_LOADW, {VALUE(REG_CHOICE), SMALL(CHOICE_SIMPLE)}, REG_SIMPLE},

			{Z_LOADW, {VALUE(REG_CHOICE), SMALL(CHOICE_TRAIL)}, REG_LOCAL+1},
			{Z_DEC_JL, {SMALL(REG_LOCAL+1), VALUE(REG_TRAIL)}, 0, RFALSE},

			{OP_LABEL(3)},
			{Z_LOADW, {VALUE(REG_AUXBASE), VALUE(REG_TRAIL)}, REG_LOCAL+0},
			{Z_STOREW, {VALUE(REG_LOCAL+0), SMALL(0), SMALL(0)}},
			{Z_INC_JLE, {SMALL(REG_TRAIL), VALUE(REG_LOCAL+1)}, 0, 3},

			{Z_RFALSE},
			{Z_END},
		}
	},
	{
		R_TRUST_ME,
		2,
			// 0 (param): number of argument registers to restore (and temp)
			// 1: temp
		(struct zinstr []) {
			{Z_JZ, {VALUE(REG_LOCAL+0)}, 0, 10},
			{Z_JE, {VALUE(REG_LOCAL+0), SMALL(1)}, 0, 11},
			{Z_JE, {VALUE(REG_LOCAL+0), SMALL(2)}, 0, 12},
			{Z_JE, {VALUE(REG_LOCAL+0), SMALL(3)}, 0, 13},
			{Z_JE, {VALUE(REG_LOCAL+0), SMALL(4)}, 0, 14},
			{Z_JE, {VALUE(REG_LOCAL+0), SMALL(5)}, 0, 15},
			{Z_JE, {VALUE(REG_LOCAL+0), SMALL(6)}, 0, 16},
			{Z_JE, {VALUE(REG_LOCAL+0), SMALL(7)}, 0, 17},
			{Z_JE, {VALUE(REG_LOCAL+0), SMALL(8)}, 0, 18},
			{Z_JE, {VALUE(REG_LOCAL+0), SMALL(9)}, 0, 19},
			{Z_JE, {VALUE(REG_LOCAL+0), SMALL(10)}, 0, 20},
			{Z_JE, {VALUE(REG_LOCAL+0), SMALL(11)}, 0, 21},
			{Z_JE, {VALUE(REG_LOCAL+0), SMALL(12)}, 0, 22},

			{Z_LOADW, {VALUE(REG_CHOICE), SMALL(CHOICE_SIZEOF + 12)}, REG_A+12},
			{OP_LABEL(22)},
			{Z_LOADW, {VALUE(REG_CHOICE), SMALL(CHOICE_SIZEOF + 11)}, REG_A+11},
			{OP_LABEL(21)},
			{Z_LOADW, {VALUE(REG_CHOICE), SMALL(CHOICE_SIZEOF + 10)}, REG_A+10},
			{OP_LABEL(20)},
			{Z_LOADW, {VALUE(REG_CHOICE), SMALL(CHOICE_SIZEOF + 9)}, REG_A+9},
			{OP_LABEL(19)},
			{Z_LOADW, {VALUE(REG_CHOICE), SMALL(CHOICE_SIZEOF + 8)}, REG_A+8},
			{OP_LABEL(18)},
			{Z_LOADW, {VALUE(REG_CHOICE), SMALL(CHOICE_SIZEOF + 7)}, REG_A+7},
			{OP_LABEL(17)},
			{Z_LOADW, {VALUE(REG_CHOICE), SMALL(CHOICE_SIZEOF + 6)}, REG_A+6},
			{OP_LABEL(16)},
			{Z_LOADW, {VALUE(REG_CHOICE), SMALL(CHOICE_SIZEOF + 5)}, REG_A+5},
			{OP_LABEL(15)},
			{Z_LOADW, {VALUE(REG_CHOICE), SMALL(CHOICE_SIZEOF + 4)}, REG_A+4},
			{OP_LABEL(14)},
			{Z_LOADW, {VALUE(REG_CHOICE), SMALL(CHOICE_SIZEOF + 3)}, REG_A+3},
			{OP_LABEL(13)},
			{Z_LOADW, {VALUE(REG_CHOICE), SMALL(CHOICE_SIZEOF + 2)}, REG_A+2},
			{OP_LABEL(12)},
			{Z_LOADW, {VALUE(REG_CHOICE), SMALL(CHOICE_SIZEOF + 1)}, REG_A+1},
			{OP_LABEL(11)},
			{Z_LOADW, {VALUE(REG_CHOICE), SMALL(CHOICE_SIZEOF + 0)}, REG_A+0},
			{OP_LABEL(10)},

			{Z_LOADW, {VALUE(REG_CHOICE), SMALL(CHOICE_ENV)}, REG_ENV},
			{Z_LOADW, {VALUE(REG_CHOICE), SMALL(CHOICE_CONT)}, REG_CONT},
			{Z_LOADW, {VALUE(REG_CHOICE), SMALL(CHOICE_TOP)}, REG_TOP},
			{Z_LOADW, {VALUE(REG_CHOICE), SMALL(CHOICE_SIMPLE)}, REG_SIMPLE},

			{Z_LOADW, {VALUE(REG_CHOICE), SMALL(CHOICE_TRAIL)}, REG_LOCAL+1},
			{Z_DEC_JL, {SMALL(REG_LOCAL+1), VALUE(REG_TRAIL)}, 0, 4},

			{OP_LABEL(3)},
			{Z_LOADW, {VALUE(REG_AUXBASE), VALUE(REG_TRAIL)}, REG_LOCAL+0},
			{Z_STOREW, {VALUE(REG_LOCAL+0), SMALL(0), SMALL(0)}},
			{Z_INC_JLE, {SMALL(REG_TRAIL), VALUE(REG_LOCAL+1)}, 0, 3},

			{OP_LABEL(4)},
			{Z_LOADW, {VALUE(REG_CHOICE), SMALL(CHOICE_CHOICE)}, REG_CHOICE},
			{Z_RFALSE},
			{Z_END},
		}
	},
	{
		R_TRY_ME_ELSE_0,
		1,
			// 0 (param): routine pointer for next choice
		(struct zinstr []) {
			{Z_JL, {VALUE(REG_ENV), VALUE(REG_CHOICE)}, 0, 1},
			{Z_SUB, {VALUE(REG_CHOICE), SMALL(2 * CHOICE_SIZEOF)}, REG_TEMP},
			{Z_JGE, {VALUE(REG_TEMP), VALUE(REG_TOP)}, 0, 2},

			{OP_LABEL(3)},
			{Z_THROW, {SMALL(FATAL_HEAP), VALUE(REG_FATALJMP)}},

			{OP_LABEL(1)},
			{Z_SUB, {VALUE(REG_ENV), SMALL(2 * CHOICE_SIZEOF)}, REG_TEMP},
			{Z_JL, {VALUE(REG_TEMP), VALUE(REG_TOP)}, 0, 3},

			{OP_LABEL(2)},

			{Z_STOREW, {VALUE(REG_TEMP), SMALL(CHOICE_CHOICE), VALUE(REG_CHOICE)}},
			{Z_STOREW, {VALUE(REG_TEMP), SMALL(CHOICE_NEXTPC), VALUE(REG_LOCAL+0)}},
			{Z_STOREW, {VALUE(REG_TEMP), SMALL(CHOICE_ENV), VALUE(REG_ENV)}},
			{Z_STOREW, {VALUE(REG_TEMP), SMALL(CHOICE_CONT), VALUE(REG_CONT)}},
			{Z_STOREW, {VALUE(REG_TEMP), SMALL(CHOICE_TRAIL), VALUE(REG_TRAIL)}},
			{Z_STOREW, {VALUE(REG_TEMP), SMALL(CHOICE_TOP), VALUE(REG_TOP)}},
			{Z_STOREW, {VALUE(REG_TEMP), SMALL(CHOICE_SIMPLE), VALUE(REG_SIMPLE)}},
			{Z_STORE, {SMALL(REG_CHOICE), VALUE(REG_TEMP)}},
			{Z_RFALSE},
			{Z_END},
		}
	},
	{
		R_RETRY_ME_ELSE_0,
		2,
			// 0 (param): routine pointer for next choice (and temp)
			// 1: temp
		(struct zinstr []) {
			{Z_STOREW, {VALUE(REG_CHOICE), SMALL(CHOICE_NEXTPC), VALUE(REG_LOCAL+0)}},
			{Z_LOADW, {VALUE(REG_CHOICE), SMALL(CHOICE_ENV)}, REG_ENV},
			{Z_LOADW, {VALUE(REG_CHOICE), SMALL(CHOICE_CONT)}, REG_CONT},
			{Z_LOADW, {VALUE(REG_CHOICE), SMALL(CHOICE_TOP)}, REG_TOP},
			{Z_LOADW, {VALUE(REG_CHOICE), SMALL(CHOICE_SIMPLE)}, REG_SIMPLE},

			{Z_LOADW, {VALUE(REG_CHOICE), SMALL(CHOICE_TRAIL)}, REG_LOCAL+1},
			{Z_DEC_JL, {SMALL(REG_LOCAL+1), VALUE(REG_TRAIL)}, 0, RFALSE},

			{OP_LABEL(3)},
			{Z_LOADW, {VALUE(REG_AUXBASE), VALUE(REG_TRAIL)}, REG_LOCAL+0},
			{Z_STOREW, {VALUE(REG_LOCAL+0), SMALL(0), SMALL(0)}},
			{Z_INC_JLE, {SMALL(REG_TRAIL), VALUE(REG_LOCAL+1)}, 0, 3},

			{Z_RFALSE},
			{Z_END},
		}
	},
	{
		R_TRUST_ME_0,
		2,
			// 0: temp
			// 1: temp
		(struct zinstr []) {
			{Z_LOADW, {VALUE(REG_CHOICE), SMALL(CHOICE_ENV)}, REG_ENV},
			{Z_LOADW, {VALUE(REG_CHOICE), SMALL(CHOICE_CONT)}, REG_CONT},
			{Z_LOADW, {VALUE(REG_CHOICE), SMALL(CHOICE_TOP)}, REG_TOP},
			{Z_LOADW, {VALUE(REG_CHOICE), SMALL(CHOICE_SIMPLE)}, REG_SIMPLE},

			{Z_LOADW, {VALUE(REG_CHOICE), SMALL(CHOICE_TRAIL)}, REG_LOCAL+1},
			{Z_DEC_JL, {SMALL(REG_LOCAL+1), VALUE(REG_TRAIL)}, 0, 4},

			{OP_LABEL(3)},
			{Z_LOADW, {VALUE(REG_AUXBASE), VALUE(REG_TRAIL)}, REG_LOCAL+0},
			{Z_STOREW, {VALUE(REG_LOCAL+0), SMALL(0), SMALL(0)}},
			{Z_INC_JLE, {SMALL(REG_TRAIL), VALUE(REG_LOCAL+1)}, 0, 3},

			{OP_LABEL(4)},
			{Z_LOADW, {VALUE(REG_CHOICE), SMALL(CHOICE_CHOICE)}, REG_CHOICE},
			{Z_RFALSE},
			{Z_END},
		}
	},
	{
		R_DEREF,
		2,
			// 0 (param): value to dereference
			// 1: temp
		(struct zinstr []) {
			{Z_JGE, {VALUE(REG_LOCAL+0), VALUE(REG_C000)}, 0, 2},
			{OP_LABEL(1)},
			{Z_ADD, {VALUE(REG_LOCAL+0), VALUE(REG_LOCAL+0)}, REG_LOCAL+1},
			{Z_LOADW, {VALUE(REG_LOCAL+1), SMALL(0)}, REG_LOCAL+1},
			{Z_JZ, {VALUE(REG_LOCAL+1)}, 0, 2},
			{Z_STORE, {SMALL(REG_LOCAL+0), VALUE(REG_LOCAL+1)}},
			{Z_JL, {VALUE(REG_LOCAL+0), VALUE(REG_C000)}, 0, 1},

			{OP_LABEL(2)},
			{Z_RET, {VALUE(REG_LOCAL+0)}},
			{Z_END},
		}
	},
	{
		R_DEREF_UNBOX,
		2,
			// 0 (param): value to dereference
			// 1: temp
		(struct zinstr []) {
			{Z_JGE, {VALUE(REG_LOCAL+0), VALUE(REG_C000)}, 0, 2},
			{OP_LABEL(1)},
			{Z_ADD, {VALUE(REG_LOCAL+0), VALUE(REG_LOCAL+0)}, REG_LOCAL+1},
			{Z_LOADW, {VALUE(REG_LOCAL+1), SMALL(0)}, REG_LOCAL+1},
			{Z_JZ, {VALUE(REG_LOCAL+1)}, 0, 2},
			{Z_STORE, {SMALL(REG_LOCAL+0), VALUE(REG_LOCAL+1)}},
			{Z_JL, {VALUE(REG_LOCAL+0), VALUE(REG_C000)}, 0, 1},

			{OP_LABEL(2)},
			{Z_TESTN, {VALUE(REG_LOCAL+0), VALUE(REG_E000)}, 0, 3},
			{Z_AND, {VALUE(REG_LOCAL+0), VALUE(REG_NIL)}, REG_LOCAL+0},
			{Z_LOADW, {SMALL(0), VALUE(REG_LOCAL+0)}, REG_LOCAL+0},

			{OP_LABEL(3)},
			{Z_RET, {VALUE(REG_LOCAL+0)}},
			{Z_END},
		}
	},
	{
		R_DEREF_OBJ,
		1,
			// 0 (param): tagged reference
			// returns 0 or raw object id
		(struct zinstr []) {
			{Z_JGE, {VALUE(REG_LOCAL+0), VALUE(REG_C000)}, 0, 2},
			{OP_LABEL(1)},
			{Z_ADD, {VALUE(REG_LOCAL+0), VALUE(REG_LOCAL+0)}, REG_LOCAL+0},
			{Z_LOADW, {VALUE(REG_LOCAL+0), SMALL(0)}, REG_LOCAL+0},
			{Z_JL, {VALUE(REG_LOCAL+0), VALUE(REG_C000)}, 0, 1},

			{OP_LABEL(2)},
			{Z_JLE, {VALUE(REG_LOCAL+0), SMALL(0)}, 0, RFALSE},
			{Z_JGE, {VALUE(REG_LOCAL+0), VALUE(REG_NIL)}, 0, RFALSE},
			{Z_RET, {VALUE(REG_LOCAL+0)}},
			{Z_END},
		}
	},
	{
		R_DEREF_OBJ_FAIL,
		1,
			// 0 (param): tagged reference
			// returns raw object id
		(struct zinstr []) {
			{Z_JGE, {VALUE(REG_LOCAL+0), VALUE(REG_C000)}, 0, 2},
			{OP_LABEL(1)},
			{Z_ADD, {VALUE(REG_LOCAL+0), VALUE(REG_LOCAL+0)}, REG_LOCAL+0},
			{Z_LOADW, {VALUE(REG_LOCAL+0), SMALL(0)}, REG_LOCAL+0},
			{Z_JL, {VALUE(REG_LOCAL+0), VALUE(REG_C000)}, 0, 1},

			{OP_LABEL(2)},
			{Z_JLE, {VALUE(REG_LOCAL+0), SMALL(0)}, 0, 4},
			{Z_JL, {VALUE(REG_LOCAL+0), VALUE(REG_NIL)}, 0, 5},

			{OP_LABEL(4)},
			{Z_THROW, {SMALL(0), VALUE(REG_FAILJMP)}},

			{OP_LABEL(5)},
			{Z_RET, {VALUE(REG_LOCAL+0)}},
			{Z_END},
		}
	},
	{
		R_DEREF_OBJ_FORCE,
		1,
			// 0 (param): tagged reference
			// returns raw object id
		(struct zinstr []) {
			{Z_JGE, {VALUE(REG_LOCAL+0), VALUE(REG_C000)}, 0, 2},
			{OP_LABEL(1)},
			{Z_ADD, {VALUE(REG_LOCAL+0), VALUE(REG_LOCAL+0)}, REG_LOCAL+0},
			{Z_LOADW, {VALUE(REG_LOCAL+0), SMALL(0)}, REG_LOCAL+0},
			{Z_JL, {VALUE(REG_LOCAL+0), VALUE(REG_C000)}, 0, 1},

			{OP_LABEL(2)},
			{Z_JLE, {VALUE(REG_LOCAL+0), SMALL(0)}, 0, 4},
			{Z_JL, {VALUE(REG_LOCAL+0), VALUE(REG_NIL)}, 0, 5},

			{OP_LABEL(4)},
			{Z_THROW, {SMALL(FATAL_EXPECTED_OBJ), VALUE(REG_FATALJMP)}},

			{OP_LABEL(5)},
			{Z_RET, {VALUE(REG_LOCAL+0)}},
			{Z_END},
		}
	},
	{
		R_GRAB_ARG1,
		0,
		(struct zinstr []) {
			{Z_CALLVN, {ROUTINE(R_GET_PAIR_RR), VALUE(REG_A+0), SMALL(REG_X+0), SMALL(REG_A+0)}},
			{Z_CALL2S, {ROUTINE(R_DEREF_UNBOX), VALUE(REG_X+0)}, REG_X+0},
			{Z_RFALSE},
			{Z_END},
		}
	},
	{
		R_READ_FLAG,
		2,
			// 0 (param): tagged reference e.g. to object, must be bound
			// 1 (param): flag
			// returns non-zero if it's an object and the flag is set
		(struct zinstr []) {
			{Z_JGE, {VALUE(REG_LOCAL+0), VALUE(REG_C000)}, 0, 2},
			{OP_LABEL(1)},
			{Z_ADD, {VALUE(REG_LOCAL+0), VALUE(REG_LOCAL+0)}, REG_LOCAL+0},
			{Z_LOADW, {VALUE(REG_LOCAL+0), SMALL(0)}, REG_LOCAL+0},
			{Z_JL, {VALUE(REG_LOCAL+0), VALUE(REG_C000)}, 0, 1},

			{OP_LABEL(2)},
			{Z_JLE, {VALUE(REG_LOCAL+0), SMALL(0)}, 0, RFALSE},
			{Z_JGE, {VALUE(REG_LOCAL+0), VALUE(REG_NIL)}, 0, RFALSE},
			{Z_JNA, {VALUE(REG_LOCAL+0), VALUE(REG_LOCAL+1)}, 0, RFALSE},
			{Z_RTRUE},
			{Z_END},
		}
	},
	{
		R_CHECK_FLAG,
		2,
			// 0 (param): tagged reference e.g. to object, known to be bound
			// 1 (param): flag
			// throws failure if flag is unset (or non-object)
		(struct zinstr []) {
			{Z_JGE, {VALUE(REG_LOCAL+0), VALUE(REG_C000)}, 0, 2},
			{OP_LABEL(1)},
			{Z_ADD, {VALUE(REG_LOCAL+0), VALUE(REG_LOCAL+0)}, REG_LOCAL+0},
			{Z_LOADW, {VALUE(REG_LOCAL+0), SMALL(0)}, REG_LOCAL+0},
			{Z_JL, {VALUE(REG_LOCAL+0), VALUE(REG_C000)}, 0, 1},

			{OP_LABEL(2)},
			{Z_JLE, {VALUE(REG_LOCAL+0), SMALL(0)}, 0, 3},
			{Z_JGE, {VALUE(REG_LOCAL+0), VALUE(REG_NIL)}, 0, 3},
			{Z_JA, {VALUE(REG_LOCAL+0), VALUE(REG_LOCAL+1)}, 0, RFALSE},

			{OP_LABEL(3)},
			{Z_THROW, {SMALL(0), VALUE(REG_FAILJMP)}},
			{Z_RFALSE},	// don't confuse txd
			{Z_END},
		}
	},
	{
		R_CHECK_FLAG_N,
		2,
			// 0 (param): tagged reference e.g. to object, known to be bound
			// 1 (param): flag
			// throws failure if flag is set
		(struct zinstr []) {
			{Z_JGE, {VALUE(REG_LOCAL+0), VALUE(REG_C000)}, 0, 2},
			{OP_LABEL(1)},
			{Z_ADD, {VALUE(REG_LOCAL+0), VALUE(REG_LOCAL+0)}, REG_LOCAL+0},
			{Z_LOADW, {VALUE(REG_LOCAL+0), SMALL(0)}, REG_LOCAL+0},
			{Z_JL, {VALUE(REG_LOCAL+0), VALUE(REG_C000)}, 0, 1},

			{OP_LABEL(2)},
			// bound value
			{Z_JLE, {VALUE(REG_LOCAL+0), SMALL(0)}, 0, RFALSE},
			{Z_JGE, {VALUE(REG_LOCAL+0), VALUE(REG_NIL)}, 0, RFALSE},
			{Z_JNA, {VALUE(REG_LOCAL+0), VALUE(REG_LOCAL+1)}, 0, RFALSE},

			// unbound value
			{OP_LABEL(3)},
			{Z_THROW, {SMALL(0), VALUE(REG_FAILJMP)}},
			{Z_RFALSE},	// don't confuse txd
			{Z_END},
		}
	},
	{
		R_SET_FLAG,
		2,
			// 0 (param): object (tagged reference)
			// 1 (param): attribute number
		(struct zinstr []) {
			{Z_CALL2S, {ROUTINE(R_DEREF_OBJ_FORCE), VALUE(REG_LOCAL+0)}, REG_LOCAL+0},
			{Z_SET_ATTR, {VALUE(REG_LOCAL+0), VALUE(REG_LOCAL+1)}},
			{Z_RFALSE},
			{Z_END},
		}
	},
	{
		R_RESET_FLAG,
		2,
			// 0 (param): object (tagged reference)
			// 1 (param): attribute number
		(struct zinstr []) {
			{Z_CALL2S, {ROUTINE(R_DEREF_OBJ), VALUE(REG_LOCAL+0)}, REG_LOCAL+0},
			{Z_JZ, {VALUE(REG_LOCAL+0)}, 0, RFALSE},
			{Z_CLEAR_ATTR, {VALUE(REG_LOCAL+0), VALUE(REG_LOCAL+1)}},
			{Z_RFALSE},
			{Z_END},
		}
	},
	{
		R_SET_LISTFLAG,
		3,
			// 0 (param): object (tagged reference)
			// 1 (param): attribute number, then temp
			// 2 (param): root register number
			// REG_TEMP (param): pointer to next-array
		(struct zinstr []) {
			{Z_CALL2S, {ROUTINE(R_DEREF_OBJ_FORCE), VALUE(REG_LOCAL+0)}, REG_LOCAL+0},
			{Z_JA, {VALUE(REG_LOCAL+0), VALUE(REG_LOCAL+1)}, 0, RFALSE},
			{Z_SET_ATTR, {VALUE(REG_LOCAL+0), VALUE(REG_LOCAL+1)}},
			{Z_LOAD, {VALUE(REG_LOCAL+2)}, REG_LOCAL+1},
			{Z_STOREW, {VALUE(REG_TEMP), VALUE(REG_LOCAL+0), VALUE(REG_LOCAL+1)}},
			{Z_STORE, {VALUE(REG_LOCAL+2), VALUE(REG_LOCAL+0)}},
			{Z_RFALSE},
			{Z_END},
		}
	},
	{
		R_RESET_LISTFLAG,
		3,
			// 0 (param): object (tagged reference)
			// 1 (param): attribute number, then temp
			// 2 (param): root register number, then temp
			// REG_TEMP (param): pointer to next-array
		(struct zinstr []) {
			{Z_CALL2S, {ROUTINE(R_DEREF_OBJ_FORCE), VALUE(REG_LOCAL+0)}, REG_LOCAL+0},
			{Z_JNA, {VALUE(REG_LOCAL+0), VALUE(REG_LOCAL+1)}, 0, RFALSE},
			{Z_CLEAR_ATTR, {VALUE(REG_LOCAL+0), VALUE(REG_LOCAL+1)}},

			{Z_LOAD, {VALUE(REG_LOCAL+2)}, REG_LOCAL+1},
			{Z_JE, {VALUE(REG_LOCAL+1), VALUE(REG_LOCAL+0)}, 0, 1},

			{OP_LABEL(2)},
			{Z_LOADW, {VALUE(REG_TEMP), VALUE(REG_LOCAL+1)}, REG_LOCAL+2},
			{Z_JE, {VALUE(REG_LOCAL+2), VALUE(REG_LOCAL+0)}, 0, 3},
			{Z_STORE, {SMALL(REG_LOCAL+1), VALUE(REG_LOCAL+2)}},
			{Z_JUMP, {REL_LABEL(2)}},

			{OP_LABEL(3)},
			{Z_LOADW, {VALUE(REG_TEMP), VALUE(REG_LOCAL+0)}, REG_LOCAL+2},
			{Z_STOREW, {VALUE(REG_TEMP), VALUE(REG_LOCAL+1), VALUE(REG_LOCAL+2)}},
			{Z_RFALSE},

			{OP_LABEL(1)},
			{Z_LOADW, {VALUE(REG_TEMP), VALUE(REG_LOCAL+0)}, REG_LOCAL+1},
			{Z_STORE, {VALUE(REG_LOCAL+2), VALUE(REG_LOCAL+1)}},
			{Z_RFALSE},
			{Z_END},
		}
	},
	{
		R_CLRALL_LISTFLAG,
		4,
			// 0 (param): attribute number
			// 1 (param): root register number
			// 2 (param): pointer to next-array
			// 3: iterator
		(struct zinstr []) {
			{Z_LOAD, {VALUE(REG_LOCAL+1)}, REG_LOCAL+3},
			{Z_JZ, {VALUE(REG_LOCAL+3)}, 0, RFALSE},

			{OP_LABEL(1)},
			{Z_CLEAR_ATTR, {VALUE(REG_LOCAL+3), VALUE(REG_LOCAL+0)}},
			{Z_LOADW, {VALUE(REG_LOCAL+2), VALUE(REG_LOCAL+3)}, REG_LOCAL+3},
			{Z_JNZ, {VALUE(REG_LOCAL+3)}, 0, 1},

			{Z_STORE, {VALUE(REG_LOCAL+1), SMALL(0)}},
			{Z_RFALSE},
			{Z_END},
		}
	},
	{
		R_SET_PARENT,
		2,
			// 0 (param): object (tagged reference)
			// 1 (param): new parent (tagged reference)
		(struct zinstr []) {
			{Z_CALL2S, {ROUTINE(R_DEREF_OBJ_FORCE), VALUE(REG_LOCAL+0)}, REG_LOCAL+0},
			{Z_CALL2S, {ROUTINE(R_DEREF_OBJ_FORCE), VALUE(REG_LOCAL+1)}, REG_LOCAL+1},
			{Z_INSERT_OBJ, {VALUE(REG_LOCAL+0), VALUE(REG_LOCAL+1)}},
			{Z_RFALSE},
			{Z_END},
		}
	},
	{
		R_RESET_PARENT,
		1,
			// 0 (param): object (tagged reference)
		(struct zinstr []) {
			{Z_CALL2S, {ROUTINE(R_DEREF_OBJ), VALUE(REG_LOCAL+0)}, REG_LOCAL+0},
			{Z_JZ, {VALUE(REG_LOCAL+0)}, 0, RFALSE},

			{Z_REMOVE_OBJ, {VALUE(REG_LOCAL+0)}},
			{Z_RFALSE},
			{Z_END},
		}
	},
	{
		R_PLUS,
		2,
			// 0 (param): first parameter (and temp)
			// 1 (param): second parameter
			// returns result
		(struct zinstr []) {
			{Z_CALL2S, {ROUTINE(R_DEREF), VALUE(REG_LOCAL+0)}, REG_LOCAL+0},
			{Z_JL, {VALUE(REG_LOCAL+0), VALUE(REG_4000)}, 0, 1},
			{Z_CALL2S, {ROUTINE(R_DEREF), VALUE(REG_LOCAL+1)}, REG_LOCAL+1},
			{Z_JL, {VALUE(REG_LOCAL+1), VALUE(REG_4000)}, 0, 1},
			{Z_ADD, {VALUE(REG_LOCAL+0), VALUE(REG_LOCAL+1)}, REG_LOCAL+0},
			{Z_SUB, {VALUE(REG_LOCAL+0), VALUE(REG_4000)}, REG_LOCAL+0},
			{Z_JGE, {VALUE(REG_LOCAL+0), VALUE(REG_4000)}, 0, 2},

			{OP_LABEL(1)},
			{Z_THROW, {SMALL(0), VALUE(REG_FAILJMP)}},

			{OP_LABEL(2)},
			{Z_RET, {VALUE(REG_LOCAL+0)}},
			{Z_END},
		}
	},
	{
		R_MINUS,
		2,
			// 0 (param): first parameter (and temp)
			// 1 (param): second parameter
			// returns result
		(struct zinstr []) {
			{Z_CALL2S, {ROUTINE(R_DEREF), VALUE(REG_LOCAL+0)}, REG_LOCAL+0},
			{Z_JL, {VALUE(REG_LOCAL+0), VALUE(REG_4000)}, 0, 1},
			{Z_CALL2S, {ROUTINE(R_DEREF), VALUE(REG_LOCAL+1)}, REG_LOCAL+1},
			{Z_JL, {VALUE(REG_LOCAL+1), VALUE(REG_4000)}, 0, 1},
			{Z_SUB, {VALUE(REG_LOCAL+0), VALUE(REG_LOCAL+1)}, REG_LOCAL+0},
			{Z_JGE, {VALUE(REG_LOCAL+0), SMALL(0)}, 0, 2},

			{OP_LABEL(1)},
			{Z_THROW, {SMALL(0), VALUE(REG_FAILJMP)}},

			{OP_LABEL(2)},
			{Z_OR, {VALUE(REG_LOCAL+0), VALUE(REG_4000)}, REG_LOCAL+0},
			{Z_RET, {VALUE(REG_LOCAL+0)}},
			{Z_END},
		}
	},
	{
		R_TIMES,
		2,
			// 0 (param): first parameter (and temp)
			// 1 (param): second parameter
			// returns result
		(struct zinstr []) {
			{Z_CALL2S, {ROUTINE(R_DEREF), VALUE(REG_LOCAL+0)}, REG_LOCAL+0},
			{Z_JL, {VALUE(REG_LOCAL+0), VALUE(REG_4000)}, 0, 1},
			{Z_CALL2S, {ROUTINE(R_DEREF), VALUE(REG_LOCAL+1)}, REG_LOCAL+1},
			{Z_JGE, {VALUE(REG_LOCAL+1), VALUE(REG_4000)}, 0, 2},

			{OP_LABEL(1)},
			{Z_THROW, {SMALL(0), VALUE(REG_FAILJMP)}},

			{OP_LABEL(2)},
			{Z_AND, {VALUE(REG_LOCAL+0), VALUE(REG_3FFF)}, REG_LOCAL+0},
			{Z_AND, {VALUE(REG_LOCAL+1), VALUE(REG_3FFF)}, REG_LOCAL+1},
			{Z_MUL, {VALUE(REG_LOCAL+0), VALUE(REG_LOCAL+1)}, REG_LOCAL+0},
			{Z_AND, {VALUE(REG_LOCAL+0), VALUE(REG_3FFF)}, REG_LOCAL+0},
			{Z_OR, {VALUE(REG_LOCAL+0), VALUE(REG_4000)}, REG_LOCAL+0},
			{Z_RET, {VALUE(REG_LOCAL+0)}},
			{Z_END},
		}
	},
	{
		R_DIVIDED,
		2,
			// 0 (param): first parameter (and temp)
			// 1 (param): second parameter
			// returns result
		(struct zinstr []) {
			{Z_CALL2S, {ROUTINE(R_DEREF), VALUE(REG_LOCAL+0)}, REG_LOCAL+0},
			{Z_JL, {VALUE(REG_LOCAL+0), VALUE(REG_4000)}, 0, 1},
			{Z_CALL2S, {ROUTINE(R_DEREF), VALUE(REG_LOCAL+1)}, REG_LOCAL+1},
			{Z_JG, {VALUE(REG_LOCAL+1), VALUE(REG_4000)}, 0, 2},	// also handles divide by zero

			{OP_LABEL(1)},
			{Z_THROW, {SMALL(0), VALUE(REG_FAILJMP)}},

			{OP_LABEL(2)},
			{Z_AND, {VALUE(REG_LOCAL+0), VALUE(REG_3FFF)}, REG_LOCAL+0},
			{Z_AND, {VALUE(REG_LOCAL+1), VALUE(REG_3FFF)}, REG_LOCAL+1},
			{Z_DIV, {VALUE(REG_LOCAL+0), VALUE(REG_LOCAL+1)}, REG_LOCAL+0},
			{Z_OR, {VALUE(REG_LOCAL+0), VALUE(REG_4000)}, REG_LOCAL+0},
			{Z_RET, {VALUE(REG_LOCAL+0)}},
			{Z_END},
		}
	},
	{
		R_MODULO,
		2,
			// 0 (param): first parameter (and temp)
			// 1 (param): second parameter
			// returns result
		(struct zinstr []) {
			{Z_CALL2S, {ROUTINE(R_DEREF), VALUE(REG_LOCAL+0)}, REG_LOCAL+0},
			{Z_JL, {VALUE(REG_LOCAL+0), VALUE(REG_4000)}, 0, 1},
			{Z_CALL2S, {ROUTINE(R_DEREF), VALUE(REG_LOCAL+1)}, REG_LOCAL+1},
			{Z_JG, {VALUE(REG_LOCAL+1), VALUE(REG_4000)}, 0, 2},	// also handles divide by zero

			{OP_LABEL(1)},
			{Z_THROW, {SMALL(0), VALUE(REG_FAILJMP)}},

			{OP_LABEL(2)},
			{Z_AND, {VALUE(REG_LOCAL+0), VALUE(REG_3FFF)}, REG_LOCAL+0},
			{Z_AND, {VALUE(REG_LOCAL+1), VALUE(REG_3FFF)}, REG_LOCAL+1},
			{Z_MOD, {VALUE(REG_LOCAL+0), VALUE(REG_LOCAL+1)}, REG_LOCAL+0},
			{Z_OR, {VALUE(REG_LOCAL+0), VALUE(REG_4000)}, REG_LOCAL+0},
			{Z_RET, {VALUE(REG_LOCAL+0)}},
			{Z_END},
		}
	},
	{
		R_RANDOM,
		2,
			// 0 (param): first parameter (and temp)
			// 1 (param): second parameter
			// returns result
		(struct zinstr []) {
			{Z_CALL2S, {ROUTINE(R_DEREF), VALUE(REG_LOCAL+0)}, REG_LOCAL+0},
			{Z_JL, {VALUE(REG_LOCAL+0), VALUE(REG_4000)}, 0, 1},
			{Z_CALL2S, {ROUTINE(R_DEREF), VALUE(REG_LOCAL+1)}, REG_LOCAL+1},
			{Z_JL, {VALUE(REG_LOCAL+1), VALUE(REG_4000)}, 0, 1},
			{Z_JGE, {VALUE(REG_LOCAL+1), VALUE(REG_LOCAL+0)}, 0, 2},

			{OP_LABEL(1)},
			{Z_THROW, {SMALL(0), VALUE(REG_FAILJMP)}},

			{OP_LABEL(2)},
			{Z_SUB, {VALUE(REG_LOCAL+1), VALUE(REG_LOCAL+0)}, REG_LOCAL+1},	// 0000..3fff
			{Z_INC, {SMALL(REG_LOCAL+1)}},					// 0001..4000
			{Z_RANDOM, {VALUE(REG_LOCAL+1)}, REG_LOCAL+1},			// 0001..4000
			{Z_DEC, {SMALL(REG_LOCAL+0)}},					// 3fff..7ffe
			{Z_ADD, {VALUE(REG_LOCAL+1), VALUE(REG_LOCAL+0)}, REG_LOCAL+1},	// 4000..7fff
			{Z_RET, {VALUE(REG_LOCAL+1)}},
			{Z_END},
		}
	},
	{
		R_GREATER_THAN,
		2,
			// 0 (param): first tagged reference
			// 1 (param): second tagged reference
			// returns boolean
		(struct zinstr []) {
			{Z_CALL2S, {ROUTINE(R_DEREF), VALUE(REG_LOCAL+0)}, REG_LOCAL+0},
			{Z_JL, {VALUE(REG_LOCAL+0), VALUE(REG_4000)}, 0, RFALSE},
			{Z_CALL2S, {ROUTINE(R_DEREF), VALUE(REG_LOCAL+1)}, REG_LOCAL+1},
			{Z_JL, {VALUE(REG_LOCAL+1), VALUE(REG_4000)}, 0, RFALSE},
			{Z_JLE, {VALUE(REG_LOCAL+0), VALUE(REG_LOCAL+1)}, 0, RFALSE},
			{Z_RTRUE},
			{Z_END},
		}
	},
	{
		R_FAIL_PRED,
		0,
		(struct zinstr []) {
			{Z_RFALSE},
			{Z_END},
		}
	},
	{
		R_QUIT_PRED,
		0,
		(struct zinstr []) {
			{Z_CALL1N, {ROUTINE(R_LINE)}},
			{Z_QUIT},
			{Z_END},
		}
	},
	{
		R_SAVE,
		0,
			// returns number 0 if saving, number 1 if coming back, fails on failure
		(struct zinstr []) {
			{Z_JNZ, {VALUE(REG_NSPAN)}, 0, 2},
			{Z_JNZ, {VALUE(REG_STATUSBAR)}, 0, 2},

			{Z_CALL1N, {ROUTINE(R_LINE)}},
			{Z_SAVE, {}, REG_TEMP},
			{Z_STORE, {SMALL(REG_SPACE), SMALL(4)}},
			{Z_JGE, {VALUE(REG_TEMP), SMALL(1)}, 0, 1},

			{Z_THROW, {SMALL(0), VALUE(REG_FAILJMP)}},

			{OP_LABEL(2)},
			{Z_THROW, {SMALL(FATAL_IO), VALUE(REG_FATALJMP)}},

			{OP_LABEL(1)},
			{Z_ADD, {VALUE(REG_TEMP), VALUE(REG_3FFF)}, REG_TEMP},
			{Z_RET, {VALUE(REG_TEMP)}},
			{Z_END},
		}
	},
	{
		R_SAVE_UNDO,
		0,
			// returns number 0 if saving, number 1 if coming back, fails on failure
		(struct zinstr []) {
			{Z_JNZ, {VALUE(REG_NSPAN)}, 0, 3},
			{Z_JNZ, {VALUE(REG_STATUSBAR)}, 0, 3},

			{Z_CALL1N, {ROUTINE(R_LINE)}},
			{Z_SAVE_UNDO, {}, REG_TEMP},

			{Z_JNE, {VALUE(REG_TEMP), SMALL(1)}, 0, 1},
			{Z_OR, {USERGLOBAL(0), SMALL(1)}, DEST_USERGLOBAL(0)},
			{OP_LABEL(1)},

			{Z_JGE, {VALUE(REG_TEMP), SMALL(1)}, 0, 2},

			{Z_THROW, {SMALL(0), VALUE(REG_FAILJMP)}},

			{OP_LABEL(3)},
			{Z_THROW, {SMALL(FATAL_IO), VALUE(REG_FATALJMP)}},

			{OP_LABEL(2)},
			{Z_STORE, {SMALL(REG_SPACE), SMALL(4)}},
			{Z_ADD, {VALUE(REG_TEMP), VALUE(REG_3FFF)}, REG_TEMP},
			{Z_RET, {VALUE(REG_TEMP)}},
			{Z_END},
		}
	},
	{
		R_SCRIPT_ON,
		0,
		(struct zinstr []) {
			{Z_CALL1N, {ROUTINE(R_LINE)}},
			{Z_OUTPUT_STREAM, {SMALL(2)}},
			{Z_STORE, {SMALL(REG_SPACE), SMALL(4)}},
			{Z_LOADW, {SMALL(0), SMALL(8)}, REG_TEMP},
			{Z_TEST, {VALUE(REG_TEMP), SMALL(1)}, 0, 1},

			{Z_THROW, {SMALL(0), VALUE(REG_FAILJMP)}},

			{OP_LABEL(1)},
			{Z_RFALSE},
			{Z_END},
		}
	},
	{
		R_GET_KEY,
		1,
			// 0: temp
			// returns single-char dictionary word
		(struct zinstr []) {
			{Z_CALL1N, {ROUTINE(R_SYNC_SPACE)}},

			{OP_LABEL(2)},
			{Z_READCHAR, {SMALL(1)}, REG_LOCAL+0},
			{Z_AND, {VALUE(REG_LOCAL+0), SMALL(0xff)}, REG_LOCAL+0},
			{Z_JE, {VALUE(REG_LOCAL+0), SMALL(8), SMALL(13), SMALL(129)}, 0, 1},
			{Z_JL, {VALUE(REG_LOCAL+0), SMALL(0x20)}, 0, 2},
			{Z_JL, {VALUE(REG_LOCAL+0), SMALL(0x30)}, 0, 1},
			{Z_JL, {VALUE(REG_LOCAL+0), SMALL(0x3a)}, 0, 4},
			{Z_JL, {VALUE(REG_LOCAL+0), SMALL(0x41)}, 0, 1},
			{Z_JL, {VALUE(REG_LOCAL+0), SMALL(0x5b)}, 0, 3},
			{Z_JL, {VALUE(REG_LOCAL+0), SMALL(127)}, 0, 1},
			{Z_JE, {VALUE(REG_LOCAL+0), SMALL(130), SMALL(131), SMALL(132)}, 0, 1},
			{Z_JL, {VALUE(REG_LOCAL+0), SMALL(155)}, 0, 2},
			{Z_JL, {VALUE(REG_LOCAL+0), SMALL(252)}, 0, 1},
			{Z_JUMP, {REL_LABEL(2)}},

			{OP_LABEL(3)},
			// convert to lowercase
			{Z_OR, {VALUE(REG_LOCAL+0), SMALL(0x20)}, REG_LOCAL+0},

			{OP_LABEL(1)},
			{Z_OR, {VALUE(REG_LOCAL+0), LARGE(0x3e00)}, REG_LOCAL+0},
			{Z_RET, {VALUE(REG_LOCAL+0)}},

			{OP_LABEL(4)},
			{Z_ADD, {VALUE(REG_LOCAL+0), LARGE(0x4000-0x30)}, REG_LOCAL+0},
			{Z_RET, {VALUE(REG_LOCAL+0)}},
			{Z_END},
		}
	},
	{
		R_GET_INPUT,
		4,
			// 0: temp
			// 1: input buffer
			// 2: parse buffer
			// 3: saved REG_COLL
			// returns output
		(struct zinstr []) {
			{Z_CALL1N, {ROUTINE(R_SYNC_SPACE)}},
			{Z_STORE, {SMALL(REG_SPACE), SMALL(4)}},

			// Store the input buffer and parse table in the aux area.
			{Z_STORE, {SMALL(REG_LOCAL+3), VALUE(REG_COLL)}},
			{Z_CALL2S, {ROUTINE(R_AUX_ALLOC), SMALL(128+1+2*48)}, REG_LOCAL+1},

			// prepare input buffer
			{Z_STOREB, {VALUE(REG_LOCAL+1), SMALL(0), SMALL(253)}},
			{Z_STOREB, {VALUE(REG_LOCAL+1), SMALL(1), SMALL(0)}},

			// prepare parse buffer
			{Z_ADD, {VALUE(REG_LOCAL+1), LARGE(256)}, REG_LOCAL+2},
			{Z_STOREB, {VALUE(REG_LOCAL+2), SMALL(0), SMALL(48)}},

			{Z_AREAD, {VALUE(REG_LOCAL+1), VALUE(REG_LOCAL+2)}, REG_LOCAL+0},
			{Z_CALLVS, {ROUTINE(R_PARSE_INPUT), VALUE(REG_LOCAL+2), VALUE(REG_LOCAL+1)}, REG_PUSH},
			{Z_STORE, {SMALL(REG_COLL), VALUE(REG_LOCAL+3)}},
			{Z_RET_POPPED},
			{Z_END},
		}
	},
	{
		R_PARSE_INPUT,
		11,
			// 0 (param): parse buffer, then temp
			// 1 (param): input buffer
			// 2: parse buffer
			// 3: loadw index into parse buffer
			// 4: tail of output list
			// 5: temp
			// 6: loadb index into parse buffer
			// 7: pointer into dictionary
			// 8: length of word
			// 9: start of word
			// 10: temp
			// returns output
		(struct zinstr []) {
			{Z_STORE, {SMALL(REG_LOCAL+2), VALUE(REG_LOCAL+0)}},
			{Z_STORE, {SMALL(REG_LOCAL+4), VALUE(REG_NIL)}},

			{Z_LOADB, {VALUE(REG_LOCAL+2), SMALL(1)}, REG_LOCAL+0},

			{Z_JZ, {VALUE(REG_LOCAL+0)}, 0, 3},

			{Z_ADD, {VALUE(REG_LOCAL+0), VALUE(REG_LOCAL+0)}, REG_LOCAL+3},
			{Z_ADD, {VALUE(REG_LOCAL+3), VALUE(REG_LOCAL+3)}, REG_LOCAL+6},
			{Z_SUB, {VALUE(REG_LOCAL+3), SMALL(1)}, REG_LOCAL+3},
			{Z_ADD, {VALUE(REG_LOCAL+6), SMALL(1)}, REG_LOCAL+6},
			// local 3: loadw offset to dict pointer of last entry in parse buffer
			// local 6: loadb offset to offset byte of last entry in parse buffer

			{OP_LABEL(4)},
			{Z_LOADW, {VALUE(REG_LOCAL+2), VALUE(REG_LOCAL+3)}, REG_LOCAL+7},
			{Z_LOADB, {VALUE(REG_LOCAL+2), VALUE(REG_LOCAL+6)}, REG_LOCAL+9},
			{Z_DEC, {SMALL(REG_LOCAL+6)}},
			{Z_LOADB, {VALUE(REG_LOCAL+2), VALUE(REG_LOCAL+6)}, REG_LOCAL+8},
			{Z_SUB, {VALUE(REG_LOCAL+6), SMALL(3)}, REG_LOCAL+6},
			{Z_SUB, {VALUE(REG_LOCAL+3), SMALL(2)}, REG_LOCAL+3},
			// 7, 8, 9 are dict pointer, length and offset, respectively

			{Z_JZ, {VALUE(REG_LOCAL+7)}, 0, 5},

			// word was found in dictionary
			{OP_LABEL(1)},
			// local 7: pointer to encoded word in static dict table
			// local 8: length of typed word
			// local 9: offset of word in input array

			{Z_STORE, {SMALL(REG_LOCAL+10), REF(G_SCRATCH)}},		// 12 bytes of scratch area
			{Z_OUTPUT_STREAM, {SMALL(3), VALUE(REG_LOCAL+10)}},
			{Z_PRINTADDR, {VALUE(REG_LOCAL+7)}},
			{Z_OUTPUT_STREAM, {LARGE(0x10000-3)}},
			{Z_LOADW, {VALUE(REG_LOCAL+10), SMALL(0)}, REG_LOCAL+10},	// length of truncated word

			{Z_SUB, {VALUE(REG_LOCAL+7), REF(G_DICT_TABLE)}, REG_LOCAL+7},
			{Z_DIV, {VALUE(REG_LOCAL+7), SMALL(6)}, REG_LOCAL+7},
			{Z_OR, {VALUE(REG_LOCAL+7), VALUE(REG_2000)}, REG_LOCAL+7},	// tagged simple dict word

			{Z_DEC_JL, {SMALL(REG_LOCAL+8), VALUE(REG_LOCAL+10)}, 0, 13},
			{Z_STORE, {SMALL(REG_LOCAL+5), VALUE(REG_NIL)}},
			{Z_ADD, {VALUE(REG_LOCAL+9), VALUE(REG_LOCAL+1)}, REG_LOCAL+9},

			{OP_LABEL(14)},
			{Z_LOADB, {VALUE(REG_LOCAL+9), VALUE(REG_LOCAL+8)}, REG_LOCAL+0},
			{Z_OR, {VALUE(REG_LOCAL+0), LARGE(0x3e00)}, REG_LOCAL+0},
			{Z_CALLVS, {ROUTINE(R_PUSH_PAIR_VV), VALUE(REG_LOCAL+0), VALUE(REG_LOCAL+5)}, REG_LOCAL+5},
			{Z_DEC_JGE, {SMALL(REG_LOCAL+8), VALUE(REG_LOCAL+10)}, 0, 14},

			{Z_CALLVS, {ROUTINE(R_PUSH_PAIR_VV), VALUE(REG_LOCAL+7), VALUE(REG_LOCAL+5)}, REG_LOCAL+7},
			{Z_OR, {VALUE(REG_LOCAL+7), VALUE(REG_E000)}, REG_LOCAL+7},

			{OP_LABEL(13)},
			{Z_STORE, {SMALL(REG_LOCAL+5), VALUE(REG_LOCAL+7)}},
			{Z_JUMP, {REL_LABEL(12)}},

			{OP_LABEL(5)},
			// word not found in dictionary; try parsing as integer
			{Z_STORE, {SMALL(REG_LOCAL+0), SMALL(0)}},
			{Z_STORE, {SMALL(REG_LOCAL+10), VALUE(REG_LOCAL+8)}},
			{Z_STORE, {SMALL(REG_LOCAL+7), VALUE(REG_LOCAL+9)}},
			{OP_LABEL(7)},
			{Z_LOADB, {VALUE(REG_LOCAL+1), VALUE(REG_LOCAL+7)}, REG_LOCAL+5},
			{Z_JL, {VALUE(REG_LOCAL+5), SMALL('0')}, 0, 8},
			{Z_JG, {VALUE(REG_LOCAL+5), SMALL('9')}, 0, 8},
			{Z_JGE, {VALUE(REG_LOCAL+0), LARGE(1639)}, 0, 8},	// would become >= 16384 after multiplication
			{Z_MUL, {VALUE(REG_LOCAL+0), SMALL(10)}, REG_LOCAL+0},
			{Z_SUB, {VALUE(REG_LOCAL+5), SMALL('0')}, REG_LOCAL+5},
			{Z_ADD, {VALUE(REG_LOCAL+5), VALUE(REG_LOCAL+0)}, REG_LOCAL+0},
			{Z_INC, {SMALL(REG_LOCAL+7)}},
			{Z_DEC_JGE, {SMALL(REG_LOCAL+10), SMALL(1)}, 0, 7},

			{Z_ADD, {VALUE(REG_LOCAL+0), VALUE(REG_4000)}, REG_LOCAL+0},
			{Z_JL, {VALUE(REG_LOCAL+0), VALUE(REG_4000)}, 0, 8},
			{Z_STORE, {SMALL(REG_LOCAL+5), VALUE(REG_LOCAL+0)}},
			{Z_JUMP, {REL_LABEL(12)}},

			{OP_LABEL(8)},
			// not an integer, or integer overflow

			{Z_JGE, {VALUE(REG_LOCAL+8), SMALL(2)}, 0, 9},

			// single-char word
			{Z_LOADB, {VALUE(REG_LOCAL+1), VALUE(REG_LOCAL+9)}, REG_LOCAL+5},
			{Z_OR, {VALUE(REG_LOCAL+5), LARGE(0x3e00)}, REG_LOCAL+5},
			{Z_JUMP, {REL_LABEL(12)}},

			{OP_LABEL(9)},
			// unknown multi-char word
			{Z_ADD, {VALUE(REG_LOCAL+1), VALUE(REG_LOCAL+9)}, REG_LOCAL+7},
			{Z_CALLVS, {ROUTINE(R_TRY_STEMMING), VALUE(REG_LOCAL+7), VALUE(REG_LOCAL+8)}, REG_LOCAL+7},
			{Z_JNZ, {VALUE(REG_LOCAL+7)}, 0, 1},

			// still unknown after stemming
			{Z_ADD, {VALUE(REG_LOCAL+8), VALUE(REG_LOCAL+9)}, REG_LOCAL+7},
			{Z_DEC, {SMALL(REG_LOCAL+7)}},
			{Z_STORE, {SMALL(REG_LOCAL+5), VALUE(REG_NIL)}},

			{OP_LABEL(11)},
			{Z_LOADB, {VALUE(REG_LOCAL+1), VALUE(REG_LOCAL+7)}, REG_LOCAL+10},
			{Z_JL, {VALUE(REG_LOCAL+10), SMALL(0x30)}, 0, 15},
			{Z_JGE, {VALUE(REG_LOCAL+10), SMALL(0x3a)}, 0, 15},
			{Z_ADD, {VALUE(REG_LOCAL+10), LARGE(0x200-0x30)}, REG_LOCAL+10},

			{OP_LABEL(15)},
			{Z_ADD, {VALUE(REG_LOCAL+10), LARGE(0x3e00)}, REG_LOCAL+10},
			{Z_CALLVS, {ROUTINE(R_PUSH_PAIR_VV), VALUE(REG_LOCAL+10), VALUE(REG_LOCAL+5)}, REG_LOCAL+5},
			{Z_DEC_JGE, {SMALL(REG_LOCAL+7), VALUE(REG_LOCAL+9)}, 0, 11},

			{Z_CALLVS, {ROUTINE(R_PUSH_PAIR_VV), VALUE(REG_LOCAL+5), VALUE(REG_NIL)}, REG_LOCAL+5},
			{Z_OR, {VALUE(REG_LOCAL+5), VALUE(REG_E000)}, REG_LOCAL+5},

			{OP_LABEL(12)},
			{Z_CALLVS, {ROUTINE(R_PUSH_PAIR_VV), VALUE(REG_LOCAL+5), VALUE(REG_LOCAL+4)}, REG_LOCAL+4},
			{Z_JGE, {VALUE(REG_LOCAL+3), SMALL(0)}, 0, 4},

			{OP_LABEL(3)},
			{Z_RET, {VALUE(REG_LOCAL+4)}},
			{Z_END},
		}
	},
	{
		R_TRY_STEMMING,
		4,
			// 0 (param): pointer to text
			// 1 (param): length of text
			// 2: character currently being switched on / temp
			// 3: fake input buffer
			// returns: 0 or pointer to matching dict entry
		(struct zinstr []) {
			// instructions added at compile-time
			{Z_END},
		}
	},
	{
		R_COPY_INPUT_WORD,
		4,
			// 0 (param): pointer to text to copy
			// 1 (param): length of text to copy
			// 2: total number of words to allocate / dest pointer
			// 3: pointer to allocated area (6 bytes parse table, then text buffer)
			// returns: pointer to allocated area (6 bytes parse table, then text buffer)
			// note: doesn't actually allocate, just checks that enough space is available
		(struct zinstr []) {
			{Z_ADD, {VALUE(REG_LOCAL+1), SMALL(6+2+1)}, REG_LOCAL+2},
			{Z_LSHIFT, {VALUE(REG_LOCAL+2), VALUE(REG_FFFF)}, REG_LOCAL+2},

			{Z_ADD, {VALUE(REG_COLL), VALUE(REG_LOCAL+2)}, REG_LOCAL+3},
			{Z_JLE, {VALUE(REG_LOCAL+3), VALUE(REG_TRAIL)}, 0, 2},

			{Z_THROW, {SMALL(FATAL_AUX), VALUE(REG_FATALJMP)}},

			{OP_LABEL(2)},
			{Z_JL, {VALUE(REG_LOCAL+3), VALUE(REG_MINAUX)}, 0, 1},
			{Z_STORE, {SMALL(REG_MINAUX), VALUE(REG_LOCAL+3)}},
			{OP_LABEL(1)},

			{Z_ADD, {VALUE(REG_COLL), VALUE(REG_COLL)}, REG_LOCAL+3},
			{Z_ADD, {VALUE(REG_LOCAL+3), VALUE(REG_AUXBASE)}, REG_LOCAL+3},

			{Z_STOREB, {VALUE(REG_LOCAL+3), SMALL(0), SMALL(1)}},
			{Z_STOREB, {VALUE(REG_LOCAL+3), SMALL(6), SMALL(253)}},
			{Z_STOREB, {VALUE(REG_LOCAL+3), SMALL(7), VALUE(REG_LOCAL+1)}},
			{Z_ADD, {VALUE(REG_LOCAL+3), SMALL(8)}, REG_LOCAL+2},
			{Z_COPY_TABLE, {VALUE(REG_LOCAL+0), VALUE(REG_LOCAL+2), VALUE(REG_LOCAL+1)}},
			{Z_RET, {VALUE(REG_LOCAL+3)}},

			{Z_END},
		}
	},
	{
		R_SPLIT_LIST,
		7,
			// 0 (param): input list
			// 1 (param): end marker
			// 2 (param): output value
			// 3: heap index of first cell
			// 4: heap index of previous cell
			// 5: temp
			// 6: temp
		(struct zinstr []) {
			{Z_CALL2S, {ROUTINE(R_DEREF), VALUE(REG_LOCAL+0)}, REG_LOCAL+0},
			{Z_CALL2S, {ROUTINE(R_DEREF), VALUE(REG_LOCAL+1)}, REG_LOCAL+1},
			{Z_JE, {VALUE(REG_LOCAL+0), VALUE(REG_LOCAL+1)}, 0, 1},

			{Z_LSHIFT, {VALUE(REG_TOP), VALUE(REG_FFFF)}, REG_LOCAL+3},
			{Z_STORE, {SMALL(REG_LOCAL+4), VALUE(REG_LOCAL+3)}},

			{OP_LABEL(2)},
			{Z_ADD, {VALUE(REG_TOP), SMALL(4)}, REG_TOP},
			{Z_JG, {VALUE(REG_TOP), VALUE(REG_ENV)}, 0, 3},
			{Z_JG, {VALUE(REG_TOP), VALUE(REG_CHOICE)}, 0, 3},

			{Z_AND, {VALUE(REG_LOCAL+0), VALUE(REG_NIL)}, REG_LOCAL+5},
			{Z_LOADW, {SMALL(0), VALUE(REG_LOCAL+5)}, REG_LOCAL+6},
			{Z_STOREW, {SMALL(0), VALUE(REG_LOCAL+4), VALUE(REG_LOCAL+6)}},
			{Z_LOADW, {SMALL(2), VALUE(REG_LOCAL+5)}, REG_LOCAL+0},
			{Z_CALL2S, {ROUTINE(R_DEREF), VALUE(REG_LOCAL+0)}, REG_LOCAL+0},
			{Z_JE, {VALUE(REG_LOCAL+0), VALUE(REG_LOCAL+1)}, 0, 5},

			{Z_ADD, {VALUE(REG_LOCAL+4), LARGE(0xc002)}, REG_LOCAL+6},
			{Z_STOREW, {SMALL(2), VALUE(REG_LOCAL+4), VALUE(REG_LOCAL+6)}},
			{Z_ADD, {VALUE(REG_LOCAL+4), SMALL(2)}, REG_LOCAL+4},

			{Z_JUMP, {REL_LABEL(2)}},

			{OP_LABEL(3)},
			{Z_THROW, {SMALL(FATAL_HEAP), VALUE(REG_FATALJMP)}},

			{OP_LABEL(5)},
			{Z_STOREW, {SMALL(2), VALUE(REG_LOCAL+4), VALUE(REG_NIL)}},
			{Z_OR, {VALUE(REG_LOCAL+3), VALUE(REG_C000)}, REG_LOCAL+3},
			{Z_CALLVN, {ROUTINE(R_UNIFY), VALUE(REG_LOCAL+3), VALUE(REG_LOCAL+2)}},
			{Z_RFALSE},

			{OP_LABEL(1)},
			{Z_CALLVN, {VALUE(REG_R_USIMPLE), VALUE(REG_NIL), VALUE(REG_LOCAL+2)}},
			{Z_RFALSE},
			{Z_END},
		}
	},
	{
		R_SPLIT_WORD,
		4,
			// 0 (param): input word
			// 1: temp
			// 2: list accumulator
			// 3: buffer
			// returns list
		(struct zinstr []) {
			{Z_CALL2S, {ROUTINE(R_DEREF), VALUE(REG_LOCAL+0)}, REG_LOCAL+0},
			{Z_TEST, {VALUE(REG_LOCAL+0), VALUE(REG_E000)}, 0, 1},
			{Z_JL, {VALUE(REG_LOCAL+0), VALUE(REG_2000)}, 0, 2},
			{Z_JL, {VALUE(REG_LOCAL+0), LARGE(0x3e00)}, 0, 3},
			{Z_JGE, {VALUE(REG_LOCAL+0), VALUE(REG_4000)}, 0, 4},

			{OP_LABEL(6)},
			// single-character dictionary word (or 0)
			{Z_CALL2S, {ROUTINE(R_PUSH_LIST_V), VALUE(REG_LOCAL+0)}, REG_PUSH},
			{Z_RET_POPPED},

			{OP_LABEL(1)},
			// extended dictionary word
			{Z_AND, {VALUE(REG_LOCAL+0), VALUE(REG_NIL)}, REG_LOCAL+0},
			{Z_LOADW, {SMALL(0), VALUE(REG_LOCAL+0)}, REG_LOCAL+1},
			{Z_JL, {VALUE(REG_LOCAL+1), SMALL(0)}, 0, 5},

			// regular word + ending
			{Z_LOADW, {SMALL(2), VALUE(REG_LOCAL+0)}, REG_LOCAL+2},
			{Z_STORE, {SMALL(REG_LOCAL+0), VALUE(REG_LOCAL+1)}},
			{Z_JUMP, {REL_LABEL(9)}},

			{OP_LABEL(3)},
			// regular dictionary word
			{Z_STORE, {SMALL(REG_LOCAL+2), VALUE(REG_NIL)}},

			{OP_LABEL(9)},
			// prepend chars to list
			{Z_STORE, {SMALL(REG_LOCAL+3), REF(G_SCRATCH)}},
			{Z_AND, {VALUE(REG_LOCAL+0), VALUE(REG_NIL)}, REG_LOCAL+0},
			{Z_MUL, {VALUE(REG_LOCAL+0), SMALL(6)}, REG_LOCAL+0},
			{Z_ADD, {VALUE(REG_LOCAL+0), REF(G_DICT_TABLE)}, REG_LOCAL+0},

			{Z_OUTPUT_STREAM, {SMALL(3), VALUE(REG_LOCAL+3)}},
			{Z_PRINTADDR, {VALUE(REG_LOCAL+0)}},
			{Z_OUTPUT_STREAM, {LARGE(0x10000-3)}},

			{Z_LOADW, {VALUE(REG_LOCAL+3), SMALL(0)}, REG_LOCAL+1},
			{Z_INC, {SMALL(REG_LOCAL+3)}},

			{OP_LABEL(10)},
			{Z_LOADB, {VALUE(REG_LOCAL+3), VALUE(REG_LOCAL+1)}, REG_LOCAL+0},
			{Z_JG, {VALUE(REG_LOCAL+0), SMALL(0x39)}, 0, 12},
			{Z_JL, {VALUE(REG_LOCAL+0), SMALL(0x30)}, 0, 12},

			{Z_ADD, {VALUE(REG_LOCAL+0), VALUE(0x200-0x30)}, REG_LOCAL+0},

			{OP_LABEL(12)},
			{Z_ADD, {VALUE(REG_LOCAL+0), LARGE(0x3e00)}, REG_LOCAL+0},
			{Z_CALLVS, {ROUTINE(R_PUSH_PAIR_VV), VALUE(REG_LOCAL+0), VALUE(REG_LOCAL+2)}, REG_LOCAL+2},
			{Z_DEC_JGE, {SMALL(REG_LOCAL+1), SMALL(1)}, 0, 10},

			{Z_RET, {VALUE(REG_LOCAL+2)}},

			{OP_LABEL(5)},
			// unknown word
			{Z_RET, {VALUE(REG_LOCAL+1)}},

			{OP_LABEL(2)},
			{Z_THROW, {SMALL(0), VALUE(REG_FAILJMP)}},

			{OP_LABEL(4)},
			// integer
			{Z_AND, {VALUE(REG_LOCAL+0), VALUE(REG_3FFF)}, REG_LOCAL+1},
			{Z_JZ, {VALUE(REG_LOCAL+1)}, 0, 6},

			{Z_STORE, {SMALL(REG_LOCAL+2), VALUE(REG_NIL)}},
			{OP_LABEL(7)},
			{Z_MOD, {VALUE(REG_LOCAL+1), SMALL(10)}, REG_LOCAL+0},
			{Z_OR, {VALUE(REG_LOCAL+0), VALUE(REG_4000)}, REG_LOCAL+0},
			{Z_CALLVS, {ROUTINE(R_PUSH_PAIR_VV), VALUE(REG_LOCAL+0), VALUE(REG_LOCAL+2)}, REG_LOCAL+2},
			{Z_DIV, {VALUE(REG_LOCAL+1), SMALL(10)}, REG_LOCAL+1},
			{Z_JNZ, {VALUE(REG_LOCAL+1)}, 0, 7},

			{Z_RET, {VALUE(REG_LOCAL+2)}},
			{Z_END},
		}
	},
	{
		R_JOIN_WORDS,
		4,
			// 0 (param): input list
			// 1: temp
			// 2: buffer
			// 3: temp, saved REG_COLL
			// returns word
		(struct zinstr []) {
			{Z_CALL2S, {ROUTINE(R_DEREF), VALUE(REG_LOCAL+0)}, REG_LOCAL+0},
			{Z_AND, {VALUE(REG_LOCAL+0), VALUE(REG_E000)}, REG_LOCAL+1},
			{Z_JNE, {VALUE(REG_LOCAL+1), VALUE(REG_C000)}, 0, 1},

			{Z_AND, {VALUE(REG_LOCAL+0), VALUE(REG_NIL)}, REG_LOCAL+1},
			{Z_LOADW, {SMALL(2), VALUE(REG_LOCAL+1)}, REG_LOCAL+3},
			{Z_CALL2S, {ROUTINE(R_DEREF), VALUE(REG_LOCAL+3)}, REG_LOCAL+3},
			{Z_JNE, {VALUE(REG_LOCAL+3), VALUE(REG_NIL)}, 0, 3},

			{Z_LOADW, {SMALL(0), VALUE(REG_LOCAL+1)}, REG_LOCAL+3},
			{Z_CALL2S, {ROUTINE(R_DEREF), VALUE(REG_LOCAL+3)}, REG_LOCAL+3},
			{Z_JL, {VALUE(REG_LOCAL+3), LARGE(0x3e00)}, 0, 3},
			{Z_JGE, {VALUE(REG_LOCAL+3), VALUE(REG_4000)}, 0, 3},

			{Z_RET, {VALUE(REG_LOCAL+3)}},

			{OP_LABEL(3)},
			{Z_STORE, {SMALL(REG_LOCAL+3), VALUE(REG_COLL)}},
			{Z_CALL2S, {ROUTINE(R_AUX_ALLOC), SMALL(1+134+1+2*1)}, REG_LOCAL+2},
			{Z_STOREB, {VALUE(REG_LOCAL+2), SMALL(0), SMALL(2*134)}},
			{Z_ADD, {VALUE(REG_LOCAL+2), SMALL(2)}, REG_LOCAL+1},
			{Z_CALLVS, {ROUTINE(R_JOIN_WORDS_SUB), VALUE(REG_LOCAL+0), LARGE(2*134), VALUE(REG_LOCAL+1)}, REG_LOCAL+1},
			{Z_JNZ, {VALUE(REG_LOCAL+1)}, 0, 2},

			{Z_STORE, {SMALL(REG_COLL), VALUE(REG_LOCAL+3)}},

			{OP_LABEL(1)},
			{Z_THROW, {SMALL(0), VALUE(REG_FAILJMP)}},

			{OP_LABEL(2)},
			{Z_STOREB, {VALUE(REG_LOCAL+2), SMALL(1), VALUE(REG_LOCAL+1)}},
			{Z_ADD, {VALUE(REG_LOCAL+2), LARGE(2+2*134)}, REG_LOCAL+1},
			{Z_STOREB, {VALUE(REG_LOCAL+1), SMALL(0), SMALL(1)}},
			{Z_TOKENISE, {VALUE(REG_LOCAL+2), VALUE(REG_LOCAL+1)}},
			{Z_CALLVS, {ROUTINE(R_PARSE_INPUT), VALUE(REG_LOCAL+1), VALUE(REG_LOCAL+2)}, REG_LOCAL+1},
			{Z_STORE, {SMALL(REG_COLL), VALUE(REG_LOCAL+3)}},

			{Z_CALL2S, {ROUTINE(R_DEREF), VALUE(REG_LOCAL+1)}, REG_LOCAL+1},
			{Z_JE, {VALUE(REG_LOCAL+1), VALUE(REG_NIL)}, 0, 1},
			{Z_AND, {VALUE(REG_LOCAL+1), VALUE(REG_NIL)}, REG_LOCAL+1},
			{Z_LOADW, {SMALL(0), VALUE(REG_LOCAL+1)}, REG_PUSH},
			{Z_RET_POPPED},
			{Z_END},
		}
	},
	{
		R_JOIN_WORDS_SUB,
		6,
			// 0 (param): input list, deref'd
			// 1 (param): buffer size (decremented)
			// 2 (param): buffer pointer (incremented)
			// 3: list element, temp
			// 4: original buffer pointer
			// 5: temp
			// returns length or 0 on error
		(struct zinstr []) {
			{Z_STORE, {SMALL(REG_LOCAL+4), VALUE(REG_LOCAL+2)}},

			{OP_LABEL(2)},
			{Z_AND, {VALUE(REG_LOCAL+0), VALUE(REG_NIL)}, REG_LOCAL+0},
			{Z_LOADW, {SMALL(0), VALUE(REG_LOCAL+0)}, REG_LOCAL+3},
			{Z_CALL2S, {ROUTINE(R_DEREF), VALUE(REG_LOCAL+3)}, REG_LOCAL+3},

			{Z_JGE, {VALUE(REG_LOCAL+3), VALUE(REG_4000)}, 0, 3},

			{Z_JL, {VALUE(REG_LOCAL+3), LARGE(0x3e00)}, 0, 4},
			// single-character word
			{Z_JZ, {VALUE(REG_LOCAL+1)}, 0, RFALSE},
			{Z_AND, {VALUE(REG_LOCAL+3), SMALL(0xff)}, REG_LOCAL+3},
			{Z_JLE, {VALUE(REG_LOCAL+3), SMALL(0x20)}, 0, RFALSE},
			{Z_JE, {VALUE(REG_LOCAL+3), SMALL('.'), SMALL(','), SMALL('\"')}, 0, RFALSE},
			{Z_JE, {VALUE(REG_LOCAL+3), SMALL(';'), SMALL('*')}, 0, RFALSE},
			{Z_JE, {VALUE(REG_LOCAL+3), SMALL('('), SMALL(')')}, 0, RFALSE},
			{Z_STOREB, {VALUE(REG_LOCAL+2), SMALL(0), VALUE(REG_LOCAL+3)}},
			{Z_DEC, {SMALL(REG_LOCAL+1)}},
			{Z_INC, {SMALL(REG_LOCAL+2)}},
			{Z_JUMP, {REL_LABEL(6)}},

			{OP_LABEL(4)},
			{Z_TESTN, {VALUE(REG_LOCAL+3), VALUE(REG_E000)}, 0, 5},
			// extended word
			{Z_AND, {VALUE(REG_LOCAL+3), VALUE(REG_NIL)}, REG_LOCAL+3},
			{Z_LOADW, {SMALL(0), VALUE(REG_LOCAL+3)}, REG_LOCAL+5},
			{Z_JL, {VALUE(REG_LOCAL+5), SMALL(0)}, 0, 8},

			{Z_STORE, {SMALL(REG_LOCAL+5), VALUE(REG_LOCAL+3)}},

			{OP_LABEL(8)},
			{Z_CALLVS, {ROUTINE(R_JOIN_WORDS_SUB), VALUE(REG_LOCAL+5), VALUE(REG_LOCAL+1), VALUE(REG_LOCAL+2)}, REG_LOCAL+5},
			{Z_JZ, {VALUE(REG_LOCAL+5)}, 0, RFALSE},
			{Z_ADD, {VALUE(REG_LOCAL+2), VALUE(REG_LOCAL+5)}, REG_LOCAL+2},
			{Z_SUB, {VALUE(REG_LOCAL+1), VALUE(REG_LOCAL+5)}, REG_LOCAL+1},
			{Z_JUMP, {REL_LABEL(6)}},

			{OP_LABEL(5)},
			{Z_JL, {VALUE(REG_LOCAL+3), VALUE(REG_2000)}, 0, RFALSE},
			// regular word
			{Z_JL, {VALUE(REG_LOCAL+1), SMALL(12)}, 0, RFALSE},
			{Z_AND, {VALUE(REG_LOCAL+3), VALUE(REG_NIL)}, REG_LOCAL+3},
			{Z_MUL, {VALUE(REG_LOCAL+3), SMALL(6)}, REG_LOCAL+3},
			{Z_ADD, {VALUE(REG_LOCAL+3), REF(G_DICT_TABLE)}, REG_LOCAL+3},
			{Z_OUTPUT_STREAM, {SMALL(3), VALUE(REG_LOCAL+2)}},
			{Z_PRINTADDR, {VALUE(REG_LOCAL+3)}},

			{OP_LABEL(7)},
			{Z_OUTPUT_STREAM, {LARGE(0x10000-3)}},
			{Z_LOADW, {VALUE(REG_LOCAL+2), SMALL(0)}, REG_LOCAL+3},
			{Z_ADD, {VALUE(REG_LOCAL+2), SMALL(2)}, REG_LOCAL+5},
			{Z_COPY_TABLE, {VALUE(REG_LOCAL+5), VALUE(REG_LOCAL+2), VALUE(REG_LOCAL+3)}},
			{Z_ADD, {VALUE(REG_LOCAL+2), VALUE(REG_LOCAL+3)}, REG_LOCAL+2},
			{Z_SUB, {VALUE(REG_LOCAL+1), VALUE(REG_LOCAL+3)}, REG_LOCAL+1},

			{OP_LABEL(6)},
			{Z_LOADW, {SMALL(2), VALUE(REG_LOCAL+0)}, REG_LOCAL+0},
			{Z_CALL2S, {ROUTINE(R_DEREF), VALUE(REG_LOCAL+0)}, REG_LOCAL+0},
			{Z_AND, {VALUE(REG_LOCAL+0), VALUE(REG_E000)}, REG_LOCAL+3},
			{Z_JE, {VALUE(REG_LOCAL+3), VALUE(REG_C000)}, 0, 2},

			{Z_JNE, {VALUE(REG_LOCAL+0), VALUE(REG_NIL)}, 0, RFALSE},

			{Z_SUB, {VALUE(REG_LOCAL+2), VALUE(REG_LOCAL+4)}, REG_PUSH},
			{Z_RET_POPPED},

			{OP_LABEL(3)},
			// number
			{Z_JL, {VALUE(REG_LOCAL+1), SMALL(8)}, 0, RFALSE},
			{Z_AND, {VALUE(REG_LOCAL+3), VALUE(REG_3FFF)}, REG_LOCAL+3},
			{Z_OUTPUT_STREAM, {SMALL(3), VALUE(REG_LOCAL+2)}},
			{Z_PRINTNUM, {VALUE(REG_LOCAL+3)}},
			{Z_JUMP, {REL_LABEL(7)}},
			{Z_END},
		}
	},
	{
		R_COLLECT_BEGIN,
		0,
		(struct zinstr []) {
			{Z_JL, {VALUE(REG_COLL), VALUE(REG_TRAIL)}, 0, 1},

			{Z_THROW, {SMALL(FATAL_AUX), VALUE(REG_FATALJMP)}},

			{OP_LABEL(1)},
			{Z_STOREW, {VALUE(REG_AUXBASE), VALUE(REG_COLL), SMALL(0)}},
			{Z_INC, {SMALL(REG_COLL)}},
			{Z_RFALSE},
			{Z_END},
		}
	},
	{
		R_COLLECT_PUSH,
		3,
			// 0 (param): tagged reference to push on aux stack
			// 1: temp
			// 2: element count
			// Simple elements (including the empty list) are serialized as themselves.
			// Variables are serialized as 8000.
			// Proper lists are serialized as n elements, followed by c000+n.
			// Improper lists are serialized as n elements, followed by the improper tail element, followed by e000+n.
			// Extended dictionary words are serialized as the optional part, followed by the mandatory part,
			// followed by 9000.
		(struct zinstr []) {
			{Z_JGE, {VALUE(REG_LOCAL+0), VALUE(REG_C000)}, 0, 2},
			{OP_LABEL(1)},
			{Z_ADD, {VALUE(REG_LOCAL+0), VALUE(REG_LOCAL+0)}, REG_LOCAL+1},
			{Z_LOADW, {VALUE(REG_LOCAL+1), SMALL(0)}, REG_LOCAL+1},
			{Z_JZ, {VALUE(REG_LOCAL+1)}, 0, 5},
			{Z_STORE, {SMALL(REG_LOCAL+0), VALUE(REG_LOCAL+1)}},
			{Z_JL, {VALUE(REG_LOCAL+0), VALUE(REG_C000)}, 0, 1},

			{OP_LABEL(2)},
			{Z_JGE, {VALUE(REG_LOCAL+0), SMALL(0)}, 0, 4},

			{Z_JL, {VALUE(REG_LOCAL+0), VALUE(REG_E000)}, 0, 11},
			// extended dictionary word
			{Z_AND, {VALUE(REG_LOCAL+0), VALUE(REG_NIL)}, REG_LOCAL+0},
			{Z_LOADW, {SMALL(2), VALUE(REG_LOCAL+0)}, REG_LOCAL+1},
			{Z_CALL2N, {ROUTINE(R_COLLECT_PUSH), VALUE(REG_LOCAL+1)}},
			{Z_LOADW, {SMALL(0), VALUE(REG_LOCAL+0)}, REG_LOCAL+1},
			{Z_CALL2N, {ROUTINE(R_COLLECT_PUSH), VALUE(REG_LOCAL+1)}},
			{Z_STORE, {SMALL(REG_LOCAL+0), LARGE(0x9000)}},
			{Z_JUMP, {REL_LABEL(4)}},

			{OP_LABEL(11)},
			// pair
			{Z_STORE, {SMALL(REG_LOCAL+2), VALUE(REG_C000)}},

			{OP_LABEL(6)},
			{Z_INC, {SMALL(REG_LOCAL+2)}},

			{Z_SUB, {VALUE(REG_LOCAL+0), VALUE(REG_4000)}, REG_LOCAL+0},	// ref to head element
			{Z_CALL2N, {ROUTINE(R_COLLECT_PUSH), VALUE(REG_LOCAL+0)}},
			{Z_INC, {SMALL(REG_LOCAL+0)}},					// ref to tail element

			// deref the tail cell
			{Z_JGE, {VALUE(REG_LOCAL+0), VALUE(REG_C000)}, 0, 9},
			{OP_LABEL(8)},
			{Z_ADD, {VALUE(REG_LOCAL+0), VALUE(REG_LOCAL+0)}, REG_LOCAL+1},
			{Z_LOADW, {VALUE(REG_LOCAL+1), SMALL(0)}, REG_LOCAL+1},
			{Z_JZ, {VALUE(REG_LOCAL+1)}, 0, 7},
			{Z_STORE, {SMALL(REG_LOCAL+0), VALUE(REG_LOCAL+1)}},
			{Z_JL, {VALUE(REG_LOCAL+0), VALUE(REG_C000)}, 0, 8},

			{OP_LABEL(9)},
			{Z_JL, {VALUE(REG_LOCAL+0), SMALL(0)}, 0, 6},

			{Z_JE, {VALUE(REG_LOCAL+0), VALUE(REG_NIL)}, 0, 10},

			{OP_LABEL(7)},
			// improper tail
			{Z_OR, {VALUE(REG_LOCAL+2), VALUE(REG_2000)}, REG_LOCAL+2},
			{Z_CALL2N, {ROUTINE(R_COLLECT_PUSH), VALUE(REG_LOCAL+0)}},

			{OP_LABEL(10)},
			{Z_JGE, {VALUE(REG_COLL), VALUE(REG_TRAIL)}, 0, 3},
			{Z_STOREW, {VALUE(REG_AUXBASE), VALUE(REG_COLL), VALUE(REG_LOCAL+2)}},
			{Z_INC, {SMALL(REG_COLL)}},
			{Z_RFALSE},

			{OP_LABEL(3)},
			{Z_THROW, {SMALL(FATAL_AUX), VALUE(REG_FATALJMP)}},

			{OP_LABEL(5)},
			// unbound variable
			{Z_STORE, {SMALL(REG_LOCAL+0), VALUE(REG_8000)}},

			{OP_LABEL(4)},
			// simple value
			{Z_JGE, {VALUE(REG_COLL), VALUE(REG_TRAIL)}, 0, 3},
			{Z_STOREW, {VALUE(REG_AUXBASE), VALUE(REG_COLL), VALUE(REG_LOCAL+0)}},
			{Z_INC, {SMALL(REG_COLL)}},
			{Z_RFALSE},
			{Z_END},
		}
	},
	{
		R_COLLECT_POP,
		4,
			// 0: popped value
			// 1: element counter
			// 2: pointer to new pair
			// 3: list accumulator
		(struct zinstr []) {
			{Z_DEC, {SMALL(REG_COLL)}},
			{Z_LOADW, {VALUE(REG_AUXBASE), VALUE(REG_COLL)}, REG_LOCAL+0},
			{Z_JGE, {VALUE(REG_LOCAL+0), SMALL(0)}, 0, 1},

			{Z_JE, {VALUE(REG_LOCAL+0), VALUE(REG_8000)}, 0, 2},
			{Z_JE, {VALUE(REG_LOCAL+0), LARGE(0x9000)}, 0, 4},

			{Z_AND, {VALUE(REG_LOCAL+0), VALUE(REG_NIL)}, REG_LOCAL+1},	// nil = 1fff
			{Z_STORE, {SMALL(REG_LOCAL+3), VALUE(REG_NIL)}},
			{Z_TESTN, {VALUE(REG_LOCAL+0), VALUE(REG_2000)}, 0, 3},

			// improper list
			{Z_CALL1S, {ROUTINE(R_COLLECT_POP)}, REG_LOCAL+3},

			{OP_LABEL(3)},
			{Z_CALL1S, {ROUTINE(R_COLLECT_POP)}, REG_LOCAL+0},
			{Z_CALLVS, {ROUTINE(R_PUSH_PAIR_VV), VALUE(REG_LOCAL+0), VALUE(REG_LOCAL+3)}, REG_LOCAL+3},
			{Z_DEC_JGE, {SMALL(REG_LOCAL+1), SMALL(1)}, 0, 3},

			{Z_RET, {VALUE(REG_LOCAL+3)}},

			{OP_LABEL(4)},
			// extended dictionary word
			{Z_CALL1S, {ROUTINE(R_COLLECT_POP)}, REG_LOCAL+0},
			{Z_CALL1S, {ROUTINE(R_COLLECT_POP)}, REG_LOCAL+1},
			{Z_CALLVS, {ROUTINE(R_PUSH_PAIR_VV), VALUE(REG_LOCAL+0), VALUE(REG_LOCAL+1)}, REG_LOCAL+0},
			{Z_OR, {VALUE(REG_LOCAL+0), VALUE(REG_E000)}, REG_LOCAL+0},

			{OP_LABEL(1)},
			// simple value (or null)
			{Z_RET, {VALUE(REG_LOCAL+0)}},

			{OP_LABEL(2)},
			// unbound variable
			{Z_CALL1S, {ROUTINE(R_PUSH_VAR)}, REG_PUSH},
			{Z_RET_POPPED},
			{Z_END},
		}
	},
	{
		R_COLLECT_END,
		2,
			// 0: list accumulator
			// 1: popped value
			// returns tagged reference to list
		(struct zinstr []) {
			{Z_STORE, {SMALL(REG_LOCAL+0), VALUE(REG_NIL)}},

			{OP_LABEL(1)},
			{Z_CALL1S, {ROUTINE(R_COLLECT_POP)}, REG_LOCAL+1},
			{Z_JZ, {VALUE(REG_LOCAL+1)}, 0, 2},

			{Z_CALLVS, {ROUTINE(R_PUSH_PAIR_VV), VALUE(REG_LOCAL+1), VALUE(REG_LOCAL+0)}, REG_LOCAL+0},
			{Z_JUMP, {REL_LABEL(1)}},

			{OP_LABEL(2)},
			{Z_RET, {VALUE(REG_LOCAL+0)}},
			{Z_END},
		}
	},
	{
		R_COLLECT_CHECK,
		3,
			// 0 (param): deref'd value to check for
			// 1: popped value
			// 2: flag
		(struct zinstr []) {
			{Z_STORE, {SMALL(REG_LOCAL+2), SMALL(0)}},

			{OP_LABEL(1)},
			{Z_DEC, {SMALL(REG_COLL)}},
			{Z_LOADW, {VALUE(REG_AUXBASE), VALUE(REG_COLL)}, REG_LOCAL+1},
			{Z_JZ, {VALUE(REG_LOCAL+1)}, 0, 2},
			{Z_JNE, {VALUE(REG_LOCAL+1), VALUE(REG_LOCAL+0)}, 0, 1},

			{Z_STORE, {SMALL(REG_LOCAL+2), SMALL(1)}},
			{Z_JUMP, {REL_LABEL(1)}},

			{OP_LABEL(2)},
			{Z_RET, {VALUE(REG_LOCAL+2)}},
			{Z_END},
		}
	},
	{
		R_COLLECT_MATCH_ALL,
		7,
			// 0 (param): tagged reference to input list
			// 1: current input word
			// 2: keyword list
			// 3: saved REG_TOP
			// 4: keyword iterator
			// 5: current keyword
			// 6: temp
		(struct zinstr []) {
			{Z_STORE, {SMALL(REG_LOCAL+3), VALUE(REG_TOP)}},
			{Z_CALL1S, {ROUTINE(R_COLLECT_END)}, REG_LOCAL+2},

			{OP_LABEL(2)},
			{Z_CALL2S, {ROUTINE(R_DEREF), VALUE(REG_LOCAL+0)}, REG_LOCAL+0},
			{Z_AND, {VALUE(REG_LOCAL+0), VALUE(REG_E000)}, REG_LOCAL+6},
			{Z_JNE, {VALUE(REG_LOCAL+6), VALUE(REG_C000)}, 0, 3},

			{Z_SUB, {VALUE(REG_LOCAL+0), VALUE(REG_4000)}, REG_LOCAL+1},	// ref to head element
			{Z_ADD, {VALUE(REG_LOCAL+1), SMALL(1)}, REG_LOCAL+0},		// ref to tail element
			{Z_CALL2S, {ROUTINE(R_DEREF), VALUE(REG_LOCAL+1)}, REG_LOCAL+1},

			{Z_JE, {VALUE(REG_LOCAL+2), VALUE(REG_NIL)}, 0, 1},
			{Z_STORE, {SMALL(REG_LOCAL+4), VALUE(REG_LOCAL+2)}},

			{OP_LABEL(4)},
			{Z_AND, {VALUE(REG_LOCAL+4), VALUE(REG_NIL)}, REG_LOCAL+4},
			{Z_LOADW, {SMALL(0), VALUE(REG_LOCAL+4)}, REG_LOCAL+5},
			{Z_LOADW, {SMALL(2), VALUE(REG_LOCAL+4)}, REG_LOCAL+4},

			{Z_CALLVS, {ROUTINE(R_WOULD_UNIFY), VALUE(REG_LOCAL+5), VALUE(REG_LOCAL+1)}, REG_LOCAL+6},
			{Z_JNZ, {VALUE(REG_LOCAL+6)}, 0, 2},
			{Z_JNE, {VALUE(REG_LOCAL+4), VALUE(REG_NIL)}, 0, 4},

			{OP_LABEL(1)},
			{Z_THROW, {SMALL(0), VALUE(REG_FAILJMP)}},

			{OP_LABEL(3)},
			{Z_STORE, {SMALL(REG_TOP), VALUE(REG_LOCAL+3)}},
			{Z_JNE, {VALUE(REG_LOCAL+0), VALUE(REG_NIL)}, 0, 1},
			{Z_RFALSE},
			{Z_END},
		}
	},
	{
		R_WOULD_UNIFY,
		3,
			// 0 (param): first value
			// 1 (param): second value
			// 2: temp
			// returns true or false
		(struct zinstr []) {
			{OP_LABEL(2)},

			// dereference first
			{Z_JGE, {VALUE(REG_LOCAL+0), VALUE(REG_C000)}, 0, 9},
			{OP_LABEL(8)},
			{Z_ADD, {VALUE(REG_LOCAL+0), VALUE(REG_LOCAL+0)}, REG_LOCAL+0},
			{Z_LOADW, {VALUE(REG_LOCAL+0), SMALL(0)}, REG_LOCAL+0},
			{Z_JL, {VALUE(REG_LOCAL+0), VALUE(REG_C000)}, 0, 8},
			{Z_JZ, {VALUE(REG_LOCAL+0)}, 0, RTRUE},
			{OP_LABEL(9)},

			// dereference second
			{Z_JGE, {VALUE(REG_LOCAL+1), VALUE(REG_C000)}, 0, 11},
			{OP_LABEL(10)},
			{Z_ADD, {VALUE(REG_LOCAL+1), VALUE(REG_LOCAL+1)}, REG_LOCAL+1},
			{Z_LOADW, {VALUE(REG_LOCAL+1), SMALL(0)}, REG_LOCAL+1},
			{Z_JL, {VALUE(REG_LOCAL+1), VALUE(REG_C000)}, 0, 10},
			{Z_JZ, {VALUE(REG_LOCAL+1)}, 0, RTRUE},
			{OP_LABEL(11)},

			{OP_LABEL(7)},
			{Z_JE, {VALUE(REG_LOCAL+0), VALUE(REG_LOCAL+1)}, 0, RTRUE},
			{Z_JL, {VALUE(REG_LOCAL+0), VALUE(REG_C000)}, 0, RTRUE},
			{Z_JL, {VALUE(REG_LOCAL+1), VALUE(REG_C000)}, 0, RTRUE},

			{Z_TESTN, {VALUE(REG_LOCAL+0), VALUE(REG_E000)}, 0, 5},
			// first is an extended dictionary word
			{Z_AND, {VALUE(REG_LOCAL+0), VALUE(REG_NIL)}, REG_LOCAL+2},
			{Z_LOADW, {SMALL(0), VALUE(REG_LOCAL+2)}, REG_LOCAL+2},
			{Z_JL, {VALUE(REG_LOCAL+2), SMALL(0)}, 0, 4},

			// with a regular dictionary word as its mandatory part
			{Z_TESTN, {VALUE(REG_LOCAL+1), VALUE(REG_E000)}, 0, 3},
			{Z_AND, {VALUE(REG_LOCAL+1), VALUE(REG_NIL)}, REG_LOCAL+1},
			{Z_LOADW, {SMALL(0), VALUE(REG_LOCAL+1)}, REG_LOCAL+1},

			{OP_LABEL(3)},
			{Z_JNE, {VALUE(REG_LOCAL+2), VALUE(REG_LOCAL+1)}, 0, RFALSE},
			{Z_RTRUE},

			{OP_LABEL(4)},
			// first is an extended dictionary word with a mandatory character list
			// fail if second is not, otherwise unbox and retry
			{Z_TESTN, {VALUE(REG_LOCAL+1), VALUE(REG_E000)}, 0, RFALSE},
			{Z_STORE, {SMALL(REG_LOCAL+0), VALUE(REG_LOCAL+2)}},
			{Z_AND, {VALUE(REG_LOCAL+1), VALUE(REG_NIL)}, REG_LOCAL+1},
			{Z_LOADW, {SMALL(0), VALUE(REG_LOCAL+1)}, REG_LOCAL+1},
			{Z_JUMP, {REL_LABEL(7)}},

			{OP_LABEL(5)},
			{Z_TESTN, {VALUE(REG_LOCAL+1), VALUE(REG_E000)}, 0, 1},
			// second is an extended dictionary word
			{Z_AND, {VALUE(REG_LOCAL+1), VALUE(REG_NIL)}, REG_LOCAL+2},
			{Z_LOADW, {SMALL(0), VALUE(REG_LOCAL+2)}, REG_LOCAL+2},
			{Z_JNE, {VALUE(REG_LOCAL+0), VALUE(REG_LOCAL+2)}, 0, RFALSE},
			{Z_RTRUE},

			{OP_LABEL(1)},

			// mismatching simple value? then fail
			{Z_JGE, {VALUE(REG_LOCAL+0), SMALL(0)}, 0, RFALSE},
			{Z_JGE, {VALUE(REG_LOCAL+1), SMALL(0)}, 0, RFALSE},

			// both are pairs
			{Z_SUB, {VALUE(REG_LOCAL+0), VALUE(REG_4000)}, REG_LOCAL+0},	// pair ref -> first var ref
			{Z_SUB, {VALUE(REG_LOCAL+1), VALUE(REG_4000)}, REG_LOCAL+1},	// pair ref -> first var ref
			{Z_CALLVS, {ROUTINE(R_WOULD_UNIFY), VALUE(REG_LOCAL+0), VALUE(REG_LOCAL+1)}, REG_LOCAL+2},
			{Z_JZ, {VALUE(REG_LOCAL+2)}, 0, RFALSE},
			{Z_INC, {SMALL(REG_LOCAL+0)}},					// second var ref
			{Z_INC, {SMALL(REG_LOCAL+1)}},					// second var ref
			{Z_JUMP, {REL_LABEL(2)}},
			{Z_END},
		}
	},
	{
		R_ACCUM_BEGIN,
		0,
		(struct zinstr []) {
			{Z_JL, {VALUE(REG_COLL), VALUE(REG_TRAIL)}, 0, 1},

			{Z_THROW, {SMALL(FATAL_AUX), VALUE(REG_FATALJMP)}},

			{OP_LABEL(1)},
			{Z_STOREW, {VALUE(REG_AUXBASE), VALUE(REG_COLL), VALUE(REG_4000)}},
			{Z_INC, {SMALL(REG_COLL)}},
			{Z_RFALSE},
			{Z_END},
		}
	},
	{
		R_ACCUM_INC,
		0,
		(struct zinstr []) {
			{Z_CALL2N, {ROUTINE(R_ACCUM_ADD), LARGE(0x4001)}},
			{Z_RFALSE},
			{Z_END},
		}
	},
	{
		R_ACCUM_ADD,
		2,
			// 0 (param): value to accumulate
			// 1: sum
		(struct zinstr []) {
			{Z_CALL1S, {ROUTINE(R_COLLECT_POP)}, REG_LOCAL+1},
			{Z_JZ, {VALUE(REG_LOCAL+1)}, 0, 1},

			{Z_CALL2S, {ROUTINE(R_DEREF), VALUE(REG_LOCAL+0)}, REG_LOCAL+0},
			{Z_JL, {VALUE(REG_LOCAL+0), VALUE(REG_4000)}, 0, 2},

			{Z_AND, {VALUE(REG_LOCAL+1), VALUE(REG_3FFF)}, REG_LOCAL+1},
			{Z_ADD, {VALUE(REG_LOCAL+0), VALUE(REG_LOCAL+1)}, REG_LOCAL+1},
			{Z_JGE, {VALUE(REG_LOCAL+1), VALUE(REG_4000)}, 0, 1},

			{OP_LABEL(2)},
			{Z_STORE, {SMALL(REG_LOCAL+1), SMALL(0)}},

			{OP_LABEL(1)},
			{Z_STOREW, {VALUE(REG_AUXBASE), VALUE(REG_COLL), VALUE(REG_LOCAL+1)}},
			{Z_INC, {SMALL(REG_COLL)}},
			{Z_RFALSE},
			{Z_END},
		}
	},
	{
		R_ACCUM_END,
		1,
			// 0: popped value
			// returns value
		(struct zinstr []) {
			{Z_CALL1S, {ROUTINE(R_COLLECT_POP)}, REG_LOCAL+0},
			{Z_JNZ, {VALUE(REG_LOCAL+0)}, 0, 1},

			{Z_THROW, {SMALL(0), VALUE(REG_FAILJMP)}},

			{OP_LABEL(1)},
			{Z_RET, {VALUE(REG_LOCAL+0)}},
			{Z_END},
		}
	},
	{
		R_AUX_ALLOC,
		2,
			// 0 (param): number of words to allocate
			// 1: temp
			// returns pointer to allocated area
		(struct zinstr []) {
			{Z_ADD, {VALUE(REG_COLL), VALUE(REG_COLL)}, REG_LOCAL+1},
			{Z_ADD, {VALUE(REG_LOCAL+1), VALUE(REG_AUXBASE)}, REG_LOCAL+1},
			{Z_ADD, {VALUE(REG_COLL), VALUE(REG_LOCAL+0)}, REG_COLL},
			{Z_JLE, {VALUE(REG_COLL), VALUE(REG_TRAIL)}, 0, 2},

			{Z_THROW, {SMALL(FATAL_AUX), VALUE(REG_FATALJMP)}},

			{OP_LABEL(2)},
			{Z_JL, {VALUE(REG_COLL), VALUE(REG_MINAUX)}, 0, 1},
			{Z_STORE, {SMALL(REG_MINAUX), VALUE(REG_COLL)}},

			{OP_LABEL(1)},
			{Z_RET, {VALUE(REG_LOCAL+1)}},
			{Z_END},
		}
	},
	{
		R_AUX_PUSH1,
		1,
			// 0 (param): word to push
		(struct zinstr []) {
			{Z_JL, {VALUE(REG_COLL), VALUE(REG_TRAIL)}, 0, 2},

			{Z_THROW, {SMALL(FATAL_AUX), VALUE(REG_FATALJMP)}},

			{OP_LABEL(2)},
			{Z_STOREW, {VALUE(REG_AUXBASE), VALUE(REG_COLL), VALUE(REG_LOCAL+0)}},
			{Z_INC, {SMALL(REG_COLL)}},
			{Z_RFALSE},
			{Z_END},
		}
	},
	{
		R_AUX_PUSH2,
		3,
			// 0 (param): first word to push
			// 1 (param): second word to push
			// 2: temp
		(struct zinstr []) {
			{Z_STORE, {SMALL(REG_LOCAL+2), VALUE(REG_COLL)}},
			{Z_ADD, {VALUE(REG_COLL), SMALL(2)}, REG_COLL},
			{Z_JLE, {VALUE(REG_COLL), VALUE(REG_TRAIL)}, 0, 2},

			{Z_THROW, {SMALL(FATAL_AUX), VALUE(REG_FATALJMP)}},

			{OP_LABEL(2)},
			{Z_STOREW, {VALUE(REG_AUXBASE), VALUE(REG_LOCAL+2), VALUE(REG_LOCAL+0)}},
			{Z_INC, {SMALL(REG_LOCAL+2)}},
			{Z_STOREW, {VALUE(REG_AUXBASE), VALUE(REG_LOCAL+2), VALUE(REG_LOCAL+1)}},
			{Z_RFALSE},
			{Z_END},
		}
	},
	{
		R_AUX_PUSH3,
		4,
			// 0 (param): first word to push
			// 1 (param): second word to push
			// 2 (param): third word to push
			// 3: temp
		(struct zinstr []) {
			{Z_STORE, {SMALL(REG_LOCAL+3), VALUE(REG_COLL)}},
			{Z_ADD, {VALUE(REG_COLL), SMALL(3)}, REG_COLL},
			{Z_JLE, {VALUE(REG_COLL), VALUE(REG_TRAIL)}, 0, 2},

			{Z_THROW, {SMALL(FATAL_AUX), VALUE(REG_FATALJMP)}},

			{OP_LABEL(2)},
			{Z_STOREW, {VALUE(REG_AUXBASE), VALUE(REG_LOCAL+3), VALUE(REG_LOCAL+0)}},
			{Z_INC, {SMALL(REG_LOCAL+3)}},
			{Z_STOREW, {VALUE(REG_AUXBASE), VALUE(REG_LOCAL+3), VALUE(REG_LOCAL+1)}},
			{Z_INC, {SMALL(REG_LOCAL+3)}},
			{Z_STOREW, {VALUE(REG_AUXBASE), VALUE(REG_LOCAL+3), VALUE(REG_LOCAL+2)}},
			{Z_RFALSE},
			{Z_END},
		}
	},
	{
		R_WORDMAP,
		5,
			// 0 (param): raw pointer to wordmap, then entry address, then payload pos
			// 1 (param): number of entries in wordmap, then payload pointer
			// 2 (param): deref'd key, then payload size
			// 3: pivot index, payload byte
			// 4: pivot element, object number
		(struct zinstr []) {
			{Z_JL, {VALUE(REG_LOCAL+1), SMALL(16)}, 0, 3},

			// If the table is very large, use binary search.
			{OP_LABEL(5)},
			{Z_AND, {VALUE(REG_LOCAL+1), LARGE(0xfffe)}, REG_LOCAL+3}, // divide length by two, then multiply by two
			{Z_LOADW, {VALUE(REG_LOCAL+0), VALUE(REG_LOCAL+3)}, REG_LOCAL+4},
			{Z_JL, {VALUE(REG_LOCAL+2), VALUE(REG_LOCAL+4)}, 0, 4},

			{Z_ADD, {VALUE(REG_LOCAL+0), VALUE(REG_LOCAL+3)}, REG_LOCAL+0},
			{Z_ADD, {VALUE(REG_LOCAL+0), VALUE(REG_LOCAL+3)}, REG_LOCAL+0},
			{Z_INC, {SMALL(REG_LOCAL+1)}}, // round up in the shift

			{OP_LABEL(4)},
			{Z_LSHIFT, {VALUE(REG_LOCAL+1), VALUE(REG_FFFF)}, REG_LOCAL+1},
			{Z_JGE, {VALUE(REG_LOCAL+1), SMALL(16)}, 0, 5},

			{OP_LABEL(3)},

			// For an unknown word, perhaps a verb, return true without pushing anything.
			{Z_SCANTABLE_N, {VALUE(REG_LOCAL+2), VALUE(REG_LOCAL+0), VALUE(REG_LOCAL+1), SMALL(0x84)}, REG_LOCAL+0, RTRUE},

			{Z_LOADW, {VALUE(REG_LOCAL+0), SMALL(1)}, REG_LOCAL+1},
			// For a common word, perhaps 'of', return false without pushing anything.
			{Z_JZ, {VALUE(REG_LOCAL+1)}, 0, RFALSE},

			// For a known word, push a number of objects and return true.
			{Z_JG, {VALUE(REG_LOCAL+1), REF(G_OBJECT_ID_END)}, 0, 2},

			// Special treatment for single-element lists: The
			// object table is located before the wordmaps, so
			// object numbers can't collide with addresses.
			{Z_CALL2N, {ROUTINE(R_AUX_PUSH1), VALUE(REG_LOCAL+1)}},
			{Z_RTRUE},

			{OP_LABEL(2)},
			{Z_LOADB, {VALUE(REG_LOCAL+1), SMALL(0)}, REG_LOCAL+2},
			{Z_STORE, {SMALL(REG_LOCAL+0), SMALL(1)}},

			{OP_LABEL(1)},
			{Z_LOADB, {VALUE(REG_LOCAL+1), VALUE(REG_LOCAL+0)}, REG_LOCAL+4},
			{Z_JL, {VALUE(REG_LOCAL+4), SMALL(0xe0)}, 0, 6},

			{Z_INC, {SMALL(REG_LOCAL+0)}},
			{Z_LOADB, {VALUE(REG_LOCAL+1), VALUE(REG_LOCAL+0)}, REG_LOCAL+3},

			{Z_AND, {VALUE(REG_LOCAL+4), SMALL(0x1f)}, REG_LOCAL+4},
			{Z_LSHIFT, {VALUE(REG_LOCAL+4), SMALL(8)}, REG_LOCAL+4},
			{Z_OR, {VALUE(REG_LOCAL+4), VALUE(REG_LOCAL+3)}, REG_LOCAL+4},

			{OP_LABEL(6)},
			{Z_CALL2N, {ROUTINE(R_AUX_PUSH1), VALUE(REG_LOCAL+4)}},
			{Z_INC_JLE, {SMALL(REG_LOCAL+0), VALUE(REG_LOCAL+2)}, 0, 1},

			{Z_RTRUE},
			{Z_END},
		}
	},
	{
		R_GET_FULLWIDTH,
		1,
			// 0: temp
			// returns width of display
		(struct zinstr []) {
			{Z_LOADB, {SMALL(0), SMALL(0x21)}, REG_LOCAL+0},	// screen width
			{Z_JL, {VALUE(REG_LOCAL+0), SMALL(40)}, 0, 1},
			{Z_RET, {VALUE(REG_LOCAL+0)}},

			{OP_LABEL(1)},
			{Z_RET, {SMALL(40)}},	// workaround for winfrotz bug
			{Z_END},
		}
	},
	{
		R_BEGIN_NOSTATUS,
		0,
		(struct zinstr []) {
			{Z_JZ, {VALUE(REG_STATUSBAR)}, 0, 1},

			{OP_LABEL(2)},
			{Z_THROW, {SMALL(FATAL_IO), VALUE(REG_FATALJMP)}},

			{OP_LABEL(1)},
			{Z_JNZ, {VALUE(REG_NSPAN)}, 0, 2},

			{Z_STORE, {SMALL(REG_STATUSBAR), SMALL(2)}},
			{Z_RFALSE},
			{Z_END},
		}
	},
	{
		R_BEGIN_STATUS,
		2,
			// 0 (param): height (msb indicates relative)
			// 1: temp
		(struct zinstr []) {
			{Z_JZ, {VALUE(REG_STATUSBAR)}, 0, 1}, // If already in a status bar, crash

			{OP_LABEL(9)},
			{Z_THROW, {SMALL(FATAL_IO), VALUE(REG_FATALJMP)}},

			{OP_LABEL(1)},
			{Z_JNZ, {VALUE(REG_NSPAN)}, 0, 9}, // If already in a span, crash

			{Z_CALL1N, {ROUTINE(R_LINE)}}, // Print a newline (why?)
			{Z_INC, {SMALL(REG_STATUSBAR)}}, // Mark that we're now in a status bar

			{Z_LOADB, {SMALL(0), SMALL(0x20)}, REG_LOCAL+1},	// Load screen height into a temporary

			{Z_JGE, {VALUE(REG_LOCAL+0), SMALL(0)}, 0, 2}, // If the height given has its MSB unset (i.e. it's positive), it's an absolute height; go to label 2
			{Z_AND, {VALUE(REG_LOCAL+0), SMALL(0xff)}, REG_LOCAL+0}, // Remove the MSB now that we've tested it
			{Z_MUL, {VALUE(REG_LOCAL+1), VALUE(REG_LOCAL+0)}, REG_LOCAL+0}, // Interpret it as a percentage of the screen height
			{Z_DIV, {VALUE(REG_LOCAL+0), SMALL(100)}, REG_LOCAL+0},
			{OP_LABEL(2)},

			{Z_JGE, {VALUE(REG_LOCAL+0), SMALL(1)}, 0, 4}, // If the calculated height is zero, treat it as one instead
			{Z_STORE, {SMALL(REG_LOCAL+0), SMALL(1)}},
			{OP_LABEL(4)},

			{Z_JLE, {VALUE(REG_LOCAL+0), VALUE(REG_LOCAL+1)}, 0, 5}, // If it's greater than the screen height, set it to the screen height instead
			{Z_STORE, {SMALL(REG_LOCAL+0), VALUE(REG_LOCAL+1)}},
			{OP_LABEL(5)},

			{Z_STORE, {SMALL(REG_CURRSPLIT), VALUE(REG_LOCAL+0)}}, // Store the calculated height in REG_CURRSPLIT

			{Z_SPLIT_WINDOW, {VALUE(REG_LOCAL+0)}}, // And actually split the screen
			{Z_SET_WINDOW, {SMALL(1)}},

			{Z_CALL1S, {ROUTINE(R_GET_FULLWIDTH)}, REG_XFULLSIZE}, // Update REG_XFULLSIZE

			{Z_TEXTSTYLE, {SMALL(0)}}, // Turn off all text styles
			{Z_TEXTSTYLE, {SMALL(1)}}, // Turn on reverse video style

			{OP_LABEL(7)},
			{Z_SET_CURSOR, {VALUE(REG_LOCAL+0), SMALL(1)}},
			{Z_STORE, {SMALL(REG_TEMP), VALUE(REG_XFULLSIZE)}},	// known to be >= 40

			{OP_LABEL(8)},
			{Z_PRINTLIT, {}, 0, 0, "         "}, // Print nine reversed spaces
			{Z_SUB, {VALUE(REG_TEMP), SMALL(9)}, REG_TEMP},
			{Z_JGE, {VALUE(REG_TEMP), SMALL(10)}, 0, 8},

			{OP_LABEL(6)},
			{Z_PRINTLIT, {}, 0, 0, " "},
			{Z_DEC_JGE, {SMALL(REG_TEMP), SMALL(1)}, 0, 6},

			{Z_DEC_JGE, {SMALL(REG_LOCAL+0), SMALL(1)}, 0, 7},

			{Z_STORE, {SMALL(REG_XREMSIZE), VALUE(REG_XFULLSIZE)}},
			{Z_STORE, {SMALL(REG_XOFFSET), SMALL(1)}},
			{Z_STORE, {SMALL(REG_YPOS), SMALL(1)}},
			{Z_SET_CURSOR, {VALUE(REG_YPOS), VALUE(REG_XOFFSET)}},
			{Z_STORE, {SMALL(REG_SPACE), SMALL(5)}},
			{Z_RFALSE},
			{Z_END},
		}
	},
	{
		R_END_STATUS,
		0,
		(struct zinstr []) {
			{Z_JG, {VALUE(REG_STATUSBAR), SMALL(1)}, 0, 1},
			{Z_SET_WINDOW, {SMALL(0)}},
			{Z_TEXTSTYLE, {SMALL(0)}},

			{OP_LABEL(1)},
			{Z_STORE, {SMALL(REG_SPACE), SMALL(5)}},
			{Z_STORE, {SMALL(REG_STATUSBAR), SMALL(0)}},
			{Z_RFALSE},
			{Z_END},
		}
	},
	{
		R_BEGIN_BOX,
		2,
			// 0 (param): style
			// 1 (param): top margin
		(struct zinstr []) {
			{Z_JZ, {VALUE(REG_NSPAN)}, 0, 2},

			{Z_THROW, {SMALL(FATAL_IO), VALUE(REG_FATALJMP)}},

			{OP_LABEL(2)},
			{Z_CALL2N, {ROUTINE(R_PAR_N), VALUE(REG_LOCAL+1)}},
			{Z_CALL2N, {ROUTINE(R_AUX_PUSH1), VALUE(REG_STYLE)}},
			{Z_JZ, {VALUE(REG_LOCAL+0)}, 0, 1},

			{Z_AND, {VALUE(REG_LOCAL+0), SMALL(0x7f)}, REG_STYLE},

			{OP_LABEL(1)},
			{Z_CALL1N, {ROUTINE(R_RESET_STYLE)}},
			{Z_RFALSE},
			{Z_END},
		}
	},
	{
		R_BEGIN_BOX_LEFT,
		3,
			// 0 (param): width (msb indicates relative)
			// 1 (param): style
			// 2 (param): top margin
		(struct zinstr []) {
			{Z_JNE, {VALUE(REG_STATUSBAR), SMALL(1)}, 0, 1},

			{Z_JGE, {VALUE(REG_LOCAL+0), SMALL(0)}, 0, 2},
			{Z_AND, {VALUE(REG_LOCAL+0), SMALL(0xff)}, REG_LOCAL+0},
			{Z_MUL, {VALUE(REG_XFULLSIZE), VALUE(REG_LOCAL+0)}, REG_LOCAL+0},
			{Z_DIV, {VALUE(REG_LOCAL+0), SMALL(100)}, REG_LOCAL+0},
			{OP_LABEL(2)},

			{Z_JGE, {VALUE(REG_LOCAL+0), SMALL(0)}, 0, 4},
			{Z_STORE, {SMALL(REG_LOCAL+0), SMALL(0)}},
			{OP_LABEL(4)},

			{Z_JLE, {VALUE(REG_LOCAL+0), VALUE(REG_XREMSIZE)}, 0, 5},
			{Z_STORE, {SMALL(REG_LOCAL+0), VALUE(REG_XREMSIZE)}},
			{OP_LABEL(5)},

			{Z_ADD, {VALUE(REG_XOFFSET), VALUE(REG_LOCAL+0)}, REG_TEMP},
			{Z_SUB, {VALUE(REG_XREMSIZE), VALUE(REG_LOCAL+0)}, REG_XREMSIZE},
			{Z_CALLVN, {ROUTINE(R_AUX_PUSH3), VALUE(REG_TEMP), VALUE(REG_XFULLSIZE), VALUE(REG_XREMSIZE)}},

			{Z_STORE, {SMALL(REG_XFULLSIZE), VALUE(REG_LOCAL+0)}},
			{Z_STORE, {SMALL(REG_XREMSIZE), VALUE(REG_XFULLSIZE)}},
			{Z_ADD, {VALUE(REG_LOCAL+2), SMALL(1)}, REG_YPOS},
			{Z_SET_CURSOR, {VALUE(REG_YPOS), VALUE(REG_XOFFSET)}},
			{Z_STORE, {SMALL(REG_SPACE), SMALL(4)}},
			{Z_RFALSE},

			{OP_LABEL(1)},
			// not inside top status area
			{Z_CALLVN, {ROUTINE(R_BEGIN_BOX), VALUE(REG_LOCAL+1), VALUE(REG_LOCAL+2)}},
			{Z_RFALSE},
			{Z_END},
		}
	},
	{
		R_BEGIN_BOX_RIGHT,
		3,
			// 0 (param): width (msb indicates relative)
			// 1 (param): style
			// 2 (param): top margin
		(struct zinstr []) {
			{Z_JNE, {VALUE(REG_STATUSBAR), SMALL(1)}, 0, 1},

			{Z_JGE, {VALUE(REG_LOCAL+0), SMALL(0)}, 0, 2},
			{Z_AND, {VALUE(REG_LOCAL+0), SMALL(0xff)}, REG_LOCAL+0},
			{Z_MUL, {VALUE(REG_XFULLSIZE), VALUE(REG_LOCAL+0)}, REG_LOCAL+0},
			{Z_DIV, {VALUE(REG_LOCAL+0), SMALL(100)}, REG_LOCAL+0},
			{OP_LABEL(2)},

			{Z_JGE, {VALUE(REG_LOCAL+0), SMALL(0)}, 0, 4},
			{Z_STORE, {SMALL(REG_LOCAL+0), SMALL(0)}},
			{OP_LABEL(4)},

			{Z_JLE, {VALUE(REG_LOCAL+0), VALUE(REG_XREMSIZE)}, 0, 5},
			{Z_STORE, {SMALL(REG_LOCAL+0), VALUE(REG_XREMSIZE)}},
			{OP_LABEL(5)},

			{Z_SUB, {VALUE(REG_XREMSIZE), VALUE(REG_LOCAL+0)}, REG_XREMSIZE},
			{Z_CALLVN, {ROUTINE(R_AUX_PUSH3), VALUE(REG_XOFFSET), VALUE(REG_XFULLSIZE), VALUE(REG_XREMSIZE)}},

			{Z_ADD, {VALUE(REG_XOFFSET), VALUE(REG_XREMSIZE)}, REG_XOFFSET},
			{Z_STORE, {SMALL(REG_XFULLSIZE), VALUE(REG_LOCAL+0)}},
			{Z_STORE, {SMALL(REG_XREMSIZE), VALUE(REG_XFULLSIZE)}},
			{Z_ADD, {VALUE(REG_LOCAL+2), SMALL(1)}, REG_YPOS},
			{Z_SET_CURSOR, {VALUE(REG_YPOS), VALUE(REG_XOFFSET)}},
			{Z_STORE, {SMALL(REG_SPACE), SMALL(4)}},
			{Z_RFALSE},

			{OP_LABEL(1)},
			// not inside top status area
			{Z_CALLVN, {ROUTINE(R_BEGIN_BOX), VALUE(REG_LOCAL+1), VALUE(REG_LOCAL+2)}},
			{Z_RFALSE},
			{Z_END},
		}
	},
	{
		R_BEGIN_SPAN,
		1,
			// 0 (param): style
		(struct zinstr []) {
			{Z_CALL1N, {ROUTINE(R_SYNC_SPACE)}},
			{Z_CALL2N, {ROUTINE(R_AUX_PUSH1), VALUE(REG_STYLE)}},
			{Z_JZ, {VALUE(REG_LOCAL+0)}, 0, 1},

			{Z_AND, {VALUE(REG_LOCAL+0), SMALL(0x7f)}, REG_STYLE},

			{OP_LABEL(1)},
			{Z_CALL1N, {ROUTINE(R_RESET_STYLE)}},
			{Z_INC, {SMALL(REG_NSPAN)}},
			{Z_RFALSE},
			{Z_END},
		}
	},
	{
		R_END_SPAN,
		0,
		(struct zinstr []) {
			{Z_DEC, {SMALL(REG_COLL)}},
			{Z_LOADW, {VALUE(REG_AUXBASE), VALUE(REG_COLL)}, REG_STYLE},
			{Z_CALL1N, {ROUTINE(R_RESET_STYLE)}},
			{Z_DEC, {SMALL(REG_NSPAN)}},
			{Z_RFALSE},
			{Z_END},
		}
	},
	{
		R_END_BOX_FLOAT,
		1,
			// 0 (param): bottom margin
		(struct zinstr []) {
			{Z_JNE, {VALUE(REG_STATUSBAR), SMALL(1)}, 0, 1},

			// leaving an inner box
			{Z_DEC, {SMALL(REG_COLL)}},
			{Z_LOADW, {VALUE(REG_AUXBASE), VALUE(REG_COLL)}, REG_XREMSIZE},
			{Z_DEC, {SMALL(REG_COLL)}},
			{Z_LOADW, {VALUE(REG_AUXBASE), VALUE(REG_COLL)}, REG_XFULLSIZE},
			{Z_DEC, {SMALL(REG_COLL)}},
			{Z_LOADW, {VALUE(REG_AUXBASE), VALUE(REG_COLL)}, REG_XOFFSET},
			{Z_STORE, {SMALL(REG_YPOS), SMALL(1)}},
			{Z_SET_CURSOR, {VALUE(REG_YPOS), VALUE(REG_XOFFSET)}},
			{Z_STORE, {SMALL(REG_SPACE), SMALL(4)}},
			{Z_RFALSE},

			{OP_LABEL(1)},
			{Z_CALL2N, {ROUTINE(R_END_BOX), VALUE(REG_LOCAL+0)}},
			{Z_RFALSE},
			{Z_END},
		}
	},
	{
		R_END_BOX,
		1,
			// 0 (param): bottom margin
		(struct zinstr []) {
			{Z_DEC, {SMALL(REG_COLL)}},
			{Z_LOADW, {VALUE(REG_AUXBASE), VALUE(REG_COLL)}, REG_STYLE},
			{Z_CALL1N, {ROUTINE(R_RESET_STYLE)}},
			{Z_CALL2N, {ROUTINE(R_PAR_N), VALUE(REG_LOCAL+0)}},
			{Z_RFALSE},
			{Z_END},
		}
	},
	{
		R_PROGRESS_BAR,
		3,
			// 0 (param): position (tagged integer)
			// 1 (param): total (tagged integer), then amount remaining
			// 2: size minus 2, then amount of fill
		(struct zinstr []) {
			{Z_CALL1N, {ROUTINE(R_LINE)}},
			{Z_CALL2S, {ROUTINE(R_DEREF), VALUE(REG_LOCAL+0)}, REG_LOCAL+0},
			{Z_CALL2S, {ROUTINE(R_DEREF), VALUE(REG_LOCAL+1)}, REG_LOCAL+1},
			{Z_JL, {VALUE(REG_LOCAL+0), VALUE(REG_4000)}, 0, RFALSE},
			{Z_JL, {VALUE(REG_LOCAL+1), VALUE(REG_4000)}, 0, RFALSE},
			{Z_SUB, {VALUE(REG_LOCAL+0), VALUE(REG_4000)}, REG_LOCAL+0},
			{Z_SUB, {VALUE(REG_LOCAL+1), VALUE(REG_4000)}, REG_LOCAL+1},

			{Z_JG, {VALUE(REG_STATUSBAR), SMALL(1)}, 0, RFALSE},
			{Z_JNZ, {VALUE(REG_STATUSBAR)}, 0, 1},
			// in main area
			{Z_CALL1S, {ROUTINE(R_GET_FULLWIDTH)}, REG_XREMSIZE},
			{Z_DEC, {SMALL(REG_XREMSIZE)}}, // prevent word wrap
			{Z_TEXTSTYLE, {SMALL(0)}},
			{Z_TEXTSTYLE, {SMALL(8)}},
			{Z_BUFFER_MODE, {SMALL(0)}},
			{OP_LABEL(1)},

			{Z_SUB, {VALUE(REG_XREMSIZE), SMALL(2)}, REG_LOCAL+2},
			{Z_JL, {VALUE(REG_LOCAL+2), SMALL(0)}, 0, 9},

			// reduce precision until signed multiplication cannot overflow
			{Z_JLE, {VALUE(REG_LOCAL+0), SMALL(0x7f)}, 0, 3},
			{OP_LABEL(4)},
			{Z_LSHIFT, {VALUE(REG_LOCAL+0), VALUE(REG_FFFF)}, REG_LOCAL+0},
			{Z_LSHIFT, {VALUE(REG_LOCAL+1), VALUE(REG_FFFF)}, REG_LOCAL+1},
			{Z_JG, {VALUE(REG_LOCAL+0), SMALL(0x7f)}, 0, 4},
			{OP_LABEL(3)},

			{Z_JGE, {VALUE(REG_LOCAL+0), VALUE(REG_LOCAL+1)}, 0, 2}, // also deals with 0 of 0 case
			{Z_MUL, {VALUE(REG_LOCAL+0), VALUE(REG_LOCAL+2)}, REG_LOCAL+2},
			{Z_DIV, {VALUE(REG_LOCAL+2), VALUE(REG_LOCAL+1)}, REG_LOCAL+2},
			{OP_LABEL(2)},

			{Z_SUB, {VALUE(REG_XREMSIZE), SMALL(2)}, REG_LOCAL+1},
			{Z_SUB, {VALUE(REG_LOCAL+1), VALUE(REG_LOCAL+2)}, REG_LOCAL+1},

			{Z_PRINTLIT, {}, 0, 0, "["},

			{Z_JL, {VALUE(REG_LOCAL+2), SMALL(1)}, 0, 6},
			{OP_LABEL(5)},
			{Z_PRINTLIT, {}, 0, 0, "="},
			{Z_DEC_JGE, {SMALL(REG_LOCAL+2), SMALL(1)}, 0, 5},
			{OP_LABEL(6)},

			{Z_JL, {VALUE(REG_LOCAL+1), SMALL(1)}, 0, 8},
			{OP_LABEL(7)},
			{Z_PRINTLIT, {}, 0, 0, " "},
			{Z_DEC_JGE, {SMALL(REG_LOCAL+1), SMALL(1)}, 0, 7},
			{OP_LABEL(8)},

			{Z_PRINTLIT, {}, 0, 0, "]"},
			{Z_STORE, {SMALL(REG_SPACE), SMALL(0)}},
			{Z_CALL1N, {ROUTINE(R_LINE)}},

			{OP_LABEL(9)},
			{Z_JNZ, {VALUE(REG_STATUSBAR)}, 0, RFALSE},
			// not inside status box
			{Z_BUFFER_MODE, {SMALL(1)}},
			{Z_TEXTSTYLE, {SMALL(0)}},
			{Z_RFALSE},
			{Z_END},
		}
	},
	{
		R_EMBED_RES,
		1,
			// 0 (param): resource id
		(struct zinstr []) {
			{Z_CALL2S, {ROUTINE(R_DEREF), VALUE(REG_LOCAL+0)}, REG_LOCAL+0},
			// instructions added at compile-time
			{Z_END},
		}
	},
	{
		R_CLEAR,
		1,
			// 0 (param): 0 for clear, -1 for clear all
		(struct zinstr []) {
			{Z_JNZ, {VALUE(REG_NSPAN)}, 0, 2},

			{Z_JZ, {VALUE(REG_STATUSBAR)}, 0, 1},

			{OP_LABEL(2)},
			{Z_THROW, {SMALL(FATAL_IO), VALUE(REG_FATALJMP)}},

			{OP_LABEL(1)},
			{Z_ERASE_WINDOW, {VALUE(REG_LOCAL+0)}},
			{Z_STORE, {SMALL(REG_SPACE), SMALL(4)}},
			{Z_RFALSE},
			{Z_END},
		}
	},
	{
		R_SEL_STOPPING,
		3,
			// 0 (param): index into select table
			// 1 (param): number of forks
			// 2: current value
		(struct zinstr []) {
			{Z_DEC, {SMALL(REG_LOCAL+1)}},

			{Z_LOADB, {REF(G_SELTABLE), VALUE(REG_LOCAL+0)}, REG_LOCAL+2},
			{Z_JE, {VALUE(REG_LOCAL+2), VALUE(REG_LOCAL+1)}, 0, 1},

			{Z_ADD, {VALUE(REG_LOCAL+2), SMALL(1)}, REG_PUSH},
			{Z_STOREB, {REF(G_SELTABLE), VALUE(REG_LOCAL+0), VALUE(REG_STACK)}},

			{OP_LABEL(1)},
			{Z_RET, {VALUE(REG_LOCAL+2)}},
			{Z_END},
		}
	},
	{
		R_SEL_RANDOM,
		4,
			// 0 (param): index into select table
			// 1 (param): number of forks
			// 2: current value
			// 3: last value
		(struct zinstr []) {
			{Z_LOADB, {REF(G_SELTABLE), VALUE(REG_LOCAL+0)}, REG_LOCAL+3},
			{Z_JZ, {VALUE(REG_LOCAL+3)}, 0, 2},

			{Z_DEC, {SMALL(REG_LOCAL+1)}},
			{Z_RANDOM, {VALUE(REG_LOCAL+1)}, REG_LOCAL+2},
			{Z_JL, {VALUE(REG_LOCAL+2), VALUE(REG_LOCAL+3)}, 0, 1},

			{Z_INC, {SMALL(REG_LOCAL+2)}},

			{OP_LABEL(1)},
			{Z_STOREB, {REF(G_SELTABLE), VALUE(REG_LOCAL+0), VALUE(REG_LOCAL+2)}},
			{Z_DEC, {SMALL(REG_LOCAL+2)}},
			{Z_RET, {VALUE(REG_LOCAL+2)}},

			{OP_LABEL(2)},
			// first time, pick any element
			{Z_RANDOM, {VALUE(REG_LOCAL+1)}, REG_LOCAL+2},
			{Z_JUMP, {REL_LABEL(1)}},
			{Z_END},
		}
	},
	{
		R_SEL_T_RANDOM,
		4,
			// 0 (param): index into select table
			// 1 (param): number of forks
			// 2: current value
			// 3: last value
		(struct zinstr []) {
			{Z_LOADB, {REF(G_SELTABLE), VALUE(REG_LOCAL+0)}, REG_LOCAL+3},
			{Z_JGE, {VALUE(REG_LOCAL+3), VALUE(REG_LOCAL+1)}, 0, 3},

			// table values 0..n-1 correspond to the stopping phase
			{Z_ADD, {VALUE(REG_LOCAL+3), SMALL(1)}, REG_LOCAL+2},
			{Z_JNE, {VALUE(REG_LOCAL+2), VALUE(REG_LOCAL+1)}, 0, 4},

			// we are selecting the last entry, so next time we should be in random mode but avoid this
			{Z_ADD, {VALUE(REG_LOCAL+1), VALUE(REG_LOCAL+1)}, REG_LOCAL+2},

			{OP_LABEL(4)},
			{Z_STOREB, {REF(G_SELTABLE), VALUE(REG_LOCAL+0), VALUE(REG_LOCAL+2)}},
			{Z_RET, {VALUE(REG_LOCAL+3)}},

			{OP_LABEL(3)},
			// table values n+1..2n correspond to the random phase, and encode the most recent value

			{Z_SUB, {VALUE(REG_LOCAL+3), VALUE(REG_LOCAL+1)}, REG_LOCAL+3},	// 1-based entry to avoid

			{Z_DEC, {SMALL(REG_LOCAL+1)}},
			{Z_RANDOM, {VALUE(REG_LOCAL+1)}, REG_LOCAL+2},
			{Z_JL, {VALUE(REG_LOCAL+2), VALUE(REG_LOCAL+3)}, 0, 1},

			{Z_INC, {SMALL(REG_LOCAL+2)}},

			{OP_LABEL(1)},

			{Z_INC, {SMALL(REG_LOCAL+1)}},
			{Z_ADD, {VALUE(REG_LOCAL+2), VALUE(REG_LOCAL+1)}, REG_LOCAL+3},
			{Z_STOREB, {REF(G_SELTABLE), VALUE(REG_LOCAL+0), VALUE(REG_LOCAL+3)}},
			{Z_DEC, {SMALL(REG_LOCAL+2)}},
			{Z_RET, {VALUE(REG_LOCAL+2)}},
			{Z_END},
		}
	},
	{
		R_SEL_T_P_RANDOM,
		3,
			// 0 (param): index into select table
			// 1 (param): number of forks
			// 2: current value
		(struct zinstr []) {
			{Z_LOADB, {REF(G_SELTABLE), VALUE(REG_LOCAL+0)}, REG_LOCAL+2},
			{Z_JE, {VALUE(REG_LOCAL+2), VALUE(REG_LOCAL+1)}, 0, 1},

			{Z_ADD, {VALUE(REG_LOCAL+2), SMALL(1)}, REG_PUSH},
			{Z_STOREB, {REF(G_SELTABLE), VALUE(REG_LOCAL+0), VALUE(REG_STACK)}},
			{Z_RET, {VALUE(REG_LOCAL+2)}},

			{OP_LABEL(1)},
			{Z_RANDOM, {VALUE(REG_LOCAL+1)}, REG_LOCAL+2},
			{Z_DEC, {SMALL(REG_LOCAL+2)}},
			{Z_RET, {VALUE(REG_LOCAL+2)}},
			{Z_END},
		}
	},
	{
		R_SEL_CYCLING,
		4,
			// 0 (param): index into select table
			// 1 (param): number of forks
			// 2: next value
			// 3: current value
		(struct zinstr []) {
			{Z_LOADB, {REF(G_SELTABLE), VALUE(REG_LOCAL+0)}, REG_LOCAL+3},

			{Z_ADD, {VALUE(REG_LOCAL+3), SMALL(1)}, REG_LOCAL+2},
			{Z_JL, {VALUE(REG_LOCAL+2), VALUE(REG_LOCAL+1)}, 0, 1},

			{Z_STORE, {SMALL(REG_LOCAL+2), SMALL(0)}},

			{OP_LABEL(1)},
			{Z_STOREB, {REF(G_SELTABLE), VALUE(REG_LOCAL+0), VALUE(REG_LOCAL+2)}},
			{Z_RET, {VALUE(REG_LOCAL+3)}},
			{Z_END},
		}
	},
	{
		R_SET_LONGTERM_VAR,
		9,
			// 0 (param): address of array containing long-term ref
			// 1 (param): index of long-term ref
			// 2 (param): new value to serialize
		(struct zinstr []) {
			{Z_CALL2S, {ROUTINE(R_DEREF_OBJ), VALUE(REG_LOCAL+1)}, REG_LOCAL+1},
			{Z_CALL2S, {ROUTINE(R_DEREF), VALUE(REG_LOCAL+2)}, REG_LOCAL+2},
			{Z_JZ, {VALUE(REG_LOCAL+1)}, 0, 6},

			{Z_LOADW, {VALUE(REG_LOCAL+0), VALUE(REG_LOCAL+1)}, REG_LOCAL+3},
			{Z_JL, {VALUE(REG_LOCAL+3), SMALL(0)}, 0, 4},

			{Z_JL, {VALUE(REG_LOCAL+2), SMALL(0)}, 0, 3},
			{Z_STOREW, {VALUE(REG_LOCAL+0), VALUE(REG_LOCAL+1), VALUE(REG_LOCAL+2)}},
			{Z_RFALSE},

			{OP_LABEL(4)},

			// 3: old long-term ref, then index to old data
			// 4: dest address, then address of old long-term ref
			// 5: source address
			// 6: size of old area
			// 7: words to copy
			// 8: temp

			{Z_STOREW, {VALUE(REG_LOCAL+0), VALUE(REG_LOCAL+1), SMALL(0)}}, // avoids corruption in case we need to abort
			{Z_AND, {VALUE(REG_LOCAL+3), VALUE(REG_3FFF)}, REG_LOCAL+3},

			{Z_ADD, {VALUE(REG_LOCAL+3), VALUE(REG_LOCAL+3)}, REG_LOCAL+4},
			{Z_ADD, {REF(G_LTBASE), VALUE(REG_LOCAL+4)}, REG_LOCAL+4},

			{Z_LOADW, {VALUE(REG_LOCAL+4), SMALL(0)}, REG_LOCAL+6},

			{Z_ADD, {VALUE(REG_LOCAL+4), VALUE(REG_LOCAL+6)}, REG_LOCAL+5},
			{Z_ADD, {VALUE(REG_LOCAL+5), VALUE(REG_LOCAL+6)}, REG_LOCAL+5},

			{Z_SUB, {VALUE(REG_LTTOP), VALUE(REG_LOCAL+3)}, REG_LOCAL+7},
			{Z_SUB, {VALUE(REG_LOCAL+7), VALUE(REG_LOCAL+6)}, REG_LOCAL+7},
			{Z_SUB, {VALUE(REG_LTTOP), VALUE(REG_LOCAL+6)}, REG_LTTOP},
			{Z_JLE, {VALUE(REG_LOCAL+7), SMALL(0)}, 0, 1},

			{Z_ADD, {VALUE(REG_LOCAL+7), VALUE(REG_LOCAL+7)}, REG_LOCAL+8},
			{Z_COPY_TABLE, {VALUE(REG_LOCAL+5), VALUE(REG_LOCAL+4), VALUE(REG_LOCAL+8)}},

			{Z_JGE, {VALUE(REG_LOCAL+3), VALUE(REG_LTTOP)}, 0, 1},
			{OP_LABEL(2)},
			{Z_LOADW, {REF(G_LTBASE2), VALUE(REG_LOCAL+3)}, REG_LOCAL+4},
			{Z_LOADW, {VALUE(REG_LOCAL+4), SMALL(0)}, REG_LOCAL+8},
			{Z_SUB, {VALUE(REG_LOCAL+8), VALUE(REG_LOCAL+6)}, REG_LOCAL+8},
			{Z_STOREW, {VALUE(REG_LOCAL+4), SMALL(0), VALUE(REG_LOCAL+8)}},
			{Z_LOADW, {REF(G_LTBASE), VALUE(REG_LOCAL+3)}, REG_LOCAL+8},
			{Z_ADD, {VALUE(REG_LOCAL+3), VALUE(REG_LOCAL+8)}, REG_LOCAL+3},
			{Z_JL, {VALUE(REG_LOCAL+3), VALUE(REG_LTTOP)}, 0, 2},

			{OP_LABEL(1)},

			{Z_JL, {VALUE(REG_LOCAL+2), SMALL(0)}, 0, 3},
			{Z_STOREW, {VALUE(REG_LOCAL+0), VALUE(REG_LOCAL+1), VALUE(REG_LOCAL+2)}},
			{Z_RFALSE},

			{OP_LABEL(5)},
			{Z_THROW, {SMALL(FATAL_LTS), VALUE(REG_FATALJMP)}},

			{OP_LABEL(6)},
			{Z_JZ, {VALUE(REG_LOCAL+2)}, 0, RFALSE},

			{Z_THROW, {SMALL(FATAL_EXPECTED_OBJ), VALUE(REG_FATALJMP)}},

			{OP_LABEL(3)},

			// 4: size of new area
			// 5: address of new long-term ref
			// 8: temp

			{Z_ADD, {VALUE(REG_LTTOP), SMALL(2)}, REG_TEMP},
			{Z_JG, {VALUE(REG_TEMP), REF(G_LTSIZE)}, 0, 5},

			{Z_CALL2N, {ROUTINE(R_LONGTERM_PUSH), VALUE(REG_LOCAL+2)}},

			{Z_SUB, {VALUE(REG_TEMP), VALUE(REG_LTTOP)}, REG_LOCAL+4},
			{Z_STOREW, {REF(G_LTBASE), VALUE(REG_LTTOP), VALUE(REG_LOCAL+4)}},

			{Z_ADD, {VALUE(REG_LOCAL+1), VALUE(REG_LOCAL+1)}, REG_LOCAL+5},
			{Z_ADD, {VALUE(REG_LOCAL+0), VALUE(REG_LOCAL+5)}, REG_LOCAL+5},
			{Z_STOREW, {REF(G_LTBASE2), VALUE(REG_LTTOP), VALUE(REG_LOCAL+5)}},

			{Z_OR, {VALUE(REG_LTTOP), VALUE(REG_8000)}, REG_LOCAL+8},
			{Z_STOREW, {VALUE(REG_LOCAL+5), SMALL(0), VALUE(REG_LOCAL+8)}},

			{Z_STORE, {SMALL(REG_LTTOP), VALUE(REG_TEMP)}},
			{Z_JLE, {VALUE(REG_LTTOP), VALUE(REG_LTMAX)}, 0, RFALSE},
			{Z_STORE, {SMALL(REG_LTMAX), VALUE(REG_LTTOP)}},
			{Z_RFALSE},
			{Z_END},
		}
	},
	{
		R_LONGTERM_PUSH,
		3,
			// 0 (param): tagged reference to push on long-term heap
			// 1: temp
			// 2: element count
			// REG_TEMP is used as stack pointer with post-increment
			// Simple elements are serialized as themselves.
			// Proper lists are serialized as n elements, followed by c000+n.
			// Improper lists are serialized as n elements, followed by the improper tail element, followed by e000+n.
			// Extended dictionary words are serialized as the optional part, followed by the mandatory part,
			// followed by 8000.
		(struct zinstr []) {
			{Z_JGE, {VALUE(REG_LOCAL+0), VALUE(REG_C000)}, 0, 2},
			{OP_LABEL(1)},
			{Z_ADD, {VALUE(REG_LOCAL+0), VALUE(REG_LOCAL+0)}, REG_LOCAL+1},
			{Z_LOADW, {VALUE(REG_LOCAL+1), SMALL(0)}, REG_LOCAL+1},
			{Z_JZ, {VALUE(REG_LOCAL+1)}, 0, 5},
			{Z_STORE, {SMALL(REG_LOCAL+0), VALUE(REG_LOCAL+1)}},
			{Z_JL, {VALUE(REG_LOCAL+0), VALUE(REG_C000)}, 0, 1},

			{OP_LABEL(2)},
			{Z_JGE, {VALUE(REG_LOCAL+0), SMALL(0)}, 0, 4},

			{Z_JL, {VALUE(REG_LOCAL+0), VALUE(REG_E000)}, 0, 11},
			// extended dictionary word
			{Z_AND, {VALUE(REG_LOCAL+0), VALUE(REG_NIL)}, REG_LOCAL+0},
			{Z_LOADW, {SMALL(2), VALUE(REG_LOCAL+0)}, REG_LOCAL+1},
			{Z_CALL2N, {ROUTINE(R_LONGTERM_PUSH), VALUE(REG_LOCAL+1)}},
			{Z_LOADW, {SMALL(0), VALUE(REG_LOCAL+0)}, REG_LOCAL+1},
			{Z_CALL2N, {ROUTINE(R_LONGTERM_PUSH), VALUE(REG_LOCAL+1)}},
			{Z_STORE, {SMALL(REG_LOCAL+0), VALUE(REG_8000)}},
			{Z_JUMP, {REL_LABEL(4)}},

			{OP_LABEL(11)},
			// pair
			{Z_STORE, {SMALL(REG_LOCAL+2), VALUE(REG_C000)}},

			{OP_LABEL(6)},
			{Z_INC, {SMALL(REG_LOCAL+2)}},

			{Z_SUB, {VALUE(REG_LOCAL+0), VALUE(REG_4000)}, REG_LOCAL+0},	// ref to head element
			{Z_CALL2N, {ROUTINE(R_LONGTERM_PUSH), VALUE(REG_LOCAL+0)}},
			{Z_INC, {SMALL(REG_LOCAL+0)}},					// ref to tail element

			// deref the tail cell
			{Z_JGE, {VALUE(REG_LOCAL+0), VALUE(REG_C000)}, 0, 9},
			{OP_LABEL(8)},
			{Z_ADD, {VALUE(REG_LOCAL+0), VALUE(REG_LOCAL+0)}, REG_LOCAL+1},
			{Z_LOADW, {VALUE(REG_LOCAL+1), SMALL(0)}, REG_LOCAL+1},
			{Z_JZ, {VALUE(REG_LOCAL+1)}, 0, 5},
			{Z_STORE, {SMALL(REG_LOCAL+0), VALUE(REG_LOCAL+1)}},
			{Z_JL, {VALUE(REG_LOCAL+0), VALUE(REG_C000)}, 0, 8},

			{OP_LABEL(9)},
			{Z_JL, {VALUE(REG_LOCAL+0), SMALL(0)}, 0, 6},

			{Z_JE, {VALUE(REG_LOCAL+0), VALUE(REG_NIL)}, 0, 10},

			{OP_LABEL(7)},
			// improper tail
			{Z_OR, {VALUE(REG_LOCAL+2), VALUE(REG_2000)}, REG_LOCAL+2},
			{Z_CALL2N, {ROUTINE(R_LONGTERM_PUSH), VALUE(REG_LOCAL+0)}},

			{OP_LABEL(10)},
			{Z_JGE, {VALUE(REG_TEMP), REF(G_LTSIZE)}, 0, 3},
			{Z_STOREW, {REF(G_LTBASE), VALUE(REG_TEMP), VALUE(REG_LOCAL+2)}},
			{Z_INC, {SMALL(REG_TEMP)}},
			{Z_RFALSE},

			{OP_LABEL(3)},
			{Z_THROW, {SMALL(FATAL_LTS), VALUE(REG_FATALJMP)}},

			{OP_LABEL(5)},
			// unbound variable
			{Z_THROW, {SMALL(FATAL_UNBOUND), VALUE(REG_FATALJMP)}},

			{OP_LABEL(4)},
			// simple value
			{Z_JGE, {VALUE(REG_TEMP), REF(G_LTSIZE)}, 0, 3},
			{Z_STOREW, {REF(G_LTBASE), VALUE(REG_TEMP), VALUE(REG_LOCAL+0)}},
			{Z_INC, {SMALL(REG_TEMP)}},
			{Z_RFALSE},
			{Z_END},
		}
	},
	{
		R_GET_LONGTERM_VAR,
		1,
			// 0 (param): long-term ref to decode
			// returns tagged reference
		(struct zinstr []) {
			{Z_JL, {VALUE(REG_LOCAL+0), SMALL(0)}, 0, 2},
			{Z_JZ, {VALUE(REG_LOCAL+0)}, 0, 1},
			{Z_RET, {VALUE(REG_LOCAL+0)}},

			{OP_LABEL(1)},
			{Z_THROW, {SMALL(0), VALUE(REG_FAILJMP)}},

			{OP_LABEL(2)},
			{Z_AND, {VALUE(REG_LOCAL+0), VALUE(REG_3FFF)}, REG_LOCAL+0},
			{Z_LOADW, {REF(G_LTBASE), VALUE(REG_LOCAL+0)}, REG_TEMP},
			{Z_ADD, {VALUE(REG_LOCAL+0), VALUE(REG_TEMP)}, REG_TEMP},
			{Z_CALL1S, {ROUTINE(R_LONGTERM_POP)}, REG_PUSH},
			{Z_RET_POPPED},
			{Z_END},
		}
	},
	{
		R_LONGTERM_POP,
		4,
			// 0: popped value
			// 1: element counter
			// 2: pointer to new pair
			// 3: list accumulator
		(struct zinstr []) {
			{Z_DEC, {SMALL(REG_TEMP)}},
			{Z_LOADW, {REF(G_LTBASE), VALUE(REG_TEMP)}, REG_LOCAL+0},
			{Z_JGE, {VALUE(REG_LOCAL+0), SMALL(0)}, 0, 1},

			{Z_JE, {VALUE(REG_LOCAL+0), VALUE(REG_8000)}, 0, 3},

			{Z_AND, {VALUE(REG_LOCAL+0), VALUE(REG_NIL)}, REG_LOCAL+1},	// nil = 1fff
			{Z_STORE, {SMALL(REG_LOCAL+3), VALUE(REG_NIL)}},
			{Z_TESTN, {VALUE(REG_LOCAL+0), VALUE(REG_2000)}, 0, 2},

			// improper list
			{Z_CALL1S, {ROUTINE(R_LONGTERM_POP)}, REG_LOCAL+3},

			{OP_LABEL(2)},
			{Z_CALL1S, {ROUTINE(R_LONGTERM_POP)}, REG_LOCAL+0},
			{Z_CALLVS, {ROUTINE(R_PUSH_PAIR_VV), VALUE(REG_LOCAL+0), VALUE(REG_LOCAL+3)}, REG_LOCAL+3},
			{Z_DEC_JGE, {SMALL(REG_LOCAL+1), SMALL(1)}, 0, 2},

			{Z_RET, {VALUE(REG_LOCAL+3)}},

			{OP_LABEL(3)},
			// extended dictionary word
			{Z_CALL1S, {ROUTINE(R_LONGTERM_POP)}, REG_LOCAL+0},
			{Z_CALL1S, {ROUTINE(R_LONGTERM_POP)}, REG_LOCAL+1},
			{Z_CALLVS, {ROUTINE(R_PUSH_PAIR_VV), VALUE(REG_LOCAL+0), VALUE(REG_LOCAL+1)}, REG_LOCAL+0},
			{Z_OR, {VALUE(REG_LOCAL+0), VALUE(REG_E000)}, REG_LOCAL+0},

			{OP_LABEL(1)},
			// simple value
			{Z_RET, {VALUE(REG_LOCAL+0)}},
			{Z_END},
		}
	},
	{
		R_PRINTNIBBLE,
		1,
			// 0 (param): what to print (will be masked)
		(struct zinstr []) {
			{Z_AND, {VALUE(REG_LOCAL+0), SMALL(15)}, REG_LOCAL+0},
			{Z_JL, {VALUE(REG_LOCAL+0), SMALL(10)}, 0, 1},
			{Z_ADD, {VALUE(REG_LOCAL+0), SMALL('a'-10)}, REG_LOCAL+0},
			{Z_PRINTCHAR, {VALUE(REG_LOCAL+0)}},
			{Z_RFALSE},

			{OP_LABEL(1)},
			{Z_PRINTNUM, {VALUE(REG_LOCAL+0)}},
			{Z_RFALSE},
			{Z_END},
		}
	},
	{
		R_PRINTHEX,
		1,
			// 0 (param): what to print
		(struct zinstr []) {
			{Z_LSHIFT, {VALUE(REG_LOCAL+0), LARGE(0xfff4)}, REG_PUSH},
			{Z_CALL2N, {ROUTINE(R_PRINTNIBBLE), VALUE(REG_STACK)}},
			{Z_LSHIFT, {VALUE(REG_LOCAL+0), LARGE(0xfff8)}, REG_PUSH},
			{Z_CALL2N, {ROUTINE(R_PRINTNIBBLE), VALUE(REG_STACK)}},
			{Z_LSHIFT, {VALUE(REG_LOCAL+0), LARGE(0xfffc)}, REG_PUSH},
			{Z_CALL2N, {ROUTINE(R_PRINTNIBBLE), VALUE(REG_STACK)}},
			{Z_CALL2N, {ROUTINE(R_PRINTNIBBLE), VALUE(REG_LOCAL+0)}},
			{Z_RFALSE},
			{Z_END},
		}
	},
	{
		R_PRINTHEX8,
		1,
			// 0 (param): what to print
		(struct zinstr []) {
			{Z_LSHIFT, {VALUE(REG_LOCAL+0), LARGE(0xfffc)}, REG_PUSH},
			{Z_CALL2N, {ROUTINE(R_PRINTNIBBLE), VALUE(REG_STACK)}},
			{Z_CALL2N, {ROUTINE(R_PRINTNIBBLE), VALUE(REG_LOCAL+0)}},
			{Z_RFALSE},
			{Z_END},
		}
	},
	{
		R_PRINT_N_ZSCII,
		4,
			// 0 (param): number of characters
			// 1 (param): pointer
			// 2: temp
			// 3: loop index
		(struct zinstr []) {
			{Z_DEC_JL, {SMALL(REG_LOCAL+0), SMALL(0)}, 0, RFALSE},

			{OP_LABEL(1)},
			{Z_LOADB, {VALUE(REG_LOCAL+1), VALUE(REG_LOCAL+3)}, REG_LOCAL+2},
			{Z_PRINTCHAR, {VALUE(REG_LOCAL+2)}},
			{Z_INC_JLE, {SMALL(REG_LOCAL+3), VALUE(REG_LOCAL+0)}, 0, 1},

			{Z_RFALSE},
			{Z_END},
		}
	},
	{
		R_PRINT_N_BYTES,
		4,
			// 0 (param): number of bytes
			// 1 (param): pointer
			// 2: temp
			// 3: loop index
		(struct zinstr []) {
			{Z_DEC_JL, {SMALL(REG_LOCAL+0), SMALL(0)}, 0, RFALSE},

			{OP_LABEL(1)},
			{Z_LOADB, {VALUE(REG_LOCAL+1), VALUE(REG_LOCAL+3)}, REG_LOCAL+2},
			{Z_CALL2N, {ROUTINE(R_PRINTHEX8), VALUE(REG_LOCAL+2)}},
			{Z_INC_JLE, {SMALL(REG_LOCAL+3), VALUE(REG_LOCAL+0)}, 0, 1},

			{Z_RFALSE},
			{Z_END},
		}
	},
	{
		R_SERIALNUMBER,
		0,
		(struct zinstr []) {
			{Z_JG, {VALUE(REG_STATUSBAR), SMALL(1)}, 0, RFALSE},
			{Z_CALL1N, {ROUTINE(R_SYNC_SPACE)}},
			{Z_CALLVN, {ROUTINE(R_PRINT_N_ZSCII), SMALL(6), SMALL(0x12)}},
			{Z_STORE, {SMALL(REG_SPACE), SMALL(0)}},
			{Z_RFALSE},
			{Z_END},
		}
	},
	{
		R_COMPILERVERSION,
		0,
		(struct zinstr []) {
			{Z_JG, {VALUE(REG_STATUSBAR), SMALL(1)}, 0, RFALSE},
			{Z_CALL1N, {ROUTINE(R_SYNC_SPACE)}},
			{Z_PRINTLIT, {}, 0, 0, "Dialog compiler version "},
			{Z_CALLVN, {ROUTINE(R_PRINT_N_ZSCII), SMALL(2), SMALL(0x3c)}},
			{Z_PRINTLIT, {}, 0, 0, "/"},
			{Z_CALLVN, {ROUTINE(R_PRINT_N_ZSCII), SMALL(2), SMALL(0x3e)}},
			{Z_STORE, {SMALL(REG_SPACE), SMALL(0)}},
			{Z_RFALSE},
			{Z_END},
		}
	},
	{
		R_DUMP_GLOBALS,
		2,
			// 0: temp
			// 1: temp
		(struct zinstr []) {
			{Z_PRINTLIT, {}, 0, 0, "\rGlobals:\r"},
			{OP_LABEL(1)},
			{Z_PRINTLIT, {}, 0, 0, "  G"},
			{Z_CALL2N, {ROUTINE(R_PRINTHEX8), VALUE(REG_LOCAL+0)}},
			{Z_PRINTLIT, {}, 0, 0, " encoded as "},
			{Z_ADD, {VALUE(REG_LOCAL+0), SMALL(0x10)}, REG_LOCAL+1},
			{Z_CALL2N, {ROUTINE(R_PRINTHEX8), VALUE(REG_LOCAL+1)}},
			{Z_PRINTLIT, {}, 0, 0, ": "},
			{Z_LOAD, {VALUE(REG_LOCAL+1)}, REG_PUSH},
			{Z_CALL2N, {ROUTINE(R_PRINTHEX), VALUE(REG_STACK)}},
			{Z_AND, {VALUE(REG_LOCAL+0), SMALL(3)}, REG_PUSH},
			{Z_JNE, {VALUE(REG_STACK), SMALL(3)}, 0, 2},
			{Z_PRINTLIT, {}, 0, 0, "\r"},
			{OP_LABEL(2)},
			{Z_INC, {SMALL(REG_LOCAL+0)}},
			{Z_JL, {VALUE(REG_LOCAL+0), SMALL(0x30)}, 0, 1},
			{Z_RFALSE},
			{Z_END},
		}
	},
	{
		R_DUMP_MEM,
		2,
			// 0 (param): start addr
			// 1 (param): number of words
		(struct zinstr []) {
			{Z_PRINTLIT, {}, 0, 0, "\r"},
			{OP_LABEL(1)},
			{Z_CALL2N, {ROUTINE(R_PRINTHEX), VALUE(REG_LOCAL+0)}},
			{Z_PRINTLIT, {}, 0, 0, ": "},
			{Z_LOADW, {VALUE(REG_LOCAL+0), SMALL(0)}, REG_PUSH},
			{Z_CALL2N, {ROUTINE(R_PRINTHEX), VALUE(REG_STACK)}},
			{Z_PRINTLIT, {}, 0, 0, "\r"},
			{Z_ADD, {VALUE(REG_LOCAL+0), SMALL(2)}, REG_LOCAL+0},
			{Z_DEC_JGE, {SMALL(REG_LOCAL+1), SMALL(1)}, 0, 1},
			{Z_RFALSE},
			{Z_END},
		}
	},
	{
		R_DUMP_COLL,
		1,
			// 0: temp
		(struct zinstr []) {
			{Z_PRINTLIT, {}, 0, 0, "\rColl:\r"},
			{Z_SUB, {VALUE(REG_COLL), SMALL(1)}, REG_LOCAL+0},
			{OP_LABEL(1)},
			{Z_CALL2N, {ROUTINE(R_PRINTHEX), VALUE(REG_LOCAL+0)}},
			{Z_PRINTLIT, {}, 0, 0, ": "},
			{Z_LOADW, {VALUE(REG_AUXBASE), VALUE(REG_LOCAL+0)}, REG_PUSH},
			{Z_CALL2N, {ROUTINE(R_PRINTHEX), VALUE(REG_STACK)}},
			{Z_PRINTLIT, {}, 0, 0, "\r"},
			{Z_DEC, {SMALL(REG_LOCAL+0)}},
			{Z_JGE, {VALUE(REG_LOCAL+0), SMALL(0)}, 0, 1},
			{Z_RFALSE},
			{Z_END},
		}
	},
	{
		R_MEMSTATS,
		4,
			// 0: loop index
			// 1: count
			// 2: temp
			// 3: heap base
		(struct zinstr []) {
			{Z_JG, {VALUE(REG_STATUSBAR), SMALL(1)}, 0, RFALSE},
			{Z_CALL1N, {ROUTINE(R_LINE)}},
			{Z_PRINTLIT, {}, 0, 0, "Peak dynamic memory usage: "},

			{Z_SUB, {REF(G_HEAPSIZE), SMALL(1)}, REG_LOCAL+0},
			{Z_STORE, {SMALL(REG_LOCAL+3), REF(G_HEAPBASE)}},

			{OP_LABEL(1)},
			{Z_LOADW, {VALUE(REG_LOCAL+3), VALUE(REG_LOCAL+0)}, REG_LOCAL+2},
			{Z_JE, {VALUE(REG_LOCAL+2), LARGE(0x1f1f)}, 0, 2},

			{Z_INC, {SMALL(REG_LOCAL+1)}},

			{OP_LABEL(2)},
			{Z_DEC_JGE, {SMALL(REG_LOCAL+0), SMALL(0)}, 0, 1},

			{Z_PRINTNUM, {VALUE(REG_LOCAL+1)}},
			{Z_PRINTLIT, {}, 0, 0, " heap words, "},

			{Z_SUB, {REF(G_AUXSIZE), SMALL(1)}, REG_LOCAL+0},
			{Z_STORE, {SMALL(REG_LOCAL+1), VALUE(REG_MINAUX)}},

			{OP_LABEL(3)},
			{Z_LOADW, {VALUE(REG_AUXBASE), VALUE(REG_LOCAL+0)}, REG_LOCAL+2},
			{Z_JE, {VALUE(REG_LOCAL+2), LARGE(0x3f3f)}, 0, 4},

			{Z_INC, {SMALL(REG_LOCAL+1)}},

			{OP_LABEL(4)},
			{Z_DEC_JGE, {SMALL(REG_LOCAL+0), VALUE(REG_MINAUX)}, 0, 3},

			{Z_PRINTNUM, {VALUE(REG_LOCAL+1)}},
			{Z_PRINTLIT, {}, 0, 0, " aux words, and "},

			{Z_PRINTNUM, {VALUE(REG_LTMAX)}},
			{Z_PRINTLIT, {}, 0, 0, " long-term words.\r"},

			{Z_STORE, {SMALL(REG_SPACE), SMALL(4)}},
			{Z_RFALSE},
			{Z_END},
		}
	},
	{
		R_TRACE_ENTER,
		3,
			// 0 (param): routine to print predicate name with args
			// 1 (param): line number of clause
			// 2 (param): file number of clause
		(struct zinstr []) {
			{Z_JZ, {VALUE(REG_TRACING)}, 0, RFALSE},
			{Z_CALL1N, {ROUTINE(R_LINE)}},
			{Z_PRINTLIT, {}, 0, 0, "ENTER ("},
			{Z_STORE, {SMALL(REG_SPACE), SMALL(1)}},
			{Z_CALL1N, {VALUE(REG_LOCAL+0)}},
			{Z_PRINTLIT, {}, 0, 0, ") at "},
			{Z_CALL2N, {ROUTINE(R_SRCFILENAME), VALUE(REG_LOCAL+2)}},
			{Z_PRINTLIT, {}, 0, 0, ":"},
			{Z_PRINTNUM, {VALUE(REG_LOCAL+1)}},
			{Z_NEW_LINE},
			{Z_STORE, {SMALL(REG_SPACE), SMALL(4)}},
			{Z_RFALSE},
			{Z_END},
		}
	},
	{
		R_TRACE_QUERY,
		3,
			// 0 (param): routine to print predicate name with args
			// 1 (param): line number of call site
			// 2 (param): file number of clause
		(struct zinstr []) {
			{Z_JZ, {VALUE(REG_TRACING)}, 0, RFALSE},
			{Z_CALL1N, {ROUTINE(R_LINE)}},
			{Z_PRINTLIT, {}, 0, 0, "QUERY ("},
			{Z_STORE, {SMALL(REG_SPACE), SMALL(1)}},
			{Z_CALL1N, {VALUE(REG_LOCAL+0)}},
			{Z_PRINTLIT, {}, 0, 0, ") at "},
			{Z_CALL2N, {ROUTINE(R_SRCFILENAME), VALUE(REG_LOCAL+2)}},
			{Z_PRINTLIT, {}, 0, 0, ":"},
			{Z_PRINTNUM, {VALUE(REG_LOCAL+1)}},
			{Z_NEW_LINE},
			{Z_STORE, {SMALL(REG_SPACE), SMALL(4)}},
			{Z_RFALSE},
			{Z_END},
		}
	},
	{
		R_SRCFILENAME,
		1,
			// 0 (param): file number
		(struct zinstr []) {
			// instructions are added by the backend
			{Z_END},
		}
	},
	{
		R_TERPTEST,
		0,
		(struct zinstr []) {
			{Z_JL, {LARGE(0x4000), LARGE(0xc000)}, 0, 1},
			{Z_ADD, {LARGE(0x8001), LARGE(0x8001)}, REG_TEMP},
			{Z_JNE, {VALUE(REG_TEMP), SMALL(2)}, 0, 3},

			{Z_CATCH, {}, REG_TEMP},
			{Z_THROW, {SMALL(0), VALUE(REG_TEMP)}},

			{OP_LABEL(1)},
			{Z_PRINTLIT, {}, 0, 0, "This interpreter is broken"},
			{Z_JUMP, {REL_LABEL(2)}},

			{OP_LABEL(3)},
			{Z_PRINTLIT, {}, 0, 0, "The game is incompatible with this interpreter"},

			{OP_LABEL(2)},
			{Z_PRINTLIT, {}, 0, 0, ". Please use another one.\r"},
			{Z_QUIT},
			{Z_END},
		}
	},
};

const int nrtroutine = sizeof(rtroutines) / sizeof(*rtroutines);


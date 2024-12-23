#define MAXSTRING 256

struct zinstr {
	uint16_t		op;
	uint32_t		oper[4];
	uint16_t		store;
	uint16_t		branch;
	char			*string;
};

#define Z0OP	0xb0
#define Z1OP	0x80
#define ZVAR	0x20

// Short form 0OP: opcode 1011nnnn (except 10111110)
// Short form 1OP: opcode 10ttnnnn (tt: 00 large, 01 small, 10 reg)
// Long form 2OP: opcode 0abnnnnn (a first oper, b second oper: 0 small, 1 reg)
// Variable form 2OP: 110nnnnn aabbccdd (oper types: 00 large, 01 small, 10 reg, 11 no more opers)
// Variable form VAR: 111nnnnn aabbccdd
// Extended form VAR: 10111110 nnnnnnnn aabbccdd

#define OP_EXT		0x1000
#define OP_NOT		0x2000		// invert sense of branch
#define OP_LABEL(x)	(0x4000 | (x))	// this is a label pseudo-instruction
#define OP_NEAR		0x8000		// force near branch

// How predicates are invoked:
//
// REG_A and following registers contain the arguments
// REG_CONT identifies the routine to resume at (i.e. jump to) after a successful end of clause
// REG_CHOICE points to a choice frame, representing what to do on local failure
// REG_SIMPLE may contain an older value of REG_CHOICE, to be restored at a successful end of clause
// REG_SIMPLEREF refers either to REG_CHOICE (for multi-calls) or REG_SIMPLE (for simple calls).
//
// End of clause:
//
// When the end of a clause is reached, REG_CHOICE is loaded indirectly from REG_SIMPLEREF,
// and REG_CONT is jumped to. This will either keep or discard the choice points.
//
// Tailcalls:
//
// For a tailcall that is a multi-call, REG_SIMPLE and REG_SIMPLEREF are left the way they are.
// * If the calling predicate was invoked as a multi-call, the tailcall will remain a multi-call.
// * If the calling predicate was invoked as a simple call, the tailcall will effectively become a simple call.
//
// For a simple tailcall, REG_SIMPLE is loaded indirectly from REG_SIMPLEREF, and REG_SIMPLEREF is
// set to point to REG_SIMPLE.
// * If the calling predicate was invoked as a simple call, the contents of the registers do not change, and
//   the tailcall will remain a simple call.
// * If the calling predicate was invoked as a multi-call, REG_SIMPLE is set to the current REG_CHOICE, and
//   a simple call is made. If the simple call succeeds, that still leaves the earlier choice points on the
//   stack.
//
// Non-tail calls:
//
// In order to accomodate non-tail calls, it is necessary to preserve the
// contents of REG_CONT, REG_SIMPLE and/or REG_SIMPLEREF. Each clause that makes
// non-tail calls will do this using an environment frame. The environment frame is
// deallocated (and the values restored) just before the tailcall.
// REG_CONT must always be saved.
// REG_SIMPLE and REG_SIMPLEREF must be saved if any simple calls are made.
// However, if static analysis shows that a predicate is only invoked by simple calls,
// then REG_SIMPLEREF is known to refer to REG_SIMPLE, and doesn't have to be preserved.
//
// Note that REG_CUT may have to be saved too, when non-tail calls are made. See below.
//
// For a non-tail multi-call, point REG_SIMPLEREF to REG_CHOICE.
// For a non-tail simple call, point REG_SIMPLEREF to REG_SIMPLE, and copy REG_CHOICE to REG_SIMPLE.

// Note that the compiler can figure out which predicates are called in one
// mode exclusively (simple or multi). Such predicates can ignore SIMPLEREF
// since its value is known. Furthermore, callers can omit setting SIMPLEREF if
// the callee is known to ignore it.

// Cutting:
//
// Cutting ("(just)") is handled internally by the predicates.

// REG_SPACE:
//	0	pending space/nospace, to be decided when printing the next char
//	1	space has been inhibited
//	2	a normal space is pending
//	3	a normal space has been printed
//	4	a line feed has been printed
//	5	a paragraph break has been printed

#define REG_STACK		0x00
#define REG_LOCAL		0x01

#define REG_TEMP		0x10
#define REG_SPACE		0x11	/* see above */
#define REG_CONT		0x12	/* where to jump after successful end of clause */
#define REG_ENV			0x13	/* current env frame, unpacked before tail call */
#define REG_CHOICE		0x14	/* current choice frame, unpacked on failure */
#define REG_CUT			0x15	/* saved choice frame at start of current predicate */
#define REG_SIMPLE		0x16	/* saved choice frame at start of simple call */
#define REG_SIMPLEREF		0x17	/* indirect reference to REG_CHOICE (multi) or REG_SIMPLE (simple)*/
#define REG_PAIR		0x18
#define REG_TRAIL		0x19	/* index into aux area of last used trail cell */
#define REG_COLL		0x1a	/* index into aux area of first free coll cell */
#define REG_TOP			0x1b	/* top of heap (which grows upwards) */
#define REG_STOP		0x1c
#define REG_TRACING		0x1d
#define REG_COLLCHK		0x1e
#define REG_FAILJMP		0x1f	/* catch/throw reference for failing */
#define REG_FATALJMP		0x20	/* catch/throw reference for runtime errors */
#define REG_MINAUX		0x21	/* for memory statistics */
#define REG_UPPER		0x22	/* is an uppercase letter pending? */

/* useful constants */

#define REG_2000		0x23
#define REG_3FFF		0x24
#define REG_4000		0x25
#define REG_8000		0x26
#define REG_C000		0x27
#define REG_FFFF		0x28
#define REG_AUXBASE		0x29
#define REG_NIL			0x2a	/* 1fff */
#define REG_R_SPA		0x2b	/* R_SPACE_PRINT_AUTO */
#define REG_R_USIMPLE		0x2c	/* R_UNIFY_SIMPLE */

#define REG_A			0x2d
#define REG_X			0x39

#define REG_PUSH		0x100
#define DEST_USERGLOBAL(x)	(0x200 | (x))

#define LARGE(x)	(0x10000 | (x))
#define REF(x)		(0x20000 | (x))
#define ROUTINE(x)	(0x30000 | (x))
#define REL_LABEL(x)	(0x40000 | (x))
#define SMALL(x)	(0x50000 | (x))
#define VALUE(x)	(0x60000 | (x))
#define USERGLOBAL(x)	(0x70000 | (x))

#define SMALL_OR_LARGE(x)	(((x) < 256)? SMALL(x) : LARGE(x))

#define RFALSE		0xffe
#define RTRUE		0xfff

#define Z_END		0xffff

#define Z_RTRUE		(Z0OP | 0x0)
#define Z_RFALSE	(Z0OP | 0x1)
#define Z_PRINTLIT	(Z0OP | 0x2)
#define Z_RESTART	(Z0OP | 0x7)
#define Z_RET_POPPED	(Z0OP | 0x8)
#define Z_CATCH		(Z0OP | 0x9)
#define Z_QUIT		(Z0OP | 0xa)
#define Z_NEW_LINE	(Z0OP | 0xb)
#define Z_VERIFY	(Z0OP | 0xd)

#define Z_JZ		(Z1OP | 0x0)
#define Z_GET_SIBLING	(Z1OP | 0x1)
#define Z_GET_CHILD	(Z1OP | 0x2)
#define Z_GET_PARENT	(Z1OP | 0x3)
#define Z_GETPROPLEN	(Z1OP | 0x4)
#define Z_INC		(Z1OP | 0x5)
#define Z_DEC		(Z1OP | 0x6)
#define Z_PRINTADDR	(Z1OP | 0x7)
#define Z_CALL1S	(Z1OP | 0x8)
#define Z_REMOVE_OBJ	(Z1OP | 0x9)
#define Z_PRINTOBJ	(Z1OP | 0xa)
#define Z_RET		(Z1OP | 0xb)
#define Z_JUMP		(Z1OP | 0xc)
#define Z_PRINTPADDR	(Z1OP | 0xd)
#define Z_LOAD		(Z1OP | 0xe)
#define Z_CALL1N	(Z1OP | 0xf)

#define Z_JE		(0x01)
#define Z_JL		(0x02)
#define Z_JG		(0x03)
#define Z_DEC_JL	(0x04)
#define Z_INC_JG	(0x05)
#define Z_TEST		(0x07)
#define Z_OR		(0x08)
#define Z_AND		(0x09)
#define Z_JA		(0x0a)
#define Z_SET_ATTR	(0x0b)
#define Z_CLEAR_ATTR	(0x0c)
#define Z_STORE		(0x0d)
#define Z_INSERT_OBJ	(0x0e)
#define Z_LOADW		(0x0f)
#define Z_LOADB		(0x10)
#define Z_GETPROP	(0x11)
#define Z_GETPROPADDR	(0x12)
#define Z_ADD		(0x14)
#define Z_SUB		(0x15)
#define Z_MUL		(0x16)
#define Z_DIV		(0x17)
#define Z_MOD		(0x18)
#define Z_CALL2S	(0x19)
#define Z_CALL2N	(0x1a)
#define Z_THROW		(0x1c)

#define Z_CALLVS	(ZVAR | 0x00)
#define Z_STOREW	(ZVAR | 0x01)
#define Z_STOREB	(ZVAR | 0x02)
#define Z_AREAD		(ZVAR | 0x04)
#define Z_PRINTCHAR	(ZVAR | 0x05)
#define Z_PRINTNUM	(ZVAR | 0x06)
#define Z_RANDOM	(ZVAR | 0x07)
#define Z_PUSH		(ZVAR | 0x08)
#define Z_SPLIT_WINDOW	(ZVAR | 0x0a)
#define Z_SET_WINDOW	(ZVAR | 0x0b)
#define Z_ERASE_WINDOW	(ZVAR | 0x0d)
#define Z_ERASE_LINE	(ZVAR | 0x0e)
#define Z_SET_CURSOR	(ZVAR | 0x0f)
#define Z_GET_CURSOR	(ZVAR | 0x10)
#define Z_TEXTSTYLE	(ZVAR | 0x11)
#define Z_OUTPUT_STREAM	(ZVAR | 0x13)
#define Z_READCHAR	(ZVAR | 0x16)
#define Z_SCANTABLE	(ZVAR | 0x17)
#define Z_CALLVN	(ZVAR | 0x19)
#define Z_COPY_TABLE	(ZVAR | 0x1d)

#define Z_SAVE		(OP_EXT | 0x00)
#define Z_RESTORE	(OP_EXT | 0x01)
#define Z_LSHIFT	(OP_EXT | 0x02)
#define Z_ASHIFT	(OP_EXT | 0x03)
#define Z_SAVE_UNDO	(OP_EXT | 0x09)
#define Z_RESTORE_UNDO	(OP_EXT | 0x0a)
#define Z_PRINTUNICODE	(OP_EXT | 0x0b)

#define Z_VERIFY_N	(OP_NOT | Z_VERIFY)
#define Z_JNZ		(OP_NOT | Z_JZ)
#define Z_JNE		(OP_NOT | Z_JE)
#define Z_JGE		(OP_NOT | Z_JL)
#define Z_JLE		(OP_NOT | Z_JG)
#define Z_JNA		(OP_NOT | Z_JA)
#define Z_TESTN		(OP_NOT | Z_TEST)
#define Z_GET_CHILD_N	(OP_NOT | Z_GET_CHILD)
#define Z_GET_SIBLING_N	(OP_NOT | Z_GET_SIBLING)
#define Z_DEC_JGE	(OP_NOT | Z_DEC_JL)
#define Z_INC_JLE	(OP_NOT | Z_INC_JG)
#define Z_SCANTABLE_N	(OP_NOT | Z_SCANTABLE)

// env frame

enum {
	ENV_ENV,
	ENV_CONT,
	ENV_CUT,	// optional
	ENV_SIMPLE,	// optional
	ENV_SIMPLEREF,	// optional
	// persistent variables follow...
};

// choice frame

enum {
	CHOICE_CHOICE,
	CHOICE_NEXTPC,
	CHOICE_ENV,
	CHOICE_CONT,
	CHOICE_TRAIL,
	CHOICE_TOP,
	CHOICE_CUT,
	CHOICE_SIMPLE,
	CHOICE_SIMPLEREF,
	CHOICE_SIZEOF
	// argument registers follow...
};

struct routine {
	int		nlocal;
	struct zinstr	*instr;
	int		nalloc_instr;
	int		ninstr;
	uint32_t	*local_labels;
	int		nalloc_lab;
	int		next_label;
	uint16_t	address;
	int		aline;
	uint16_t	actual_routine;
	struct routine	*next_in_hash;
};

struct rtroutine {
	int		rnumber;
	int		nlocal;
	struct zinstr	*instr;
};

extern struct rtroutine rtroutines[];
extern const int nrtroutine;

enum {
	G_AUXBASE = 1,
	G_HEAPBASE,
	G_HEAPSIZE,
	G_HEAPEND,
	G_AUXSIZE,
	G_INPBUF0,
	G_INPBUF1,
	G_ARG_REGISTERS,
	G_TEMPSPACE_REGISTERS,
	G_DICT_TABLE,
	G_OBJECT_ID_END,
	G_SELTABLE,
	G_PROGRAM_ENTRY_POINT,
	G_ERROR_ENTRY_POINT,

	G_FIRST_FREE
};

enum {
	R_ENTRY,
	R_OUTERLOOP,
	R_UNICODE,	// put this before innerloop so that txd will resume
	R_INNERLOOP,

	R_SPACE_PRINT_AUTO,
	R_SPACE_PRINT_NOSPACE,
	R_NOSPACE_PRINT_AUTO,
	R_NOSPACE_PRINT_NOSPACE,
	R_PRINT_UPPER,
	R_NOSPACE,
	R_SPACE,
	R_SPACE_N,
	R_LINE,
	R_PAR,
	R_PAR_N,
	R_SYNC_SPACE,
	R_PRINT_VALUE,
	R_PRINT_LIST,
	R_ENABLE_STYLE,
	R_CURSORTO,

	R_UNIFY,
	R_UNIFY_SIMPLE,
	R_PUSH_VAR,
	R_PUSH_VAR_SETENV,
	R_PUSH_VARS_SETENV,

	R_PUSH_PAIR_VV,	// these four must be consecutive
	R_PUSH_PAIR_VR,
	R_PUSH_PAIR_RV,
	R_PUSH_PAIR_RR,

	R_PUSH_LIST_VV,
	R_PUSH_LIST_VVV,

	R_GET_LIST_V,	// these two must be consecutive
	R_GET_LIST_R,

	R_GET_PAIR_VV,	// these four must be consecutive
	R_GET_PAIR_VR,
	R_GET_PAIR_RV,
	R_GET_PAIR_RR,

	R_ALLOCATE,	// these four must be consecutive
	R_ALLOCATE_C,
	R_ALLOCATE_S,
	R_ALLOCATE_CS,

	R_DEALLOCATE,
	R_DEALLOCATE_S,
	R_DEALLOCATE_CS,

	R_TRY_ME_ELSE,
	R_RETRY_ME_ELSE,
	R_TRUST_ME,

	R_TRY_ME_ELSE_0,
	R_RETRY_ME_ELSE_0,
	R_TRUST_ME_0,

	R_DEREF,
	R_DEREF_OBJ,
	R_DEREF_OBJ_FAIL,
	R_DEREF_OBJ_FORCE,
	R_DEREF_SIMPLE,

	R_GRAB_ARG1,

	R_READ_FLAG,
	R_CHECK_FLAG,
	R_CHECK_FLAG_N,

	R_PLUS,
	R_MINUS,
	R_TIMES,
	R_DIVIDED,
	R_MODULO,
	R_RANDOM,
	R_LESS_OR_GREATER,

	R_FAIL_PRED,
	R_QUIT_PRED,
	R_SAVE_PRED,
	R_SAVE_UNDO_PRED,
	R_SCRIPT_ON_PRED,
	R_GET_KEY,
	R_GET_INPUT_PRED,
	R_GET_RAW_INPUT_PRED,
	R_OBJECT_PRED,
	R_OBJECT_SUB,
	R_HASPARENT_PRED,
	R_HASPARENT_SUB,
	R_CONTAINS_PRED,
	R_CONTAINS_SUB,
	R_CONTAINSCHK_PRED,
	R_SPLIT_PRED,
	R_SPLIT_SUB,
	R_SPLIT_SPECIAL_PRED,
	R_SPLIT_SPECIAL_SUB,

	R_COLLECT_BEGIN,
	R_COLLECT_PUSH,
	R_COLLECT_POP,
	R_COLLECT_END,
	R_COLLECTCHK_BEGIN,
	R_COLLECTCHK_END,

	R_AUX_ALLOC,
	R_AUX_PUSH1,
	R_AUX_PUSH2,
	R_AUX_PUSH3,
	R_PUSH_WORDTABLE,

	R_BEGINSTATUS,
	R_ENDSTATUS,

	R_PUSH_STOP,
	R_STOP_PRED,

	R_SEL_STOPPING,
	R_SEL_RANDOM,
	R_SEL_T_RANDOM,
	R_SEL_T_P_RANDOM,
	R_SEL_CYCLING,

	R_SET_COMPLEX_GLOBAL,
	R_UNIFY_COMPLEX_GLOBAL,

	R_PRINTNIBBLE,
	R_PRINTHEX,
	R_PRINTHEX8,
	R_PRINT_N_ZSCII,
	R_SERIALNUMBER_PRED,
	R_COMPILERVERSION_PRED,
	R_DUMP_GLOBALS,
	R_DUMP_HEAP,
	R_DUMP_COLL,
	R_MEMINFO_PRED,
	R_MEMSTATS_PRED,

	R_TRACE_ENTER,
	R_TRACE_QUERY,
	R_SRCFILENAME,

	R_TERPTEST,

	R_FIRST_FREE
};

enum {
	FATAL_HEAP = 1,
	FATAL_AUX,
	FATAL_EXPECTED_OBJ,
	FATAL_EXPECTED_SIMPLE
};

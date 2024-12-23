#define AAVM_FORMAT_MAJOR 0
#define AAVM_FORMAT_MINOR 2

#define AA_NOP			0x00
#define AA_FAIL			0x01
#define AA_SET_CONT		0x02	// CODE
#define AA_PROCEED		0x03
#define AA_JMP			0x04	// CODE
#define AA_JMP_MULTI		0x05	// CODE
#define AA_JMPL_MULTI		0x85	// CODE
#define AA_JMP_SIMPLE		0x06	// CODE
#define AA_JMPL_SIMPLE		0x86	// CODE
#define AA_JMP_TAIL		0x07	// CODE
#define AA_PUSH_ENV		0x08	// BYTE/0
#define AA_POP_ENV		0x09
#define AA_POP_ENV_PROCEED	0x89
#define AA_PUSH_CHOICE		0x0a	// BYTE/0 CODE
#define AA_POP_CHOICE		0x0b	// BYTE/0
#define AA_POP_PUSH_CHOICE	0x0c	// BYTE/0 CODE
#define AA_CUT_CHOICE		0x0d
#define AA_GET_CHO		0x0e	// DEST
#define AA_SET_CHO		0x0f	// VALUE
#define AA_ASSIGN		0x10	// VALUE DEST
#define AA_MAKE_VAR		0x11	// DEST
#define AA_MAKE_PAIR_D		0x12	// DEST DEST DEST
#define AA_MAKE_PAIR_WB		0x13	// WORD/VBYTE DEST DEST
#define AA_AUX_PUSH_VAL		0x14	// VALUE
#define AA_AUX_PUSH_RAW		0x15	// WORD/VBYTE
#define AA_AUX_POP_VAL		0x16	// DEST
#define AA_AUX_POP_LIST		0x17	// DEST
#define AA_AUX_POP_LIST_CHK	0x18	// VALUE
#define AA_AUX_POP_LIST_MATCH	0x19	// VALUE
#define AA_SPLIT_LIST		0x1b	// VALUE VALUE DEST
#define AA_STOP			0x1c
#define AA_PUSH_STOP		0x1d	// CODE
#define AA_POP_STOP		0x1e
#define AA_LOAD_WORD		0x20	// VALUE/0 INDEX DEST
#define AA_LOAD_BYTE		0x21	// VALUE/0 INDEX DEST
#define AA_LOAD_VAL		0x22	// VALUE/0 INDEX DEST
#define AA_STORE_WORD		0x24	// VALUE/0 INDEX VALUE
#define AA_STORE_BYTE		0x25	// VALUE/0 INDEX VALUE
#define AA_STORE_VAL		0x26	// VALUE/0 INDEX VALUE
#define AA_SET_FLAG		0x28	// VALUE/0 INDEX
#define AA_RESET_FLAG		0x29	// VALUE/0 INDEX
#define AA_UNLINK		0x2d	// VALUE/0 INDEX INDEX VALUE
#define AA_SET_PARENT_V		0x2e	// VALUE/VBYTE VALUE
#define AA_SET_PARENT_B		0x2f	// VALUE/VBYTE VBYTE
#define AA_IF_RAW_EQ		0x30	// WORD/0 VALUE CODE
#define AA_IF_BOUND		0x31	// VALUE CODE
#define AA_IF_EMPTY		0x32	// VALUE CODE
#define AA_IF_NUM		0x33	// VALUE CODE
#define AA_IF_PAIR		0x34	// VALUE CODE
#define AA_IF_OBJ		0x35	// VALUE CODE
#define AA_IF_WORD		0x36	// VALUE CODE
#define AA_IF_UNIFY		0x37	// VALUE VALUE CODE
#define AA_IF_GT		0x38	// VALUE VALUE CODE
#define AA_IF_EQ		0x39	// WORD/VBYTE VALUE CODE
#define AA_IF_MEM_EQ		0x3a	// VALUE/0 INDEX VALUE CODE
#define AA_IF_FLAG		0x3b	// VALUE/0 INDEX CODE
#define AA_IF_CWL		0x3c	// CODE
#define AA_IFN_RAW_EQ		0x40	// WORD/0 VALUE CODE
#define AA_IFN_BOUND		0x41	// VALUE CODE
#define AA_IFN_EMPTY		0x42	// VALUE CODE
#define AA_IFN_NUM		0x43	// VALUE CODE
#define AA_IFN_PAIR		0x44	// VALUE CODE
#define AA_IFN_OBJ		0x45	// VALUE CODE
#define AA_IFN_WORD		0x46	// VALUE CODE
#define AA_IFN_UNIFY		0x47	// VALUE VALUE CODE
#define AA_IFN_GT		0x48	// VALUE VALUE CODE
#define AA_IFN_EQ		0x49	// WORD/VBYTE VALUE CODE
#define AA_IFN_MEM_EQ		0x4a	// VALUE/0 INDEX VALUE CODE
#define AA_IFN_FLAG		0x4b	// VALUE/0 INDEX CODE
#define AA_IFN_CWL		0x4c	// CODE
#define AA_ADD_RAW		0x50	// VALUE VALUE DEST
#define AA_INC_RAW		0xd0	// VALUE DEST
#define AA_SUB_RAW		0x51	// VALUE VALUE DEST
#define AA_DEC_RAW		0xd1	// VALUE DEST
#define AA_RAND_RAW		0x52	// BYTE DEST
#define AA_ADD_NUM		0x58	// VALUE VALUE DEST
#define AA_INC_NUM		0xd8	// VALUE DEST
#define AA_SUB_NUM		0x59	// VALUE VALUE DEST
#define AA_DEC_NUM		0xd9	// VALUE DEST
#define AA_RAND_NUM		0x5a	// VALUE VALUE DEST
#define AA_MUL_NUM		0x5b	// VALUE VALUE DEST
#define AA_DIV_NUM		0x5c	// VALUE VALUE DEST
#define AA_MOD_NUM		0x5d	// VALUE VALUE DEST
#define AA_PRINT_A_STR_A	0x60	// STRING
#define AA_PRINT_N_STR_A	0xe0	// STRING
#define AA_PRINT_A_STR_N	0x61	// STRING
#define AA_PRINT_N_STR_N	0xe1	// STRING
#define AA_NOSPACE		0x62
#define AA_SPACE		0xe2
#define AA_LINE			0x63
#define AA_PAR			0xe3
#define AA_SPACE_N		0x64	// VALUE
#define AA_PRINT_VAL		0x65	// VALUE
#define AA_ENTER_DIV		0x66	// INDEX
#define AA_LEAVE_DIV		0xe6
#define AA_ENTER_STATUS		0x67	// INDEX
#define AA_LEAVE_STATUS		0xe7
#define AA_ENTER_LINK_RES	0x68	// VALUE
#define AA_LEAVE_LINK_RES	0xe8
#define AA_ENTER_LINK		0x69	// VALUE
#define AA_LEAVE_LINK		0xe9
#define AA_SET_STYLE		0x6b	// BYTE
#define AA_RESET_STYLE		0xeb	// BYTE
#define AA_EMBED_RES		0x6c	// VALUE
#define AA_CAN_EMBED_RES	0xec	// VALUE DEST
#define AA_PROGRESS		0x6d	// VALUE VALUE
#define AA_EXT0			0x70	// BYTE
#define AA_SAVE			0x72	// CODE
#define AA_SAVE_UNDO		0xf2	// CODE
#define AA_GET_INPUT		0x73	// DEST
#define AA_GET_KEY		0xf3	// DEST
#define AA_VM_INFO		0x74	// BYTE DEST
#define AA_SET_IDX		0x78	// VALUE
#define AA_CHECK_EQ		0x79	// WORD/VBYTE CODE
#define AA_CHECK_GT_EQ		0x7a	// WORD/VBYTE CODE CODE
#define AA_CHECK_GT		0x7b	// VALUE/BYTE CODE
#define AA_CHECK_WORDMAP	0x7c	// INDEX CODE
#define AA_TRACEPOINT		0x7f	// STRING STRING STRING WORD

#define AA_LABEL		0x80
#define AA_SKIP			0x81

#define AA_NEG_FLIP		0x70	// IF_x ^ IFN_x

#define AAEXT0_QUIT		0x00
#define AAEXT0_RESTART		0x01
#define AAEXT0_RESTORE		0x02
#define AAEXT0_UNDO		0x03
#define AAEXT0_UNSTYLE		0x04
#define AAEXT0_PRINT_SERIAL	0x05
#define AAEXT0_CLEAR		0x06
#define AAEXT0_CLEAR_ALL	0x07
#define AAEXT0_SCRIPT_ON	0x08
#define AAEXT0_SCRIPT_OFF	0x09
#define AAEXT0_TRACE_ON		0x0a
#define AAEXT0_TRACE_OFF	0x0b
#define AAEXT0_INC_CWL		0x0c
#define AAEXT0_DEC_CWL		0x0d
#define AAEXT0_UPPERCASE	0x0e
#define AAEXT0_N		0x0f

#define OVAR_PARENT		0
#define OVAR_CHILD		1
#define OVAR_SIBLING		2

#define REG_A			0x00
#define REG_X			0x0d
#define REG_TMP			0x3d
#define REG_NIL			0x3e
#define REG_IDX			0x3f

#define AASTYLE_REVERSE		1
#define AASTYLE_BOLD		2
#define AASTYLE_ITALIC		4
#define AASTYLE_FIXED		8

#define AAFEAT_UNDO		0x40
#define AAFEAT_SAVE		0x41
#define AAFEAT_LINKS		0x42
#define AAFEAT_QUIT		0x43

#define AA_N_INITREG		3

enum {
	AAO_NONE = 0,	// aaopinfo, instructions
	AAO_ZERO,	// aaopinfo, instructions
	AAO_BYTE,	// aaopinfo, instructions
	AAO_VBYTE,	// aaopinfo, instructions
	AAO_WORD,	// aaopinfo, instructions
	AAO_INDEX,	// aaopinfo, instructions
	AAO_CONST,	// instructions
	AAO_REG,	// instructions
	AAO_VAR,	// instructions
	AAO_STORE_REG,	// instructions
	AAO_STORE_VAR,	// instructions
	AAO_CODE,	// aaopinfo, instructions
	AAO_CODE2,	// instructions
	AAO_CODE1,	// instructions
	AAO_STRING,	// aaopinfo, instructions
	AAO_LABEL,	// only for AA_LABEL
	AAO_DEST,	// aaopinfo
	AAO_VALUE	// aaopinfo
};

enum {
	AAMETA_TITLE = 1,
	AAMETA_AUTHOR,
	AAMETA_NOUN,
	AAMETA_BLURB,
	AAMETA_RELDATE,
	AAMETA_COMPVER
};

typedef struct aaoper {
	unsigned int type:8;
	unsigned int value:24;
} aaoper_t;

struct aainstr {
	uint8_t		op;		// AA_* possibly with 0x80 added
	uint8_t		flags;
	struct aaoper	oper[4];
};

#define AAIF_REACHABLE	0x01

struct aaopinfo {
	uint8_t		op;
	uint8_t		oper[4];
	uint8_t		alt_oper0;
	char		*name;
};

extern struct aaopinfo aaopinfo[256];
extern char *aaext0name[AAEXT0_N];

void aavm_init();

struct gxinstr {
	uint32_t			opcode;
	struct gxoperand	oper[8];
};

struct gxoperand {
	uint8_t		type;
	uint32_t	value;
};

enum {
	GX_UNUSED = 0, // We put this at the beginning (= 0) so that uninitialized values can be easily recognized
	GX_CONSTANT,
	GX_REGISTER,
	GX_LOCAL,
	GX_ROUTINE,
	GX_OBJECT,
	GX_LABEL,
	GX_DISCARD, // Discard the result of an operation (store mode 0)
	GX_ROM,
	GX_RAM,
};

#define CONSTANT(x) {GX_CONSTANT, (x)}
#define REGISTER(x) {GX_REGISTER, (x)}
#define LOCAL(x) {GX_LOCAL, (x)}
#define GROUTINE(x) {GX_ROUTINE, (x)}
#define OBJECT(x) {GX_OBJECT, (x)}
#define LABEL(x) {GX_LABEL, (x)}
#define DISCARD {GX_DISCARD, 0}

struct gxroutine {
	int				nlocal; // How many local variables this routine has
	struct gxinstr	*instr; // The instructions comprising the routine
	int				nalloc_instr; // How many instructions we have space for
	int				ninstr; // How many of those are actually used
	uint32_t		*local_labels; // Pointers to each local label in the routine
	int				nalloc_lab; // How many of those pointers we have space for
	int				next_label; // The index of the next pointer to allocate
	uint32_t		address; // Where we've put it in the Glulx memory space
	int				aline; // TODO - what is this?
	uint32_t		actual_routine; // TODO - what is this?
	struct gxroutine	*next_in_hash; // To make it a linked list
};

struct gxrtroutine { // A "runtime routine" defined in C source
	int				rnumber; // The index of this routine in the runtime
	int				nlocal; // As above
	struct gxinstr	*instr; // As above
};

extern struct gxrtroutine gxrtroutines[]; // Defined in runtime_gx.c
extern const int ngxrtroutine;

#define GX_END	(-1)	// A fake opcode used to mark the end of a routine
#define GX_NOP		0x000 // ∅

#define GX_ADD		0x010 // A B -> S
#define GX_SUB		0x011 // A B -> S
#define GX_MUL		0x012 // A B -> S
#define GX_DIV		0x013 // A B -> S
#define GX_MOD		0x014 // A B -> S
#define GX_NEG		0x015 // A -> S

#define GX_AND		0x018 // A B -> S
#define GX_OR		0x019 // A B -> S
#define GX_XOR		0x01A // A B -> S
#define GX_NOT		0x01B // A -> S
#define GX_SHIFTL	0x01C // A B -> S
#define GX_SSHIFTR	0x01D // A B -> S
#define GX_USHIFTR	0x01E // A B -> S

#define GX_JUMP		0x020 // ? J
#define GX_JZ		0x022 // A ? J
#define GX_JNZ		0x023 // A ? J
#define GX_JEQ		0x024 // A B ? J
#define GX_JNE		0x025 // A B ? J
#define GX_JLT		0x026 // A B ? J
#define GX_JGE		0x027 // A B ? J
#define GX_JGT		0x028 // A B ? J
#define GX_JLE		0x029 // A B ? J
// Unsigned versions
#define GX_JLTU		0x02A // A B ? J
#define GX_JGEU		0x02B // A B ? J
#define GX_JGTU		0x02C // A B ? J
#define GX_JLEU		0x02D // A B ? J

#define GX_CALL		0x030 // A B -> S, call routine A passing B values from the stack (first arg topmost)
#define GX_RETURN	0x031 // A
#define GX_CATCH	0x032 // -> S ? J, if encountered normally, save token in S and branch to J, if returning from throw, save thrown value in S and don't branch
#define GX_THROW	0x033 // A B, throw to token B and store A there
#define GX_TAILCALL	0x034 // A B, works like GX_CALL

#define GX_COPY		0x040 // A -> S
#define GX_COPYS	0x041 // A -> S
#define GX_COPYB	0x042 // A -> S
#define GX_SEXS		0x044 // A -> S
#define GX_SEXB		0x045 // A -> S
#define GX_ALOAD	0x048 // A B -> S, read A+4B
#define GX_ALOADS	0x049 // A B -> S, read A+2B
#define GX_ALOADB	0x04A // A B -> S, read A+B
#define GX_ALOADBIT	0x04B // A B -> S, read the B'th bit of A (B can be ≥8, 0 is LSB)
#define GX_ASTORE	0x04C // A B C, store C at A+4B
#define GX_ASTORES	0x04D // A B C, store C at A+2B
#define GX_ASTOREB	0x04E // A B C, store C at A+B 
#define GX_ASTOREBIT	0x04F // A B C, store C at the B'th bit of A (B can be ≥8, 0 is LSB)

#define GX_STKCOUNT	0x050 // -> S
#define GX_STKPEEK	0x051 // A -> S
#define GX_STKSWAP	0x052 // ∅
#define GX_STKROLL	0x053 // A B, rotate the top A values by B places
#define GX_STKCOPY	0x054 // A, push the top A values

#define GX_STREAMCHAR	0x070 // A
#define GX_STREAMNUM	0x071 // A
#define GX_STREAMSTR	0x072 // A
#define GX_STREAMUNI	0x073 // A

#define GX_GESTALT		0x100 // A B -> S
#define GX_TRAP			0x101 // A
#define GX_GETMEMSIZE	0x102 // -> S
#define GX_SETMEMSIZE	0x103 // A -> S, A must be a multiple of 0x100, stores 0 on success, 1 on failure
#define GX_JUMPABS		0x104 // A

#define GX_GESTALT_GXVERSION	0
#define GX_GESTALT_TERPVERSION	1
#define GX_GESTALT_MRESIZE		2
#define GX_GESTALT_UNDO			3
#define GX_GESTALT_IOSYS		4
#define GX_GESTALT_UNICODE		5
#define GX_GESTALT_MEMCOPY		6
#define GX_GESTALT_MALLOC		7
#define GX_GESTALT_MEMHEAP		8
// 9-10 are accelfuncs, 11 is floats
#define GX_GESTALT_EXTUNDO		12
// 12 is doubles

#define GX_RANDOM	0x110 // A -> S, random from 0 to A-1 (pos), or A+1 to 0 (neg), or full 32-bit range (zero)
#define GX_SRAND	0x111 // A

#define GX_QUIT		0x120 // ∅
#define GX_VERIFY	0x121 // -> S, stores 0 on success, 1 on failure
#define GX_RESTART	0x122 // ∅
#define GX_SAVE		0x123 // A -> S, pass output stream, stores 0 on success, 1 on failure, -1 if coming back
#define GX_RESTORE	0x124 // A -> S, pass input stream, stores 1 on failure
#define GX_SAVEUNDO	0x125 // -> S, stores 0 on success, 1 on failure, -1 if coming back
#define GX_RESTUNDO	0x126 // -> S, stores 1 on failure
#define GX_PROTECT	0x127 // A B, protects range from A to A+B
#define GX_HASUNDO	0x128 // -> S, stores 0 on success, 1 on failure
#define GX_DISCUNDO	0x129 // ∅
#define GX_GLK		0x130 // A B -> S, interface is like GX_CALL

#define GX_GETSTBL	0x140 // -> S
#define GX_SETSTBL	0x141 // A
#define GX_GETIOSYS	0x148 // -> S T
#define GX_SETIOSYS	0x149 // A B

#define GX_IOSYS_NULL	0
#define GX_IOSYS_FILTER	1
#define GX_IOSYS_GLK	2

// Linear, binary, linked list
#define GX_LINSEARCH	0x150 // A B C D E F G -> S, A is key value, B is key size, C is array start, D is struct size, E is number of structs, F is offset in struct, G is options
#define GX_BINSEARCH	0x151 // A B C D E F G -> S, params same as LINSEARCH
#define GX_LSTSEARCH	0x152 // A B C D E F -> S, A is key value, B is key size, C is array start, D is offset of key, E is offset of next pointer, F is options
#define GX_SEARCHOPT_KEYINDIRECT	0x1
#define GX_SEARCHOPT_ZEROTERMINATES	0x2
#define GX_SEARCHOPT_RETURNINDEX	0x4

#define GX_CALLF	0x160 // A -> S
#define GX_CALLFI	0x161 // A B -> S
#define GX_CALLFII	0x162 // A B C -> S
#define GX_CALLFIII	0x163 // A B C D -> S

#define GX_MZERO	0x170 // A B, erase [B, B+A)
#define GX_MCOPY	0x171 // A B C, copy [B, B+A) to [C, C+A)
#define GX_MALLOC	0x178 // A -> S, stores 0 on failure
#define GX_MFREE	0x179 // A

// All further opcodes are for accelfuncs (Inform-only) and floating-point processing (not used here)

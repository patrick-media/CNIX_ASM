#ifndef EMU_H
#define EMU_H

#include"util.h"

#define MEMORY_MAX 0xFFFF

typedef struct {
	unsigned char *a, *b, *c, *d; // regular registers (8-bit)
	unsigned short xa, xb, xc, xd; // e(x)tended registers (16-bit) - bottom 8 bits are regular registers
	unsigned short pc; // (p)rogram (c)ounter
	unsigned char fl; // (fl)ags
	unsigned short sp, ep; // (s)tack (p)ointer & stack (e)nd (p)ointer
	/*
	* (fl)ags byte:
	* 0 - reserved
	* 1 - reserved
	* 2 - reserved
	* 3 - reserved
	* 4 - reserved
	* 5 - zero bit - 1 if "cmp" instruction returns equal
	* 6 - carry flag - 1 if "cmp" instruction returns less than
	* 7 - display mode - 0 for default text mode, 1 for pixel mode
	*/
} registers16_t;

typedef struct {
	registers16_t regs;
	unsigned char mem[ MEMORY_MAX ];
} machine16_t;

typedef enum asm_instr_e {
	I_ERR = -1,
	I_NOP = 0,
	I_DEF,  // 0x01
	I_MOV,  // 0x02
	I_ADD,  // 0x03
	I_SUB,  // 0x04
	I_MUL,  // 0x05
	I_DIV,  // 0x06
	I_SHL,  // 0x07
	I_SHR,  // 0x08
	I_IN,   // 0x09
	I_OUT,  // 0x0A
	I_CMP,  // 0x0B
	I_PUSH, // 0x0C
	I_POP,  // 0x0D
	I_RESV, // 0x0E
	I_SYM   // 0x0F
} asm_instr_t; // asm instructions

int emu( char* filename_in, int mode );

#endif // EMU_H
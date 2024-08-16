#ifndef ASM16_89_H
#define ASM16_89_H

#include"util.h"

// avoid redefinitions (defined in file that these components are used)
#ifdef INTERNAL16

// invalid argument shortcut (unused)
#define ASM_INVL_ARG( instr, arg_type, arg_no ) "'" arg_type "' is not a valid argument type for instruction '" instr "' arg" #arg_no ".\n"
// string copy macro for two strings
#define FILL_STRING_S( from, into, size ) for( int _i = 0; _i < ( size ); _i++ ) into[ _i ] = from[ _i ];
// string copy macro for a string and a single character
#define FILL_STRING( from, into, size ) for( int _i = 0; _i < ( size ); _i++ ) into[ _i ] = from;

// number of registers
#define MAX_REG 12
// number of instructions
#define NUM_INSTR 16
// max number of tokens to allow
#define NUM_TOKENS 512
// number of ASCII characters in ASCII table
#define MAX_ASCII 128
// max number of symbol table entries to allow
#define MAX_SYM 64

// maximum number a byte (unsigned char) can hold
#define BYTE_MAX 0xFF

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
	I_MOVL, // 0x0E
	I_MOVG  // 0x0F
} asm_instr_t; // asm instructions

typedef enum asm_arg_type_e {
	//ARG_INIT = -2, // unused?
	ARG_ERR = -1,
	ARG_NULL = 0,
	ARG_REGISTER,
	ARG_INTEGER,
	ARG_STRING,
	ARG_ADDRESS
} asm_arg_type_t; // asm argument types

typedef struct  asm_token_arg_s {
	asm_arg_type_t type;
	size_t data;
} asm_token_arg_t; // asm argument token

typedef struct asm_token_arg_found_s {
	bool arg0_reg : 1;
	bool arg0_int : 1;
	bool arg0_str : 1;
	bool arg0_addr : 1;
	bool arg1_reg : 1;
	bool arg1_int : 1;
	bool arg1_str : 1;
	bool arg1_addr : 1;
} asm_token_arg_found_t;

typedef struct asm_token_op_s {
	asm_instr_t instruction;
	// first argument is typically the "out" - for "mov", the second argument's
	// value will be placed in the first argument (which will be a register).
	asm_token_arg_t args[ 2 ];
	// symtable position in which this instruction or set of data is attached to
	int nsym;
} asm_token_op_t; // asm operation token

typedef struct tok_iteration_s {
	char strval[ 128 ];
	int place;
	//char prev; // unused?
} tok_iteration_t;

typedef struct asm_instruction_s {
	asm_instr_t instr_num;
	char* instr_str;
	unsigned int argc;
	/*
	* 0 - Reserved
	* 0 - Reserved
	* 0 - Reserved
	* 0 - Reserved
	* 0 - ARG_REGISTER
	* 0 - ARG_INTEGER
	* 0 - ARG_STRING
	* 0 - ARG_ADDRESS
	*/
	unsigned char arg0t;
	unsigned char arg1t;
} asm_instruction_t;

typedef struct machine_data_s {
	char* valid_reg[ MAX_REG ];
	asm_instruction_t valid_instr[ NUM_INSTR ];
} machine_data_t;

typedef struct bin_symtable_entry_s {
	int address;
	char name[ 128 ];
} bin_symtable_entry_t;

typedef struct bin_symtable_s {
	bin_symtable_entry_t entries[ MAX_SYM ];
	int place;
	unsigned int program_bytes;
} bin_symtable_t;

#endif // INTERNAL16

int casm16_89( char* filename_in, char* filename_out );

#endif // ASM16_89_H
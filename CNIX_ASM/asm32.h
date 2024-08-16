#ifndef ASM_32_H
#define ASM_32_H

#include"util.h"

/*
* ==== KEY ====
* {} - required
* [] - optional
* If an operand (ex: op1) doesn't have any brackets, it is required.
* imm - immediate value
* chr - character
* str - string
* reg - register
* addr - address
* 
* 
* ==== CONDITIONS [CN] ====
* EQ -> Equal (=)
* NE -> Not equal (!=)
* GR -> Greater than (>)
* LS -> Less than (<)
* GE -> Greater than or equal to (>=)
* LE -> Less than or equal to (<=)
* ZR -> Zero ( = 0 )
* 
* 
* ==== SIZES {SZ} ====
* B -> Byte (8 bits)
* S -> Short (16 bits)
* I -> Integer (32 bits)
* L -> Long (64 bits)
* 
* 
* ==== SPECIAL SYMBOLS ====
* @ -> pre-processor compiler symbol
* # -> integer literal
* "" -> string
* '' -> character
* % -> register
* () -> math expression
* [] -> dereference operator
* + -> addition
* - -> subtraction
* * -> multiplication
* / -> division
* : -> label
* 
* 
* ==== INSTRUCTIONS ====
* Note: op1 will always be a register unless otherwise noted (there are limited exceptions)
* 
* 0x00 NOP
*	>> nop
*	No operation
* 
* 0x01 MOV{SZ}[CN] op1, op2
*	> op1 = op2;
*	>> movl %exa, %exb
*	Move the value of op2 into op1
*	op2: reg, addr, imm, chr
* 
* 0x02 LDM{SZ}[CN] op1, op2
*	> op1 = *op2;
*	>> ldms %xa, [0x7FFF]
*	Set register (or other modifiable memory)
*	op2: reg, addr
* 
* 0x03 STR{SZ}[CN] op1, op2
*	> *op1 = op2;
*	>> strb [%a], #0x04
*	Store value at memory address
*	op2: imm, chr
* 
* 0x04 ADD{SZ}[CN] op1, op2
*	> op1 = op1 + op2;
*	>> addi %exa, #0xFF
*	Add two values
*	op2: reg, imm, chr
* 
* 0x05 SUB{SZ}[CN] op1, op2
*	> op1 = op1 - op2
*	>> subb %d, [0x400]
*	Subtract two values
*	op2: reg, imm, chr
* 
* 0x06 MUL{SZ}[CN] op1, op2
*	> op1 = op1 * op2;
*	>> muls %xc, 'j'
*	Multiply two values
*	op2: reg, imm, chr
* 
* 0x07 DIV{SZ}[CN] op1, op2
*	> op1 = op1 / op2;
*	>> divi %exb, %exa
*	Divide two values
*	op2: reg, imm, chr
* 
* 0x08 SHL[CN] op1, [op2]
*	> op1 = op1 << ( op2 ? op2 : 1 )
*	>> shl %lxc
*	>> shl %exc, #0x3
*	Shift op1 left op2 times, or one time if op2 is not specified.
*	op2: reg, imm
* 
* 0x09 SHR[CN] op1, [op2]
*	> op1 = op1 >> ( op2 ? op2 : 1 )
*	>> shr %d
*	>> shr %b, #0x2
*	Shift op1 right op2 times, or one time if op2 is not specified
*	op2: reg, imm
* 
* 0x0A ROR[CN] op1, [op2]
*	> op1 = op1 >> ( op2 ? op2 : 1 ) + ( c << ( 7 - op2 ) ? ( 7 - op2 ) : 1 )
*	>> ror %lxd
*	>> ror %lxa, #0x1
*	Rotate the bits in op1 op2 times, or one time if op2 is not specified, plus carry
*	op2: reg, imm
* 
* 0x0B AND[CN] op1, op2
*	> op1 = op1 & op2;
*	>> and %xb, %xc
*	Perform a bitwise AND on op1 and op2
*	op2: reg, imm, chr
* 
* 0x0C OR[CN] op1, op2
*	> op1 = op1 | op2;
*	>> or %xb, #0x10
*	Perform a bitwise OR on op1 and op2
*	op2: reg, imm, chr
* 
* 0x0D NOR[CN] op1, op2
*	> op1 = !( op1 | op2 );
*	>> nor %xa, #0x90
*	Perform a bitwise negated OR on op1 and op2
*	op2: reg, imm, chr
* 
* 0x0E XOR[CN] op1, op2
*	> op1 = op1 ^ op2;
*	>> xor %b, %b
*	Perform a bitwise exclusive OR on op1 and op2
*	op2: reg, imm, chr
* 
* 0x0F NOT[CN] op1
*	> op1 = ~op1;
*	>> not %lxb
*	Perform a bitwise NOT on op1
* 
* 0x10 PUSH{SZ}[CN] op1
*	> *sp = op1; sp = sp - size;
*	>> pushb %c
*	Push op1 to the stack and decrement the stack pointer
* 
* 0x11 POP{SZ}[CN] op1
*	> op1 = *sp; sp = sp + size;
*	>> popi %exd
*	Pop the last value pushed to the stack into op1 and increment the stack pointer.
* 
* 0x12 PUSHA
*	>> pusha
*	Push all registers with their appropriate sizes
* 
* 0x13 POPA
*	>> popa
*	Pop all previously pushed values back into their corresponding registers with appropriate sizes
* 
* 0x14 PUSHF
*	>> pushf
*	Push the flags register to the stack (register size denoted by pre-processor definition)
* 
* 0x15 IN{SZ}[CN] op1, op2
*	> op1 = *op2; (approx.)
*	>> inb %d, #0x44
*	Receive input from a port specified by op2, and place the result in op1
*	op2: imm
* 
* 0x16 OUT{SZ}[CN] op1, op2
*	> op2 = *op1; (approx.)
*	>> outl %lxb, #0x40
*	Send the value in op1 to the port specified by op2
*	op2: imm
* 
* 0x17 B[CN] op1
*	> pc = &op1; (approx.)
*	>> b [0x8FE3]
*	Branch: set the program counter to the address specified by op1
*	op1: reg, imm, addr, label
*	
* 0x18 BR[CN] op1
*	> *sp = ( pc + x ); sp = sp - size; pc = &op1;
*	>> br .func
*	(Branch with return) push the current address + x to the stack and set the program counter to the address specified by op1
*	op1: addr, label, deref. reg
* 
* 0x19 RTB
*	> pc = *sp; sp = sp + size;
*	>> rtb
*	Return from branch: pop the saved address from the stack to return to the caller's following instruction
* 
* 0x1A CMP
*	> carry = op1 < op2 ? 1 : 0;
*	> zero = op1 == op2 ? 1 : 0;
*	>> cmp %a, #0x04
*	Compare two operands. Set only the zero flag if they are equal, only the carry flag if the first is less than the second, and no flags if the first is greater.
*/

#ifdef INTERNAL32

// possible operations
typedef enum {
	OP_NULL = 0,
	OP_ADD,
	OP_SUB,
	OP_MUL,
	OP_DIV
} op_t;
/*
// intended to modify operations to cut down on individual actions (add logic and shift directions)
typedef enum {
	OPM_NONE = 0,
	OPM_LEFT, // shift left
	OPM_RIGHT, // shift right
	OPM_AND, // bitwise and
	OPM_OR, // bitwise or
	OPM_XOR, // bitwise xor
	OPM_NEGATE, // bitwise not/nor
} op_modifier_t;
*/
typedef enum {
	I_NUL = 0,
	I_NOP,
	I_MOV, // move values
	I_LDM, // move values
	I_STR, // move values
	I_ADD, // math
	I_SUB, // math
	I_MUL, // math
	I_DIV, // math
	I_SHL, // math
	I_SHR, // math
	I_ROR, // math
	I_AND, // logic
	I_OR, // logic
	I_NOR, // logic - multiple operations
	I_XOR, // logic
	I_NOT, // logic
	I_PUSH, // stack
	I_POP, // stack
	I_PUSHA, // stack - multiple operations
	I_POPA, // stack - multiple operations
	I_PUSHF, // stack
	I_IN, // io
	I_OUT, // io
	I_B, // program counter + stack
	I_BR, // program counter + stack
	I_RTB, // program counter + stack
	I_CMP // comparison (math)
} key_e;

typedef struct {
	key_e value;
	char* name;
} kwds_t;

typedef struct tnode_s tnode_t;
typedef struct tnode_s {
	op_t op;
	//op_modifier_t op_m;
	tnode_t *lvalue;
	tnode_t *rvalue;
	long long value;
} tnode_t;

typedef struct {
	enum {
		T_EOF = 0,
		T_LIT_INT,
		T_PLUS,
		T_MINUS,
		T_STAR,
		T_SLASH
	} type;
	int val;
} token_t;

#endif // INTERNAL32

int asm32( char* filename_in, char* filename_out );

#endif // ASM_32_H
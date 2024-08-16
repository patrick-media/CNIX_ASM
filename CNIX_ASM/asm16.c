// in order to avoid redefinitions outside of this function
#define INTERNAL16
#include"asm16.h"

// if defined, the program will be ran with an insane
// amount of data spat out
#define VERBOSE_ALL_DATA

// pre defined
#define V_PRINT( ... )

// only define this to printf if VERBOSE_ALL_DATA is defined
#ifdef VERBOSE_ALL_DATA
#define V_PRINT( ... ) printf( __VA_ARGS__ );
#endif

/*

% - register
ex: %a

$ - integer
ex: $0F

"" / '' - char (sometimes string)
ex: "j" / 'j'
ex (string): "test"

() - math
ex: (1 + 2)

@ - compiler symbol
ex: @macro

# - memory address
ex: #0EFF0

Syntax:
d    [reg | int | str | addr]
mov  [reg | addr], [reg | int | str | addr]
add  [reg | addr], [reg | int | addr] - all arithmetic operations: arg0 must be a register or address to store the result
sub  [reg | addr], [reg | int | addr]
mul  [reg | addr], [reg | int | addr]
div  [reg | addr], [reg | int | addr]
shl  [reg | addr]
shr  [reg | addr]
in   [reg | int | str | addr], [addr] - first is where port data goes, second is port to read from
out  [addr], [reg | int | str | addr] - first is port to write to, second is data to write
cmp  [reg | int | str | addr], [reg | int | str | addr]
push [reg | int | str | addr]
pop  [reg | addr]

*/

// internal functions
static tok_iteration_t _get_token( char* buffer, int place );
static bool _asm_integer( tok_iteration_t tok_next, asm_token_op_t* tok_op_master, int tok_place, int argno );
static bool _asm_register( tok_iteration_t tok_next, asm_token_op_t* tok_op_master, int tok_place, int argno );
static bool _asm_string( tok_iteration_t tok_next, asm_token_op_t* tok_op_master, int tok_place, int argno );
static bool _asm_char( tok_iteration_t tok_next, asm_token_op_t* tok_op_master, int tok_place, int argno );
static bool _asm_address( tok_iteration_t tok_next, asm_token_op_t* tok_op_master, int tok_place, int argno );
static bool _asm_symbol( tok_iteration_t tok_next, asm_token_op_t* tok_op_master, int tok_place, int argno );
static int _asm_fprintf( HANDLE f_file, int _input );

// contains data about the machine's registers and instructions
// registers: capable of returning their number
// instructions: argument count, valid arguments per arg0/arg1, etc.
const machine_data_t machine_data = {
	.valid_reg = {
		// regular - 16-bit
		"%a", "%b", "%c", "%d",
		// extended - 32-bits
		"%xa", "%xb", "%xc", "%xd",
		// cpu-managed registers:
		// (p)rogram (c)ounter, (fl)ags, (s)tack (p)ointer, stack (e)nd (p)ointer
		"%pc", "%fl", "%sp", "%ep"
    },
	.valid_instr = {
		/* valid argument bits:
		* 0-3 - reserved
		* 4 - reg
		* 5 - int
		* 6 - str/chr
		* 7 - addr
		*/
		{ I_NOP,  "nop",  0, 0b00000000, 0b00000000 },
		{ I_DEF,  "d",    1, 0b00001111, 0b00000000 },
		{ I_MOV,  "mov",  2, 0b00001001, 0b00001111 },
		{ I_ADD,  "add",  2, 0b00001001, 0b00001101 },
		{ I_SUB,  "sub",  2, 0b00001001, 0b00001101 },
		{ I_MUL,  "mul",  2, 0b00001001, 0b00001101 },
		{ I_DIV,  "div",  2, 0b00001001, 0b00001101 },
		{ I_SHL,  "shl",  1, 0b00001001, 0b00000000 },
		{ I_SHR,  "shr",  1, 0b00001001, 0b00000000 },
		{ I_IN,   "in",   2, 0b00001001, 0b00000001 },
		{ I_OUT,  "out",  2, 0b00000001, 0b00001111 },
		{ I_CMP,  "cmp",  2, 0b00001111, 0b00001111 },
		{ I_PUSH, "push", 1, 0b00001111, 0b00000000 },
		{ I_POP,  "pop",  1, 0b00001001, 0b00000000 },
		{ I_MOVL, "movl", 2, 0b00001001, 0b00001111 },
		{ I_MOVG, "movg", 2, 0b00001001, 0b00001111 }
     }
};

// useful when converting strings to ascii integers
const char ascii_table[ MAX_ASCII ] = {
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 0-14
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 15-29
	0, 0, 0, '!', '"', '#', '$', '%', '&', '\'',  // 30-39
	'(', ')', '*', '+', ',', '-', '.', '/', '0',  // 40-48
	'1', '2', '3', '4', '5', '6', '7', '8', '9',  // 49-57
	':', ';', '<', '=', '>', '?', '@', 'A', 'B',  // 58-66
	'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K',  // 67-75
	'L', 'M', 'N', 'O', 'P', 'Q', 'R', 'S', 'T',  // 76-84
	'U', 'V', 'W', 'X', 'Y', 'Z', '[', '\\', ']', // 85-93
	'^', '_', '`', 'a', 'b', 'c', 'd', 'e', 'f',  // 94-102
	'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n', 'o',  // 103-111
	'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x',  // 112-120
	'y', 'z', '{', '|', '}', '~', 0               // 120-127
};

// symbol table initialization
bin_symtable_t symtable = { 0 };

/*
* Purpose:
*	Parse all instructions, arguments, and other components of the given assembly
*	source file into a non-string token representation. This is then translated
*	into the raw hexadecimal form, which can be interpreted by the emulator.
* 
* Parameters:
*	filename_in - file to read from (asm source *.csm)
*	filename_out - file to write to (binary output *.cb)
* 
* Returns:
*	0 - success
*	1 - file error
*	2 - token parsing error
*	3 - memory alloc error (reserved global)
*/
int casm16_89( char* filename_in, char* filename_out ) {
	// this function's return value - originally cleanup was going to occur
	// but this was before i knew items were free()d upon the program exiting.
	// keeping it for very much strictly possible and not guaranteed ramifications
	int asm_retval = 0;

	// possible symbol table entries - used for the first sweep
	// so symbols are global anywhere instead of needing to be
	// declared first
	char symtable_possible[ MAX_SYM ][ MAX_SYM ] = { 0 };

	// allocations
	LPOFSTRUCT f_file_of = calloc( 64, sizeof( *f_file_of ) ); // error check below
	HANDLE f_file = OpenFile( filename_in, f_file_of, OF_READ ); // error check below
	unsigned int f_filesize = GetFileSize( f_file, NULL );
	asm_token_op_t* tok_op_master = calloc( NUM_TOKENS, sizeof( *tok_op_master ) ); // error check below
	asm_token_arg_found_t tok_arg_found = { 0 };

	// add padding for filesize so overflows don't occur. it's gonna get free()d later anyway
	// if it's less than 512 bytes, make the buffer 512 bytes.
	if( f_filesize < 512 ) {
		f_filesize = 512;
	}
	// if it's greater than 512 bytes but not divisible by 512, add padding (128)
	else if( ( f_filesize % 512 ) > 0 ) {
		f_filesize += 128;
	}
	char* f_buffer = calloc( f_filesize, sizeof( *f_buffer ) ); // error check below

	// check for calloc errors
	ASSERT_RET( f_file, 3, "[MEMORY ERROR] f_file failed.\n" );
	ASSERT_RET( f_file_of, 3, "[MEMORY ERROR] f_file_of failed to allocate.\n" );
	ASSERT_RET( f_buffer, 3, "[MEMORY ERROR] f_buffer failed to allocate.\n" );
	ASSERT_RET( tok_op_master, 3, "[MEMORY ERROR] tok_op_master failed to allocate.\n" );

	// read file, error if it returns false
	ASSERT_RET( ReadFile( f_file, f_buffer, f_filesize, NULL, NULL ), 1, "Error reading file.\n" );

	// initialize symtable
	for( int i = 0; i < MAX_SYM; i++ ) {
		FILL_STRING( '\0', symtable.entries[ i ].name, 128 );
		symtable.entries[ i ].address = -1;
	}
	// initialize token nsym
	for( int i = 0; i < NUM_TOKENS; i++ ) {
		tok_op_master[ i ].nsym = -1;
	}
	symtable.place = 0;
	symtable.program_bytes = 1; // accounting for the version tag at the beginning

	// end of file - stop checking it
	bool f_eof = false;
	// buf_place - file buffer place
	// tok_place - tok_op_master place
	int buf_place = 0, tok_place = 0;
	// try count
	int tryc = 0;
	// next token data
	tok_iteration_t tok_next = { 0 };

	// initialize tok_next.strval
	FILL_STRING( '\0', tok_next.strval, 128 );

	// done checking for symbols
	bool sym_done = false;
	// symbol try count
	int sym_tryc = 0;

	// search for symbols in the file
	while( !sym_done ) {
		// stop if the try count = the file size
		if( sym_tryc > f_filesize ) {
			buf_place = 0;
			break;
		}
		sym_tryc++;

		// advance token to next piece of data
		// check everything for labels
		tok_next = _get_token( f_buffer, buf_place );
		buf_place = tok_next.place;

		if( tok_next.strval[ strlen( tok_next.strval )-1 ] == ':' ) {
			tok_next.strval[ strlen( tok_next.strval )-1 ] = 0;
			FILL_STRING_S( tok_next.strval, symtable_possible[ symtable.place ], 128 );
			symtable.place++;
		}

		// end of file, stop checking
		if( strcmp( tok_next.strval, "\"eof\"" ) == 0 ) {
			// reset buf_place for the real check, this just checks for things that
			// need to be found before instructions and arguments are checked
			buf_place = 0;
			sym_done = true;
		}
	}

	// reset variables
	sym_done = false;
	sym_tryc = 0;
	symtable.place = 0;

	// search for symbol addresses in file
	while( !sym_done ) {
		// stop if the try count = the file size
		if( sym_tryc > f_filesize ) {
			buf_place = 0;
			break;
		}
		sym_tryc++;

		// advance token to next piece of data
		// check everything for labels
		tok_next = _get_token( f_buffer, buf_place );
		buf_place = tok_next.place;

		// increment bytes for opcodes
		for( int i = 0; i < NUM_INSTR; i++ ) {
			if( strcmp( tok_next.strval, machine_data.valid_instr[ i ].instr_str ) == 0 ) {
				symtable.program_bytes += 2; // +1 for opcode, +1 for argdesc
			}
		}
		// increment bytes for integer / address values
		if( tok_next.strval[ 0 ] == '$' || tok_next.strval[ 0 ] == '#' ) {
			tok_next.strval[ 0 ] = '0';
			unsigned long temp = strtoul( tok_next.strval, NULL, 16 );
			symtable.program_bytes++; // 1 byte min
			if( temp > 0xFF ) {
				symtable.program_bytes++; // 2 bytes
			}
			if( temp > 0xFFFF ) {
				symtable.program_bytes += 2; // 4 bytes max
			}
		}
		// increment bytes for SINGLE CHARACTER values
		if( tok_next.strval[ 0 ] == '"' || tok_next.strval[ 0 ] == '\'' ) {
			if( strcmp( tok_next.strval, "\"eof\"" ) != 0 ) {
				symtable.program_bytes++;
			}
		}
		// increment bytes for registers
		if( tok_next.strval[ 0 ] == '%' ) {
			symtable.program_bytes++;
		}
		// increment bytes for symbol name
		for( int i = 0; i < MAX_SYM; i++ ) {
			if( strcmp( tok_next.strval, symtable_possible[ i ] ) == 0 ) {
				symtable.program_bytes += 2; // labels are 2 bytes each
			}
		}

		if( _asm_symbol( tok_next, tok_op_master, tok_place, NULL ) ) {
			symtable.entries[ symtable.place ].address = symtable.program_bytes;
			// found a label
			V_PRINT( "Symbol found: (freestanding) <%s> #%d at byte 0x%zX\n", symtable.entries[ symtable.place ].name,
				symtable.place,
				symtable.entries[ symtable.place ].address );
			symtable.place++;
		}

		// end of file, stop checking
		if( strcmp( tok_next.strval, "\"eof\"" ) == 0 ) {
			// reset buf_place for the real check, this just checks for things that
			// need to be found before instructions and arguments are checked
			buf_place = 0;
			sym_done = true;
		}
	}

	// main processing loop - organizes written csm file into serializable data
	while( !f_eof ) {
		// if the loop tries to find something for more times than exist
		// token slots, abord
		if( tryc > 2*NUM_TOKENS ) break;
		tryc++;

		// initialize tok_arg_found
		tok_arg_found.arg0_reg = false;
		tok_arg_found.arg0_int = false;
		tok_arg_found.arg0_str = false;
		tok_arg_found.arg0_addr = false;
		tok_arg_found.arg1_reg = false;
		tok_arg_found.arg1_int = false;
		tok_arg_found.arg1_str = false;
		tok_arg_found.arg1_addr = false;

		// initialize tok_next.strval (again)
		FILL_STRING( '\0', tok_next.strval, 128 );

		// get next token data
		// buf_place = tok_next.place after every _get_token() call
		// use buf_place as the "place" param in _get_token()
		tok_next = _get_token( f_buffer, buf_place );
		buf_place = tok_next.place;

		// used to avoid aborting program when argument isn't found.
		// there isn't a type for symbol because it doesn't work
		// well with the cb binary optimization, so i'm hacking
		// it in.
		bool sym_arg0 = false, sym_arg1 = false;

		// check for instruction
		for( int md_i = 0; md_i < NUM_INSTR; md_i++ ) {
			// break if the first argument was found
			// the first argument must be found for the second,
			// so we don't need to worry about the second
			if( tok_arg_found.arg0_reg ||
				tok_arg_found.arg0_int ||
				tok_arg_found.arg0_str ||
				tok_arg_found.arg0_addr ) {
				break;
			}

			if( strcmp( tok_next.strval, machine_data.valid_instr[ md_i ].instr_str ) == 0 &&
				machine_data.valid_instr[ md_i ].argc > 0 ) {
				// +1 byte for opcode
				// +1 byte for arg descriptor
				symtable.program_bytes += 2;

				// initialize data
				tok_op_master[ tok_place ].instruction = machine_data.valid_instr[ md_i ].instr_num;

				// clear strval so it doesn't bleed into the next token
				FILL_STRING( '\0', tok_next.strval, 128 );

				// advance token (to nearest railroad)
				tok_next = _get_token( f_buffer, buf_place );
				buf_place = tok_next.place;

				// check for valid symbol
				for( int sym_i = 0; sym_i < MAX_SYM; sym_i++ ) {
					if( symtable.entries[ sym_i ].name[ 0 ] == 0 ) continue;
					if( strcmp( tok_next.strval, symtable.entries[ sym_i ].name ) == 0 ) {
						// copy string
						memcpy( &tok_op_master[ tok_place ].args[ 0 ].data, symtable.entries[ sym_i ].name, 128 );
						// 2 bytes because the cb bin address could be 2 bytes
						symtable.program_bytes += 2;
						// not ARG_ERR
						tok_op_master[ tok_place ].args[ 0 ].type = ARG_INTEGER;
						// copy symtable index that relates to this instruction
						tok_op_master[ tok_place ].nsym = sym_i;
						// don't abort later
						sym_arg0 = true;
						break;
					}
				}

				// all are not found by default, will be changed if found
				tok_arg_found.arg0_reg = false;
				tok_arg_found.arg0_int = false;
				tok_arg_found.arg0_str = false;
				tok_arg_found.arg0_addr = false;
				tok_arg_found.arg1_reg = false;
				tok_arg_found.arg1_int = false;
				tok_arg_found.arg1_str = false;
				tok_arg_found.arg1_addr = false;

				// search for arguments based on instruction arg types
				if( machine_data.valid_instr[ md_i ].arg0t & 0b00000001 ) {
					// ARG_ADDRESS
					tok_arg_found.arg0_addr = _asm_address( tok_next, tok_op_master, tok_place, 0 );
				}
				if( machine_data.valid_instr[ md_i ].arg0t & 0b00000010 ) {
					// ARG_STRING
					if( machine_data.valid_instr[ md_i ].instr_num == I_DEF ) {
						tok_arg_found.arg0_str = _asm_string( tok_next, tok_op_master, tok_place, 0 );
					}
					// ARG_STRING (but char type, too lazy to shift around arg types)
					else {
						tok_arg_found.arg0_str = _asm_char( tok_next, tok_op_master, tok_place, 0 );
					}
				}
				if( machine_data.valid_instr[ md_i ].arg0t & 0b00000100 ) {
					// ARG_INTEGER
					tok_arg_found.arg0_int = _asm_integer( tok_next, tok_op_master, tok_place, 0 );
				}
				if( machine_data.valid_instr[ md_i ].arg0t & 0b00001000 ) {
					// ARG_REGISTER
					tok_arg_found.arg0_reg = _asm_register( tok_next, tok_op_master, tok_place, 0 );
				}

				// error if no arguments were found
				if( !tok_arg_found.arg0_reg &&
					!tok_arg_found.arg0_int &&
					!tok_arg_found.arg0_str &&
					!tok_arg_found.arg0_addr &&
					tok_op_master[ tok_place ].instruction != I_NOP &&
					!sym_arg0 ) { // hard to read - "if" statement ends here
					printf( "Error reading argument 0: no argument was found. token %d\n", tok_place );
					asm_retval = 2;
					goto asm_cleanup;
				}

				// assume that an argument was found after some error checking
				symtable.program_bytes++;

				// error check: if a valid argument was not found, bail
				if( tok_op_master[ tok_place ].args[ 0 ].type == ARG_ERR ) {
					printf( "Error reading argument 0: type ARG_ERR returned. token %d\n", tok_place );
					asm_retval = 2;
					goto asm_cleanup;
				}

				// clear strval
				FILL_STRING( '\0', tok_next.strval, 128 );

				// advance token - second argument
				if( machine_data.valid_instr[ md_i ].argc == 2 ) {
					tok_next = _get_token( f_buffer, buf_place );

					// check for valid symbol
					for( int sym_i = 0; sym_i < MAX_SYM; sym_i++ ) {
						if( symtable.entries[ sym_i ].name[ 0 ] == 0 ) continue;
						// but we don't need to reference the address of tok_next.strval since it is
						// capable of being treated like char*. see comment below @ checking for EOF
						if( strcmp( tok_next.strval, symtable.entries[ sym_i ].name ) == 0 ) {
							// copy string
							memcpy( &tok_op_master[ tok_place ].args[ 1 ].data, symtable.entries[ sym_i ].name, 128 );
							// 2 bytes because the cb bin address could be 2 bytes
							symtable.program_bytes += 2;
							// not ARG_ERR
							tok_op_master[ tok_place ].args[ 1 ].type = ARG_INTEGER;
							// copy symtable index that relates to this instruction
							tok_op_master[ tok_place ].nsym = sym_i;
							// don't abort later
							sym_arg1 = true;
							break;
						}
					}

					// search for arguments based on instruction arg types
					if( machine_data.valid_instr[ md_i ].arg1t & 0b00000001 ) {
						// ARG_ADDRESS
						tok_arg_found.arg1_addr = _asm_address( tok_next, tok_op_master, tok_place, 1 );
					}
					if( machine_data.valid_instr[ md_i ].arg1t & 0b00000010 ) {
						// "d" can process strings, but only singular characters are allowed in other instructions
						// TODO maybe forget about strings except for "d"? chars will be made into ints in the cb anyway
						// 8/12/23 ^ this is how the program works for now, strings are simply too long to store
						// ARG_STRING
						if( machine_data.valid_instr[ md_i ].instr_num == I_DEF ) {
							tok_arg_found.arg1_str = _asm_string( tok_next, tok_op_master, tok_place, 1 );
						}
						// ARG_STRING (but char type, too lazy to shift around arg types)
						else {
							tok_arg_found.arg1_str = _asm_char( tok_next, tok_op_master, tok_place, 1 );
						}
					}
					if( machine_data.valid_instr[ md_i ].arg1t & 0b00000100 ) {
						// ARG_INTEGER
						tok_arg_found.arg1_int = _asm_integer( tok_next, tok_op_master, tok_place, 1 );
					}
					if( machine_data.valid_instr[ md_i ].arg1t & 0b00001000 ) {
						// ARG_REGISTER
						tok_arg_found.arg1_reg = _asm_register( tok_next, tok_op_master, tok_place, 1 );
					}

					// error if no arguments were found
					if( !tok_arg_found.arg1_reg &&
						!tok_arg_found.arg1_int &&
						!tok_arg_found.arg1_str &&
						!tok_arg_found.arg1_addr &&
						tok_op_master[ tok_place ].instruction != I_NOP &&
						!sym_arg1 ) { // hard to read - "if" statement ends here
						printf( "Error reading argument 1: no argument was found. token %d\n", tok_place );
						asm_retval = 2;
						goto asm_cleanup;
					}

					// assume a second argument was found
					symtable.program_bytes++;

					// error check: if a valid argument was not found, bail
					if( tok_op_master[ tok_place ].args[ 1 ].type == ARG_ERR ) {
						printf( "Error reading argument 1: token %d\n", tok_place );
						asm_retval = 2;
						goto asm_cleanup;
					}
				}

				// check for EOF - must be defined properly in file
				if( tok_op_master[ tok_place ].instruction == I_DEF ) {
					// makes it cleaner i guess
					size_t data = tok_op_master[ tok_place ].args[ 0 ].data;
					// i've learned with size_t to reference the address of the data, not just the data
					if( strcmp( &data, "eof" ) == 0 ) {
						f_eof = true;
					}
				}

				// incrememnt token number
				tok_place++;
			}
		}
	}

	// print token data
	for( int i = 0; i < tok_place; i++ ) {
		// print initial information: token num, instruction
		V_PRINT( "final token %d:\n", i );
		V_PRINT( "\tinstruction = %d (%s)\n", tok_op_master[ i ].instruction, machine_data.valid_instr[ tok_op_master[ i ].instruction ].instr_str );
		V_PRINT( "\targ0 data = " );
		// test arg0 type & format printing properly
		switch( tok_op_master[ i ].args[ 0 ].type ) {
		case ARG_REGISTER:
			// register: print string containing register name
			// this is converted into a register id in the bin section
			V_PRINT( "%zs (reg)\n", tok_op_master[ i ].args[ 0 ].data );
			break;
		case ARG_INTEGER:
			// integer: print hex value
			V_PRINT( "%zX (int)\n", tok_op_master[ i ].args[ 0 ].data );
			break;
		case ARG_STRING:
			// string: print string if instruction is "d"
			if( tok_op_master[ i ].instruction == I_DEF ) {
				V_PRINT( "%zs (str)\n", tok_op_master[ i ].args[ 0 ].data );
			} else {
				// character: print single character otherwise
				V_PRINT( "%c (chr)\n", tok_op_master[ i ].args[ 0 ].data );
			}
			break;
		case ARG_ADDRESS:
			// address: print hex value
			V_PRINT( "%zX (addr)\n", tok_op_master[ i ].args[ 0 ].data );
			break;
		default:
			// ARG_NULL may be a symbol. check the symbol table for corresponding data (only if a symbol is referenced in the token)
			if( strcmp( &tok_op_master[ i ].args[ 0 ].data, symtable.entries[ tok_op_master[ i ].nsym ].name ) == 0 && tok_op_master[ i ].nsym > -1 ) {
				V_PRINT( "%zs (symbol)\n", &tok_op_master[ i ].args[ 0 ].data );
			} else {
				// there is no value in the argument data
				V_PRINT( "[no value]\n");
			}
			break;
		}
		V_PRINT( "\targ1 data = " );
		// test arg1 type & format printing properly
		switch( tok_op_master[ i ].args[ 1 ].type ) {
		case ARG_REGISTER:
			// register: print string containing register name
			// this is converted into a register id in the bin section
			V_PRINT( "%zs (reg)\n", tok_op_master[ i ].args[ 1 ].data );
			break;
		case ARG_INTEGER:
			// integer: print hex value
			V_PRINT( "%zX (int)\n", tok_op_master[ i ].args[ 1 ].data );
			break;
		case ARG_STRING:
			// string: print string if instruction is "d"
			if( tok_op_master[ i ].instruction == I_DEF ) {
				V_PRINT( "%zs (str)\n", tok_op_master[ i ].args[ 1 ].data );
			} else {
				// character: print single character otherwise
				V_PRINT( "%c (chr)\n", tok_op_master[ i ].args[ 1 ].data );
			}
			break;
		case ARG_ADDRESS:
			// address: print hex value
			V_PRINT( "%zX (addr)\n", tok_op_master[ i ].args[ 1 ].data );
			break;
		default:
			// ARG_NULL may be a symbol. check the symbol table for corresponding data (only if a symbol is referenced in the token)
			if( strcmp( &tok_op_master[ i ].args[ 1 ].data, symtable.entries[ tok_op_master[ i ].nsym ].name ) == 0 && tok_op_master[ i ].nsym > -1 ) {
				V_PRINT( "%zs (symbol)\n", &tok_op_master[ i ].args[ 1 ].data );
			} else {
				// there is no value in the argument data
				V_PRINT( "[no value]\n");
			}
			break;
		}
		V_PRINT( "\n" );
	}

	// done with these variables
	free( f_buffer );
	CloseHandle( f_file );
	
	// f_file_out currently contains data on whether the requested output file
	// exists or not
	HANDLE f_file_out = OpenFile( filename_out, f_file_of, OF_EXIST );

	// create file/rewrite existing file
	if( f_file_out ) {
		// file exists, rewrite
		f_file_out = CreateFileA( ( char* )filename_out,
			GENERIC_WRITE,
			FILE_SHARE_WRITE,
			NULL,
			CREATE_ALWAYS,
			FILE_ATTRIBUTE_NORMAL,
			NULL );
		ASSERT_RET( f_file, 1, "Error rewriting file.\n" );
	}
	else {
		// create file
		f_file_out = CreateFileA( ( char* )filename_out,
			GENERIC_WRITE,
			FILE_SHARE_WRITE,
			NULL,
			CREATE_ALWAYS,
			FILE_ATTRIBUTE_NORMAL,
			NULL );
		ASSERT_RET( f_file, 1, "Error creating new file.\n" );
	}

	// save the place in filename_out where just the file name is
	int filename_out_place = 0;
	// get just the file name, not including the path
	for( int i = 0; i < strlen( filename_out ); i++ ) {
		if( filename_out[ i ] == '\\' ) {
			filename_out_place = i;
		}
	}
	filename_out_place++;
	// line/col for printing the binary data (will become DHEX mode)
	int bin_line = 0;
	int bin_col = 0;

	V_PRINT( "\n\n" );
	// print the file name, excluding the path
	for( int i = filename_out_place; i < strlen( filename_out ); i++ ) {
		V_PRINT( "%c", filename_out[ i ] );
	}
	// print header + first row
	V_PRINT( "\n" );
	V_PRINT( "       0  1  2  3  4  5  6  7  8  9  a  b  c  d  e  f" );
	// first row label
	V_PRINT( "\n0000  " );

	// byte place - used for resolving addresses
	unsigned int bytes_place = 0;

	// check if a newline should be printed (reached column F)
	// this is for VERBOSE_ALL_DATA, but will be reused for RUN_DHEX
	#define CHECK_NEWLINE() if( bin_col > 15 ) { \
		bin_col = 0; \
		bin_line++; \
		V_PRINT( "\n" ); \
		if( bin_line < 0xF ) V_PRINT( "00%x  ", bin_line*0x10 ); \
		if( bin_line < 0xF0 && bin_line > 0xF ) V_PRINT( "0%x  ", bin_line*0x10 ); \
		if( bin_line < 0xF00 && bin_line > 0xF0 ) V_PRINT( "%x  ", bin_line*0x10 ); \
	}

	// version header
	// 00 - cnix-asm --syntax casm16-89
	// 01 - cnix-asm --syntax casm32
	_asm_fprintf( f_file, 0 );
	bin_col++;
	bytes_place++;
	// no need to check for newline, it'll never be at the end of the line

	// print binary data
	for( int i = 0; i < tok_place; i++ ) {
		/*
		sizes - 1 bit: byte (8 bits), word (16 bits)
		types - 2 bits: register, integer, string, address

		FORMAT: (bit 0)00000000(bit 7)

				  arg
		desc op   desc  etc data
		0-3  4-7  8-15  ... - desc describes how many more bytes qualify as each argument

		desc
		0 - arg0 type (00 - register, 01 - integer, 10 - string, 11 - address)
		1 - arg0 type  /\
		2 - arg1 type  |
		3 - arg1 type  |

		op
		4 - opcode
		5 - opcode
		6 - opcode
		7 - opcode

		arg desc
		8  - arg0 bytes (00-11, 0, 1, 2, or 4 bytes)
		9  - arg0 bytes  /\
		10 - arg1 bytes  |
		11 - arg1 bytes  |
		12 - argc (0 - 1 arg, 1 - 2 args)
		13 - reserved
		14 - reserved
		15 - reserved
		*/

		/*
		NOTE on how registers are stored.
		0 - special? (%fl, %pc, %sp, %ep, %cp)
		1 - extended? (%xa, %xb, %xc, %xd)
		2 - reg num (won't be used)
		3 - reg num (won't be used)
		4 - reg num (won't be used)
		5 - reg num
		6 - reg num
		7 - reg num
		*/
		if( tok_op_master[ i ].args[ 0 ].type == ARG_REGISTER ) {
			size_t data = tok_op_master[ i ].args[ 0 ].data;
			for( int k = 0; k < MAX_REG; k++ ) {
				if( strcmp( data, machine_data.valid_reg[ k ] ) == 0 ) {
					int temp = 0;
					if( k >= 4 && k < 8 ) {
						// extended
						temp = k - 4;
						temp |= 1 << 6;
					} else if( k >= 8 ) {
						// special
						temp = k - 8;
						temp |= 1 << 7;
					} else {
						// regular
						temp = k;
					}
					// rewrite data to register number
					tok_op_master[ i ].args[ 0 ].data = temp;
				}
			}
		}
		if( tok_op_master[ i ].args[ 1 ].type == ARG_REGISTER ) {
			size_t data = tok_op_master[ i ].args[ 1 ].data;
			for( int k = 0; k < MAX_REG; k++ ) {
				if( strcmp( data, machine_data.valid_reg[ k ] ) == 0 ) {
					int temp = 0;
					if( k >= 4 && k < 8 ) {
						// extended
						temp = k - 4;
						temp |= 1 << 6;
					} else if( k >= 8 ) {
						// special
						temp = k - 8;
						temp |= 1 << 7;
					} else {
						// regular
						temp = k;
					}
					// rewrite data to register number
					tok_op_master[ i ].args[ 1 ].data = temp;
				}
			}
		}

		unsigned char opcode = 0;
		// add instruction to opcode bits
		opcode |= tok_op_master[ i ].instruction;
		// add arg0 type to opcode bits
		// -1 because enum counting starts at 1 (ARG_NULL will only be zero for "nop", check anyway)
		// shift bits 4 to the left to avoid overwriting opcode info
		if( tok_op_master[ i ].args[ 0 ].type != ARG_NULL ) {
			opcode |= ( tok_op_master[ i ].args[ 0 ].type - 1 ) << 6;
		}
		// arg1 could be ARG_NULL, so we need to make sure this doesn't roll over
		if( tok_op_master[ i ].args[ 1 ].type != ARG_NULL ) {
			opcode |= ( tok_op_master[ i ].args[ 1 ].type - 1 ) << 4;
		}

		unsigned char argdesc = 0;
		// ARG0
		// data fits in one byte
		if( tok_op_master[ i ].args[ 0 ].data <= 0xFF ) argdesc |= 1 << 6; // 01
		// data fits in more than one byte (2 bytes assumed now)
		if( tok_op_master[ i ].args[ 0 ].data > 0xFF ) argdesc |= 1 << 7; // 10
		// data fits in more than two bytes
		if( tok_op_master[ i ].args[ 0 ].data > 0xFFFF ) argdesc |= 1 << 6; // 11
		// ARG1
		// data fits in one byte
		if( tok_op_master[ i ].args[ 1 ].data <= 0xFF ) argdesc |= 1 << 4; // 01
		// data fits in more than one byte (2 bytes assumed now)
		if( tok_op_master[ i ].args[ 1 ].data > 0xFF ) argdesc |= 1 << 5; // 10
		// data fits in more than two bytes
		if( tok_op_master[ i ].args[ 1 ].data > 0xFFFF ) argdesc |= 1 << 4; // 11
		// no unsigned integer rollover
		if( machine_data.valid_instr[ tok_op_master[ i ].instruction ].argc > 0 ) {
			argdesc |= ( machine_data.valid_instr[ tok_op_master[ i ].instruction ].argc - 1 ) << 3;
		}
		//printf( "\n\nargdesc = %x\n", argdesc );

		// skip "d" instruction for EOF
		if( tok_op_master[ i ].instruction == I_DEF && strcmp( ( char* )tok_op_master[ i ].args[ 0 ].data, "eof" ) == 0 ) {
			continue;
		}

		// check if arg0 data is a symbol
		if( strcmp( &tok_op_master[ i ].args[ 0 ].data, symtable.entries[ tok_op_master[ i ].nsym ].name ) == 0 &&
			symtable.entries[ tok_op_master[ i ].nsym ].address > 0 ) {
			// clear arg1 desc
			opcode &= 0b00111111;
			// replace it with address before being written to the file
			opcode |= 0b11000000;
			// clear arg0 part of argdesc
			argdesc &= 0b00111111;
			// replace it to describe two bytes
			argdesc |= 0b10000000;
		}
		// check if arg1 data is a symbol
		if( strcmp( &tok_op_master[ i ].args[ 1 ].data, symtable.entries[ tok_op_master[ i ].nsym ].name ) == 0 &&
			symtable.entries[ tok_op_master[ i ].nsym ].address > 0 ) {
			// clear arg1 desc
			opcode &= 0b11001111;
			// replace it with address before being written to the file
			opcode |= 0b00110000;
			// clear arg1 part of argdesc
			argdesc &= 0b11001111;
			// replace it to describe two bytes
			argdesc |= 0b00100000;
		}

		// print instruction hex opcode
		_asm_fprintf( f_file, opcode );
		bin_col++;
		bytes_place++;
		CHECK_NEWLINE();

		// print argdesc
		if( tok_op_master[ i ].instruction != I_DEF ) {
			_asm_fprintf( f_file, argdesc );
			bin_col++;
			bytes_place++;
			CHECK_NEWLINE();
		}

		// looks cleaner
		size_t arg0_data = tok_op_master[ i ].args[ 0 ].data;
		// check if data is a register, then process it appropriately
		if( tok_op_master[ i ].args[ 0 ].type == ARG_REGISTER ) {
			// write to file
			_asm_fprintf( f_file, arg0_data );
			// increment column
			bin_col++;
			// increment bytes
			bytes_place++;
			// check for a newline
			CHECK_NEWLINE();
		}
		else if( tok_op_master[ i ].args[ 0 ].type == ARG_STRING ) {
			// convert character into its respective ascii value
			for( int k = 33; k < MAX_ASCII; k++ ) {
				if( arg0_data == ascii_table[ k ] ) {
					// write to file
					_asm_fprintf( f_file, k );
					// increment column
					bin_col++;
					// increment bytes
					bytes_place++;
					// check for a newline
					CHECK_NEWLINE();
					// stop looping
					break;
				}
			}
		}
		else {
			// check if data is a symbol
			if( strcmp( &tok_op_master[ i ].args[ 0 ].data, symtable.entries[ tok_op_master[ i ].nsym ].name ) == 0 &&
				symtable.entries[ tok_op_master[ i ].nsym ].address > 0 ) {
				// looks cleaner
				// +1 because the version number is now at the head of the cb binary file.
				unsigned int sym_data = symtable.entries[ tok_op_master[ i ].nsym ].address + 1;
				// check if data is more than 1 byte
				if( sym_data > 0xFF ) {
					// write to file (mask first byte)
					_asm_fprintf( f_file, sym_data & 0xFF );
					// increment column
					bin_col++;
					// increment bytes
					bytes_place++;
					// check for newline
					CHECK_NEWLINE();

					// write to file (mask second byte)
					_asm_fprintf( f_file, ( sym_data >> 8 ) & 0xFF );
					// increment column
					bin_col++;
					// increment bytes
					bytes_place++;
					// check for newline
					CHECK_NEWLINE();
				} else {
					// data is not more than 1 byte
					// write to file (mask first byte anyway if the check goes wrong)
					_asm_fprintf( f_file, sym_data & 0xFF );
					// increment column
					bin_col++;
					// increment bytes
					bytes_place++;
					// check for newline
					CHECK_NEWLINE();

					// write to file (symbol is always 2 bytes in size, so force it to be 2 bytes)
					_asm_fprintf( f_file, 0 );
					// increment column
					bin_col++;
					// increment bytes
					bytes_place++;
					// check for newline
					CHECK_NEWLINE();
				}
			}
			else {
				// data is anything else
				// write to file (mask first byte of n bytes)
				_asm_fprintf( f_file, arg0_data & 0xFF );
				// increment column
				bin_col++;
				// increment bytes
				bytes_place++;
				// check for newline
				CHECK_NEWLINE();

				// 1 byte has been written so far
				int bytec = 1;
				// loop until arg0_data is less than 0xFF
				while( arg0_data > BYTE_MAX ) {
					// one more byte will be written
					bytec++;
					// shift by one byte
					arg0_data >>= 8;
					
					// write to file (mask nth byte)
					_asm_fprintf( f_file, arg0_data & 0xFF );
					// increment column
					bin_col++;
					// increment bytes
					bytes_place++;
					// check for newline
					CHECK_NEWLINE();
				}
				// check if this instruction's argdesc specifies 4 bytes
				// (it's possible only 3 were written since 3 bytes is not
				// an option for a size specifier)
				if( ( ( argdesc & 0b00110000 ) >> 6 ) == 3 ) {
					// check if 4 bytes were written
					if( bytec < 4 ) {
						// write to file (just a zero to take up space & make the space 4 bytes)
						_asm_fprintf( f_file, 0 );
						// increment column
						bin_col++;
						// increment bytes
						bytes_place++;
						// check for newline
						CHECK_NEWLINE();
					}
				}
			}
		}

		// looks cleaner
		size_t arg1_data = tok_op_master[ i ].args[ 1 ].data;
		// only check if this instruction's arg count (argc) is more than 1
		if( machine_data.valid_instr[ tok_op_master[ i ].instruction ].argc > 1 ) {
			// check if data is a register, then process it appropriately
			if( tok_op_master[ i ].args[ 1 ].type == ARG_REGISTER ) {
				// write to file
				_asm_fprintf( f_file, arg1_data );
				// increment column
				bin_col++;
				// increment bytes
				bytes_place++;
				// check for newline
				CHECK_NEWLINE();
			}
			else if( tok_op_master[ i ].args[ 1 ].type == ARG_STRING ) {
				// convert character into its respective ascii value
				for( int k = 33; k < MAX_ASCII; k++ ) {
					if( arg1_data == ascii_table[ k ] ) {
						// write to file
						_asm_fprintf( f_file, k );
						// increment column
						bin_col++;
						// increment bytes
						bytes_place++;
						// check for newline
						CHECK_NEWLINE();
						// stop looking
						break;
					}
				}
			}
			else {
				// check if data is a symbol
				if( strcmp( &tok_op_master[ i ].args[ 1 ].data, symtable.entries[ tok_op_master[ i ].nsym ].name ) == 0 &&
					symtable.entries[ tok_op_master[ i ].nsym ].address > 0 ) {
					// looks cleaner
					// +1 because the version number is now at the head of the cb binary file.
					unsigned int sym_data = symtable.entries[ tok_op_master[ i ].nsym ].address + 1;
					// check if data is more than 1 byte
					if( sym_data > 0xFF ) {
						// write to file (mask first byte)
						_asm_fprintf( f_file, sym_data & 0xFF );
						// increment column
						bin_col++;
						// increment bytes
						bytes_place++;
						// check for newline
						CHECK_NEWLINE();

						// write to file (mask second byte)
						_asm_fprintf( f_file, ( sym_data >> 8 ) & 0xFF );
						// increment column
						bin_col++;
						// increment bytes
						bytes_place++;
						// check for newline
						CHECK_NEWLINE();
					}
					else {
						// data is not more than one byte
						// write to file (mask first byte anyway)
						_asm_fprintf( f_file, sym_data & 0xFF );
						// increment column
						bin_col++;
						// increment bytes
						bytes_place++;
						// check for newline
						CHECK_NEWLINE();

						// write to file (just a zero to take up space & make the space 4 bytes)
						_asm_fprintf( f_file, 0 );
						// increment column
						bin_col++;
						// increment bytes
						bytes_place++;
						// check for newline
						CHECK_NEWLINE();
					}
				}
				else {
					// data is anything else
					// write to file (mask first byte of n bytes)
					_asm_fprintf( f_file, arg1_data & 0xFF );
					// increment columns
					bin_col++;
					// increment bytes
					bytes_place++;
					// check for newline
					CHECK_NEWLINE();

					// one byte has been written
					int bytec = 1;
					// loop as long as arg1_data is greater than 0xFF
					while( arg1_data > BYTE_MAX ) {
						// one more byte has been written
						bytec++;
						// shift by one byte
						arg1_data >>= 8;
						
						// write to file (mask nth byte)
						_asm_fprintf( f_file, arg1_data & 0xFF );
						// increment column
						bin_col++;
						// increment bytes
						bytes_place++;
						// check for newline
						CHECK_NEWLINE();
					}
					// check if this instruction's argdesc specifies 4 bytes
					// (it's possible only 3 were written since 3 bytes is not
					// an option for a size specifier)
					if( ( ( argdesc & 0b00110000 ) >> 4 ) == 3 ) {
						// check if 4 bytes were written
						if( bytec < 4 ) {
							// write to file (just a zero to take up space & make the space 4 bytes)
							_asm_fprintf( f_file, 0 );
							// increment column
							bin_col++;
							// increment bytes
							bytes_place++;
							// check for newline
							CHECK_NEWLINE();
						}
					}
				}
			}
		}
	}

	// close the file handle to avoid errors or unnecessary memory usage
	CloseHandle( f_file_out );

	// originally this was going to contain many more free() statements
	// but optimizations have been made.
	// now, it simply returns control to the main() function in main.c
	// while free()ing the remaining items
asm_cleanup:
	free( f_file_of );
	free( tok_op_master );
	return asm_retval;
	// over 1k lines with comments lol
}

/*
* Purpose:
*	Find individual tokens from a file. Keep the place where the buffer
*	was being read from and stop at special delimiting characters (space,
*	newline, carriage return, comma, null-terminator, and colon).
* 
* Parameters:
*	buffer - the buffer containing the file data read earlier
*	place - the point in the buffer to continue reading from
* 
* Returns:
*	Next token found in file buffer, delimited by several characters
*	(see "purpose" for delimiter list).
*	exits upon memory allocation error (code 3)
*/
static tok_iteration_t _get_token( char* buffer, int place ) {
	// allocate data for a temporary variable
	char* temp = calloc( 128, sizeof( char ) );
	// make sure calloc() didn't fail
	ASSERT( temp, 3, "[MEMORY ERROR] _get_token: temp failed to allocate.\n" );

	// variables will be used outside of the scope of the "for" loop
	// i begins at the place passed by the caller
	int i = place, k = 0;

	// loop through buffer to find delimiting characters
	for( ; i < strlen( buffer ); i++, k++ ) {
		if( buffer[ i ] == ' ' ) {
			break; // space
		}
		if( buffer[ i ] == '\n' || buffer[ i ] == '\r' ) {
			// increment beyond this character
			i++;
			break; // newline / carriage return
		}
		// increment + break: this can cause problems -> "mov %a,$0F" vs "mov %a, $0F" only the right-most works with this method
		// ^ maybe fixed this - nope not anymore
		if( buffer[ i ] == ',' ) {
			// increment beyond this character
			i++;
			break; // comma
		}
		if( buffer[ i ] == '\0' ) { // don't read past the buffer
			break;
		}
		if( buffer[ i-1 ] == ':' ) {
			// increment beyond this character
			i++;
			break; // colon
		}
		// add character data to temp variable
		temp[ k ] = buffer[ i ];
	}
	// move i to the next character
	i++;
	// set the end of the temporary variable to be null-terminated
	// "k" was incremented but the "for" loop was broken before any data
	// was written to this position, so it is blank.
	temp[ k ] = '\0';

	// initialize return value
	tok_iteration_t retval = { 0 };

	// populate return value string with temp data found from buffer
	FILL_STRING_S( temp, retval.strval, 128 );
	// set the place to continue reading the buffer from
	retval.place = i;

	// data can be overwritten, strings and numbers have been copied with
	// pointer-proof mechanisms
	free( temp );
	// return token data
	return retval;
}

/*
* Purpose:
*	Find integer literals in instruction arguments and
*	save them appropriately.
* 
* Parameters:
*	tok_next - most recent token data
*	tok_op_master - master token storage variable
*	tok_place - which index of tok_op_master to modify
*	argno - which argument is being checked
* 
* Returns:
*	false (0) - something prevented the function from finding its desired data
*	true (1) - the function found data that fits its criteria
*	exits upon memory allocation error (code 3)
*/
static bool _asm_integer( tok_iteration_t tok_next, asm_token_op_t* tok_op_master, int tok_place, int argno ) {
	// allocate data for a temporary variable
	char* temp = calloc( 128, sizeof( char ) );
	// make sure calloc() didn't fail
	ASSERT( temp, 3, "[MEMORY ERROR] _asm_integer: temp failed to allocate.\n" );

	// check if the first character is "$" - this must be true for all integer literals
	if( tok_next.strval[ 0 ] == '$' ) {
		// populate temp variable with token data
		FILL_STRING_S( tok_next.strval, temp, 128 );

		// get rid of $ so we can just use the raw numbers for conversion
		temp[ 0 ] = '0';

		// all numbers are treated as hex by default, not prefixed by "0x"
		// so using radix/base 16 is fine.
		unsigned long long num = strtoul( temp, NULL, 16 );

		// set type appropriately
		tok_op_master[ tok_place ].args[ argno ].type = ARG_INTEGER;
		// set data
		tok_op_master[ tok_place ].args[ argno ].data = num;

		// this can be overwritten since the data is already set and
		// it is not a pointer and cannot be treated like a pointer
		free( temp );
		// success
		return true;
	}
	// free memory, function failed
	free( temp );
	// fail by default
	return false;
}

/*
* Purpose:
*	Find a valid register name in an instruction's arguments. The string value
*	of the register is put into the data, which will be translated into a register
*	identifier when the binary data is compiled.
* 
* Parameters:
*	tok_next - most recent token data
*	tok_op_master - master token storage variable
*	tok_place - which index of tok_op_master to modify
*	argno - which argument is being checked
* 
* Returns:
*	false (0) - something prevented the function from finding its desired data
*	true (1) - the function found data that fits its criteria
*	exits upon memory allocation error (code 3)
*/
static bool _asm_register( tok_iteration_t tok_next, asm_token_op_t* tok_op_master, int tok_place, int argno ) {
	// loop through valid registers
	for( int i = 0; i < MAX_REG; i++ ) {
		// check if token data and register names match
		if( strcmp( tok_next.strval, machine_data.valid_reg[ i ] ) == 0 ) {
			// found a register
			// set type accordingly
			tok_op_master[ tok_place ].args[ argno ].type = ARG_REGISTER;
			// set data accordinly. this will be changed in the final compilation section
			tok_op_master[ tok_place ].args[ argno ].data = machine_data.valid_reg[ i ];
			// success
			return true;
		}
	}
	// fail - criteria not found
	return false;
}

/*
* Purpose:
*	Validate strings of more than one character and store
*	appropriately. This is not currently used outside of
*	checking for the end of the file (d "eof")
* 
* Parameters:
*	tok_next - most recent token data
*	tok_op_master - master token storage variable
*	tok_place - which index of tok_op_master to modify
*	argno - which argument is being checked
* 
* Returns:
*	false (0) - something prevented the function from finding its desired data
*	true (1) - the function found data that fits its criteria
*	exits upon memory allocation error (code 3) and invalid syntax (code 2)
*/
static bool _asm_string( tok_iteration_t tok_next, asm_token_op_t* tok_op_master, int tok_place, int argno ) {
	// allocate data for a temporary variable
	char* temp = calloc( 128, sizeof( *temp ) );
	// make sure calloc() didn't fail
	ASSERT( temp, 3, "[MEMORY ERROR] _asm_string: temp failed to allocate.\n" );

	// initialize string so null-terminators occur at the correct spots
	FILL_STRING( '\0', temp, 128 );
	// check if the first character is >"<
	if( tok_next.strval[ 0 ] == '"' ) {
		// make sure the last character is also >"<, abort if not
		if( tok_next.strval[ strlen( tok_next.strval )-1 ] == '"' ) {
			// loop through string data and store in temp
			// only save the string inside quotes, so leave one
			// off the start and end (this is why i = 1 and the string
			// length - 1 is used).
			for( int i = 1; i < strlen( tok_next.strval )-1; i++ ) {
				// for example: temp[ 0 ] = tok_next.strval[ 1 ]
				// skips the quotes on both sides
				temp[ i-1 ] = tok_next.strval[ i ];
			}
			// set type accordingly
			tok_op_master[ tok_place ].args[ argno ].type = ARG_STRING;
			// set data accordingly
			tok_op_master[ tok_place ].args[ argno ].data = temp;

			// don't free here because calloc() will allocate the same
			// free memory for some reason. if temp is free()'d the data
			// gets overwritten.
			// success
			return true;
		} else {
			// syntax error
			printf( "Invalid string detected (str) - missing quote?: token %d argument %d (likely line %d)\n", tok_place, argno+1, tok_place+1 );
			exit( 2 );
		}
	}
	// data can be rewritten, it wasn't used
	free( temp );
	// fail by default
	return false;
}

/*
* Purpose:
*	Search for single characters surrounded by "" or ''
*	in a token provided by the caller.
* 
* Parameters:
*	tok_next - most recent token data
*	tok_op_master - master token storage variable
*	tok_place - which index of tok_op_master to modify
*	argno - which argument is being checked
* 
* Returns:
*	false (0) - something prevented the function from finding its desired data
*	true (1) - the function found data that fits its criteria
*	exits upon memory allocation error (code 3) and invalid syntax (code 2)
*/
static bool _asm_char( tok_iteration_t tok_next, asm_token_op_t* tok_op_master, int tok_place, int argno ) {
	// check if the first character is >"< or >'<
	if( tok_next.strval[ 0 ] == '"' ||
		tok_next.strval[ 0 ] == '\'' ) {
		// check if the last character is >"< or >'< - there should only be three total characters
		if( tok_next.strval[ 2 ] == '"' ||
			tok_next.strval[ 2 ] == '\'' ) {
			// set the argument type appropriately
			tok_op_master[ tok_place ].args[ argno ].type = ARG_STRING;

			// this will be used to determine whether a valid ascii character was found
			bool found = false;
			// loop through valid ascii characters table - start at index 33 because
			// the first 32 are NULL symbols
			for( int i = 33; i < MAX_ASCII; i++ ) {
				// check if there is a match
				if( tok_next.strval[ 1 ] == ascii_table[ i ] ) {
					// set data accordinly
					tok_op_master[ tok_place ].args[ argno ].data = ascii_table[ i ];
					// ascii character found
					found = true;
				}
			}
			// check if a valid character was found. fail if not
			if( !found ) return false;
			// success
			return true;
		} else {
			// syntax error
			printf( "Invalid string detected (chr) - missing quote?: token %d argument %d (likely line %d)\n", tok_place, argno+1, tok_place+1 );
			exit( 2 );
		}
	}
	// fail by default
	return false;
}

/*
* Purpose:
*	Find valid addresses in instruction arguments, specified by "#"
*	preceeding a hexadecimal integer.
* 
* Parameters:
*	tok_next - most recent token data
*	tok_op_master - master token storage variable
*	tok_place - which index of tok_op_master to modify
*	argno - which argument is being checked
* 
* Returns:
*	false (0) - something prevented the function from finding its desired data
*	true (1) - the function found data that fits its criteria
*	exits upon memory allocation error (code 3) and invalid syntax (code 2)
*/
static bool _asm_address( tok_iteration_t tok_next, asm_token_op_t* tok_op_master, int tok_place, int argno ) {
	// allocate data for a temporary variable
	char* temp = calloc( 128, sizeof( *temp ) );
	// make sure calloc() didn't fail
	ASSERT( temp, 3, "[MEMORY ERROR] _asm_address: temp failed to allocate.\n" );

	// check if the first character is "#" - addresses must start with this
	if( tok_next.strval[ 0 ] == '#' ) {
		// make sure a valid character exists at the next location
		if( !tok_next.strval[ 1 ] ) {
			// syntax error
			printf( "Error processing address: token %d argument %d (likely line %d)\n", tok_place, argno+1, tok_place+1 );
			exit( 2 );
		}
		// populate temp variable with token data
		FILL_STRING_S( tok_next.strval, temp, 128 );

		// get rid of # so we can just use the raw numbers for conversion
		temp[ 0 ] = '0';

		// all numbers are treated as hex by default, not prefixed by "0x"
		// so using radix/base 16 is fine.
		unsigned long long num = strtoul( temp, NULL, 16 );

		// set argument type
		tok_op_master[ tok_place ].args[ argno ].type = ARG_ADDRESS;
		// set argument data
		tok_op_master[ tok_place ].args[ argno ].data = num;

		// temp can be rewritten since the data is not saved to a pointer
		// or a variable that can act like a pointer
		free( temp );
		// success
		return true;
	}
	// free memory - even if the data was needed the function has failed,
	// so we can reuse it
	free( temp );
	// fail by default
	return false;
}

/*
* Purpose:
*	Find the address and name of all symbols in the file. Symbols are found
*	before this function is called, but only the name. This allows symbols
*	to be referenced before they appear in the file, preventing scope issues.
* 
* Parameters:
*	tok_next - most recent token data
*	tok_op_master - master token storage variable
*	tok_place - which index of tok_op_master to modify
*	argno - which argument is being checked
* 
* Returns:
*	false (0) - something prevented the function from finding its desired data
*	true (1) - the function found data that fits its criteria
*	exits upon memory allocation error (code 3)
*/
static bool _asm_symbol( tok_iteration_t tok_next, asm_token_op_t* tok_op_master, int tok_place, int argno ) {
	// allocate data for a temporary variable
	char* temp = calloc( 128, sizeof( *temp ) );
	// make sure calloc() didn't fail
	ASSERT( temp, 3, "[MEMORY ERROR] _asm_symbol: temp failed to allocate.\n" );

	// check if the last character is a colon
	if( tok_next.strval[ strlen( tok_next.strval )-1 ] == ':' ) {
		// fill temp variable with token data
		FILL_STRING_S( tok_next.strval, temp, 128 );

		// set the end of the string (colon) to null terminator
		temp[ strlen( temp )-1 ] = 0;

		// loop through symbol table entry addresses
		for( int i = 0; i < MAX_SYM; i++ ) {
			// check if the entry address is equal to the current byte count (same symbol)
			if( symtable.entries[ i ].address == symtable.program_bytes ) {
				// fail, duplicate symbol
				return false;
			}
		}

		// populate symbol table entry name field with temp variable data
		FILL_STRING_S( temp, symtable.entries[ symtable.place ].name, 128 );

		// set address to current program bytes
		symtable.entries[ symtable.place ].address = symtable.program_bytes;
		// set the current token symbol reference to the current symbol table entry
		//tok_op_master[ tok_place ].nsym = symtable.place;

		// success
		return true;
	}
	// free data, function failed
	free( temp );
	// fail by default
	return false;
}

/*
* Purpose:
*	Write data to the standard output if VERBOSE_DATA_ALL is defined, and write data
*	to a specified file. Contains checks to make sure single digits are prefixed with
*	a "dummy" zero.
* 
* Parameters:
*	f_file - file to write to
*	_input - data to write
* 
* Returns:
*	1 - success
*	exits program upon error (code 3 for memory errors, others for WIN32 WriteFile() errors)
*/
static int _asm_fprintf( HANDLE f_file, int _input ) {
	// allocate data for a temporary variable
	char* bin_itoa_buf = calloc( 128, sizeof( *bin_itoa_buf ) );
	// make sure calloc() didn't fail
	ASSERT( bin_itoa_buf, 3, "[MEMORY ERROR] _asm_fprintf: bin_itoa_buf failed to allocate.\n" );

	// by default, there are 2 characters (bytes) to write to the file
	int char2write = 2;
	// check if _input will only be one digit
	if( _input < 0x10 ) {
		// if true, put a dummy zero
		V_PRINT( "0" );
		ASSERT( WriteFile( f_file, ( char* )"0", 1, NULL, NULL ), 1, "Failed to write byte to file.\n" );
		// now, only 1 character (byte) needs to be writte
		char2write = 1;
	}
	// VERBOSE_DATA_ALL print data
	V_PRINT( "%x ", _input );
	// convert int to string (stored in bin_itoa_buf, base 16)
	_itoa_s( _input, bin_itoa_buf, 128, 16 );
	// write number (now character) to file
	ASSERT( WriteFile( f_file, bin_itoa_buf, char2write, NULL, NULL ), 1, "Failed to write byte to file.\n" );
	// amend a space afterward
	ASSERT( WriteFile( f_file, ( char* )" ", 1, NULL, NULL), 1, "Failed to write byte to file.\n");
	// this doesn't need to return anything, but return success anyway.
	// not zero because when checking in an "if" statement, it will be
	// evaluated as false.
	return 1;
}
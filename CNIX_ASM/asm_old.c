#define _CRT_SECURE_NO_WARNINGS
#include"asm.h"

/*

% - register
ex: %a

$ - integer
ex: $0F

"" - string
ex: "test"

() - math
ex: (1 + 2)

@ - compiler symbol
ex: @macro

# - memory address
ex: #0EFF0

*/

#define MAX_REG 10
#define NUM_TOKENS 512

// internal functions
tok_iteration_t _get_token( char* buffer, int place );
bool _asm_integer( tok_iteration_t tok_next, asm_token_op_t *tok_op_master, int tok_place, int argno );
bool _asm_register( tok_iteration_t tok_next, asm_token_op_t *tok_op_master, int tok_place, int argno );
bool _asm_string( tok_iteration_t tok_next, asm_token_op_t *tok_op_master, int tok_place, int argno );
bool _asm_address( tok_iteration_t tok_next, asm_token_op_t* tok_op_master, int tok_place, int argno );

const char* valid_reg[ MAX_REG ] = {
	"%a", "%b", "%c", "%d", // regular - 16-bits
	"%xa", "%xb", "%xc", "%xd", // extended - 32-bits
	"%pc", "%fl" // cpu-managed registers
};
const char* debug_instr[ 14 ] = {
	"NOP", "d", "mov", "add", "sub", "mul",
	"div", "shl", "shr", "in", "out",
	"cmp", "push", "pop"
};

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
int asm( char* filename_in, char* filename_out ) {
	int asm_retval = 0;

	LPOFSTRUCT f_of_struct = calloc( 64, sizeof( LPOFSTRUCT ) ); // error check below
	HANDLE* f_file = OpenFile( filename_in, f_of_struct, OF_READ );
	int f_filesize = GetFileSize( f_file, NULL );
	char* f_buffer = calloc( f_filesize, sizeof( char ) ); // error check below
	asm_token_op_t* tok_op_master = calloc( NUM_TOKENS, sizeof( asm_token_op_t ) ); // error check below
	asm_token_arg_found_t tok_arg_found;

	// check for calloc errors
	if( !f_of_struct || !f_buffer || !tok_op_master ) {
		asm_retval = 3;
		goto asm_cleanup;
	}
	// read file, error ifit returns false
	if( ReadFile( f_file, f_buffer, f_filesize, NULL, NULL) ) {
		printf( "Successfully read file.\n" );
	} else {
		printf( "Error reading file.\n" );
		asm_retval = 1;
		goto asm_cleanup;
	}

	// pointers used here need to be for sure initialized
	// so we do it after checking the first several pointers
	// initialize tok_op_master
	/*
	bool tok_op_master_calloc0[ NUM_TOKENS ];
	bool tok_op_master_calloc1[ NUM_TOKENS ];
	for( int i = 0; i < NUM_TOKENS; i++ ) {
	// assume it worked unless it didn't
	tok_op_master_calloc0[ i ] = true;
	tok_op_master_calloc1[ i ] = true;
	//tok_op_master[ i ].instruction = I_INIT; // initialization value, separate from error
	tok_op_master[ i ].instruction = I_ERR; // just use the error value, can't hurt anything(?)
	tok_op_master[ i ].args[ 0 ].type = ARG_ERR;
	tok_op_master[ i ].args[ 0 ].data = 0;
	tok_op_master[ i ].args[ 1 ].type = ARG_ERR;
	tok_op_master[ i ].args[ 1 ].data = 0;
	/*
	if( !tok_op_master[ i ].args[ 0 ].data ) {
	tok_op_master_calloc0[ i ] = false; // fail
	asm_retval = 3;
	goto asm_cleanup;
	}
	if( !tok_op_master[ i ].args[ 1 ].data ) {
	tok_op_master_calloc1[ i ] = false; // fail
	asm_retval = 3;
	goto asm_cleanup;
	}

	}
	*/

	bool f_eof = false;
	int buf_place = 0, tok_place = 0;
	while( !f_eof ) {
		// initialize tok_arg_found
		tok_arg_found.arg0_reg = false;
		tok_arg_found.arg0_int = false;
		tok_arg_found.arg0_str = false;
		tok_arg_found.arg1_reg = false;
		tok_arg_found.arg1_int = false;
		tok_arg_found.arg1_str = false;

		tok_iteration_t tok_next = _get_token( f_buffer, buf_place );
		/*
		if( strcmp( tok_next.strval, "eof" ) == 0 ) {
		f_eof = true;
		}
		*/
		buf_place = tok_next.place;

		//printf( "[] tok_place = %d\n", tok_place );
		printf( "[%d] tok_next.strval = '%s'\n", tok_place, *tok_next.strval );

		// 0x01
		// d [symbol]
		if( strcmp( tok_next.strval, "d" ) == 0 ) {
			// initialize data
			tok_op_master[ tok_place ].instruction = I_DEF;
			// clear strval so it doesn't bleed into the next token
			*tok_next.strval = 0;
			// advance token (to st. james place)
			tok_next = _get_token( f_buffer, buf_place );
			buf_place = tok_next.place;

			// search for all possible arg types
			tok_arg_found.arg0_reg = _asm_register( tok_next, tok_op_master, tok_place, 0 );
			tok_arg_found.arg0_int = _asm_integer( tok_next, tok_op_master, tok_place, 0 );
			tok_arg_found.arg0_str = _asm_string( tok_next, tok_op_master, tok_place, 0 );
			tok_arg_found.arg0_addr = _asm_address( tok_next, tok_op_master, tok_place, 0 );

			//printf( "%s\n", tok_next.strval );
			//printf( "%s\n", tok_op_master[ tok_place ].args[ 0 ].data );

			// error checking - register not allowed
			if( tok_arg_found.arg0_reg ) {
				printf( "Error reading argument 0: token %d\n", tok_place );
				printf( ASM_INVL_ARG( "d", "reg", 0 ) );
				asm_retval = 2;
				goto asm_cleanup;
			}
			//printf( "%s\n", tok_op_master[ tok_place ].args[ 0 ].data );
			// error checking - address not allowed
			if( tok_arg_found.arg0_addr ) {
				printf( "Error reading argument 0: token %d\n", tok_place );
				printf( ASM_INVL_ARG( "d", "addr", 0 ) );
			}

			// error check: if a valid argument was not found, bail
			if( tok_op_master[ tok_place ].args[ 0 ].type == ARG_ERR ) {
				printf( "Error reading argument 0: token %d\n", tok_place );
				asm_retval = 2;
				goto asm_cleanup;
			}

			// end of file EOF
			if( strcmp( tok_op_master[ tok_place ].args[ 0 ].data, "eof" ) == 0 ) {
				f_eof = true;
			}
			// add more later
			else {
				printf( "Error reading argument 1: token %d\n", tok_place );
				printf( "Invalid definition ('d ...').\n" );
				asm_retval = 2;
				goto asm_cleanup;
			}

			// clear strval
			*tok_next.strval = 0;

			// there is no second argument.
			/*
			// advance token - second argument
			tok_next = _get_token( f_buffer, buf_place );
			buf_place = tok_next.place;
			printf( "%s\n", tok_next.strval );

			// search for all possible arg types
			tok_arg_found.arg1_reg = _asm_register( tok_next, tok_op_master, tok_place, 1 );
			tok_arg_found.arg1_int = _asm_integer( tok_next, tok_op_master, tok_place, 1 );
			tok_arg_found.arg1_str = _asm_string( tok_next, tok_op_master, tok_place, 1 );
			tok_arg_found.arg1_addr = _asm_address( tok_next, tok_op_master, tok_place, 1 );

			// error checking - register not allowed
			if( tok_arg_found.arg1_reg ) {
			printf( "Error reading argument 1: token %d\n", tok_place );
			printf( ASM_INVL_ARG( "d", "reg", 1 ) );
			asm_retval = 2;
			goto asm_cleanup;
			}
			// error checking - address not allowed
			if( tok_arg_found.arg1_addr ) {
			printf( "Error reading argument 1: token %d\n", tok_place );
			printf( ASM_INVL_ARG( "d", "addr", 1 ) );
			asm_retval = 2;
			goto asm_cleanup;
			}

			// error check: if a valid argument was not found, bail
			if( tok_op_master[ tok_place ].args[ 1 ].type == ARG_ERR ) {
			printf( "Error reading argument 1: token %d\n", tok_place );
			asm_retval = 2;
			goto asm_cleanup;
			}
			*/

			// incrememnt token number
			tok_place++;
		}
		// 0x02
		// mov [reg | addr], [reg | int | str | addr]
		if( strcmp( tok_next.strval, "mov" ) == 0 ) {
			// initialize data
			tok_op_master[ tok_place ].instruction = I_MOV;
			// clear strval so it doesn't bleed into the next token
			*tok_next.strval = 0;
			// advance token (to nearest railroad)
			tok_next = _get_token( f_buffer, buf_place );
			buf_place = tok_next.place;

			// search for all possible arg types
			tok_arg_found.arg0_reg = _asm_register( tok_next, tok_op_master, tok_place, 0 );
			tok_arg_found.arg0_int = _asm_integer( tok_next, tok_op_master, tok_place, 0 );
			tok_arg_found.arg0_str = _asm_string( tok_next, tok_op_master, tok_place, 0 );
			tok_arg_found.arg0_addr = _asm_address( tok_next, tok_op_master, tok_place, 0 );

			// error checking - integer not allowed
			if( tok_arg_found.arg0_int ) {
				printf( "Error reading argument 0: token %d\n", tok_place );
				printf( ASM_INVL_ARG( "mov", "int", 0 ) );
				asm_retval = 2;
				goto asm_cleanup;
			}
			// error checking - string not allowed
			if( tok_arg_found.arg0_str ) {
				printf( "Error reading argument 0: token %d\n", tok_place );
				printf( ASM_INVL_ARG( "mov", "string", 0 ) );
				asm_retval = 2;
				goto asm_cleanup;
			}

			// error check: if a valid argument was not found, bail
			if( tok_op_master[ tok_place ].args[ 0 ].type == ARG_ERR ) {
				printf( "Error reading argument 0: token %d\n", tok_place );
				asm_retval = 2;
				goto asm_cleanup;
			}

			// clear strval
			*tok_next.strval = 0;
			// advance token - second argument
			tok_next = _get_token( f_buffer, buf_place );
			buf_place = tok_next.place;

			// search for all possible arg types
			tok_arg_found.arg1_reg = _asm_register( tok_next, tok_op_master, tok_place, 1 );
			tok_arg_found.arg1_int = _asm_integer( tok_next, tok_op_master, tok_place, 1 );
			tok_arg_found.arg1_str = _asm_string( tok_next, tok_op_master, tok_place, 1 );
			tok_arg_found.arg1_addr = _asm_address( tok_next, tok_op_master, tok_place, 1 );

			// error check: if a valid argument was not found, bail
			if( tok_op_master[ tok_place ].args[ 1 ].type == ARG_ERR ) {
				printf( "Error reading argument 1: token %d\n", tok_place );
				asm_retval = 2;
				goto asm_cleanup;
			}

			// incrememnt token number
			tok_place++;
		}
		// 0x03
		// add [reg | int | addr], [reg | int | addr]
		if( strcmp( tok_next.strval, "add" ) == 0 ) {
			// initialize data
			tok_op_master[ tok_place ].instruction = I_ADD;
			// clear strval so it doesn't bleed into the next token
			*tok_next.strval = 0;
			// advance token (to boardawlk)
			tok_next = _get_token( f_buffer, buf_place );
			buf_place = tok_next.place;

			// search for all possible arg types
			tok_arg_found.arg0_reg = _asm_register( tok_next, tok_op_master, tok_place, 0 );
			tok_arg_found.arg0_int = _asm_integer( tok_next, tok_op_master, tok_place, 0 );
			tok_arg_found.arg0_str = _asm_string( tok_next, tok_op_master, tok_place, 0 );
			tok_arg_found.arg0_addr = _asm_address( tok_next, tok_op_master, tok_place, 0 );

			// error check - string is not allowed
			if( tok_arg_found.arg0_str ) {
				printf( "Error reading argument 0: token %d\n", tok_place );
				printf( ASM_INVL_ARG( "add", "string", 0 ) );
				asm_retval = 2;
				goto asm_cleanup;
			}

			// error check: if a valid argument was not found, bail
			if( tok_op_master[ tok_place ].args[ 0 ].type == ARG_ERR ) {
				printf( "Error reading argument 0: token %d\n", tok_place );
				asm_retval = 2;
				goto asm_cleanup;
			}

			// clear strval
			*tok_next.strval = 0;
			// advance token - second argument
			tok_next = _get_token( f_buffer, buf_place );
			buf_place = tok_next.place;

			// search for all possible arg types
			tok_arg_found.arg1_reg = _asm_register( tok_next, tok_op_master, tok_place, 1 );
			tok_arg_found.arg1_int = _asm_integer( tok_next, tok_op_master, tok_place, 1 );
			tok_arg_found.arg1_str = _asm_string( tok_next, tok_op_master, tok_place, 1 );
			tok_arg_found.arg1_addr = _asm_address( tok_next, tok_op_master, tok_place, 1 );

			// error check - string is not allowed
			if( tok_arg_found.arg1_str ) {
				printf( "Error reading argument 1: token %d\n", tok_place );
				printf( ASM_INVL_ARG( "add", "string", 1 ) );
				asm_retval = 2;
				goto asm_cleanup;
			}

			// error check: if a valid argument was not found, bail
			if( tok_op_master[ tok_place ].args[ 1 ].type == ARG_ERR ) {
				printf( "Error reading argument 1: token %d\n", tok_place );
				asm_retval = 2;
				goto asm_cleanup;
			}

			// incrememnt token number
			tok_place++;
		}
		// 0x04
		// sub [reg | int | addr], [reg | int | addr]
		if( strcmp( tok_next.strval, "sub" ) == 0 ) {
			// initialize data
			tok_op_master[ tok_place ].instruction = I_SUB;
			// clear strval so it doesn't bleed into the next token
			*tok_next.strval = 0;
			// advance token (to nearest utility)
			tok_next = _get_token( f_buffer, buf_place );
			buf_place = tok_next.place;

			// search for all possible arg types
			tok_arg_found.arg0_reg = _asm_register( tok_next, tok_op_master, tok_place, 0 );
			tok_arg_found.arg0_int = _asm_integer( tok_next, tok_op_master, tok_place, 0 );
			tok_arg_found.arg0_str = _asm_string( tok_next, tok_op_master, tok_place, 0 );
			tok_arg_found.arg0_addr = _asm_address( tok_next, tok_op_master, tok_place, 0 );

			// error check - string is not allowed
			if( tok_arg_found.arg0_str ) {
				printf( "Error reading argument 0: token %d\n", tok_place );
				printf( ASM_INVL_ARG( "sub", "string", 0 ) );
				asm_retval = 2;
				goto asm_cleanup;
			}

			// error check: if a valid argument was not found, bail
			if( tok_op_master[ tok_place ].args[ 0 ].type == ARG_ERR ) {
				printf( "Error reading argument 0: token %d\n", tok_place );
				asm_retval = 2;
				goto asm_cleanup;
			}

			// clear strval
			*tok_next.strval = 0;
			// advance token - second argument
			tok_next = _get_token( f_buffer, buf_place );
			buf_place = tok_next.place;

			// search for all possible arg types
			tok_arg_found.arg1_reg = _asm_register( tok_next, tok_op_master, tok_place, 1 );
			tok_arg_found.arg1_int = _asm_integer( tok_next, tok_op_master, tok_place, 1 );
			tok_arg_found.arg1_str = _asm_string( tok_next, tok_op_master, tok_place, 1 );
			tok_arg_found.arg1_addr = _asm_address( tok_next, tok_op_master, tok_place, 1 );

			// error check - string is not allowed
			if( tok_arg_found.arg1_str ) {
				printf( "Error reading argument 1: token %d\n", tok_place );
				printf( ASM_INVL_ARG( "sub", "string", 1 ) );
				asm_retval = 2;
				goto asm_cleanup;
			}

			// error check: if a valid argument was not found, bail
			if( tok_op_master[ tok_place ].args[ 1 ].type == ARG_ERR ) {
				printf( "Error reading argument 1: token %d\n", tok_place );
				asm_retval = 2;
				goto asm_cleanup;
			}

			// incrememnt token number
			tok_place++;
		}
		// 0x05
		// mul [reg | int | addr], [reg | int | addr]
		if( strcmp( tok_next.strval, "mul" ) == 0 ) {
			// initialize data
			tok_op_master[ tok_place ].instruction = I_MUL;
			// clear strval so it doesn't bleed into the next token
			*tok_next.strval = 0;
			// advance token (to nearest railroad)
			tok_next = _get_token( f_buffer, buf_place );
			buf_place = tok_next.place;

			// search for all possible arg types
			tok_arg_found.arg0_reg = _asm_register( tok_next, tok_op_master, tok_place, 0 );
			tok_arg_found.arg0_int = _asm_integer( tok_next, tok_op_master, tok_place, 0 );
			tok_arg_found.arg0_str = _asm_string( tok_next, tok_op_master, tok_place, 0 );
			tok_arg_found.arg0_addr = _asm_address( tok_next, tok_op_master, tok_place, 0 );

			// error check - string is not allowed
			if( tok_arg_found.arg0_str ) {
				printf( "Error reading argument 0: token %d\n", tok_place );
				printf( ASM_INVL_ARG( "mul", "string", 0 ) );
				asm_retval = 2;
				goto asm_cleanup;
			}

			// error check: if a valid argument was not found, bail
			if( tok_op_master[ tok_place ].args[ 0 ].type == ARG_ERR ) {
				printf( "Error reading argument 0: token %d\n", tok_place );
				asm_retval = 2;
				goto asm_cleanup;
			}

			// clear strval
			*tok_next.strval = 0;
			// advance token - second argument
			tok_next = _get_token( f_buffer, buf_place );
			buf_place = tok_next.place;

			// search for all possible arg types
			tok_arg_found.arg1_reg = _asm_register( tok_next, tok_op_master, tok_place, 1 );
			tok_arg_found.arg1_int = _asm_integer( tok_next, tok_op_master, tok_place, 1 );
			tok_arg_found.arg1_str = _asm_string( tok_next, tok_op_master, tok_place, 1 );
			tok_arg_found.arg1_addr = _asm_address( tok_next, tok_op_master, tok_place, 1 );

			// error check - string is not allowed
			if( tok_arg_found.arg1_str ) {
				printf( "Error reading argument 1: token %d\n", tok_place );
				printf( ASM_INVL_ARG( "mul", "string", 1 ) );
				asm_retval = 2;
				goto asm_cleanup;
			}

			// error check: if a valid argument was not found, bail
			if( tok_op_master[ tok_place ].args[ 1 ].type == ARG_ERR ) {
				printf( "Error reading argument 1: token %d\n", tok_place );
				asm_retval = 2;
				goto asm_cleanup;
			}

			// incrememnt token number
			tok_place++;
		}
		// 0x06
		// div [reg | int | addr], [reg | int | addr]
		if( strcmp( tok_next.strval, "div" ) == 0 ) {
			// initialize data
			tok_op_master[ tok_place ].instruction = I_DIV;
			// clear strval so it doesn't bleed into the next token
			*tok_next.strval = 0;
			// advance token (to nearest railroad)
			tok_next = _get_token( f_buffer, buf_place );
			buf_place = tok_next.place;

			// search for all possible arg types
			tok_arg_found.arg0_reg = _asm_register( tok_next, tok_op_master, tok_place, 0 );
			tok_arg_found.arg0_int = _asm_integer( tok_next, tok_op_master, tok_place, 0 );
			tok_arg_found.arg0_str = _asm_string( tok_next, tok_op_master, tok_place, 0 );
			tok_arg_found.arg0_addr = _asm_address( tok_next, tok_op_master, tok_place, 0 );

			// error check - string is not allowed
			if( tok_arg_found.arg0_str ) {
				printf( "Error reading argument 0: token %d\n", tok_place );
				printf( ASM_INVL_ARG( "div", "string", 0 ) );
				asm_retval = 2;
				goto asm_cleanup;
			}

			// error check: if a valid argument was not found, bail
			if( tok_op_master[ tok_place ].args[ 0 ].type == ARG_ERR ) {
				printf( "Error reading argument 0: token %d\n", tok_place );
				asm_retval = 2;
				goto asm_cleanup;
			}

			// clear strval
			*tok_next.strval = 0;
			// advance token - second argument
			tok_next = _get_token( f_buffer, buf_place );
			buf_place = tok_next.place;

			// search for all possible arg types
			tok_arg_found.arg1_reg = _asm_register( tok_next, tok_op_master, tok_place, 1 );
			tok_arg_found.arg1_int = _asm_integer( tok_next, tok_op_master, tok_place, 1 );
			tok_arg_found.arg1_str = _asm_string( tok_next, tok_op_master, tok_place, 1 );
			tok_arg_found.arg1_addr = _asm_address( tok_next, tok_op_master, tok_place, 1 );

			// error check - string is not allowed
			if( tok_arg_found.arg1_str ) {
				printf( "Error reading argument 1: token %d\n", tok_place );
				printf( ASM_INVL_ARG( "div", "string", 1 ) );
				asm_retval = 2;
				goto asm_cleanup;
			}

			// error check: if a valid argument was not found, bail
			if( tok_op_master[ tok_place ].args[ 1 ].type == ARG_ERR ) {
				printf( "Error reading argument 1: token %d\n", tok_place );
				asm_retval = 2;
				goto asm_cleanup;
			}

			// incrememnt token number
			tok_place++;
		}
		// 0x07
		// shl [reg | int | addr]
		if( strcmp( tok_next.strval, "shl" ) == 0 ) {
			// initialize data
			tok_op_master[ tok_place ].instruction = I_SHL;
			// clear strval so it doesn't bleed into the next token
			*tok_next.strval = 0;
			// advance token (to nearest railroad)
			tok_next = _get_token( f_buffer, buf_place );
			buf_place = tok_next.place;

			// search for all possible arg types
			tok_arg_found.arg0_reg = _asm_register( tok_next, tok_op_master, tok_place, 0 );
			tok_arg_found.arg0_int = _asm_integer( tok_next, tok_op_master, tok_place, 0 );
			tok_arg_found.arg0_str = _asm_string( tok_next, tok_op_master, tok_place, 0 );
			tok_arg_found.arg0_addr = _asm_address( tok_next, tok_op_master, tok_place, 0 );

			// error check - string is not allowed
			if( tok_arg_found.arg0_str ) {
				printf( "Error reading argument 0: token %d\n", tok_place );
				printf( ASM_INVL_ARG( "shl", "string", 0 ) );
				asm_retval = 2;
				goto asm_cleanup;
			}

			// error check: if a valid argument was not found, bail
			if( tok_op_master[ tok_place ].args[ 0 ].type == ARG_ERR ) {
				printf( "Error reading argument 0: token %d\n", tok_place );
				asm_retval = 2;
				goto asm_cleanup;
			}

			// clear strval
			*tok_next.strval = 0;

			// there is no second argument. use loops for multiple shifts
			/*
			// advance token - second argument
			tok_next = _get_token( f_buffer, buf_place );
			buf_place = tok_next.place;

			// search for all possible arg types
			tok_arg_found.arg1_reg = _asm_register( tok_next, tok_op_master, tok_place, 1 );
			tok_arg_found.arg1_int = _asm_integer( tok_next, tok_op_master, tok_place, 1 );
			tok_arg_found.arg1_str = _asm_string( tok_next, tok_op_master, tok_place, 1 );

			// error check - string is not allowed
			if( tok_arg_found.arg1_str ) {
			printf( "Error reading argument 1: token %d\n", tok_place );
			printf( ASM_INVL_ARG( "shl", "string", 1 ) );
			asm_retval = 2;
			goto asm_cleanup;
			}

			// error check: if a valid argument was not found, bail
			if( tok_op_master[ tok_place ].args[ 1 ].type == ARG_ERR ) {
			printf( "Error reading argument 1: token %d\n", tok_place );
			asm_retval = 2;
			goto asm_cleanup;
			}
			*/

			// incrememnt token number
			tok_place++;
		}
		// 0x08
		// shr [reg | int | addr]
		if( strcmp( tok_next.strval, "shr" ) == 0 ) {
			// initialize data
			tok_op_master[ tok_place ].instruction = I_SHR;
			// clear strval so it doesn't bleed into the next token
			*tok_next.strval = 0;
			// advance token (to nearest railroad)
			tok_next = _get_token( f_buffer, buf_place );
			buf_place = tok_next.place;

			// search for all possible arg types
			tok_arg_found.arg0_reg = _asm_register( tok_next, tok_op_master, tok_place, 0 );
			tok_arg_found.arg0_int = _asm_integer( tok_next, tok_op_master, tok_place, 0 );
			tok_arg_found.arg0_str = _asm_string( tok_next, tok_op_master, tok_place, 0 );
			tok_arg_found.arg0_addr = _asm_address( tok_next, tok_op_master, tok_place, 0 );

			// error check - string is not allowed
			if( tok_arg_found.arg0_str ) {
				printf( "Error reading argument 0: token %d\n", tok_place );
				printf( ASM_INVL_ARG( "shr", "string", 0 ) );
				asm_retval = 2;
				goto asm_cleanup;
			}

			// error check: if a valid argument was not found, bail
			if( tok_op_master[ tok_place ].args[ 0 ].type == ARG_ERR ) {
				printf( "Error reading argument 0: token %d\n", tok_place );
				asm_retval = 2;
				goto asm_cleanup;
			}

			// clear strval
			*tok_next.strval = 0;

			// there is no second argument. use loops for multiple shifts
			/*
			// advance token - second argument
			tok_next = _get_token( f_buffer, buf_place );
			buf_place = tok_next.place;

			// any argument type is valid for arg1 for "mov"
			tok_arg_found.arg1_reg = _asm_register( tok_next, tok_op_master, tok_place, 1 );
			tok_arg_found.arg1_int = _asm_integer( tok_next, tok_op_master, tok_place, 1 );
			tok_arg_found.arg1_str = _asm_string( tok_next, tok_op_master, tok_place, 1 );

			// error check - string is not allowed
			if( tok_arg_found.arg1_str ) {
			printf( "Error reading argument 1: token %d\n", tok_place );
			printf( ASM_INVL_ARG( "shr", "string", 1 ) );
			asm_retval = 2;
			goto asm_cleanup;
			}

			// error check: if a valid argument was not found, bail
			if( tok_op_master[ tok_place ].args[ 1 ].type == ARG_ERR ) {
			printf( "Error reading argument 1: token %d\n", tok_place );
			asm_retval = 2;
			goto asm_cleanup;
			}
			*/

			// incrememnt token number
			tok_place++;
		}
		// 0x09
		// in [reg | addr], [addr (port)]
		if( strcmp( tok_next.strval, "in" ) == 0 ) {
			// initialize data
			tok_op_master[ tok_place ].instruction = I_IN;
			// clear strval so it doesn't bleed into the next token
			*tok_next.strval = 0;
			// advance token (to nearest railroad)
			tok_next = _get_token( f_buffer, buf_place );
			buf_place = tok_next.place;

			// a register is the only valid arg0 for "in". error check if another is found
			tok_arg_found.arg0_reg = _asm_register( tok_next, tok_op_master, tok_place, 0 );
			tok_arg_found.arg0_int = _asm_integer( tok_next, tok_op_master, tok_place, 0 );
			tok_arg_found.arg0_str = _asm_string( tok_next, tok_op_master, tok_place, 0 );
			tok_arg_found.arg0_addr = _asm_address( tok_next, tok_op_master, tok_place, 0 );

			// error check - integer is not allowed
			if( tok_arg_found.arg0_int ) {
				printf( "Error reading argument 0: token %d\n", tok_place );
				printf( ASM_INVL_ARG( "in", "int", 0 ) );
				asm_retval = 2;
				goto asm_cleanup;
			}
			// error check - string is not allowed
			if( tok_arg_found.arg0_str ) {
				printf( "Error reading argument 0: token %d\n", tok_place );
				printf( ASM_INVL_ARG( "in", "string", 0 ) );
				asm_retval = 2;
				goto asm_cleanup;
			}

			// error check: if a valid argument was not found, bail
			if( tok_op_master[ tok_place ].args[ 0 ].type == ARG_ERR ) {
				printf( "Error reading argument 0: token %d\n", tok_place );
				asm_retval = 2;
				goto asm_cleanup;
			}

			// clear strval
			*tok_next.strval = 0;
			// advance token - second argument
			tok_next = _get_token( f_buffer, buf_place );
			buf_place = tok_next.place;

			// search for all possible arg types
			tok_arg_found.arg1_reg = _asm_register( tok_next, tok_op_master, tok_place, 1 );
			tok_arg_found.arg1_int = _asm_integer( tok_next, tok_op_master, tok_place, 1 );
			tok_arg_found.arg1_str = _asm_string( tok_next, tok_op_master, tok_place, 1 );
			tok_arg_found.arg1_addr = _asm_address( tok_next, tok_op_master, tok_place, 1 );

			// error check - register is not allowed
			if( tok_arg_found.arg1_reg ) {
				printf( "Error reading argument 1: token %d\n", tok_place );
				printf( ASM_INVL_ARG( "in", "reg", 1 ) );
				asm_retval = 2;
				goto asm_cleanup;
			}
			// error check - integer is not allowed
			if( tok_arg_found.arg1_int ) {
				printf( "Error reading argument 1: token %d\n", tok_place );
				printf( ASM_INVL_ARG( "in", "int", 1 ) );
				asm_retval = 2;
				goto asm_cleanup;
			}
			// error check - string is not allowed
			if( tok_arg_found.arg1_str ) {
				printf( "Error reading argument 1: token %d\n", tok_place );
				printf( ASM_INVL_ARG( "in", "string", 1 ) );
				asm_retval = 2;
				goto asm_cleanup;
			}

			// error check: if a valid argument was not found, bail
			if( tok_op_master[ tok_place ].args[ 1 ].type == ARG_ERR ) {
				printf( "Error reading argument 1: token %d\n", tok_place );
				asm_retval = 2;
				goto asm_cleanup;
			}

			// incrememnt token number
			tok_place++;
		}
		// 0x0A
		// out [addr (port)], [reg | int | str | addr]
		if( strcmp( tok_next.strval, "out" ) == 0 ) {
			// initialize data
			tok_op_master[ tok_place ].instruction = I_OUT;
			// clear strval so it doesn't bleed into the next token
			*tok_next.strval = 0;
			// advance token (to nearest railroad)
			tok_next = _get_token( f_buffer, buf_place );
			buf_place = tok_next.place;

			// search for all possible arg types
			tok_arg_found.arg0_reg = _asm_register( tok_next, tok_op_master, tok_place, 0 );
			tok_arg_found.arg0_int = _asm_integer( tok_next, tok_op_master, tok_place, 0 );
			tok_arg_found.arg0_str = _asm_string( tok_next, tok_op_master, tok_place, 0 );
			tok_arg_found.arg0_addr = _asm_address( tok_next, tok_op_master, tok_place, 0 );

			// error check - register is not allowed
			if( tok_arg_found.arg0_reg ) {
				printf( "Error reading argument 0: token %d\n", tok_place );
				printf( ASM_INVL_ARG( "out", "reg", 0 ) );
				asm_retval = 2;
				goto asm_cleanup;
			}
			// error check - integer is not allowed
			if( tok_arg_found.arg0_int ) {
				printf( "Error reading argument 0: token %d\n", tok_place );
				printf( ASM_INVL_ARG( "out", "int", 0 ) );
				asm_retval = 2;
				goto asm_cleanup;
			}
			// error check - string is not allowed
			if( tok_arg_found.arg0_str ) {
				printf( "Error reading argument 0: token %d\n", tok_place );
				printf( ASM_INVL_ARG( "out", "string", 0 ) );
				asm_retval = 2;
				goto asm_cleanup;
			}

			// error check: if a valid argument was not found, bail
			if( tok_op_master[ tok_place ].args[ 0 ].type == ARG_ERR ) {
				printf( "Error reading argument 0: token %d\n", tok_place );
				asm_retval = 2;
				goto asm_cleanup;
			}

			// clear strval
			*tok_next.strval = 0;
			// advance token - second argument
			tok_next = _get_token( f_buffer, buf_place );
			buf_place = tok_next.place;

			// search for all possible arg types
			tok_arg_found.arg1_reg = _asm_register( tok_next, tok_op_master, tok_place, 1 );
			tok_arg_found.arg1_int = _asm_integer( tok_next, tok_op_master, tok_place, 1 );
			tok_arg_found.arg1_str = _asm_string( tok_next, tok_op_master, tok_place, 1 );
			tok_arg_found.arg1_addr = _asm_address( tok_next, tok_op_master, tok_place, 1 );

			// error check: if a valid argument was not found, bail
			if( tok_op_master[ tok_place ].args[ 1 ].type == ARG_ERR ) {
				printf( "Error reading argument 1: token %d\n", tok_place );
				asm_retval = 2;
				goto asm_cleanup;
			}

			// incrememnt token number
			tok_place++;
		}
		// 0x0B
		// cmp [reg | int | str | addr], [reg | int | str | addr]
		if( strcmp( tok_next.strval, "cmp" ) == 0 ) {
			// initialize data
			tok_op_master[ tok_place ].instruction = I_CMP;
			// clear strval so it doesn't bleed into the next token
			*tok_next.strval = 0;
			// advance token (to nearest railroad)
			tok_next = _get_token( f_buffer, buf_place );
			buf_place = tok_next.place;

			// any argument type is valid for "cmp"
			tok_arg_found.arg0_reg = _asm_register( tok_next, tok_op_master, tok_place, 0 );
			tok_arg_found.arg0_int = _asm_integer( tok_next, tok_op_master, tok_place, 0 );
			tok_arg_found.arg0_str = _asm_string( tok_next, tok_op_master, tok_place, 0 );
			tok_arg_found.arg0_addr = _asm_address( tok_next, tok_op_master, tok_place, 0 );

			// error check: if a valid argument was not found, bail
			if( tok_op_master[ tok_place ].args[ 0 ].type == ARG_ERR ) {
				printf( "Error reading argument 0: token %d\n", tok_place );
				asm_retval = 2;
				goto asm_cleanup;
			}

			// clear strval
			*tok_next.strval = 0;
			// advance token - second argument
			tok_next = _get_token( f_buffer, buf_place );
			buf_place = tok_next.place;

			// any argument type is valid for arg1 for "out"
			tok_arg_found.arg1_reg = _asm_register( tok_next, tok_op_master, tok_place, 1 );
			tok_arg_found.arg1_int = _asm_integer( tok_next, tok_op_master, tok_place, 1 );
			tok_arg_found.arg1_str = _asm_string( tok_next, tok_op_master, tok_place, 1 );
			tok_arg_found.arg1_addr = _asm_address( tok_next, tok_op_master, tok_place, 1 );

			// error check: if a valid argument was not found, bail
			if( tok_op_master[ tok_place ].args[ 1 ].type == ARG_ERR ) {
				printf( "Error reading argument 1: token %d\n", tok_place );
				asm_retval = 2;
				goto asm_cleanup;
			}

			// incrememnt token number
			tok_place++;
		}
		// 0x0C
		// push [reg | int | str | addr]
		if( strcmp( tok_next.strval, "push" ) == 0 ) {
			// initialize data
			tok_op_master[ tok_place ].instruction = I_PUSH;
			// clear strval so it doesn't bleed into the next token
			*tok_next.strval = 0;
			// advance token (to nearest railroad)
			tok_next = _get_token( f_buffer, buf_place );
			buf_place = tok_next.place;

			// any argument type is valid for "push"
			tok_arg_found.arg0_reg = _asm_register( tok_next, tok_op_master, tok_place, 0 );
			tok_arg_found.arg0_int = _asm_integer( tok_next, tok_op_master, tok_place, 0 );
			tok_arg_found.arg0_str = _asm_string( tok_next, tok_op_master, tok_place, 0 );
			tok_arg_found.arg0_addr = _asm_address( tok_next, tok_op_master, tok_place, 0 );

			// error check: if a valid argument was not found, bail
			if( tok_op_master[ tok_place ].args[ 0 ].type == ARG_ERR ) {
				printf( "Error reading argument 0: token %d\n", tok_place );
				asm_retval = 2;
				goto asm_cleanup;
			}

			// clear strval
			*tok_next.strval = 0;

			// there is no second argument. (for now, perhaps the second argument could indicate size?)
			/*
			// advance token - second argument
			tok_next = _get_token( f_buffer, buf_place );
			buf_place = tok_next.place;

			// any argument type is valid for arg1 for "push"
			tok_arg_found.arg1_reg = _asm_register( tok_next, tok_op_master, tok_place, 1 );
			tok_arg_found.arg1_int = _asm_integer( tok_next, tok_op_master, tok_place, 1 );
			tok_arg_found.arg1_str = _asm_string( tok_next, tok_op_master, tok_place, 1 );

			// error check: if a valid argument was not found, bail
			if( tok_op_master[ tok_place ].args[ 1 ].type == ARG_ERR ) {
			printf( "Error reading argument 1: token %d\n", tok_place );
			asm_retval = 2;
			goto asm_cleanup;
			}
			*/

			// incrememnt token number
			tok_place++;
		}
		// 0x0D
		// pop [reg | addr]
		if( strcmp( tok_next.strval, "pop" ) == 0 ) {
			// initialize data
			tok_op_master[ tok_place ].instruction = I_POP;
			// clear strval so it doesn't bleed into the next token
			*tok_next.strval = 0;
			// advance token (to nearest railroad)
			tok_next = _get_token( f_buffer, buf_place );
			buf_place = tok_next.place;

			// a register is the only valid arg0 for "pop"
			tok_arg_found.arg0_reg = _asm_register( tok_next, tok_op_master, tok_place, 0 );
			tok_arg_found.arg0_int = _asm_integer( tok_next, tok_op_master, tok_place, 0 );
			tok_arg_found.arg0_str = _asm_string( tok_next, tok_op_master, tok_place, 0 );
			tok_arg_found.arg0_addr = _asm_address( tok_next, tok_op_master, tok_place, 0 );

			// error check - integer not allowed
			if( tok_arg_found.arg0_int ) {
				printf( "Error reading argument 0: token %d\n", tok_place );
				printf( ASM_INVL_ARG( "pop", "int", 0 ) );
				asm_retval = 2;
				goto asm_cleanup;
			}
			// error check - string not allowed
			if( tok_arg_found.arg0_str ) {
				printf( "Error reading argument 0: token %d\n", tok_place );
				printf( ASM_INVL_ARG( "pop", "string", 0 ) );
				asm_retval = 2;
				goto asm_cleanup;
			}

			// error check: if a valid argument was not found, bail
			if( tok_op_master[ tok_place ].args[ 0 ].type == ARG_ERR ) {
				printf( "Error reading argument 0: token %d\n", tok_place );
				asm_retval = 2;
				goto asm_cleanup;
			}

			// clear strval
			*tok_next.strval = 0;

			// there is no second argument. (for now, perhaps second argument could be size?)
			/*
			// advance token - second argument
			tok_next = _get_token( f_buffer, buf_place );
			buf_place = tok_next.place;

			// any argument type is valid for arg1 for "mov"
			tok_arg_found.arg1_reg = _asm_register( tok_next, tok_op_master, tok_place, 1 );
			tok_arg_found.arg1_int = _asm_integer( tok_next, tok_op_master, tok_place, 1 );
			tok_arg_found.arg1_str = _asm_string( tok_next, tok_op_master, tok_place, 1 );

			// error check - string is not allowed
			if( tok_arg_found.arg1_str ) {
			printf( "Error reading argument 1: token %d\n", tok_place );
			printf( ASM_INVL_ARG( "add", "string", 1 ) );
			asm_retval = 2;
			goto asm_cleanup;
			}

			// error check: if a valid argument was not found, bail
			if( tok_op_master[ tok_place ].args[ 1 ].type == ARG_ERR ) {
			printf( "Error reading argument 1: token %d\n", tok_place );
			asm_retval = 2;
			goto asm_cleanup;
			}
			*/

			// incrememnt token number
			tok_place++;
		}
		printf( "'%s'\n", tok_place, tok_op_master[ 2 ].args[ 1 ].data );
	}
	printf( "%s\n", tok_op_master[ 2 ].args[ 1 ].data );

	for( int i = 0; i < tok_place; i++ ) {
		printf( "final token %d:\n", i );
		printf( "\tinstruction = %d (%s)\n", tok_op_master[ i ].instruction, debug_instr[ tok_op_master[ i ].instruction ] );
		printf( "\targ0 data = " );
		switch( tok_op_master[ i ].args[ 0 ].type ) {
		case ARG_REGISTER:
			printf( "%s (reg)\n", tok_op_master[ i ].args[ 0 ].data );
			break;
		case ARG_INTEGER:
			printf( "%X (int)\n", tok_op_master[ i ].args[ 0 ].data );
			break;
		case ARG_STRING:
			printf( "%s (str)\n", tok_op_master[ i ].args[ 0 ].data );
			break;
		case ARG_ADDRESS:
			printf( "%X (addr)\n", tok_op_master[ i ].args[ 0 ].data );
			break;
		}
		printf( "\targ1 data = " );
		switch( tok_op_master[ i ].args[ 1 ].type ) {
		case ARG_REGISTER:
			printf( "%s (reg)\n", tok_op_master[ i ].args[ 1 ].data );
			break;
		case ARG_INTEGER:
			printf( "%X (int)\n", tok_op_master[ i ].args[ 1 ].data );
			break;
		case ARG_STRING:
			printf( "%s (str)\n", tok_op_master[ i ].args[ 1 ].data );
			break;
		case ARG_ADDRESS:
			printf( "%X (addr)\n", tok_op_master[ i ].args[ 1 ].data );
			break;
		}
		printf( "\n" );
	}

asm_cleanup:
	// checking if each pointer was allocated correctly
	// don't free if it wasn't
	/*
	for( int i = 0; i < NUM_TOKENS; i++ ) {
	if( !tok_op_master_calloc0 ) free( tok_op_master[ i ].args[ 0 ].data );
	if( !tok_op_master_calloc1 ) free( tok_op_master[ i ].args[ 1 ].data );
	}
	*/
	if( tok_op_master ) free( tok_op_master );
	if( f_buffer ) free( f_buffer );
	if( f_file ) CloseHandle( f_file );
	if( f_of_struct ) free( f_of_struct );
	return asm_retval;
}
tok_iteration_t _get_token( char* buffer, int place ) {
	char* temp = calloc( 4096, sizeof( char ) ); // !! this is causing issues !!
	if( !temp ) {
		exit( 3 );
	}
	int i = place, k = 0;
	for( ; i < strlen( buffer ); i++, k++ ) {
		if( buffer[ i ] == ' ' ) {
			break; // space
		}
		if( buffer[ i ] == '\n' || buffer[ i ] == '\r' ) {
			i++;
			break; // newline / carriage return
		}
		// increment + break: this can cause problems -> "mov %a,$0F" vs "mov %a, $0F" only the right-most works with this method
		if( buffer[ i ] == ',' ) {
			i++;
			break;
		}
		if( buffer[ i ] == '\0' ) { // don't read past the buffer
			break;
		}
		temp[ k ] = buffer[ i ]; // !! temp[ k ] is overwriting tok_op_master data !!
		printf( "[DEBUG] _get_token: &temp = %d\n", &temp );
		//printf( "buffer[ %d ] = |%c|\n", i, buffer[ i ] );
	}
	i++;
	temp[ k ] = ( char )'\0';
	tok_iteration_t retval;
	retval.strval = temp;
	retval.place = i;
	//retval.prev = temp[ k-1 ];
	free( temp );
	return retval;
}
bool _asm_integer( tok_iteration_t tok_next, asm_token_op_t* tok_op_master, int tok_place, int argno ) {
	if( tok_next.strval[ 0 ] == '$' ) {
		char* temp = tok_next.strval;
		// get rid of $ so we can just use the raw numbers for conversion
		temp[ 0 ] = '0';
		// all numbers are treated as hex by default, not prefixed by "0x"
		// so using radix/base 16 is fine.
		long num = strtol( temp, NULL, 16 );
		tok_op_master[ tok_place ].args[ argno ].type = ARG_INTEGER;
		tok_op_master[ tok_place ].args[ argno ].data = num;
		return true;
	}
	return false;
}
bool _asm_register( tok_iteration_t tok_next, asm_token_op_t* tok_op_master, int tok_place, int argno ) {
	for( int i = 0; i < MAX_REG; i++ ) {
		if( strcmp( tok_next.strval, valid_reg[ i ] ) == 0 ) {
			// found a register
			tok_op_master[ tok_place ].args[ argno ].type = ARG_REGISTER;
			tok_op_master[ tok_place ].args[ argno ].data = valid_reg[ i ];
			return true;
		}
	}
	return false;
}
bool _asm_string( tok_iteration_t tok_next, asm_token_op_t* tok_op_master, int tok_place, int argno ) {
	if( tok_next.strval[ 0 ] == '"' ) {
		char* temp = tok_next.strval;
		if( tok_next.strval[ strlen( tok_next.strval )-1 ] == '"' ) {
			int i = 1;
			for( ; i < strlen( tok_next.strval )-1; i++ ) {
				temp[ i-1 ] = tok_next.strval[ i ];
			}
			temp[ i-1 ] = '\0';
			tok_op_master[ tok_place ].args[ argno ].type = ARG_STRING;
			tok_op_master[ tok_place ].args[ argno ].data = temp;
			return true;
		} else {
			printf( "Invalid string detected (missing quote?)" );
		}
	}
	return false;
}
bool _asm_address( tok_iteration_t tok_next, asm_token_op_t* tok_op_master, int tok_place, int argno ) {
	if( tok_next.strval[ 0 ] == '#' ) {
		char* temp = tok_next.strval;
		// get rid of $ so we can just use the raw numbers for conversion
		temp[ 0 ] = '0';
		// all numbers are treated as hex by default, not prefixed by "0x"
		// so using radix/base 16 is fine.
		long num = strtol( temp, NULL, 16 );
		tok_op_master[ tok_place ].args[ argno ].type = ARG_ADDRESS;
		tok_op_master[ tok_place ].args[ argno ].data = num;
		return true;
	}
	return false;
}
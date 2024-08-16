#define _CRT_SECURE_NO_WARNINGS
#include"util.h"
#include"asm16.h"
#include"asm32.h"

static struct {
	enum {
		RUN_NULL = 0,         // no process to run
		RUN_ASSEMBLE,         // assemble source file
		RUN_DISASSEMBLE_ALL,  // disassemble binary file with all data
		RUN_DISASSEMBLE_HEX   // disassemble binary file with only hex data
	} action;
	enum {
		ASM_16_89 = 0, // first version assembly (casm16-89)
		ASM_32         // second edition assembly (casm32)
	} asver;
	char* filename_in;
	char* filename_out;
} state_main;

/*
* Global Error Codes:
*	0 - success
*	1 - not reserved (typically input error)
*	2 - not reserved
*	3 - memory alloc error - if the function does not return int then exit( 3 )
*/
int main( int argc, char* argv[] ) {
	// allocate variables
	state_main.filename_in = calloc( 256, sizeof( char ) );
	ASSERT( state_main.filename_in, 3, "[MEMORY ERROR] state_main.filename_out failed to allocate.\n" );
	state_main.filename_out = calloc( 256, sizeof( char ) );
	ASSERT( state_main.filename_out, 3, "[MEMORY ERROR] state_main.filename_out failed to allocate.\n" );

	// set default actions if none are specified
	state_main.action = RUN_NULL;
	state_main.asver = ASM_16_89;

	for( int i = 0; i < argc; i++ ) {
		// SYNTAX / BINARY MODE
		// --syntax: syntax modifier for different assembly versions
		if( strcmp( argv[ i ], "--syntax" ) == 0 ) {
			if( strcmp( argv[ i+1 ], "casm16-89" ) == 0 ) {
				state_main.asver = ASM_16_89;
			} else if( strcmp( argv[ i+1 ], "casm32" ) == 0 ) {
				state_main.asver = ASM_32;
			} else {
				printf( "Error: no syntax version specified.\n" );
				free( state_main.filename_in );
				free( state_main.filename_out );
				exit( 1 );
			}
		}

		// COMPILE
		// -as: specify input assembly file (required)
		if( strcmp( argv[ i ], "-as" ) == 0 ) {
			state_main.action = RUN_ASSEMBLE;
			if( i+1 >= argc ) {
				printf( "Error: not enough arguments.\n" );
				free( state_main.filename_in );
				free( state_main.filename_out );
				exit( 1 );
			}
			// copy cmd line argument data into input file name
			// tried to do extension checking but it didn't work
			memcpy( state_main.filename_in, argv[ i+1 ], strlen( argv[ i+1 ] ) );
		}

		// -o: specify output file (either -o or -O required)
		if( strcmp( argv[ i ], "-o" ) == 0 ) {
			// no file specified after cmd line argument
			if( i+1 >= argc ) {
				printf( "Error: not enough arguments.\n" );
				free( state_main.filename_in );
				free( state_main.filename_out );
				exit( 1 );
			}
			// copy argument data into output file variable
			memcpy( state_main.filename_out, argv[ i+1 ], strlen( argv[ i+1 ] ) );
		}
		// -O: reuse input file name as output file (-as test.csm -O = test.cb file created)
		if( strcmp( argv[ i ], "-O" ) == 0 ) {
			// input file was not found
			if( !state_main.filename_in ) {
				printf( "Error: cannot reuse input file as output file (not found?)\n" );
				free( state_main.filename_in );
				free( state_main.filename_out );
				exit( 1 );
			}
			// copy input file name to output file name
			memcpy( state_main.filename_out, state_main.filename_in, strlen( state_main.filename_in ) );
			// begin extension replacement
			for( int k = 0; k < strlen( state_main.filename_out ); k++ ) {
				// find a period
				if( state_main.filename_out[ k ] == '.' ) {
					// make sure the extension is ".csm"
					if( state_main.filename_out[ k+1 ] == 'c' &&
						state_main.filename_out[ k+2 ] == 's' &&
						state_main.filename_out[ k+3 ] == 'm' ) { // hard to read - "if" statement ends here
																  // the extension already starts with 'c', so just append 'b' and a null terminator
						state_main.filename_out[ k+2 ] = 'b';
						state_main.filename_out[ k+3 ] = 0;
						// stop looking
						break;
					}
				}
			}
		}

		// DISASSEMBLE
		// -disas: set the comiler mode to disassemble a given input file
		if( strcmp( argv[ i ], "-disas" ) == 0 ) {
			// show all data by default
			if( strcmp( argv[ i+1 ], "hex" ) == 0 ) {
				// only show hex data
				state_main.action = RUN_DISASSEMBLE_HEX;
			}
			else if( strcmp( argv[ i+1 ], "all" ) == 0 ) {
				// confirm that all data will be displayed
				state_main.action = RUN_DISASSEMBLE_ALL;
			} else {
				// error, no argument provided
				printf( "Error: no disassembly detail specification provided.\n" );
				free( state_main.filename_in );
				free( state_main.filename_out );
				exit( 1 );
			}
		}

		// -bin: specify ".cb" file to disassemble (required)
		if( strcmp( argv[ i ], "-bin" ) == 0 ) {
			// no file specified after cmd line argument
			if( i+1 >= argc ) {
				printf( "Error: not enough arguments.\n" );
				free( state_main.filename_in );
				free( state_main.filename_out );
				exit( 1 );
			}
			// copy argument data into input file variable
			memcpy( state_main.filename_in, argv[ i+1 ], strlen( argv[ i+1 ] ) );
		}
	}

	// return value of functions
	int action_return = 0;
	switch( state_main.action ) {
	case RUN_NULL:
		break;
	case RUN_ASSEMBLE:
		action_return = -1;
		if( state_main.asver == ASM_16_89 ) action_return = casm16_89( state_main.filename_in, state_main.filename_out );
		if( state_main.asver == ASM_32 ) action_return = asm32( state_main.filename_in, state_main.filename_out );
		// notify user if function exits with an error code
		if( action_return > 0 ) {
			if( state_main.asver == ASM_16_89 ) printf( "The assembler exited with a return value of %d, 0x%X; casm16-89\n", action_return, action_return );
			if( state_main.asver == ASM_32 ) printf( "The assembler exited with a return value of %d, 0x%X; casm32\n", action_return, action_return );
		}
		if( action_return < 0 ) printf( "The assembler was not given a syntax version and could not proceed.\n" );
		break;
	case RUN_DISASSEMBLE_ALL:
		printf( "Status RUN_DISASSEMBLE_ALL has not been implemented yet.\n" );
	case RUN_DISASSEMBLE_HEX:
		printf( "Status RUN_DISASSEMBLE_HEX has not been implemented yet.\n" );
		break;
	}
	// clean up
	free( state_main.filename_in );
	free( state_main.filename_out );
	// application will always exit success, other functions may not
	return 0;
}
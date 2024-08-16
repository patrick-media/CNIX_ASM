#include"emu.h"
#include<SDL.h>

struct {
	SDL_Window* window;
	SDL_Texture* texture;
	SDL_Renderer* renderer;
	unsigned int* pixels;
	bool quit;
} state_emu;

machine16_t machine16 = { 0 };

unsigned char _emu_token_f( char* _str, unsigned int place );
void printf_bin8( unsigned char _num, bool ln );
short* _emu_register_op( int regnum, bool ext, bool sp );
short* _emu_arg( int arg, int desc, int op );

unsigned int g_token_place = 0;

void* free_list[ 128 ] = { 0 };
int fl_place = 0;

/*
* Modes:
*	0 - casm16-89
*	1 - asm32
*/
int emu( char* filename_in, int mode ) {
	// allocate pixels
	state_emu.pixels = calloc( WIDTH*HEIGHT, sizeof( *state_emu.pixels ) );
	// check for error
	ASSERT_ERRC( state_emu.pixels, 3, "[MEMORY ERROR] state_emu.pixels failed to allocate.\n" );

	// BEGIN SDL SETUP

	// initialize SDL
	ASSERT(
		!SDL_Init( SDL_INIT_VIDEO ),
		"SDL failed to initialize: %s\n",
		SDL_GetError()
	);
	// create SDL window
	state_emu.window = SDL_CreateWindow(
		"TEST",
		SDL_WINDOWPOS_CENTERED_DISPLAY( 0 ),
		SDL_WINDOWPOS_CENTERED_DISPLAY( 0 ),
		W_WIDTH,
		W_HEIGHT,
		SDL_WINDOW_ALLOW_HIGHDPI
	);
	// error check
	ASSERT( state_emu.window, "failed to create SDL window: %s\n", SDL_GetError() );
	// create SDL renderer
	state_emu.renderer = SDL_CreateRenderer( state_emu.window, -1, SDL_RENDERER_PRESENTVSYNC );
	// error check
	ASSERT( state_emu.renderer, "failed to create SDL renderer: %s\n", SDL_GetError() );
	// create SDL texture
	state_emu.texture = SDL_CreateTexture(
		state_emu.renderer,
		SDL_PIXELFORMAT_ARGB8888,
		SDL_TEXTUREACCESS_STREAMING,
		WIDTH,
		HEIGHT
	);
	// error check
	ASSERT( state_emu.texture, "failed to create SDL texture: %s\n", SDL_GetError() );

	// END SDL SETUP

	machine16.regs.a = &machine16.regs.xa;
	machine16.regs.b = &machine16.regs.xb;
	machine16.regs.c = &machine16.regs.xc;
	machine16.regs.d = &machine16.regs.xd;
	printf( "memory begin\t = %x\n", &machine16.mem[ 0 ] );
	printf( "&a\t\t = %x\n", machine16.regs.a );
	printf( "&b\t\t = %x\n", machine16.regs.b );
	printf( "&c\t\t = %x\n", machine16.regs.c );
	printf( "&d\t\t = %x\n", machine16.regs.d );
	printf( "&xa\t\t = %x\n", &machine16.regs.xa );
	printf( "&xb\t\t = %x\n", &machine16.regs.xb );
	printf( "&xc\t\t = %x\n", &machine16.regs.xc );
	printf( "&xd\t\t = %x\n", &machine16.regs.xd );
	printf( "&pc\t\t = %x\n", &machine16.regs.pc );
	printf( "&sp\t\t = %x\n", &machine16.regs.sp );
	printf( "&ep\t\t = %x\n\n", &machine16.regs.ep );

	LPOFSTRUCT f_file_of = calloc( 64, sizeof( *f_file_of ) );
	HANDLE f_file = OpenFile( filename_in, f_file_of, OF_READ );
	unsigned int f_filesize = GetFileSize( f_file, NULL );

	if( f_filesize < 512 ) f_filesize = 512;
	else if( ( f_filesize % 512 ) > 0 ) f_filesize += 128;
	char* f_buffer = calloc( f_filesize, sizeof( *f_buffer ) );

	ASSERT_ERRC( f_file_of, 3, "[MEMORY ERROR] f_file_of failed to allocate.\n" );
	ASSERT_ERRC( f_file, 3, "[MEMORY ERROR] f_file failed to allocate.\n" );
	ASSERT_ERRC( f_buffer, 3, "[MEMORY ERROR] f_buffer failed to allocate.\n" );
	ASSERT_ERRC( ReadFile( f_file, f_buffer, f_filesize, NULL, NULL ), 1, "Error reading file.\n" );

	unsigned char fillmem = _emu_token_f( f_buffer, g_token_place );
	for( int i = 0; i < f_filesize; i++ ) {
		machine16.mem[ i ] = fillmem;
		fillmem = _emu_token_f( f_buffer, g_token_place );
	}
	// reset since the program was just loaded into memory and we reused functions
	g_token_place = 0;
	
	/*
	for( int i = 0; i < f_filesize/3; i++ ) {
		if( machine16.mem[ i ] < 0x10 ) printf( "0" );
		printf( "%x ", machine16.mem[ i ] );
	}
	printf( "\n\n" );
	*/

	// asm version
	unsigned char asm_ver = machine16.mem[ 0 ];

	// variables to use in the simulation
	unsigned short m_address = 1;

	// !! TODO !! interpret labels properly as addresses to jump to instead of addresses to read data from
	int tryc = 0;
	while( !state_emu.quit ) {
		// MAIN LOOP
		// poll events
		SDL_Event ev;
		while( SDL_PollEvent( &ev ) ) {
			switch( ev.type ) {
			case SDL_QUIT:
				state_emu.quit = true;
				break;
			}
		}

		// BEGIN EMULATION LOOP

		unsigned char op = machine16.mem[ m_address ] & 0b00001111;
		unsigned char desc = machine16.mem[ m_address ] & 0b11110000;
		m_address++; // we read one byte, so advance
		unsigned char argdesc = machine16.mem[ m_address ];
		m_address++; // one byte read, advance

		unsigned int op1 = 0;
		unsigned int op2 = 0;
		if( op != I_NOP ) {
			// check for first arg0 bit (1 byte if true)
			if( argdesc & 0b01000000 ) {
				// check for second arg0 bit (3 bytes if true)
				if( argdesc & 0b10000000 ) {
					// 4 bytes
					for( int i = 0; i < 4; i++ ) {
						op1 |= machine16.mem[ m_address ] << i*8;
						//printf( "read memory %x\n", m_address );
						m_address++;
					}
				} else {
					// 1 byte
					op1 |= machine16.mem[ m_address ];
					//printf( "read memory %x\n", m_address );
					m_address++;
				}
			} else {
				// check for second arg0 bit (2 bytes if true)
				if( argdesc & 0b10000000 ) {
					// 2 bytes
					op1 |= machine16.mem[ m_address ];
					//printf( "read memory %x\n", m_address );
					m_address++;
					op1 |= machine16.mem[ m_address ] << 8;
					//printf( "read memory %x\n", m_address );
					m_address++;
				} else {
					// error
					printf( "Error: arg0 invalid argdesc.\n" );
					exit( 2 );
				}
			}
			if( op != I_DEF && op != I_SHL && op != I_SHR && op != I_PUSH && op != I_POP ) {
				// check for first arg1 bit (1 byte if true)
				if( argdesc & 0b00010000 ) {
					// check for second arg0 bit (3 bytes if true)
					if( argdesc & 0b00100000 ) {
						// 4 bytes
						for( int i = 0; i < 4; i++ ) {
							op2 |= machine16.mem[ m_address ] << i*8;
							//printf( "read memory %x\n", m_address );
							m_address++;
						}
					} else {
						// 1 byte
						op2 |= machine16.mem[ m_address ];
						//printf( "read memory %x\n", m_address );
						m_address++;
					}
				} else {
					// check for second arg0 bit (2 bytes if true)
					if( argdesc & 0b00100000 ) {
						// 2 bytes
						op2 |= machine16.mem[ m_address ];
						//printf( "read memory %x\n", m_address );
						m_address++;
						op2 |= machine16.mem[ m_address ] << 8;
						//printf( "read memory %x\n", m_address );
						m_address++;
					} else {
						// error
						printf( "Error: arg1 invalid argdesc.\n" );
						exit( 2 );
					}
				}
			}
		}

		printf( "op1 = %x\n", op1 );
		printf( "op2 = %x\n", op2 );

		// generalized operation pointers
		// will point to registers or values in memory
		short* result = 0;
		short* operand = 0;

		// establish opcode
		switch( op ) {
		// done
		case I_NOP:
		{
			break;
		}
		case I_DEF:
			//printf( "handled d\n" );
			break;
		// done
		case I_MOV:
		{
			// ARG0
			result = _emu_arg( 0, desc, op1 );
			// ARG1
			operand = _emu_arg( 1, desc, op2 );
			if( result != NULL && operand != NULL ) {
				*result = *operand;
			}
			else {
				printf( "[MOV] Error: result or operand is a null-pointer.\n" );
				exit( 2 );
			}
			break;
		}
		// done
		case I_ADD:
		{
			// ARG0
			result = _emu_arg( 0, desc, op1 );
			// ARG1
			operand = _emu_arg( 1, desc, op2 );
			if( result != NULL && operand != NULL ) {
				*result += *operand;
			}
			else {
				printf( "[ADD] Error: result or operand is a null-pointer.\n" );
				exit( 2 );
			}
			break;
		}
		// done
		case I_SUB:
		{
			// ARG0
			result = _emu_arg( 0, desc, op1 );
			// ARG1
			operand = _emu_arg( 1, desc, op2 );
			if( result != NULL && operand != NULL ) {
				*result -= *operand;
			}
			else {
				printf( "[SUB] Error: result or operand is a null-pointer.\n" );
				exit( 2 );
			}
			break;
		}
		// done
		case I_MUL:
		{
			// ARG0
			result = _emu_arg( 0, desc, op1 );
			// ARG1
			operand = _emu_arg( 1, desc, op2 );
			if( result != NULL && operand != NULL ) {
				*result *= *operand;
			}
			else {
				printf( "[MUL] Error: result or operand is a null-pointer.\n" );
				exit( 2 );
			}
			break;
		}
		// done
		case I_DIV:
		{
			// ARG0
			result = _emu_arg( 0, desc, op1 );
			// ARG1
			operand = _emu_arg( 1, desc, op2 );
			if( result != NULL && operand != NULL ) {
				*result /= *operand;
			}
			else {
				printf( "[DIV] Error: result or operand is a null-pointer.\n" );
				exit( 2 );
			}
			break;
		}
		// done
		case I_SHL:
		{
			// ARG0
			result = _emu_arg( 0, desc, op1 );
			if( result != NULL ) {
				*result <<= 1;
			}
			else {
				printf( "[SHL] Error: result is a null-pointer.\n" );
				exit( 2 );
			}
			break;
		}
		// done
		case I_SHR:
		{
			// ARG0
			result = _emu_arg( 0, desc, op1 );
			if( result != NULL ) {
				*result >>= 1;
			}
			else {
				printf( "[SHR] Error: result is a null-pointer.\n" );
				exit( 2 );
			}
			break;
		}
		// TODO: logic for these ones since they're more complicated
		case I_IN:
			//printf( "handled in\n" );
			break;
		case I_OUT:
			//printf( "handled out\n" );
			break;
		case I_CMP:
		{
			// ARG0
			result = _emu_arg( 0, desc, op1 );
			// ARG1
			operand = _emu_arg( 1, desc, op2 );
			if( result != NULL && operand != NULL ) {
				// TODO this isn't right currently
				*result += *operand;
			}
			else {
				printf( "[CMP] Error: result or operand is a null-pointer.\n" );
				exit( 2 );
			}
			break;
		}
		case I_PUSH:
			//printf( "handled push\n" );
			break;
		case I_POP:
			//printf( "handled pop\n" );
			break;
		case I_RESV:
			printf( "Unhandled opcode: I_RESV\n" );
			exit( 2 );
			break;
		case I_SYM:
			printf( "Unhandled opcode: I_SYM\n" );
			exit( 2 );
			break;
		}

		printf( "Register dump cycle %d:\n", tryc );
		printf( "a = %x\tb = %x\tc = %x\td = %x\n", *machine16.regs.a, *machine16.regs.b, *machine16.regs.c, *machine16.regs.d );
		printf( "xa = %x\txb = %x\txc = %x\txd = %x\n", machine16.regs.xa, machine16.regs.xb, machine16.regs.xc, machine16.regs.xd );
		printf( "Memory dump cycle %d:\n", tryc );
		printf( "location 0xDEAD = %x\n\n", machine16.mem[ 0xDEAD ] );

		memset( state_emu.pixels, 0, sizeof( state_emu.pixels ) );
		// display bit
		if( machine16.regs.fl & 1 ) {
			printf( "Unhandled flag mode: display bit 00000001\n" );
			exit( 2 );
		} else {
			/*
			for( int i = 0; i < 0x190; i++ ) {
				memset( state_emu.pixels[ i ], machine16.mem[ 0xFE6F + i ], 1 );
			}
			*/
		}
		memset( state_emu.pixels, 0xFF0000FF, 200 );

		tryc++;
		if( tryc == 9 ) state_emu.quit = true;

		// END EMULATION LOOP

		// END MAIN LOOP
		// set textures
		SDL_UpdateTexture( state_emu.texture, NULL, state_emu.pixels, WIDTH*4 );
		SDL_RenderCopyEx(
			state_emu.renderer,
			state_emu.texture,
			NULL, NULL, 0.0, NULL,
			SDL_FLIP_VERTICAL
		);
		SDL_RenderPresent( state_emu.renderer );
	}
	free( state_emu.pixels );
	SDL_DestroyTexture( state_emu.texture );
	SDL_DestroyRenderer( state_emu.renderer );
	SDL_DestroyWindow( state_emu.window );
	return 0;
}

unsigned char _emu_token_f( char* _str, unsigned int place ) {
	char* temp = calloc( 3, sizeof( *temp ) );
	ASSERT_ERRC( temp, 3, "[MEMORY ERROR] temp failed to allocate.\n" );

	int i = place;
	for( int k = 0; k < 2; i++, k++ ) {
		if( _str[ i ] == '\0' ) return NULL;
		temp[ k ] = _str[ i ];
	}
	temp[ 2 ] = '\0';
	i++;
	g_token_place = i;
	return strtoul( temp, NULL, 16 ) & 0xFF;
}
void printf_bin8( unsigned char _num, bool ln ) {
	for( int i = 0; i < 8; i++ ) {
		if( _num & ( 0b10000000 >> i ) ) printf( "1" );
		else printf( "0" );
	}
	if( ln ) printf( "\n" );
}
short* _emu_register_op( int regnum, bool ext, bool sp ) {
	if( regnum == 0 ) {
		if( ext ) {
			return &machine16.regs.xa;
		} else if( sp ) {
			return &machine16.regs.pc;
		} else {
			return machine16.regs.a;
		}
	}
	if( regnum == 1 ) {
		if( ext ) {
			return &machine16.regs.xb;
		} else if( sp ) {
			return &machine16.regs.fl;
		} else {
			return machine16.regs.b;
		}
	}
	if( regnum == 2 ) {
		if( ext ) {
			return &machine16.regs.xc;
		} else if( sp ) {
			return &machine16.regs.sp;
		} else {
			return machine16.regs.c;
		}
	}
	if( regnum == 3 ) {
		if( ext ) {
			return &machine16.regs.xd;
		} else if( sp ) {
			return &machine16.regs.ep;
		} else {
			return machine16.regs.d;
		}
	}
}
short* _emu_arg( int arg, int desc, int op ) {
	if( arg == 0 ) {
		if( ( ( desc & 0b11000000 ) >> 6 ) == 0 ) {
			// arg = register
			int regnum = op & 0b00111111;
			bool ext = op & 0b01000000;
			bool sp = op & 0b10000000;
			return _emu_register_op( regnum, ext, sp );
			//printf( "(reg) result = %x\n", ret );
		}
		else if( ( ( desc & 0b11000000 ) >> 6 ) == 1 || ( ( desc & 0b11000000 ) >> 6 ) == 2 ) {
			// arg = int / str - treated the same
			short* temp = calloc( 1, sizeof( *temp ) );
			free_list[ fl_place ] = temp;
			fl_place++;
			*temp = op & 0xFFFF;
			return temp;
		}
		else if( ( ( desc & 0b11000000 ) >> 6 ) == 3 ) {
			// arg = addr
			return &machine16.mem[ op & 0xFFFF ];
		}
		else {
			printf( "Error: invalid desc found in arg0: desc = %x.\n", desc );
			exit( 2 );
		}
	} else {
		if( ( ( desc & 0b00110000 ) >> 4 ) == 0 ) {
			// arg = register
			int regnum = op & 0b00111111;
			bool ext = op & 0b01000000;
			bool sp = op & 0b10000000;
			return _emu_register_op( regnum, ext, sp );
		}
		else if( ( ( desc & 0b00110000 ) >> 4 ) == 1 || ( ( desc & 0b00110000 ) >> 4 ) == 2 ) {
			// arg = int / str - treated the same
			short* temp = calloc( 1, sizeof( *temp ) );
			free_list[ fl_place ] = temp;
			fl_place++;
			*temp = op & 0xFFFF;
			return temp;
		}
		else if( ( ( desc & 0b00110000 ) >> 4 ) == 3 ) {
			// arg = addr
			return &machine16.mem[ op & 0xFFFF ];
		}
		else {
			printf( "Error: invalid desc found in arg1: desc = %x.\n", desc );
			exit( 2 );
		}
	}
}
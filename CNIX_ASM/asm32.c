#define INTERNAL32
#include"asm32.h"

static kwds_t kwds[] = {
	{ I_NOP, "nop" },
	{ I_MOV, "mov" },
	{ I_LDM, "ldm" },
	{ I_STR, "str" },
	{ I_ADD, "add" },
	{ I_SUB, "sub" },
	{ I_MUL, "mul" },
	{ I_DIV, "div" },
	{ I_SHL, "shl" },
	{ I_SHR, "shr" },
	{ I_ROR, "ror" },
	{ I_AND, "and" },
	{ I_OR, "or" },
	{ I_NOR, "nor" },
	{ I_XOR, "xor" },
	{ I_NOT, "not" },
	{ I_PUSH, "push" },
	{ I_POP, "pop" },
	{ I_PUSHA, "pusha" },
	{ I_POPA, "popa" },
	{ I_IN, "in" },
	{ I_OUT, "out" },
	{ I_B, "b" },
	{ I_BR, "br" },
	{ I_RTB, "rtb" }
};

char skip( void );
int op( char c );
int ps_atoi( char c );
tnode_t* mknode( op_t op, tnode_t* lvalue, tnode_t* rvalue, long long value );
void delnode( tnode_t* node );
void scan( void );
int scanint( char c );

const char* statement1 = "45 +  5- 9";

char* g_buffer = 0;
token_t g_token = { 0 };
int g_line = 0;
char g_pb = 0;
tnode_t g_ast = { 0 };

int asm32( char* filename_in, char* filename_out ) {
	//g_buffer = calloc( 32, sizeof( *g_buffer ) );
	g_buffer = statement1;
	
	char c = 0;
	while( true ) {
		scan();
		if( g_token.type == T_EOF ) break;
		if( g_token.type == T_LIT_INT ) printf( "val = %d\n", g_token.val );
		if( g_token.type > 1 ) printf( "op = %d\n", g_token.type );
	}

	//free( g_buffer );
	return 0;
}
void expr( void ) {

}
int ps_atoi( char c ) {
	for( int i = 48; i < 58; i++ ) {
		if( c == i ) return i - 48;
	}
	return -1;
}
int op( char c ) {
	switch( c ) {
	case '+':
		return T_PLUS;
	case '-':
		return T_MINUS;
	case '*':
		return T_STAR;
	case '/':
		return T_SLASH;
	case '\0':
		return T_EOF;
	default:
		return T_LIT_INT;
	}
	return -1;
}
char skip( void ) {
	char c = next();
	while( c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' ) {
		if( c == '\n' ) g_line++;
		printf( "skip\n" );
		//printf( "skip: c = '%c'\n", c );
		c = next();
	}
	return c;
}
tnode_t* mknode( op_t op, tnode_t* lvalue, tnode_t* rvalue, long long value ) {
	tnode_t* retnode = calloc( 1, sizeof( *retnode ) );
	if( !retnode ) {
		printf( "mknode: failed to allocate memory for AST node.\n" );
		exit( 1 );
	}
	retnode->op = op;
	retnode->lvalue = lvalue;
	retnode->rvalue = rvalue;
	retnode->value = value;
	return retnode;
}
void delnode( tnode_t* node ) {
	free( node );
}
int next( void ) {
	printf( "next\n" );
	//printf( "next: *g_buffer = '%c'\n", *g_buffer );
	return *g_buffer++;
}
void scan( void ) {
	printf( "scan\n" );
	//printf( "g_buffer1 = %s\n", g_buffer );
	char c = skip();
	//printf( "g_buffer2 = %s\n", g_buffer );
	//printf( "scan: c = '%c'\n", c );
	switch( c ) {
	case '\0':
		g_token.type = T_EOF;
		break;
	case '+':
		g_token.type = T_PLUS;
		break;
	case '-':
		g_token.type = T_MINUS;
		break;
	case '*':
		g_token.type = T_STAR;
		break;
	case '/':
		g_token.type = T_SLASH;
		break;
	default:
		if( isdigit( c ) ) {
			g_token.val = scanint( c );
			g_token.type = T_LIT_INT;
			break;
		}
		printf( "Syntax error: unknown token '%c' on line %d.\n", c, g_line );
		exit( 1 );
		break;
	}
	return;
}
int scanint( char c ) {
	int value = 0;
	c = next();
	printf( "scanint\n" );
	while( isdigit( c ) ) {
		printf( "scanint loop\n" );
		value = value * 10 + ps_atoi( c );
		//printf( "scanint: c = '%c'\n", c );
		c = next();
		//printf( "scanint2: c = '%c'\n", c );
	}
	printf( "scanint exit\n" );
	printf( "*g_buffer = '%c'\n", *g_buffer );
	//printf( "scanint3: c = '%c'\n", c );
	return value;
}
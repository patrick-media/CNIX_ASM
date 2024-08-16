#ifndef ASM_UTIL_H
#define ASM_UTIL_H

#include<stdbool.h>
#include<stdio.h>
#include<string.h>
#include<stdlib.h>
#include<Windows.h>

#define ASSERT( _e, errc, ... ) if( ( !_e ) ) { fprintf( stderr, __VA_ARGS__ ); exit( errc ); }
#define ASSERT_RET( _e, errc, ... ) if( ( !_e ) ) { fprintf( stderr, __VA_ARGS__ ); asm_retval = errc; goto asm_cleanup; }

#endif // ASM_UTIL_H
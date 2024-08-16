#ifndef EMU_UTIL_H
#define EMU_UTIL_H

#include<stdbool.h>
#include<stdio.h>
#include<string.h>
#include<stdlib.h>
#include<Windows.h>

#define ASSERT( _e, ... ) if( ( !_e ) ) { fprintf( stderr, __VA_ARGS__ ); exit( 1 ); }
#define ASSERT_ERRC( _e, errc, ... ) if( ( !_e ) ) { fprintf( stderr, __VA_ARGS__ ); exit( errc ); }

#define W_WIDTH  800
#define W_HEIGHT 800
#define WIDTH 200
#define HEIGHT 200

#endif // EMU_UTIL_H
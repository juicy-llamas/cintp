
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include "print_errno.h"

int main () {
	printf( "OUT:\n\t" );
	FILE* hewwo = fopen( "main.c", "r+" );
	printf( "hewwo: 0x%lx\n\terror: %s\n\t", hewwo, errnoname( errno ) );
	FILE* hewwo2 = fopen( "main.c", "r+" );
	printf( "hewwo2: 0x%lx\n\terror: %s\n\t", hewwo2, errnoname( errno ) );

	fclose( hewwo );

	FILE* hewwo3 = fopen( "main.c", "r+" );
	printf( "hewwo3: 0x%lx\n\terror: %s\n\t", hewwo3, errnoname( errno ) );
	FILE* hewwo4 = fopen( "main.c", "r+" );
	printf( "hewwo4: 0x%lx\n\terror: %s\n", hewwo4, errnoname( errno ) );
// 	for ( int i = 0; i < 512; i++ ) {
// 		putchar( getc( hewwo2 ) );
// 		putchar( getc( hewwo3 ) );
// 		putchar( getc( hewwo4 ) );
// 	}
	fprintf( hewwo4, "this is new" );
	fprintf( hewwo3, "hewwo" );
	fclose( hewwo3 );
	fclose( hewwo4 );


}

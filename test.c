#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
// fork & unlink (and stat)
#include <unistd.h>
#include <sys/types.h>
// ioctl
#include <sys/ioctl.h>
#include <termios.h>
// signal, self explanatory
#include <signal.h>
// wait
#include <sys/wait.h>
// time
#include <time.h>
// stat
#include <sys/stat.h>
// uints
#include <stdint.h>

int save_code = 0;
FILE* itmp = 0;
struct termios defaults;

unsigned int lptrbuf_s = 32;
struct lin* lptrbuf = 0;

struct editor_state E;

int main () {

	if ( atexit( cleanup ) != 0 )
		ERR_QUIT( "atexit failed" );

// 	term init
	{
// 		get term size for editor
		struct winsize dimn;
		if ( ioctl( STDOUT_FILENO, TIOCGWINSZ, &dimn ) == -1 )
			ERR_QUIT( "ioctl failed" );

// 		unset canoical mode so we can get that juicy character by character play (and esc sequences don't print)
		struct termios attr;
		tcgetattr( STDOUT_FILENO, &defaults );
		memcpy( &attr, &defaults, sizeof( struct termios ) );
		attr.c_lflag &= ~( ICANON | ECHO | ISIG );
		attr.c_iflag &= ~( IXON | ICRNL | BRKINT );
		tcsetattr( STDOUT_FILENO, TCSANOW, &attr );
	}

	init_E( 0 );

	int c;
	while ( ( c = getchar() ) != 0x11 ) {
		if ( E.line_amt >= E.line_lim ) {
			RREALLOC( E.lptrbuf, E.lptrbuf_s, E.lptrbuf_s << 1 );
			E.line_amt = ( (E.lptrbuf_s >> 1) + (E.lptrbuf_s >> 2) ) / sizeof( struct lin );
		}
		if ( E.char_amt >= E.char_lim ) {
			RREALLOC( E.cur_lp, E., lptrbuf[ cur_l ].size << 1 );
			E.char_lim = ( (lptrbuf[ cur_l ].size >> 1) + (lptrbuf[ cur_l ].size >> 2) );
		}
		putchar( c );
	}
}

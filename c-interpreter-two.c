
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

#define inam "./itmp.c"
#define onam "./omtp"
// How long do we wait for GCC to do its thing?
#define TIMEOUT 3

#define LOG( fmt, ... ) 											\
printf( "LOG (file %s, line %i): " fmt "\n",						\
		__FILE__, __LINE__, ##__VA_ARGS__ );

#define ERR( fmt, ... ) ({											\
	char* estr = strerror( errno );									\
	fprintf( stderr, "ERROR (file %s, line %i): " 					\
			fmt ": %s\n", __FILE__, __LINE__, 						\
			##__VA_ARGS__, estr );									\
})

#define ERR_QUIT( fmt, ... ) ({										\
	char* estr = strerror( errno );									\
	fprintf( stderr, "FATAL ERROR (file %s, line %i): " 			\
			fmt ": %s\n", __FILE__, __LINE__, 						\
			##__VA_ARGS__, estr );									\
	cleanup();														\
	abort();														\
})

// stub that wraps realloc
#define RREALLOC( ptr, sz, nsz ) ({									\
	printf( "RREALLOC: line = %i, sz = %i, nsz = %i\n", __LINE__, sz, nsz ); \
	sz = nsz;														\
	if ( !( ptr = realloc( ptr, sz ) ) )							\
		ERR_QUIT( "mem broke" );									\
})

#define MMALLOC( ptr, sz, nsz ) ({									\
	printf( "MMALLOC: line = %i, sz = %i, nsz = %i\n", __LINE__, sz, nsz ); \
	sz = nsz;														\
	if ( !( ptr = realloc( ptr, sz ) ) )							\
		ERR_QUIT( "mem broke" );									\
})

struct lin {
	char* line;
	uint32_t size;
	uint32_t chramt;
} __attribute__((__packed__));

#define LIN_S sizeof( struct lin )
#define LINE_S 64

int save_code = 0;
FILE* itmp = 0;
struct termios defaults;

unsigned int lptrbuf_s = 32;
struct lin* lptrbuf = 0;

void free_lptrbuf () {
	if ( lptrbuf ) {
		for ( unsigned int i = 0; i < lptrbuf_s / sizeof( struct lin ); i++ ) {
			if ( lptrbuf[ i ].line )
				free( lptrbuf[ i ].line );
		}
		free( lptrbuf );
	}
}

// sig handler deletes the generated files if you exit with SIGINT, as well as closes the fd
void cleanup () {
	if ( itmp )
		fclose( itmp );
	if ( !save_code )
		unlink( inam );
	unlink( onam );
	tcsetattr( STDOUT_FILENO, TCSANOW, &defaults );

	free_lptrbuf();

	printf( "\n" );
	exit( 0 );
}

unsigned int lg_p_1 ( unsigned int k ) {
	int i = 8;
	int j = 0x00008000 * ( k <= 0xFFFF ) + 0x00010000 * ( k > 0xFFFF );
	while ( i ) {
		j >>= ( k < j ) * i;
		j <<= ( k >= j ) * i;
		i >>= 1;
	}
	return ( j + 0x80000000 * ( k >> 31 ) ) << 1;
}

// default file text
static const char* default_file[] = {
	"#include <stdio.h>",
	"#include <stdlib.h>",
	"#include <string.h>",
	"",
	"int main ( int argc, char** argv ) {",
		"\t",
		"\t",
		"\treturn 0;",
	"}"
};
static int default_file_cs = 5;

int main ( int argc, char* argv[] ) {
	int child_pid = 0;
	int	template_selector = 1;
	char* template_fname = 0;
	int err_code = 0;

// 	arg handling. you can specify up to two prog args, anything after arg 3 is assumed to be for gcc.
	int arg_amt = 1;
	for ( int i = 1; i < argc && i < 3; i++ ) {
		if ( strcmp( argv[ i ], "sav" ) == 0 ) {
			save_code = 1;
			arg_amt++;
		} else if ( strcmp( argv[ i ], "ndef" ) == 0 ) {
			template_selector = 0;
			arg_amt++;
		} else if ( strcmp ( argv[ i ], "bench" ) == 0 ) {
			template_selector = 2;
			arg_amt++;
		} else if ( strcmp ( argv[ i ], "temp" ) == 0 ) {
			template_selector = 3;
			template_fname = argv[ ++i ];
			arg_amt += 2;
		} else if ( strcmp( argv[ i ], "help" ) == 0 ) {
			puts( "this is the help" );
			exit( 0 );
		}
	}
// 	i moved these here cause the help text
	if ( save_code )
		puts( "note: saving your code in itmp" );
	switch ( template_selector ) {
		case 0:
			puts( "note: not loading default stub" );
			break;
		case 2:
			puts( "note: loading benchmark template" );
			break;
		case 3:
			printf( "note: using custom template '%s'\n", template_fname );
			break;
	}

// 	5 extra args, and there are arg_amt args that are not for gcc
	char* gccargs[ argc + 5 - arg_amt ];
	gccargs[ 0 ] = "gcc";
	gccargs[ 1 ] = inam;
	gccargs[ 2 ] = "-o";
	gccargs[ 3 ] = onam;

	for ( int i = arg_amt; i < argc; i++ )
		gccargs[ i + 4 - arg_amt ] = argv[ i ];
	gccargs[ argc + 4 - arg_amt ] = 0;

// 	printing args, might remove this.
	{
		printf( "note: these are your args. love them. cherish them.\n" );
		int ii = 0;
		while ( gccargs[ ii ] != 0 ) {
			printf( "\targ %i: '%s'\n", ii, gccargs[ ii ] );
			ii++;
		}
	}

	if ( atexit( cleanup ) != 0 )
		ERR_QUIT( "atexit failed" );

// 	term init
	struct winsize dimn;
	{
// 		get term size for editor
		if ( ioctl( STDOUT_FILENO, TIOCGWINSZ, &dimn ) == -1 )
			ERR_QUIT( "ioctl failed" );

// 		unset canoical mode so we can get that juicy character by character play
		struct termios attr;
		tcgetattr( STDOUT_FILENO, &defaults );
		memcpy( &attr, &defaults, sizeof( struct termios ) );
		attr.c_lflag &= ~( ICANON | ECHO | ISIG );
		attr.c_iflag &= ~( IXON | ICRNL | BRKINT);
		tcsetattr( STDOUT_FILENO, TCSANOW, &attr );
	}

	{
// 		does itmp.c exist?
		struct stat buf;
		int cond = stat( "./itmp.c", &buf ) == 0;
		if ( cond ) {
			puts( "note: there's a file from last time (named itmp.c), press y to recover it or anything else to truncate it" );
// 			if so, prompt the user to recover it
			if ( (char)getchar() == 'y' ) {
				puts("");
				itmp = fopen( inam, "rw" );
				if ( itmp == 0 ) {
					puts( "opening the file failed, press y if you want to create a new file, or any other key to quit." );
					if ( (char)getchar() != 'y' )
						ERR_QUIT( "\ncannot open file" );
					itmp = (FILE*)-1;
				}
			}
			puts("");
		}
// 		if it doesn't exist or recovery failed, then just truncate / create a new file
		if ( !cond || itmp == 0 || itmp == (FILE*)-1 ) {
			if ( cond )
				puts( "the file was truncated." );
			itmp = fopen( inam, "w" );
			if ( itmp == 0 )
				ERR_QUIT( "program can't create a file here, try a different directory" );
		}
	}

	LOG( "LIN_S: %li", LIN_S );

	printf( "note: because i'm lazy, don't return -1, 127, or 2 as these are reserved for common errors\n" );
	printf( "note: ^Q will quit the program (and save if you have that enabled).\n" );
	printf( "press a (printable) key to enter the 'editor'.\n" );

	int c = 0;
	int cur_l = 0;
	int tot_l = 0;
	int posx = 0;
	int xlim = 0;
	int llim = 0;

	while ( (c = getchar()) != 0x11 ) {
// 		this clears stdin and is handy
		if ( freopen( "/dev/tty", "rw", stdin ) == 0 )
			ERR_QUIT( "reopening stdin failed" );
// 		clear the screen
		if ( system( "clear" ) != 0 )
			ERR( "problems clearing screen" );

		if ( (char)c == 's' ) {
			template_selector = 0;
		}
		c = 0;

// 		sets the default if applicable
		if ( template_selector ) {
			const char** tmplt;
// 			get the right template
			switch ( template_selector ) {
				default:
					ERR( "template selector invalid warning" );
				case 1:
					tmplt = default_file;
					tot_l = sizeof( default_file ) / sizeof( char* );
					break;
			}
// 			we first zero the entire structure if it exists
			if ( lptrbuf ) {
				for ( int i = 0; i < lptrbuf_s / sizeof( struct lin ); i++ ) {
					bzero( lptrbuf[ i ].line, lptrbuf[ i ].size );
					lptrbuf[ i ].chramt = 0;
				}
			}
// 			we initially malloc the line structure, or realloc if it exists.
			int new_sz = lg_p_1( tot_l * sizeof( struct lin ) );
			RREALLOC( lptrbuf, lptrbuf_s, new_sz > lptrbuf_s ? new_sz : lptrbuf_s );
			printf( "tot_l: %u, new_sz: %u, lptrbuf_s: %u\n", tot_l, new_sz, lptrbuf_s );
// 			we then copy the lines into the structures
			for ( int i = 0; i < tot_l; i++ ) {
				int tmplnsz = strlen( tmplt[ i ] ) + 1;
				lptrbuf[ i ].chramt = tmplnsz;
				int nsz = lg_p_1( tmplnsz );
				nsz = ( LINE_S > nsz ? LINE_S : nsz );
				RREALLOC( lptrbuf[ i ].line, lptrbuf[ i ].size, nsz > lptrbuf[ i ].size ? nsz : lptrbuf[ i ].size );
				memcpy( lptrbuf[ i ].line, tmplt[ i ], tmplnsz );
				puts( lptrbuf[ i ].line );
			}
		}


// 		a routimentary text editor

		llim = ( (lptrbuf_s >> 1) + (lptrbuf_s >> 2) );
		xlim = ( (lptrbuf[ cur_l ].size >> 1) + (lptrbuf[ cur_l ].size >> 2) );

		while ( (c = getchar()) != 0x04 && c != 0x11 ) {
// 			realloc buffers if we get beyond 3/4 close to filling them (sure not the most space efficient method, but does save reallocs and is fast).
			if ( cur_l >= llim ) {
				RREALLOC( lptrbuf, lptrbuf_s, lptrbuf_s << 1 );
				llim = ( (lptrbuf_s >> 1) + (lptrbuf_s >> 2) );
			}
			if ( lptrbuf[ cur_l ].chramt >= xlim ) {
				RREALLOC( lptrbuf[ cur_l ].line, lptrbuf[ cur_l ].size, lptrbuf[ cur_l ].size << 1 );
				xlim = ( (lptrbuf[ cur_l ].size >> 1) + (lptrbuf[ cur_l ].size >> 2) );
			}

			switch ( (char)c ) {
				case 0x1B:
					break;
				case 0x08:
					break;
				case 0x0A:
				case 0x0D:
					break;
				default:
					putchar( c );
					lptrbuf[ cur_l ].line[ posx++ ] = (char)c;
					lptrbuf[ cur_l ].chramt++;
			}
		}

		puts( "" );
		if ( c == -1 && posx == -1 )
			ERR( "realloc failed, saving what you have." );
// 		save file

// 		if user entered ^Q in editor, we just quit without running
		if ( c == 0x11 )
			break;

// 		compiling the code (with a child process so I can use execv)
		child_pid = fork();
		if ( child_pid == 0 ) {
			if ( execv( "/usr/bin/gcc", gccargs ) == -1 )
				ERR_QUIT( "(CHILD PROCESS) calling gcc failed" );
		} else if ( child_pid == -1 )
			ERR_QUIT( "calling fork failed" );

// 		early on I had a problem with fork bombing; this prevents that completely.
		if ( child_pid == 0 )
			ERR_QUIT( "(CHILD PROCESS) shouldn't be here" );

// 		either wait returns an error (the process terminated) or we timeout; then we proceed.
		time_t begin = time( 0 );
		time_t end = begin;
		int wait_ret = 0;
		while( ( wait_ret = wait( &err_code ) ) != 0 && wait_ret != -1 && ( end - begin < TIMEOUT ) )
			end = time( 0 );

// 		executing the code
		if ( err_code ) {
			if ( end - begin < TIMEOUT )
				ERR( "compilation failed with err %i", err_code );
			else
				ERR( "compilation timed out (current timeout is %i seconds)", TIMEOUT );
		} else {
			if ( ( err_code = system( onam ) ) == -1 )
				ERR( "c code failed to execute or returned -1" );
			else if ( err_code == 127 )
				ERR( "shell failed to execute or c code returned 127" );
			else if ( err_code == 2 )
				ERR( "executable doesn't exist or c code returned 2" );
			else if ( err_code != 0 )
				LOG( "c code returned nonzero exit code (or something else went wrong)" );
		}

// 		resetting stdin and waiting for user input
		if ( freopen( "/dev/tty", "rw", stdin ) == 0 )
			ERR_QUIT( "reopening stdin failed" );

		printf( "press the enter key to overwrite and create another program, or press ^Q to end program.\n" );
	}
}

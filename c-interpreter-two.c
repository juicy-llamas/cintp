
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

#define inam "./itmp.c"
#define onam "./omtp"
// How long do we wait for GCC to do its thing?
#define TIMEOUT 3
#define BLOCK_S 4096

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

int save_code = 0;
FILE* itmp = 0;
struct termios defaults;

// sig handler deletes the generated files if you exit with SIGINT, as well as closes the fd
void cleanup () {
	if ( itmp )
		fclose( itmp );
	if ( !save_code )
		unlink( inam );
	unlink( onam );
	tcsetattr( STDOUT_FILENO, TCSANOW, &defaults );
	printf( "\n" );
	exit( 0 );
}

// default file text
static const char default_file[] = "#include <stdio.h>\n#include <stdlib.h>\n#include <string.h>\n\nint main ( int argc, char** argv ) {\n\t\n\t\n\treturn 0;\n}";

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

// 	since it's only one buffer for the entire program, it's not really necessary to free since mem will get reclaimed anyways.
	char* filebuf = malloc( BLOCK_S );
	long unsigned int filesz = BLOCK_S;
	bzero( filebuf, filesz );

	int i = 0;

	printf( "note: because i'm lazy, don't return -1, 127, or 2 as these are reserved for common errors\n" );
	printf( "note: ^Q will quit the program (and save if you have that enabled).\n" );
	printf( "press a (printable) key to enter the 'editor'.\n" );

	while ( getchar() != 0x11 ) {
// 		this clears stdin and is handy
		if ( freopen( "/dev/tty", "rw", stdin ) == 0 )
			ERR_QUIT( "reopening stdin failed" );
		i++;
// 		clear the screen
		if ( system( "clear" ) != 0 )
			ERR( "problems clearing screen" );

		int amt = 0;
// 		sets the default if applicable
		if ( template_selector ) {
			const char* tmplt;
// 			get the right template
			switch ( template_selector ) {
				case 1:
					tmplt = default_file;
					amt = sizeof( default_file ) - 1;
					break;
			}
// 			print and store template in buffer, realloc if necessary
			if ( amt > filesz ) {
				int rem = amt % BLOCK_S;
				filesz = rem + amt + BLOCK_S;
				if ( !( filebuf = realloc( filebuf, filesz ) ) )
					ERR_QUIT( "realloc failed" );
			}
			memcpy( filebuf, tmplt, amt );
			fwrite( tmplt, amt, 1, stdout );
		}

// 		a routimentary text editor
		int c = 0;
		int posx = 0;
		int posy = 0;
		while ( (c = getchar()) != 0x04 && c != 0x11 ) {
// 			realloc buffer if we're getting close to filling it
			if ( amt >= filesz - 2 ) {
				filesz += BLOCK_S;
				if ( !( filebuf = realloc( filebuf, filesz ) ) ) {
					c = -1; posx = -1;
					break;
				}
			}

// 			bksp (don't want to bksp into buffer so amt > 0)
			if ( (char)c == 0x7F ) {
				if ( amt > 0 ) {
					filebuf[ --amt ] = 0;
					putchar( 0x08 );
					putchar( ' ' );
					putchar( 0x08 );
				}
			} else if ( (char)c == 0x1B ) {
				putchar( 0x1B );
				putchar( getchar() );
				putchar( getchar() );
				putchar( getchar() );
			} else {
				putchar( c );
				filebuf[ amt++ ] = (char)c;
			}
			printf( "\ndebug: %X\n", c );
		}

		puts( "" );
		if ( c == -1 && posx == -1 )
			ERR( "realloc failed, saving what you have." );
// 		save file
		if ( fwrite( filebuf, amt, 1, itmp ) == 0 )
			ERR_QUIT( "writing failed" );
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

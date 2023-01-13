hewwois newain.h"

int save_code = 0;
FILE* itmp = 0;
struct termios defaults;

unsigned int lptrbuf_s = 32;
struct lin* lptrbuf = 0;

struct editor_state E;

int main ( int argc, char* argv[] ) {
	int child_pid = 0;
	int template_selector = 1;
	char* template_fname = 0;
	int err_code = 0;
	unsigned int tab_width = 0;

// 	arg handling. you can specify up to two prog args, anything after arg 3 is assumed to be for gcc.
	int arg_amt = 1;
	for ( int i = 1; i < argc && i < 3; i++ ) {
		if ( strcmp( argv[ i ], "sav" ) == 0 ) {
			save_code = 1;
			arg_amt++;
		} else if ( strcmp( argv[ i ], "ndef" ) == 0 ) {
			template_select
			or = 0;
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

// 		unset canoical mode so we can get that juicy character by character play (and esc sequences don't print)
		struct termios attr;
		tcgetattr( STDOUT_FILENO, &defaults );
		memcpy( &attr, &defaults, sizeof( struct termios ) );
		attr.c_lflag &= ~( ICANON | ECHO | ISIG );
		attr.c_iflag &= ~( IXON | ICRNL | BRKINT );
		tcsetattr( STDOUT_FILENO, TCSANOW, &attr );
	}

// 	tab width
	tab_width = get_tab_width();
	printf( "tab width: %i\n", tab_width );

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

	printf( "note: because i'm lazy, don't return -1, 127, or 2 as these are reserved for common errors\n" );
	printf( "note: ^Q will quit the program (and save if you have that enabled).\n" );
	printf( "press a (printable) key to enter the 'editor'.\n" );

	int c = 0;
	int cur_l = 0;
	int tot_l = 0;
	int posx = 0;
	int ctabs = 0;
	int xlim = 0;
	int llim = 0;
	int scr_l = 0;

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
					cur_l = default_file_cs;
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
			bzero( lptrbuf, lptrbuf_s );
// 			printf( "tot_l: %u, new_sz: %u, lptrbuf_s: %u\n", tot_l, new_sz, lptrbuf_s );
// 			we then copy the lines into the structures
			for ( int i = 0; i < tot_l; i++ ) {
				int tmplnsz = strlen( tmplt[ i ] ) + 1;
				lptrbuf[ i ].chramt = tmplnsz - 1;
				int nsz = lg_p_1( tmplnsz );
				nsz = ( LINE_S > nsz ? LINE_S : nsz );

				RREALLOC( lptrbuf[ i ].line, lptrbuf[ i ].size, nsz > lptrbuf[ i ].size ? nsz : lptrbuf[ i ].size );
				memcpy( lptrbuf[ i ].line, tmplt[ i ], tmplnsz );
				puts( lptrbuf[ i ].line );

				int j = 0;
				while ( lptrbuf[ i ].line[ j ] != 0 )
					lptrbuf[ i ].tabs += ( lptrbuf[ i ].line[ j ] == '\t' );

				posx = posx + ( i == cur_l ) * lptrbuf[ i ].chramt;
			}
		}

// 		a routimentary text editor

		llim = ( (lptrbuf_s >> 1) + (lptrbuf_s >> 2) ) / sizeof( struct lin );
		xlim = ( (lptrbuf[ cur_l ].size >> 1) + (lptrbuf[ cur_l ].size >> 2) );
		scr_l = 0;
		plop_cursor( cur_l, );
		ctabs = lptrbuf[ cur_l ].tabs;

		while ( (c = getchar()) != 0x04 && c != 0x11 ) {
// 			realloc buffers if we get beyond 3/4 close to filling them (sure not the most space efficient method, but does save reallocs and is fast).
			if ( tot_l >= llim ) {
				RREALLOC( lptrbuf, lptrbuf_s, lptrbuf_s << 1 );
				llim = ( (lptrbuf_s >> 1) + (lptrbuf_s >> 2) ) / sizeof( struct lin );
			}
			if ( lptrbuf[ cur_l ].chramt >= xlim ) {
				RREALLOC( lptrbuf[ cur_l ].line, lptrbuf[ cur_l ].size, lptrbuf[ cur_l ].size << 1 );
				xlim = ( (lptrbuf[ cur_l ].size >> 1) + (lptrbuf[ cur_l ].size >> 2) );
			}

			switch ( (char)c ) {
				case 0x1B:
					if ( getchar() == '[' ) {
						int tmp = getchar();
// 						posmax = posx * ( tmp != 'A' && tmp != 'B' );
						switch( tmp ) {
							case 'A':
// 								puts( "UP KEY" );
								if ( cur_l > 0 ) {
									cur_l--;
// 									posx = ( posmax - lptrbuf[ cur_l ].chramt ) *
// 											( posmax < lptrbuf[ cur_l ].chramt ) +
// 											lptrbuf[ cur_l ].chramt;
									escape_seq( "F" );
// 									off_cursor( scrx );
								}
								break;
							case 'B':
// 								puts( "DOWN KEY" );
								if ( cur_l < tot_l-1 ) {
									cur_l++;
// 									posx = ( posmax - lptrbuf[ cur_l ].chramt ) *
// 											( posmax < lptrbuf[ cur_l ].chramt ) +
// 											lptrbuf[ cur_l ].chramt;
									escape_seq( "E" );
// 									off_cursor( scrx );
								}
								break;
							case 'C':
// 								puts( "RIGHT KEY" );
								if ( posx < lptrbuf[ cur_l ].chramt ) {
									ctabs += lptrbuf[ cur_l ].line[ posx ] == '\t';
									posx++;
									off_cursor( posx + ctabs*(tab_width - 1) );
								}
								break;
							case 'D':
// 								puts( "LEFT KEY" );
								if ( posx > 0 ) {
									ctabs -= lptrbuf[ cur_l ].line[ posx ] == '\t';
									posx--;
									off_cursor( posx + ctabs*(tab_width - 1) );
								}
								break;
							case 'H':
// 								puts( "HOME KEY" );
								posx = 0;
// 								set cursor pos
								char escseq[ 7 ];
								int term_l = cur_l - scr_l;
								escseq[ 0 ] = '0'; escseq[ 1 ] = ';';

								escseq[ 6 ] = 0;
								escape_seq( escseq );
								break;
							case 'F':
								puts( "END KEY" );
								break;
							case '3':
								if ( getchar() == '~' ) {
// 									puts( "DEL KEY" );
// 									same as bksp but doesn't decrement posx
									for ( int i = posx; i < lptrbuf[ cur_l ].chramt - 1; i++ ) {
										lptrbuf[ cur_l ].line[ i ] = lptrbuf[ cur_l ].line[ i+1 ];
									}
									lptrbuf[ cur_l ].line[ lptrbuf[ cur_l ].chramt - 1 ] = 1;
									lptrbuf[ cur_l ].chramt--;
								}
								break;
							case '5':
								if ( getchar() == '~' ) {
									puts( "PGUP KEY" );
								}
								break;
							case '6':
								if ( getchar() == '~' ) {
									puts( "PGDN KEY" );
								}
								break;
						}
					}
					break;
				case 0x7F:
// 					puts( "BKSP" );
// 					if there are chars in front of us, we move them LEFT, overwriting the char in posx-1
					if ( posx > 0 ) {
						for ( int i = --posx; i < lptrbuf[ cur_l ].chramt - 1; i++ ) {
							lptrbuf[ cur_l ].line[ i ] = lptrbuf[ cur_l ].line[ i+1 ];
						}
						lptrbuf[ cur_l ].line[ lptrbuf[ cur_l ].chramt - 1 ] = 0;
						lptrbuf[ cur_l ].chramt--;
	// 					print bksp
						escape_seq( "D" );
						escape_seq( "K" );
						escape_seq( "s" );
						printf( "%s", ( lptrbuf[ cur_l ].line + posx ) );
						escape_seq( "u" );
					} else if ( cur_l > 0 ) {
						if ( lptrbuf[ cur_l ].chramt + lptrbuf[ cur_l-1 ].chramt > lptrbuf[ cur_l-1 ].size )
							RREALLOC( lptrbuf[ cur_l-1 ].line, lptrbuf[ cur_l-1 ].size,
									  lg_p_1( lptrbuf[ cur_l ].chramt + lptrbuf[ cur_l-1 ].chramt ) );
						strncpy( lptrbuf[ cur_l-1 ].line + lptrbuf[ cur_l-1 ].chramt, lptrbuf[ cur_l ].line, lptrbuf[ cur_l ].chramt );
						free( lptrbuf[ cur_l ].line );
// 						printf( "curl = %i\n", cur_l );
						lptrbuf[ cur_l ].line = 0;
						for ( int i = cur_l; i < tot_l-2; i++ ) {
							lptrbuf[ i ].line = lptrbuf[ i+1 ].line;
							lptrbuf[ i ].chramt = lptrbuf[ i+1 ].chramt;
							lptrbuf[ i ].size = lptrbuf[ i+1 ].size;
						}
						cur_l--; tot_l--;
						escape_seq( "J" );
						escape_seq( "s" );
						for ( int i = cur_l+1; i < tot_l; i++ )
							puts( lptrbuf[ i ].line );
						escape_seq( "u" );
					}
// 					printf( "cur_l: %i, posx: %i, line: '%s'\n", cur_l, posx, lptrbuf[ cur_l ].line );
					break;
				case 0x0A:
				case 0x0D:
// 					shift all lines in file up, then clear the next line for a new one.
					for ( int i = tot_l-1; i > cur_l; i-- ) {
						lptrbuf[ i+1 ].line = lptrbuf[ i ].line;
						lptrbuf[ i+1 ].chramt = lptrbuf[ i ].chramt;
						lptrbuf[ i+1 ].size = lptrbuf[ i ].size;
					}
					cur_l++;
					tot_l++;
					lptrbuf[ cur_l ].line = 0;
					lptrbuf[ cur_l ].chramt = 0;
					lptrbuf[ cur_l ].size = 0;
// 					malloc the correct size, move the little bit after the cursor into the next line (malloc so new line can hold this), and delete that bit from the prev line
					int sz = lptrbuf[ cur_l-1 ].chramt - posx;
					MMALLOC( lptrbuf[ cur_l ].line, lptrbuf[ cur_l ].size, lg_p_1( sz ) );
					memcpy( lptrbuf[ cur_l ].line, lptrbuf[ cur_l-1 ].line + posx, sz );
					bzero( lptrbuf[ cur_l-1 ].line + posx, sz );
					lptrbuf[ cur_l-1 ].chramt -= sz;
					lptrbuf[ cur_l ].chramt = sz;
//					zero posx, clear text below us, and insert a new line
					posx = 0;
					escape_seq( "J" );
					putchar( 0x0A );
					escape_seq( "s" );
					for ( int i = cur_l; i < tot_l; i++ )
						puts( lptrbuf[ i ].line );
					escape_seq( "u" );
					break;
				case '\t':
					lptrbuf[ cur_l ].tabs++;
					ctabs++;
				default:
// 					if there are chars in front of us, we move them one to the right.
					for ( int i = lptrbuf[ cur_l ].chramt - 1; i >= posx; i-- ) {
						lptrbuf[ cur_l ].line[ i+1 ] = lptrbuf[ cur_l ].line[ i ];
					}
					lptrbuf[ cur_l ].line[ posx++ ] = (char)c;
					lptrbuf[ cur_l ].chramt++;
					scrx++;
// 					remove part after cursor
					escape_seq( "K" );
					putchar( c );
					escape_seq( "s" );
					printf( "%s", lptrbuf[ cur_l ].line + posx );
					escape_seq( "u" );
// 					printf( "cur_l: %i, posx: %i, line: '%s'\n", cur_l, posx, lptrbuf[ cur_l ].line );
			}
// 			posmax = posx * ( c != 0x1B );
		}

		puts( "" );
		if ( c == -1 && posx == -1 )
			ERR( "realloc failed, saving what you have." );
// 		save file
		for ( int i = 0; lptrbuf[ i ].line != 0; i++ ) {
			fwrite( lptrbuf[ i ].line, lptrbuf[ i ].chramt, 1, itmp );
			fputc( '\n', itmp );
		}
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

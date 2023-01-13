#include "main.h"

// pretty simple. note it is possible that lptrbuf is unallocated.
void free_lptrbuf () {
	if ( lptrbuf ) {
		for ( unsigned int i = 0; i < lptrbuf_s / sizeof( struct lin ); i++ ) {
			if ( lptrbuf[ i ].line )
				free( lptrbuf[ i ].line );
		}
		free( lptrbuf );
	}
}

// ~~sig handler~~ exit fn deletes the generated files
// ~~if you exit with SIGINT~~ if you exit with any method
// as well as closes the fd
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

void escape_seq ( char* seq ) {
	putchar( 0x1B );
	putchar( '[' );
	while ( *seq )
		putchar( *(seq++) );
}

// ofc x and y are under 999
void plop_cursor ( unsigned int y, unsigned int x ) {
	x++; y++;
	char cx[ 3 ] = {0};
	char cy[ 3 ] = {0};
	int i = 0;
	if ( x > 999 || y > 999 )
		ERR_QUIT( "bug; inputs to plop_cursor exceed max value" );

// 	printf( "x: %i, y: %i\n", x, y );

	do {
		cx[ 2-i ] = '0' + (x % 10);
		x /= 10;
		i++;
	} while ( x && i < 3 );
	int j = 0;
	do {
		cy[ 2-j ] = '0' + (y % 10);
		y /= 10;
		j++;
	} while ( y && j < 3 );

// 	PARR( cx, 3 );
// 	PARR( cy, 3 );

	char escseq[ 9 ] = {0};
	memcpy( escseq, cy+3-j, j );

// 	PARR( escseq, 8 );

	escseq[ j ] = ';';

// 	PARR( escseq, 8 );

	memcpy( escseq+j+1, cx+3-i, i );

// 	PARR( escseq, 8 );

	escseq[ j+i+1 ] = 'H';
	escseq[ j+i+2 ] = 0;

// 	PARR( escseq, 8 );

	escape_seq( escseq );
}

// position cursor just x dir
void off_cursor ( unsigned int x ) {
	char cx[ 3 ];
	char escseq[ 5 ];
	int i = 0;
	x++;
	if ( x > 999 )
		ERR_QUIT( "bug; inputs to off_cursor exceed max value" );

// 	extract digits from small -> large and put them in reverse order in cx.
	do {
		cx[ 2-i ] = '0' + (x % 10);
		x /= 10;
		i++;
	} while ( x && i < 3 );

// 	we do a bit of copying (i > 0, so if i=1, then cx+3-i=2, so we copy the first char, then 2nd if i=2, so on).
	memcpy( escseq, cx+3-i, i );
	escseq[ i ] = 'G';
	escseq[ i+1 ] = 0;
	escape_seq( escseq );
}

//
unsigned int get_cursor_pos () {
	unsigned int x = 0;
	unsigned int y = 0;
	int c = 0;

// 	6n is ANSI for 'transmit cursor pos in form ^[[Y..;X..R' where Y.. = row digits and X.. = column digits'
	escape_seq( "6n" );
	if ( getchar() != 0x1B || getchar() != '['  )
		return -1;

// 	convert column digits from char into num (char order is reverse of num order,
// 	so 1000 will come as 1,0,0,0, so for every new char we multiply old by 10)
	while ( (c = getchar()) != ';' ) {
		y *= 10;
		y += c - '0';
	}

// 	row digits
	while ( (c = getchar()) != 'R' ) {
		x *= 10;
		x += c - '0';
	}

//  return in form 0x0YYY0XXX
	return x + (y << 16);
}

// must be in non-canoical mode (i think)
unsigned int get_tab_width () {
	escape_seq( "2K" );
	escape_seq( "G" );
	escape_seq( "s" );
	unsigned int f = get_cursor_pos();
	printf( "\t" );
	unsigned int l = get_cursor_pos();
	escape_seq( "u" );
	return l - f;
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

// Total amount of space that is before us
// Needs: char_index, cur_lp, tab_width
void get_tab_space () {
	E.char_tbof = E.char_index;
	for ( int i = 0; i < E.char_index; i++ )
		E.char_tbof += ( E.cur_lp[ i ] == '\t' ) * ( E.tab_width - ( E.char_tbof % E.tab_width ) - 1 );
}

// Total amount of space that is after us
// Needs: char_index, char_amt, cur_lp, tab_width
void get_tab_space_aft () {
	E.char_tbof_a = E.char_amt - E.char_index;
	for ( int i = E.char_index; i < E.char_amt; i++ )
		E.char_tbof_a += ( E.cur_lp[ i ] == '\t' ) * ( E.tab_width - ( E.char_tbof_a % E.tab_width ) - 1 );
}

// Gets the current scr_x, accounting for tabs. Also sets scr_st
// Needs: pscr_w, char_tbof
void get_scr_x () {
	E.scr_x = E.char_tbof;
	E.scr_st = ( E.scr_x / E.pscr_w ) - 6;
	E.scr_x -= E.scr_st;
}

// clears and rerenders the last part of a line.
// Needs: cur_line, char_index, scr_w, scr_x, char_tbof_a
void render_l_line () {
	escape_seq( "K" );
	escape_seq( "s" );
	fwrite( E.cur_lp + E.char_index, E.scr_w - E.scr_x, 1, stdout );
	if ( E.char_tbof_a > E.scr_w - E.scr_x )
		putchar( '$' );
	escape_seq( "u" );
}

void line_prefix () {
// 	Currently nothing, but if lines have a prefix, it should be noted here, and this function should set E.scr_x_off
}

// clears and rerenders a full line from scratch
void render_line () {
	escape_seq( "2K" );
	line_prefix();
	if ( E.char_tbof > E.scr_x ) {
		putchar( '$' );
		escape_seq( "D" );
	}
	fwrite( E.cur_lp, E.scr_w - E.scr_x_off, 1, stdout );
	if ( E.char_tbof_a > E.scr_w - E.scr_x )
		putchar( '$' );
	off_cursor( E.scr_x );
}

// clears screen below cursor, moves cursor to beginning of next line, and prints
void render_down () {
	escape_seq( "J" );
	if ( E.char_tbof_a > E.scr_w - E.scr_x )
		putchar( '$' );
	int cur_line_sav = E.cur_line;
	for ( int i = E.scr_y; i < E.pscr_h; i++ ) {
		E.cur_line++;
	}
	E.cur_line = cur_line_sav;
}

void init_E ( FILE* tmplt, int tmplt_lines ) {


	if ( E.lptrbuf ) {
		ERR( "BUG: E is already initialized" );
		return;
	}
// 	we initially malloc the line structure
	int new_sz = lg_p_1( tmplt_lines * sizeof( struct lin ) );
	MMALLOC( E.lptrbuf, E.lptrbuf_s, new_sz );
	bzero( E.lptrbuf, E.lptrbuf_s );

// 	we then copy the lines into the structures
	for ( int i = 0; i < tot_l; i++ ) {
		int tmplnsz = strlen( tmplt[ i ] ) + 1;
		E.lptrbuf[ i ].chramt = tmplnsz - 1;
		int nsz = lg_p_1( tmplnsz );
		nsz = ( LINE_S > nsz ? LINE_S : nsz );

		RREALLOC( lptrbuf[ i ].line, lptrbuf[ i ].size, nsz );
		memcpy( lptrbuf[ i ].line, tmplt[ i ], tmplnsz );
		puts( lptrbuf[ i ].line );

		int j = 0;
		while ( lptrbuf[ i ].line[ j ] != 0 )
			lptrbuf[ i ].tabs += ( lptrbuf[ i ].line[ j ] == '\t' );

		posx = posx + ( i == cur_l ) * lptrbuf[ i ].chramt;
	}
}

// void line_up () {
// 	if ( E.cur_line > 0 ) {
// 		E.cur_line--;
// 		E.cur_lp = &(lptrbuf[ cur_line ].line);
// 		E.char_index = E.index_scr * ( E.index_scr >
// 		escape_seq( "F" );
// 	}
// }
//
// void line_down () {
// 	if ( E.cur_line < E.line_amt-1 ) {
// 		E.cur_line++;
// 		E.cur_lp = &(lptrbuf[ cur_line ].line);
// 		escape_seq( "E" );
// 	}
// }

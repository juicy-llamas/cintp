
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>
#include <sys/types.h>

#define CHAR_BASE 64
#define LINE_BASE 32

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
	sz = nsz;														\
	if ( !( ptr = realloc( ptr, sz ) ) )							\
		ERR_QUIT( "mem broke" );									\
})

#define MMALLOC( ptr, sz, nsz ) ({									\
	sz = nsz;														\
	if ( !( ptr = malloc( sz ) ) )									\
		ERR_QUIT( "mem broke" );									\
})

#define PARR( ptr, sz ) ({											\
	printf( "line %i: [ ", __LINE__ );								\
	for ( int i = 0; i < sz-1; i++ ) {								\
		printf( "%02X, ", *( (unsigned char*)ptr + i ) );			\
	}																\
	printf( "%02X ]\n", *( (unsigned char*)ptr + sz - 1 ) );		\
})

#define PDBG( fmt, ... ) ({											\
	escseq( "s" );													\
	escseq( "2;1H" );												\
	escseq( "K" );													\
	escseq( "1;1H" );												\
	escseq( "K" );													\
	printf( "LINE %i: " fmt, __LINE__, ##__VA_ARGS__ );				\
	escseq( "u" );													\
})

static char* T_color1 = "38;2;20;255;20;40m";
static char* T_color2 = "30;48;5;219m";
static char* T_color3 = "38;5;219;40m";

struct lin {
	char* line;
	int size;
	int chramt;
};

// attrs for F_lptrbuf
struct lin* F_lptrbuf = 0;		// holds structs that hold line ptrs and other info
int F_lptrbuf_s = 0;			// capacity of F_lptrbuf
int F_total_lines = 0;			// total amount of lines
int F_realloc_lim = 0;			// when do we need to realloc (should be 3/4 of F_lptrbuf_s)
int F_cur_line = 0;				// where we are in F_lptrbuf rn.
// cur line attrs
char* L_cur_lptr = 0;			// short for F_lptrbuf[ F_cur_line ].line
int L_chramt = 0;				// short for F_lptrbuf[ F_cur_line ].chramt
int L_cur_char = 0;				// index in F_lptrbuf[ F_cur_line ].line
int L_size = 0;					// short for F_lptrbuf[ F_cur_line ].size
int L_realloc_lim = 0;			// analogous to F_realloc_lim for local lines

// terminal related params
int T_tot_cols = 0;				// width of terminal
int T_tot_rows = 0;				// total height of terminal
int T_left_off = 0;				// reserved left space
int T_up_off = 0;				// reserved top space
int T_right_off = 0;			// reserved right
int T_down_off = 0;				// reserved bottom
int T_avail_cols = 0;			// useable width (width - reserved left - reserved right)
int T_avail_rows = 0;			// useable top (width - top - bottom)
int T_cur_col = 0;				// current x coord
int T_cur_row = 0;				// current y coord
int T_tab_space = 0;			// counts tab space behind the cursor at the given moment
int T_indOfBegCol = 0;			// L_cur_char = T_indOfBegCol + T_cur_col
int T_indOfBegRow = 0;			// F_cur_line = T_indOfBegRow + T_cur_row
int T_tab_width = 0;			// width of a tab

struct termios TERMINAL_DEFAULTS;

/*
 * GENERAL STUFF
 */

void free_F_lptrbuf () {
	if ( F_lptrbuf ) {
		for ( unsigned int i = 0; i < F_lptrbuf_s / sizeof( struct lin ); i++ ) {
			if ( F_lptrbuf[ i ].line )
				free( F_lptrbuf[ i ].line );
		}
		free( F_lptrbuf );
	}
}

void escseq( const char* seq );

void cleanup () {
	escseq( "H" );
	escseq( "J" );
	tcsetattr( STDOUT_FILENO, TCSANOW, &TERMINAL_DEFAULTS );
	free_F_lptrbuf();
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

/*
 * F_lptrbuf / MEM STUFF
 */

void set_cur_line();

void zero_F_lptrbuf () {
	for ( unsigned int i = 0; i < F_lptrbuf_s / sizeof( struct lin ); i++ ) {
		if ( F_lptrbuf[ i ].line )
			memset( F_lptrbuf[ i ].line, 0, F_lptrbuf[ i ].size );
		F_lptrbuf[ i ].chramt = 0;
	}
	F_total_lines = 1;
	set_cur_line( 0 );
}

void malloc_new_line ( int index ) {
// 	realloc if necessary
	if ( (F_total_lines+1) * sizeof( struct lin ) >= F_realloc_lim ) {
		RREALLOC( F_lptrbuf, F_lptrbuf_s, lg_p_1( F_lptrbuf_s ) );
		F_realloc_lim = (F_lptrbuf_s >> 2) + (F_lptrbuf_s >> 1);
	}
// 	shift lines
	for ( int i = F_total_lines-1; i >= index; i-- ) {
		memcpy( &(F_lptrbuf[ i+1 ]), &(F_lptrbuf[ i ]), sizeof( struct lin ) );
	}
	MMALLOC( F_lptrbuf[ index ].line, F_lptrbuf[ index ].size, CHAR_BASE );
	memset( F_lptrbuf[ index ].line, 0, CHAR_BASE );
	F_lptrbuf[ index ].chramt = 0;
	F_total_lines++;
}

void get_file ( FILE* file_p ) {
	int i = 0;
	size_t siz = 0;

	if ( !file_p )
		ERR_QUIT( "BUG: get file recieved null file_p argument" );

	zero_F_lptrbuf();

	while ( ( F_lptrbuf[ i ].chramt = getline( &(F_lptrbuf[ i ].line), &siz, file_p ) ) != -1 ) {
		F_lptrbuf[ i ].line[ --F_lptrbuf[ i ].chramt ] = 0;
		F_lptrbuf[ i ].size = siz;
		malloc_new_line( ++i );
		siz = F_lptrbuf[ i ].size;
	}
	RREALLOC( F_lptrbuf[ i ].line, F_lptrbuf[ i ].size, siz > CHAR_BASE ? siz : CHAR_BASE );
	F_lptrbuf[ i ].chramt = 0;
}

void set_cur_line ( int index ) {
	F_cur_line = index;
	L_cur_lptr = F_lptrbuf[ F_cur_line ].line;
	L_size = F_lptrbuf[ F_cur_line ].size;
	L_chramt = F_lptrbuf[ F_cur_line ].chramt;
	L_realloc_lim = (L_size << 1) + (L_size << 2);
}

void mem_init () {
	MMALLOC( F_lptrbuf, F_lptrbuf_s, LINE_BASE * sizeof( struct lin ) );
	F_realloc_lim = (F_lptrbuf_s >> 2) + (F_lptrbuf_s >> 1);

	malloc_new_line( 0 );
	set_cur_line( 0 );
}

void append_char_to_line ( char c ) {
	if ( L_size >= L_realloc_lim ) {
		L_realloc_lim = L_size + ( L_size >> 1 );
		RREALLOC( L_cur_lptr, L_size, L_size << 1 );
	}
	L_cur_lptr[ L_cur_char++ ] = c;
}

/*
 * TERMINAL HELPER FNS
 */

// seq is a null terminated string
void escseq ( const char* seq ) {
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

	char esc_str[ 9 ] = {0};
	memcpy( esc_str, cy+3-j, j );
	esc_str[ j ] = ';';
	memcpy( esc_str+j+1, cx+3-i, i );
	esc_str[ j+i+1 ] = 'H';
	esc_str[ j+i+2 ] = 0;
	escseq( esc_str );
}

// position cursor just x dir
void off_cursor ( unsigned int x ) {
	char cx[ 3 ];
	char esc_str[ 5 ];
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
	memcpy( esc_str, cx+3-i, i );
	esc_str[ i ] = 'G';
	esc_str[ i+1 ] = 0;
	escseq( esc_str );
}

unsigned int get_cursor_pos () {
	unsigned int x = 0;
	unsigned int y = 0;
	int c = 0;

// 	6n is ANSI for 'transmit cursor pos in form ^[[Y..;X..R' where Y.. = row digits and X.. = column digits'
	escseq( "6n" );
	if ( getchar() != 0x1B || getchar() != '['  )
		return -1;

// 	convert column digits from char into num (char order is reverse of num order,
// 	so 1000 will come as 1,0,0,0, so for every new char we multiply old by 10)
	while ( (c = getchar()) != ';' ) {
		y *= 10;
		y += c - '0';
	}
	while ( (c = getchar()) != 'R' ) {		// 	row digits
		x *= 10;
		x += c - '0';
	}

	return x + (y << 16);					//  return in form 0x0YYY0XXX
}

/*
 * TERMINAL MAIN FNS
 */

// must be in non-canoical mode (i think)
unsigned int get_T_tab_width () {
	escseq( "2K" );
	escseq( "G" );
	escseq( "s" );
	unsigned int f = get_cursor_pos();
	printf( "\t" );
	unsigned int l = get_cursor_pos();
	escseq( "u" );
	return l - f;
}

int term_size () {
	struct winsize dimn;
	int ret = 0;
	int t1;
	int t2;
	if ( ioctl( STDOUT_FILENO, TIOCGWINSZ, &dimn ) == -1 ) {
// 		ioctl didn't work, so we do it the janky way
		escseq( "999;999H" );						// plop cursor beyond win limit
		unsigned int coords = get_cursor_pos();		// cursor must be at edge of win
		t1 = coords & 0xFFFF;
		t2 = coords >> 16;
	} else {
// 		ioctl worked, we get the attrs from the struct
		t1 = dimn.ws_col;
		t2 = dimn.ws_row;
	}
	if ( t1 != T_tot_cols || t2 != T_tot_rows )
		ret = 1;
	T_tot_cols = t1;
	T_tot_rows = t2;
	T_avail_cols = T_tot_cols - T_left_off - T_right_off;
	T_avail_rows = T_tot_rows - T_up_off - T_down_off;

	if ( T_tot_rows < 2 || T_tot_cols < 2 )
		ERR_QUIT( "terminal too small" );

	return ret;
}

void conF_numbers () {
// 	calculate space needed
	int max = T_indOfBegRow + T_avail_rows + 1;
	T_left_off = 2;
	while ( max ) {
		T_left_off++;
		max /= 10;
	}
	T_avail_cols = T_tot_cols - T_left_off - T_right_off;
}

void draw_line_number ( int line_no ) {
	char buf[ T_left_off ];
	buf[ 0 ] = ' ';
	int i = T_left_off - 2;
	while ( line_no && i > 0 ) {
		buf[ i ] = '0' + (line_no % 10);
		line_no /= 10;
		i--;
	}
	while ( i > 0 ) {
		buf[ i-- ] = ' ';
	}
	buf[ T_left_off - 1 ] = 0;

	escseq( T_color2 );
	printf( "%s", buf );
	escseq( T_color1 );
	putchar( ' ' );
}

// redraw the given line
void term_reline ( int line_to_draw, int this_col_off ) {
	plop_cursor( line_to_draw - T_indOfBegRow + T_up_off, 0 );
	escseq( "2K" );				// clear the line

	draw_line_number( line_to_draw+1 ); // print stuff before line

// 	printf( "len: %i, c: %i\n", len, F_lptrbuf[ line_to_draw ].chramt );
	int ind = this_col_off;
	int i = 0;
	for ( ; ( i < T_avail_cols ) && ( ind < F_lptrbuf[ line_to_draw ].chramt ); i++ ) {
		if ( F_lptrbuf[ line_to_draw ].line[ ind ] == '\t' ) {
			int k = T_tab_width - ( i % T_tab_width );
			T_tab_space += ( k - 1 ) * ( F_cur_line == line_to_draw && ind < L_cur_char );
			while ( k-- && i <= 1+T_avail_cols ) {
				putchar( ' ' );
				i++;
			}
			i--;
		} else
			putchar( F_lptrbuf[ line_to_draw ].line[ ind ] );
		ind++;
	}
	if ( ind < F_lptrbuf[ line_to_draw ].chramt ) {
		escseq( "D" );
		escseq( "C" );
		escseq( T_color3 );
		putchar( '$' );
		escseq( T_color1 );
	}
}

void term_refresh () {
// 	move to top and clear
	escseq( "3;1H" );
	escseq( "J" );
	T_tab_space = 0;
// 	print lines
	for ( int i = T_indOfBegRow; i < T_indOfBegRow + T_avail_rows; i++ ) {
		term_reline( i, 0 );
	}
// 	print stuff after
// 	move cursor
	T_cur_row = F_cur_line - T_indOfBegRow + T_up_off;
	T_cur_col = L_cur_char + T_tab_space - T_indOfBegCol + T_left_off;
	plop_cursor( T_cur_row, T_cur_col );
}

void term_init () {
// 	unset canoical mode so we can get that juicy character by character play (and esc sequences don't print)
	struct termios attr;
	tcgetattr( STDOUT_FILENO, &TERMINAL_DEFAULTS );
	memcpy( &attr, &TERMINAL_DEFAULTS, sizeof( struct termios ) );
	attr.c_lflag &= ~( ICANON | ECHO | ISIG );
	attr.c_iflag &= ~( IXON | ICRNL | BRKINT );
	tcsetattr( STDOUT_FILENO, TCSANOW, &attr );

	T_up_off = 2;

	T_tab_width = get_T_tab_width();
	escseq( "H" );		// move to top left
	escseq( "J" );		// clear screen
	escseq( T_color1 );
}

/*
 * EVENT HANDLING
 */

int arrow_up () {
	int limcomp = F_cur_line > 0;
	F_cur_line -= limcomp;
	if ( F_cur_line < T_indOfBegRow ) {
		PDBG( "arrow up (past)\nF_cur_line: %i", F_cur_line );
// 		return 1;
	} else {
		PDBG( "arrow up\nF_cur_line: %i", F_cur_line );
// 		escseq( "A" );
	}
	set_cur_line( F_cur_line );
	PDBG( "arrow up\nF_cur_line: %i", F_cur_line );
	return 0;
}

int arrow_down () {
	int limcomp = F_cur_line < F_total_lines-1;
	F_cur_line += limcomp;
	if ( F_cur_line >= T_indOfBegRow + T_avail_rows ) {
		PDBG( "arrow down (past)\nF_cur_line: %i", F_cur_line );
	} else {
		PDBG( "arrow down\nF_cur_line: %i", F_cur_line );
// 		escseq( "B" );
	}
	set_cur_line( F_cur_line );
	return 0;
}

int arrow_right () {
	int limcomp = L_cur_char < L_chramt;
	L_cur_char += limcomp;
	if ( T_cur_col >= T_avail_cols ) {
		PDBG( "arrow right (past)" );
	} else {
		if ( L_cur_lptr[ L_cur_char-1 ] == '\t' && limcomp ) {
// 			off_cursor( T_cur_col );
		} else if ( limcomp ) {
// 			escseq( "C" );
		}
		PDBG( "arrow right \nT_cur_col: %i %s", T_cur_col, L_cur_lptr[ L_cur_char-1 ] == '\t' ? "(TAB)" : "" );
	}
	return 0;
}

int arrow_left () {
	int limcomp = L_cur_char > 0;
	L_cur_char -= limcomp;
	if ( L_cur_char < T_indOfBegCol ) {
		PDBG( "arrow left (past)" );
	} else {
		if ( L_cur_lptr[ L_cur_char+1 ] == '\t' && limcomp ) {
// 			off_cursor( T_cur_col );
		} else if ( limcomp ) {
// 			escseq( "D" );
		}
		PDBG( "arrow left\nT_cur_col: %i %s", T_cur_col, L_cur_lptr[ L_cur_char+1 ] == '\t' ? "(TAB)" : "" );
	}
	return 0;
}

int main () {
	if ( atexit( cleanup ) != 0 )
		ERR_QUIT( "atexit failed" );

	char c = 0;
	term_init();
	mem_init();

	FILE* fil = fopen( "main.c", "r+" );
	get_file( fil );

	do {
		switch ( c ) {
			case 0x1B:
				if ( getchar() == '[' ) {
					c = getchar();
					switch ( c ) {
						case 'A':
							arrow_up();
							break;
						case 'B':
							arrow_down();
							break;
						case 'C':
							arrow_right();
							break;
						case 'D':
							arrow_left();
							break;
					}
				}
				break;
			case '\n':
				break;
			case '\t':
				break;
			case 0x07:
				break;
			default:

			case 0:
				break;
		}
		term_size();
		conF_numbers();
		term_refresh();
	// 	printf( "tab width: %i, T_tot_cols: %i, T_tot_rows: %i, T_avail_cols: %i, T_avail_rows: %i\n", T_tab_width, T_tot_cols, T_tot_rows, T_avail_cols, T_avail_rows );
	} while ( ( c = getchar() ) != 0x11 );
}


#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <termios.h>
#include <unistd.h>

#define DEFAULT_SIZE 128

struct line {
	int bufsize;
	int amt;
	char* buf;
};

struct file {
	int bufsize;
	int amt;
	struct line* buf;
	FILE* fp;
	char* file_name;
	int cur_ch;
	int cur_ln;
};

// prog const
struct termios TERMINAL_DEFAULTS;

/*
 * HELPER FNS
 */

unsigned int lg_p_1 ( unsigned int k ) {
	k -= 1;
	int i = 8;
	int j = 0x00008000 * ( k <= 0xFFFF ) + 0x00010000 * ( k > 0xFFFF );
	while ( i ) {
		j >>= ( k < j ) * i;
		j <<= ( k >= j ) * i;
		i >>= 1;
	}
	return ( j + 0x80000000 * ( k >> 31 ) ) << 1;
}

#define SCREAM_AND_PANIK( fmt, ... ) ({								\
	int k = system( "clear" );										\
	fprintf( stderr,												\
		"O NO (line %i, file '%s'): \n\t" fmt "\n\tsterror: %s\n"	\
		"\tk: %i\n", 												\
		__LINE__, __FILE__, ##__VA_ARGS__, strerror( errno ), k );	\
	abort();														\
})

// polymorphism XD
#define RALLOC_CHK( st, tp ) ({						\
	if ( st->amt * sizeof( tp ) >= 					\
			( st->bufsize >> 2 ) + 					\
			( st->bufsize >> 1 ) ) {				\
		st->bufsize = lg_p_1( st->bufsize );		\
		st->buf = realloc( st->buf, st->bufsize );	\
		if ( st->buf == 0 )							\
			SCREAM_AND_PANIK( "realloc "			\
				"(0x%lX / %i)", (long unsigned int) st->buf,			\
				st->bufsize );						\
	} 												\
})

#define PARR( ptr, sz ) ({											\
	printf( "line %i: [ ", __LINE__ );								\
	for ( int i = 0; i < sz-1; i++ ) {								\
		printf( "%02X, ", *( (unsigned char*)ptr + i ) );			\
	}																\
	printf( "%02X ]\n", *( (unsigned char*)ptr + sz - 1 ) );		\
})

/*
 * FILE STRUCT
 * MEMORY OPERATORS
 */

void free_line ( struct line* lin ) {
	if ( lin == 0 || lin->buf == 0 )
		SCREAM_AND_PANIK( "bug; lin or lin->buf is 0" );
	free( lin->buf );
	lin->buf = 0;
}
#define DEALLOC_LINE( p ) ({ free_line( p ); free( p ); })

void free_file ( struct file* fil ) {
	if ( fil == 0 || fil->buf == 0 )
		SCREAM_AND_PANIK( "bug; fil or fil->buf is 0" );
	fclose( fil->fp );
	fil->fp = 0;
	free( fil->file_name );
	for ( int i = 0; i < fil->bufsize / sizeof( struct line ); i++ ) {
		if ( fil->buf[ i ].buf )
			free_line( &(fil->buf[ i ]) );
	}
	free( fil->buf );
	fil->buf = 0;
}
#define DEALLOC_FILE( p ) ({ free_file( p ); free( p ); })

// assumes fil is allocated
int read_file ( struct file* fil ) {
	int bytes = 0;
// 	if the file is not open, open it (note: to reuse the struct, you need to close the prev file yourself)
	if ( !fil->fp ) {
		fil->fp = fopen( fil->file_name, "r+" );
		if ( !fil->fp )
			return 1;
	}
	fil->amt = 0;
// 	get the file line by line
	do {
		bytes = getline( &(fil->buf[ fil->amt ].buf),
						 (size_t*)&(fil->buf[ fil->amt ].bufsize),
						 fil->fp );
		RALLOC_CHK( fil, struct line );
		fil->buf[ fil->amt ].amt = ( bytes - 1 ) * ( bytes > 0 );
		fil->buf[ fil->amt ].buf[ bytes - 1 ] = 0;
		fil->amt++;
	} while ( bytes != -1 );
// 	the loop overcounts the amt, so decrease by one, and set the cur pos at the end of the file.
	fil->amt--;
	fil->cur_ln = fil->amt - 1;
	fil->cur_ch = fil->buf[ fil->cur_ln ].amt - 1;
	return 0;
}

// creates a fil struct and fills it with the file in file_name
struct file* alloc_file ( const char* file_name ) {
	if ( file_name == 0 ) return 0;
// 	allocate the file struct
	struct file* fil = malloc( sizeof( struct file ) );
	fil->buf = malloc( DEFAULT_SIZE );
	fil->fp = 0;
	fil->bufsize = DEFAULT_SIZE;
	fil->cur_ln = 0;
	fil->cur_ch = 0;
// 	copy the file name
	int s = strlen( file_name );
	fil->file_name = malloc( s + 1 );
	fil->file_name[ s ] = 0;
	if ( !fil->file_name )
		SCREAM_AND_PANIK( "alloc_file: malloc didn't work" );
	strcpy( fil->file_name, file_name );
// 	read the file
	if ( !read_file( fil ) ) {
		return fil;
	} else {
		free( fil );
		return 0;
	}
}

// dumps the file into debug_out
void dbg_dump_st_contents ( const struct file* fllf ) {
	FILE* dbgfd = fopen( "debug_out", "w+" );
	for ( struct line* i = fllf->buf; i < (fllf->buf + fllf->amt); i++ )
		fprintf( dbgfd, "%s\n", i->buf );
	fclose( dbgfd );
}

// writes the file. 2 == no open file, 1 == writing error, and 0 means success.
int file_write ( struct file* fil ) {
	if ( !fil || !fil->fp )
		return 2;
	for ( struct line* i = fil->buf; i < (fil->buf + fil->amt); i++ ) {
		if ( fprintf( fil->fp, "%s\n", i->buf ) < 0 )
			return 1;
	}
	return 0;
}

/*
 * LINE OPERATIONS
 */

// inserts a new line after the previous one, does no copying
void f_insert_line ( struct file* fil, int siz ) {
	fil->amt++;
	RALLOC_CHK( fil, struct line );
// 	shift the prev lines up the array
	fil->cur_ln++;
	if ( fil->cur_ln < 0 || fil->cur_ln >= fil->amt )
		SCREAM_AND_PANIK( "bug; in insert_line, cur_ln out of bounds" );
	for ( int i = fil->amt; i > fil->cur_ln; i-- )
		fil->buf[ i ] = fil->buf[ i-1 ];
// 	allocate a new line with the given size (or DEFAULT_SIZE) and null-terminate it
	struct line* cl = &(fil->buf[ fil->cur_ln ]);
	fil->cur_ch = 0;
	siz = DEFAULT_SIZE * !siz + siz;
	cl->buf = malloc( siz ); if ( !cl->buf ) SCREAM_AND_PANIK( "malloc" );
	cl->bufsize = siz;
	cl->amt = 0;
	cl->buf[ 0 ] = 0;
}

// inserts a char in the line by pushing all chars after to the right
void f_insert_char ( struct file* fil, int ch ) {
	struct line* cl = fil->buf + fil->cur_ln;

	if ( fil->cur_ln < 0 || fil->cur_ln >= fil->amt )
		SCREAM_AND_PANIK( "bug; in insert_char, cur_ln out of bounds" );

	cl->amt++;
	RALLOC_CHK( cl, char );

	for ( int i = cl->amt; i > fil->cur_ch; i-- )
		cl->buf[ i ] = cl->buf[ i-1 ];
	cl->buf[ fil->cur_ch ] = (char)ch;
// 	not strictly necessary but makes the type functions easier.
	fil->cur_ch++;
}

// deletes the current line (frees it) and shifts subsequent lines up.
void f_delete_line ( struct file* fil ) {
	struct line* cl = fil->buf + fil->cur_ln;
	if ( fil->amt > 0 ) {
// 		we free the current line, shift the others back, and move our pos to the line that comes in.
		free_line( cl );
		for ( int i = fil->cur_ln; i <= fil->amt; i++ )
			fil->buf[ i ] = fil->buf[ i+1 ];
		fil->amt--;
		fil->cur_ch = cl->amt - 1;
		if ( fil->buf[ fil->cur_ln ].buf == 0 || fil->buf[ fil->cur_ln - 1 ].buf == 0 || fil->buf[ fil->cur_ln + 1 ].buf == 0 )
			SCREAM_AND_PANIK( "(delete line) buffers: 0x%016lX, 0x%016lX, 0x%016lX", (long unsigned int)fil->buf[ fil->cur_ln-1 ].buf, (long unsigned int)fil->buf[ fil->cur_ln ].buf, (long unsigned int)fil->buf[ fil->cur_ln+1 ].buf );
	} else {
// 		if we have no lines left, we just clear the line buffer
		memset( cl->buf, 0, cl->bufsize );
		cl->amt = 0;
		fil->cur_ch = 0;
	}

}

char* value_to_write_in;
char** string_to_set;

// helper fn, does what it says it does (moves cur line into previous one)
void f_mv_this_line_into_prev ( struct file* fil ) {
	struct line* cl = fil->buf + fil->cur_ln;
	struct line* pl = cl - 1;
	if ( pl >= 0 ) {
// 		below line is not necessary but useful for bksp/del fns
		int n_amt = cl->amt + pl->amt;
		int n_siz = lg_p_1( cl->amt + pl->amt + 1 );
// 		copy the current line into the line before
		pl->buf = realloc( pl->buf, n_siz ); if ( !pl->buf && n_siz != 0 ) SCREAM_AND_PANIK( "realloc" );
		memcpy( pl->buf + pl->amt, cl->buf, cl->amt );
		pl->bufsize = n_siz;
// 		delete the current line
		f_delete_line( fil );
		pl->amt = n_amt;
		fil->cur_ln--;
	}
}

// 'deletes' the char at cur_ch by moving others in its place. used for bksp and del.
void f_delete_char ( struct file* fil ) {
	struct line* cl = fil->buf + fil->cur_ln;
	*value_to_write_in = (char)fil->cur_ch;
	if ( fil->cur_ch < 0 ) {
// 		this is akin to backspacing the leftmost char on a line.
		if ( fil->cur_ln > 0 ) {
			f_mv_this_line_into_prev( fil );
		} else
			fil->cur_ch = 0;
	} else if ( fil->cur_ch >= cl->amt ) {
// 		this is akin to deleting the rightmost char
		if ( fil->cur_ln < fil->amt ) {
			fil->cur_ln++;
			*value_to_write_in = fil->cur_ch;
			*string_to_set = "deleting";
			f_mv_this_line_into_prev( fil );
		} else
			fil->cur_ch = cl->amt;
	} else {
// 	 	normal operation: shift chars from right left, starting from leftmost
		for ( int i = fil->cur_ch; i <= cl->amt; i++ )
			cl->buf[ i ] = cl->buf[ i+1 ];
		cl->amt--;
	}
}

// akin to pressing enter: moves chars after the cursor (cur_ch) to the next line
void f_enter ( struct file* fil ) {
	int p_ch = fil->cur_ch;
	int n_amt = fil->buf[ fil->cur_ln ].amt - p_ch;
	int n_size = lg_p_1( n_amt + 1 );

	if ( fil->buf[ fil->cur_ln ].buf == 0 || fil->buf[ fil->cur_ln - 1 ].buf == 0 || fil->buf[ fil->cur_ln + 1 ].buf == 0 )
		SCREAM_AND_PANIK( "(enter) buffers: 0x%016lX, 0x%016lX, 0x%016lX", (long unsigned int)fil->buf[ fil->cur_ln-1 ].buf, (long unsigned int)fil->buf[ fil->cur_ln ].buf, (long unsigned int)fil->buf[ fil->cur_ln+1 ].buf );

	f_insert_line( fil, n_size );

// 	copy all of the chars from the previous line (after the cursor) into this next line
	memcpy( fil->buf[ fil->cur_ln ].buf, fil->buf[ fil->cur_ln - 1 ].buf + p_ch, n_amt );
	fil->buf[ fil->cur_ln ].buf[ n_amt ] = 0;
	fil->buf[ fil->cur_ln - 1 ].buf[ p_ch ] = 0;
	fil->buf[ fil->cur_ln ].amt = n_amt;
	fil->buf[ fil->cur_ln - 1 ].amt = p_ch;

// 	printf( "bufs:\n\t\"%s\"\n\t\"%s\"\n\tamt1 = %i, amt2 = %i, siz1 = %i, siz2 = %i\n", fil->buf[ fil->cur_ln - 1 ].buf, fil->buf[ fil->cur_ln ].buf, fil->buf[ fil->cur_ln - 1 ].amt, fil->buf[ fil->cur_ln ].amt, fil->buf[ fil->cur_ln - 1 ].bufsize, fil->buf[ fil->cur_ln ].bufsize );
}

/*
 * GLOBAL TERMINAL FUNCTIONS
 */

// most term fns use this so I have to put a stub here.
void escseq ( const char* seq );
// also this one
unsigned int get_cursor_pos ();

int global_tab_width = 0;

unsigned int get_global_tab_width () {
	escseq( "2K" );
	escseq( "G" );
	escseq( "s" );
	unsigned int f = get_cursor_pos();
	printf( "\t" );
	unsigned int l = get_cursor_pos();
	escseq( "u" );
	return l - f;
}

void global_term_init () {
	struct termios attr;
	tcgetattr( STDOUT_FILENO, &TERMINAL_DEFAULTS );
	memcpy( &attr, &TERMINAL_DEFAULTS, sizeof( struct termios ) );
	attr.c_lflag &= ~( ICANON | ECHO | ISIG );
	attr.c_iflag &= ~( IXON | ICRNL | BRKINT );
	tcsetattr( STDOUT_FILENO, TCSANOW, &attr );

	global_tab_width = get_global_tab_width();
	global_tab_width = global_tab_width + (!global_tab_width) * 4;
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
		SCREAM_AND_PANIK( "bug; inputs to plop_cursor exceed max value" );

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
		SCREAM_AND_PANIK( "bug; inputs to off_cursor exceed max value" );

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

unsigned long int get_term_size () {
	struct winsize dimn;
	unsigned int r, c;
	if ( ioctl( STDOUT_FILENO, TIOCGWINSZ, &dimn ) == -1 ) {
// 		ioctl didn't work, so we do it the janky way
		escseq( "999;999H" );						// plop cursor beyond win limit
		unsigned int coords = get_cursor_pos();		// cursor must be at edge of win
		r = coords & 0xFFFF;
		c = coords >> 16;
	} else {
// 		ioctl worked, we get the attrs from the struct
		r = dimn.ws_col;
		c = dimn.ws_row;
	}
	return ( ((unsigned long int)c) << 32 ) + r;
}

/*
 * TERMINAL STRUCT ALLOC/DEALLOC
 */

struct term {
// 	our current position
	int cur_row;
	int cur_col;
// 	hardly used--total terminal space including margins
	int tot_row;
	int tot_col;
// 	the size of the terminal in practice, accounting for margin text I might add.
	int use_row;
	int use_col;
// 	tab width for this terminal
	int tab_width;
// 	when we page down or to the right, we are at an offset from the file buffer. This stores that offset.
	int char_off;
	int lin_off;
// 	how many terminals worth of columns / rows we have passed.
	int tot_col_nm;
	int tot_row_nm;
// 	cursor limits for paging.
	int cursor_lm_r;
	int cursor_lm_l;
// 	like cursor limits for line paging. How many lines of context you get when you go down.
	int ln_view;
// 	this saves the column when you switch lines.
	int col_sav;
// 	this is for debugging purposes
	char spc;
	char* mess;
};

struct term* alloc_term () {
	struct term* tm = malloc( sizeof( struct term ) );

	unsigned long int siz = get_term_size();
	tm->tot_row = siz >> 32;
	tm->tot_col = siz & 0xFFFFFFFF;
	tm->cur_row = 0;
	tm->cur_col = 0;
	tm->use_row = tm->tot_row - 1;
	tm->use_col = tm->tot_col;
	tm->tab_width = global_tab_width;
	tm->char_off = 0;
	tm->lin_off = 0;
	tm->tot_col_nm = 0;
	tm->tot_row_nm = 0;
	tm->cursor_lm_r = 3;
// 	not implemented
	tm->cursor_lm_l = 0;
// 	line view scales with avail rows.
	tm->ln_view = ( tm->use_row - 4 ) >> 2;
	tm->ln_view *= tm->ln_view > 0;
	tm->col_sav = 0;

	return tm;
}

/*
 * TERM FILE PAGING FNS
 */

// This function takes approximate cur_col and tot_col_nm vals and configures new ones that rounds
// up (round_mode == 0) or down (round_mode != 0) to the nearest char.
void recompute_offsets_from_curcol ( struct term* tm, struct file* fl, int round_mode ) {
	int chr_ind = 0;
	int new_col = 0;
	int nm_passes = 0;
	int last_off = tm->char_off;
	int last_chroff = 0;
	int last_coloff = 0;

// 	initially setting this to 0
	tm->char_off = 0;

	if ( fl->cur_ln < 0 || fl->cur_ln >= fl->amt )
		SCREAM_AND_PANIK( "bug; cur_ln out of bounds" );

// 	It works by counting all of the chars up to the cursor. Not efficient, but it works, which is more than I can say of any other fn I've written.
	while ( ( new_col < tm->cur_col || nm_passes < tm->tot_col_nm ) && chr_ind < fl->buf[ fl->cur_ln ].amt ) {
// 		While we haven't reached our end pos, keep adding chars one by one, accounting for tab chars.
		int tab_cond = fl->buf[ fl->cur_ln ].buf[ chr_ind ] == '\t';
// 		Keep track of the offset from the prev char to the cur one.
		last_off = tab_cond ? tm->tab_width - ( new_col % tm->tab_width ) : 1;
		new_col += last_off;
		chr_ind++;

// 		If we run out of margin, we then page right
		if ( new_col >= tm->use_col - tm->cursor_lm_r ) {
// 			increase our number of pages
			nm_passes++;
// 			to go backwards, we keep track of the offset before the one we're at
			last_coloff = new_col - last_off;
// 			we shift our column down
			new_col -= tm->use_col - tm->cursor_lm_r;
// 			we recount the offset of our last char
			last_off = tab_cond ? tm->tab_width - ( new_col % tm->tab_width ) : 1;
			new_col += last_off;
// 			we store our last char off (for going back) and update our current one.
			last_chroff = tm->char_off * ( nm_passes > 1 );
			tm->char_off = chr_ind - 1;
// 			when we hit the right arrow key and increase cur_col beyond the lim, we need to account for the fact
			if ( nm_passes > tm->tot_col_nm )
				tm->cur_col -= tm->use_col - tm->cursor_lm_r;
		}
	}

// 	this is exclusively for rounding down: we basically undo our last char addition
	if ( round_mode ) {
		new_col -= last_off;
		chr_ind--;
		new_col = new_col > 0 ? new_col : 0;
		chr_ind = chr_ind > 0 ? chr_ind : 0;
// 		if we get over the limit, and we aren't at the beginning of the line, we set everything to the way it was before and decrement our pages
		if ( ( new_col < 1 ) && nm_passes ) {
			new_col = last_coloff;
			nm_passes--;
			tm->char_off = last_chroff;
		}
	}

// 	we update the current values
	tm->cur_col = new_col;
	tm->tot_col_nm = nm_passes;
	fl->cur_ch = chr_ind;
}

// The idea for these fns are that they are an efficient alternative to the above fn, only using relative offsets instead of looping through all of the characters.
void move_col_backwards ( struct term* tm, struct file* fl ) {
}

void move_col_forwards ( struct term* tm, struct file* fl ) {
}

void page_up ( struct term* tm, struct file* fil ) {
	tm->lin_off -= tm->use_row;
	tm->lin_off *= tm->lin_off > 0;
	fil->cur_ln -= tm->use_row;
	fil->cur_ln *= fil->cur_ln > 0;
	tm->cur_row = fil->cur_ln - tm->lin_off;
	recompute_offsets_from_curcol( tm, fil, 0 );
}

void page_down ( struct term* tm, struct file* fil ) {
	tm->lin_off += tm->use_row;
	int linofflim = fil->amt - tm->use_row;
	tm->lin_off = tm->lin_off <= linofflim ? tm->lin_off : linofflim;
	fil->cur_ln += tm->use_row;
	fil->cur_ln = fil->cur_ln < fil->amt ? fil->cur_ln : fil->amt - 1;
	tm->cur_row = fil->cur_ln - tm->lin_off;
	recompute_offsets_from_curcol( tm, fil, 0 );
}

void check_ln_up ( struct term* tm, struct file* fil ) {
	if ( tm->cur_row < tm->ln_view && tm->lin_off > 0 ) {
		tm->lin_off -= tm->use_row - 2 * tm->ln_view;
		int backup = -tm->lin_off;
		backup *= backup > 0;
		tm->lin_off += backup;
		tm->cur_row = tm->use_row - tm->ln_view - 1 - backup;
	}
	tm->cur_row += tm->cur_row < 0;
}

//
void line_up ( struct term* tm, struct file* fil ) {
// 	decrement vals
	tm->cur_row--;
	fil->cur_ln -= fil->cur_ln > 0;
// 	line pos saving mechanic
	if ( tm->col_sav )
		tm->cur_col = tm->col_sav;
	else
		tm->col_sav = tm->cur_col;
// 	if it goes north of terminal, then page up (but don't use the pgup fn)
	check_ln_up( tm, fil );
// 	handle columns
	recompute_offsets_from_curcol( tm, fil, 0 );
}

void check_ln_down ( struct term* tm, struct file* fil ) {
	if ( tm->cur_row >= tm->use_row - tm->ln_view && tm->lin_off < fil->amt - tm->use_row ) {
		tm->lin_off += tm->use_row - 2 * tm->ln_view;
		int backup = tm->use_row - ( fil->amt - tm->lin_off );
		backup *= backup > 0;
		tm->lin_off -= backup;
		tm->cur_row = tm->ln_view + backup;
	}
	tm->cur_row -= tm->cur_row >= tm->use_row;
}

// same premise as line up but opposite
void line_down ( struct term* tm, struct file* fil ) {
	tm->cur_row++;
	fil->cur_ln += fil->cur_ln < fil->amt-1;
	if ( tm->col_sav )
		tm->cur_col = tm->col_sav;
	else
		tm->col_sav = tm->cur_col;

	check_ln_down( tm, fil );

	recompute_offsets_from_curcol( tm, fil, 0 );
}

/*
 * TERM RENDER FNS
 */

void render_full_term ( struct term* tm, struct file* fil ) {
// 	a temp buf
	char buf[ tm->use_col + 1 ];
	buf[ tm->use_col ] = 0;
// 	clear the term / move to the top.
	escseq( "2J" );
	escseq( "H" );

// 	printf( "use_row: %i\nuse_col: %i\nchar_off: %i\nlin_off: %i\n", tm->use_row, tm->use_col, tm->char_off, tm->lin_off );
// 	exit( 0 );
	int r_lim = tm->use_row + ( tm->use_row >= fil->amt ) * ( fil->amt - tm->use_row );
	for ( int r = 0; r < r_lim; r++ ) {
		if ( r + tm->lin_off >= fil->amt || tm->lin_off < 0 )
			SCREAM_AND_PANIK( "bug; while rendering term, encountered line that does not exist (tm->lin_off is bad)" );
// 		printf( "r = %i", r );
		int chr = ( r == fil->cur_ln ) * tm->char_off, col = 0;
// 		printf( ", chr = %i, amt = %i\n", chr, fil->buf[ r + tm->lin_off ].amt );
		while ( col < tm->use_col && chr < fil->buf[ r + tm->lin_off ].amt ) {
			if ( chr < 0 || chr >= fil->buf[ r + tm->lin_off ].amt )
				SCREAM_AND_PANIK( "bug; chr overflowed." );
			char cc = fil->buf[ r + tm->lin_off ].buf[ chr ];
			if ( cc == '\t' ) {
				int lim = col + tm->tab_width - (col % tm->tab_width);
				for ( ; col < lim && col < tm->use_col; col++ )
					buf[ col ] = ' ';
			} else
				buf[ col++ ] = cc;
			chr++;
		}
		printf( "%s", buf );
		if ( r != r_lim - 1 )
			printf( "\n" );
		memset( buf, 0, tm->use_col );
	}

// 	dunno if this is necessary
// 	plop_cursor( tm->tot_row, 0 );
	printf( "\nrow: %i, col: %i, frow: %i, fcol: %i, char_off: %i, char amt: %i, horz pages: %i, line_off: %i, line amt: %i, r_lim: %i, spc: %u, '%s'",
	tm->cur_row, tm->cur_col, fil->cur_ln, fil->cur_ch, tm->char_off, fil->buf[ fil->cur_ln ].amt, tm->tot_col_nm, tm->lin_off, fil->amt, r_lim, tm->spc, tm->mess );
	plop_cursor( tm->cur_row, tm->cur_col );
}

void render_term_line ( struct term* tm, struct file* fil ) {

}

/*
 * MAIN LOOP
 */

struct file** file_list;
int fl_size;
struct term** term_list;
int tl_size;

void cleanup () {
	escseq( "H" );
	escseq( "J" );
	tcsetattr( STDOUT_FILENO, TCSANOW, &TERMINAL_DEFAULTS );
	for ( int i = 0; i < fl_size; i++ )
		if ( file_list[ i ] ) DEALLOC_FILE( file_list[ i ] );
	for ( int i = 0; i < tl_size; i++ )
		if ( term_list[ i ] ) free( term_list[ i ] );
	free( file_list );
	free( term_list );
}

int main ( int argc, char* argv[] ) {
	if ( argc < 2 ) return -1;

	global_term_init();
	atexit( cleanup );

	file_list = malloc( sizeof(void*) * 8 );
	term_list = malloc( sizeof(void*) * 4 );
	memset( file_list, 0, sizeof(void*) * 8 );
	memset( term_list, 0, sizeof(void*) * 4 );

	struct file* fllf = alloc_file( argv[ 1 ] );
	struct term* trm = alloc_term();
	*file_list = fllf;
	*term_list = trm;

	value_to_write_in = &(trm->spc);
	string_to_set = &(trm->mess);

	fllf->cur_ln = 0;
	trm->cur_row = 0;
	fllf->cur_ch = 0;

	int c = 0;
	do {
		int tmp_variable;
		if ( c != 0x1B )
			trm->col_sav = 0;
		switch ( c ) {
			case 0x1B:
				if ( getchar() == '[' ) {
					c = getchar();
					if ( c != 'A' && c != 'B' )
						trm->col_sav = 0;
					switch ( c ) {
						case 'A':
							line_up( trm, fllf );
// 							arrow_up();
							break;
						case 'B':
							line_down( trm, fllf );
// 							arrow_down();
							break;
						case 'C':
							tmp_variable = trm->cur_col;
							trm->cur_col++;
							recompute_offsets_from_curcol( trm, fllf, 0 );
// 							this is ghastly ik, runtime of c+n
							if ( trm->cur_col == tmp_variable ) {
								trm->cur_col = 0;
								line_down( trm, fllf );
							}
// 							arrow_right();
							break;
						case 'D':
							tmp_variable = trm->cur_col;
							recompute_offsets_from_curcol( trm, fllf, 1 );
							if ( trm->cur_col == tmp_variable ) {
								trm->cur_col = 0x7FFFFFFF;
								line_up( trm, fllf );
								trm->col_sav = trm->cur_col;
							}
// 							arrow_left();
							break;
// 						case '2':
// 							c = getchar();
// 							if ( c != '~' ) break;
// 							break;
						case '3':
							c = getchar();
							if ( c != '~' ) break;
							f_delete_char( fllf );
							break;
						case '5':
							c = getchar();
							if ( c != '~' ) break;
							page_up( trm, fllf );
							break;
						case '6':
							c = getchar();
							if ( c != '~' ) break;
							page_down( trm, fllf );
							break;
					}
					trm->spc = (char)c;
					trm->mess = "esc";
				}
				break;
			case '\r':
			case '\n':
				f_enter( fllf );
				trm->cur_col = 0;
				trm->cur_row = fllf->cur_ln - trm->lin_off;
				check_ln_down( trm, fllf );
				trm->cur_row = fllf->cur_ln - trm->lin_off;
				break;
			case 0x7F:
				fllf->cur_ch--;
				tmp_variable = fllf->buf[ fllf->cur_ln ].buf[ fllf->cur_ch ] == '\t';
				trm->cur_col -= tmp_variable ? trm->tab_width - ( trm->cur_col % trm->tab_width ) : 1;
// 				if we've run out of chars, we need to go to the prev line
				if ( trm->cur_col < 0 ) {
					if ( trm->cur_row > 0 ) {
						trm->cur_row--;
						check_ln_up( trm, fllf );
// 						this is kinda a hack to get this as high as possible so it always goes to the end of the line.
						trm->cur_col = 0x7FFFFFFF;
// 						yet another hack to get the off for the prev line
						fllf->cur_ln--;
						recompute_offsets_from_curcol( trm, fllf, 0 );
						fllf->cur_ln++;
						fllf->cur_ch = -1;
						f_delete_char( fllf );
// 						trm->spc = fllf->cur_ch;
					} else {
// 						if there are no more lines, we just stay where we are
						trm->cur_col = 0;
						fllf->cur_ch = 0;
					}
					break;
				}
				f_delete_char( fllf );
				recompute_offsets_from_curcol( trm, fllf, 0 );
				break;
			case 0:
				break;
			default:
				if ( c >= 0x20 || c == '\t' ) {
					trm->cur_col++;
					f_insert_char( fllf, c );
					recompute_offsets_from_curcol( trm, fllf, 0 );
				}
		}

		trm->spc = trm->spc ? trm->spc : (char)c;
		trm->mess = trm->mess ? trm->mess : "norm";
// 		set_term_coords( trm, fllf );
		unsigned long int siz = get_term_size();
		trm->tot_row = siz >> 32;
		trm->tot_col = siz & 0xFFFFFFFF;
		trm->use_row = trm->tot_row - 1;
		trm->use_col = trm->tot_col;
		render_full_term( trm, fllf );
		c = getchar();
		trm->spc = 0;
		trm->mess = 0;
// 	} while ( c != 0x11 );
	} while ( c != '`' );

	dbg_dump_st_contents( fllf );
	puts( "here" );
	DEALLOC_FILE( fllf );
	puts( "here" );
	return 0;
}

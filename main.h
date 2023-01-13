
#ifndef __MAIN_HEAD
#define __MAIN_HEAD

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
	sz = nsz;														\
	if ( !( ptr = realloc( ptr, sz ) ) )							\
		ERR_QUIT( "mem broke" );									\
})

#define MMALLOC( ptr, sz, nsz ) ({									\
	sz = nsz;														\
	if ( !( ptr = realloc( ptr, sz ) ) )							\
		ERR_QUIT( "mem broke" );									\
})

#define PARR( ptr, sz ) ({											\
	printf( "line %i: [ ", __LINE__ );								\
	for ( int i = 0; i < sz-1; i++ ) {								\
		printf( "%02X, ", *( (unsigned char*)ptr + i ) );			\
	}																\
	printf( "%02X ]\n", *( (unsigned char*)ptr + sz - 1 ) );		\
})

struct lin {
	char* line;
	int size;
	int chramt;
	int tabs;
};

struct editor_state {
	int flags;
// 	line pointer stuff
	struct lin* lptrbuf;	// moving lptrbuf in here, stores all of the lines
	int lptrbuf_s;			// lptrbuf_s
	int line_lim;
	int cur_line;			// current line
	char* cur_lp;			// current line pointer, shorthand for E.lptrbuf[ E.cur_line ].line
	int line_amt;			// total line amount
//	index of line
	int char_index;			// char offset in current line
	int char_amt;			// total chars in line, shorthand for E.lptrbuf[ E.cur_line ].chramt;
	int char_lim;			// limit before realloc with E.cur_lp
	int char_tbof;			// char_index + tab offsets to get total space behind us.
	int char_tbof_a;		// the total space in front of us.
// 	terminal position
	int scr_st;				// char index of the start of the line in screen space;
							// if a line overflows to be greater than scr_w but not scr_w*2, this would be scr_w-6.
	int scr_x;				// screen column (starts at 1, equals char_index + 1 + scr_tabs * (tab_width - 1) )
	int index_scr;			// when you switch lines, this keeps track of the x offset.
	int scr_y;				// screen row (equals cur_line + 1 - client controlled offset)
//	defining terminal area
	int tab_width;			// current tab width.
	int scr_w;				// screen width in columns.
	int scr_h;				// screen height in rows.
	int pscr_w;				// *pageable* screen width in columns.
	int pscr_h;				// *pageable* screen height in rows.
	int scr_x_off;			// start of pageable space.
//	file pointer(s)
	FILE* itmp;
};

extern struct editor_state E;

#define LIN_S sizeof( struct lin )
#define LINE_S 64

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

extern int save_code;
extern FILE* itmp;
extern struct termios defaults;
extern unsigned int lptrbuf_s;
extern struct lin* lptrbuf;

void free_lptrbuf();
void cleanup();
void escape_seq( char* seq );
void plop_cursor( unsigned int y, unsigned int x );
void off_cursor ( unsigned int x );
unsigned int get_tab_width();
unsigned int lg_p_1( unsigned int k );
void get_tab_space();
void get_tab_space_aft();
void get_scr_x();
void render_l_line();
void line_prefix();
void render_line();
void render_down();

#endif

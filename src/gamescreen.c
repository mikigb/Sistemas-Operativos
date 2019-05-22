#include <ncurses.h>
#include <stdlib.h>

#define TEAM1     'A'
#define TEAM2     'B'
#define TEAM3     'C'
#define TEAM4     'D'
#define TEAM5     'E'
#define MISSILE   '*'

#define TEAM1_PAIR     1
#define TEAM2_PAIR     2
#define TEAM3_PAIR     3
#define TEAM4_PAIR     4
#define TEAM5_PAIR     5
#define MISSILE_PAIR   6
#define REST           7


void screen_init() {
	initscr();
	clear();
	noecho();
	cbreak();
	keypad(stdscr, FALSE);
	curs_set(0);
	/* initialize colors */

    if (has_colors() == FALSE) {
        endwin();
        printf("Your terminal does not support color\n");
        exit(1);
    }

    start_color();
    init_pair(TEAM1_PAIR, COLOR_WHITE, COLOR_YELLOW);
    init_pair(TEAM2_PAIR, COLOR_WHITE, COLOR_BLUE);
    init_pair(TEAM3_PAIR, COLOR_WHITE, COLOR_GREEN);
    init_pair(TEAM4_PAIR, COLOR_WHITE, COLOR_RED);
    init_pair(TEAM5_PAIR, COLOR_WHITE, COLOR_CYAN);
    init_pair(MISSILE_PAIR, COLOR_MAGENTA, COLOR_WHITE);
    init_pair(REST, COLOR_BLACK, COLOR_WHITE);

    clear();
}

void screen_addch(int row, int col, char symbol)
{
	if(symbol == TEAM1) {
		attron(COLOR_PAIR(TEAM1_PAIR));
	    mvaddch(row,col,symbol);
	    attroff(COLOR_PAIR(TEAM1_PAIR));
	} else if(symbol == TEAM2) {
		attron(COLOR_PAIR(TEAM2_PAIR));
	    mvaddch(row,col,symbol);
	    attroff(COLOR_PAIR(TEAM2_PAIR));
	} else if(symbol == TEAM3) {
		attron(COLOR_PAIR(TEAM3_PAIR));
	    mvaddch(row,col,symbol);
	    attroff(COLOR_PAIR(TEAM3_PAIR));
	} else if(symbol == TEAM4) {
		attron(COLOR_PAIR(TEAM4_PAIR));
	    mvaddch(row,col,symbol);
	    attroff(COLOR_PAIR(TEAM4_PAIR));
	} else if(symbol == TEAM5) {
		attron(COLOR_PAIR(TEAM5_PAIR));
	    mvaddch(row,col,symbol);
	    attroff(COLOR_PAIR(TEAM5_PAIR));
	} else if(symbol == MISSILE) {
		attron(COLOR_PAIR(MISSILE_PAIR));
	    mvaddch(row,col,symbol);
	    attroff(COLOR_PAIR(MISSILE_PAIR));
	}else {
		attron(COLOR_PAIR(REST));
	    mvaddch(row,col,symbol);
	    attroff(COLOR_PAIR(REST));
	}
}

void screen_refresh()
{
	refresh();
}


void screen_end() {
	endwin();
}


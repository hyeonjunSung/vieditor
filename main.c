#include <ncurses.h>
#include <string.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <ctype.h>

#define KEY_ESC 27
#define KEY_COLON 58
#define KEY_INSERT 105
#define KEY_ENT 10
#define KEY_A 97 
#define KEY_W 119
#define KEY_Q 113
#define KEY_D 100
#define KEY_R 114
#define KEY_X 120
#define TRUE 1
#define FALSE 0
#define FILE_MODE 0644

void initializeCurses();
void moveCursor(WINDOW* wnd, int ch);
void setPrompt(char *prompt, WINDOW* wndEdit, WINDOW* wndCmd);
void insertModeEnter(WINDOW* wndEdit);
void insertModePrintChar(WINDOW* wnd, int ch);
void insertModeBackspace(WINDOW *wnd);
void saveFile(WINDOW *wnd, char *argv[]);
void commandModeDeleteLine(WINDOW *wnd);
void commandModeDeleteOneChar(WINDOW *wnd);
void commandModeInsertA(WINDOW *wndEdit, WINDOW *wndCmd);
void commandModeReplaceChar(WINDOW *wnd,int ch);

int main(int argc, char *argv[])
{
	int fd;		//file descriptor
	int ch;		//keyboard input char
	int ch2;
	int doExit = FALSE;
	int mode = 0;
	WINDOW *editWindow;
	WINDOW *cmdWindow;
	int load[2];
	int fileCheck = 0; 	

	if( argc != 2 )
	{
		printf("Usage : ./test [file_name]\n");
		exit(1);
	}

	/* If file is not exist, make a file */
	if ( fd = open(argv[1], O_RDWR | O_APPEND) < 0 ){
		if( fd = creat(argv[1], FILE_MODE) < 0 ){
			printf("Create Errori\n");
		}
		fileCheck = 1;
	}

	/* WINDOW START */
	initializeCurses();

	/* Divide Screen, Edit / Command */
	editWindow = derwin(stdscr, LINES - 1, COLS, 0, 0);
	cmdWindow = derwin(stdscr, 1, COLS, LINES - 1, 0);
	refresh();

	/* Permit Special Character */
	keypad(stdscr, TRUE);
	keypad(editWindow, TRUE);
	keypad(cmdWindow, TRUE);

	/* File Open and Load */
	fd = open(argv[1], O_RDWR | O_APPEND);
	while( read(fd, load, 1) > 0 )
		waddch(editWindow, load[0]); 
	wmove(editWindow, 0, 0);
	wrefresh(editWindow);
	
	/* Select Mode */
	while(doExit != TRUE)
	{
		ch = getch();	
		if( mode == 0 ) 	//Command mode
		{
			switch(ch)
			{
			case KEY_COLON :
				mode = 1;
				setPrompt(":", editWindow,cmdWindow);
				break;
			case KEY_INSERT :
				mode = 2;
				setPrompt(" -- INSERT MODE -- ", editWindow, cmdWindow);
				break;
			case KEY_A :
				mode = 2;
				commandModeInsertA(editWindow, cmdWindow);
				break;
			case KEY_X :
				commandModeDeleteOneChar(editWindow);
				break;
			case KEY_D :
				commandModeDeleteLine(editWindow);
				break;
			case KEY_R :
				setPrompt(" -- REPLACE MODE -- ", editWindow, cmdWindow);
				commandModeReplaceChar(editWindow, ch);
				setPrompt(" ", editWindow, cmdWindow);
				break;
			case KEY_UP :
			case KEY_DOWN :
			case KEY_LEFT :
			case KEY_RIGHT :
				moveCursor(editWindow, ch);
				break;
			default :
				break;
			}
		}	
		else if( mode == 1 )	// Colon Mode
		{
			switch(ch)
			{
			case KEY_ESC :
				mode = 0;
				setPrompt(" ",editWindow, cmdWindow);
				break;
			case KEY_W:
				ch2 = KEY_W;
				setPrompt(":w",editWindow, cmdWindow);
				break;
			case KEY_Q:
				ch2 = KEY_Q;
				setPrompt(":q",editWindow, cmdWindow);
				break;	
			case KEY_ENT:
				if(ch2 == KEY_W)
				{
					setPrompt("File is written!", editWindow, cmdWindow); 
					wrefresh(cmdWindow);
					saveFile(editWindow, &argv[1]);	//save edit contents
					fileCheck--;
				}
				else if(ch2 == KEY_Q)
				{		
					fileCheck++;
					close(fd);
					doExit = TRUE;	//exit
				}
				break;
			default :
				break;
			}
					
		}
		else if( mode == 2 )	// Insert Mode
		{	
			switch(ch)
			{
			case KEY_ESC :
				mode = 0;
				setPrompt(" ",editWindow, cmdWindow);
				break;
			case KEY_ENT :
				insertModeEnter(editWindow);
				break;	
			case KEY_UP :
			case KEY_DOWN :
			case KEY_LEFT :
			case KEY_RIGHT :
				moveCursor(editWindow, ch);
				break;
			case KEY_BACKSPACE :
				insertModeBackspace(editWindow);
				break;
			default :
				insertModePrintChar(editWindow, ch);
				break;
			}	
		}
	}
	endwin();
	return 0;
}
	
void initializeCurses()
{
        initscr();
        raw();
        keypad(stdscr, TRUE);
        noecho();
}


void moveCursor(WINDOW* wnd, int ch)
{
	int y,x;

	getyx(wnd, y, x);
	
	switch(ch)
	{
		case KEY_UP :
			wmove(wnd, --y, x);
			break;
		case KEY_DOWN :
			wmove(wnd, ++y, x);
			break;
		case KEY_LEFT :
			wmove(wnd, y, --x);
			break;
		case KEY_RIGHT :
			wmove(wnd, y, ++x);
			break;
	}
	wrefresh(wnd);
}

void setPrompt(char* prompt, WINDOW* wndEdit, WINDOW* wndCmd)
{
        int ey, ex;
	int my, mx;
        getyx(wndEdit, ey, ex);

	getmaxyx(wndCmd, my, mx);
        wmove(wndCmd, 0, 0);
        wclrtoeol(wndCmd);
        mvwprintw(wndCmd, 0, 0, prompt);
	mvwprintw(wndCmd, 0, mx - 20, "(%d, %d)", ex + 1, ey + 1);
	wrefresh(wndCmd);

        wmove(wndEdit, ey, ex);
	wrefresh(wndEdit);
}

void insertModeEnter(WINDOW* wndEdit) 
{
	int i, j = 0;
	int dy, dx;
	int my, mx;
	int *buf;
	int count;

	getyx(wndEdit, dy, dx);
        getmaxyx(wndEdit, my, mx);
        buf = (int*)malloc(sizeof(int*)*mx);

        count = mx - dx;

        while( count > 0 )//if the line is not full
        {
	        buf[i++] = winch(wndEdit);
                wdelch(wndEdit);
                count--;
        }
        buf[i] = '\0';//if the line is full, last char is "\0"
        wmove(wndEdit, ++dy, 0);//move cursor to next line
        winsertln(wndEdit);//print cursor move to next line
	while(buf[j] != '\0')//print character of last line continually
        {
        	waddch(wndEdit, buf[j]);
                j++;
        }
        free(buf);
        wmove(wndEdit, dy, 0);
        wrefresh(wndEdit);
}

void insertModePrintChar(WINDOW* wnd,int ch)
{
	int dy, dx;
        getyx(wnd, dy, dx);
        winsch(wnd,ch);
        dx++;
        wmove(wnd, dy, dx);
	wrefresh(wnd);
}

void insertModeBackspace(WINDOW *wnd)
{
	int dy, dx;
        getyx(wnd, dy, dx);
        wmove(wnd, dy, --dx);
        if( dx < 0 ) {		// if the line has no word
                --dy;
                while( dx != '\n' ){ // until last char
                        dx++;
                }
                wmove(wnd, dy, dx);
                wdelch(wnd);
        }
        else
                wdelch(wnd);
	wrefresh(wnd);
}

/* Get Last Line Coordinate */
int getLastLine(WINDOW *wnd, int my, int mx) {
        int i, j;
        wmove(wnd, my - 1, mx - 1);
        for (i = my - 1; i >= 0; i--) {
                for (j = mx - 1; j >= 0; j--) {
                        if (isspace(winch(wnd)))
                                wmove(wnd, i, j);
                        else
                                return i;
                }
        }
        return 0;
}

/* Get Last Char Coordinate In One Line*/
int getLastChar(WINDOW *wnd, int mx) {
        int y, x;
        getyx(wnd, y, x);
        x = mx - 1;
        wmove(wnd, y, x);
        while (isspace(winch(wnd)) && x > 0)
                wmove(wnd, y, --x);
        wmove(wnd, y, 0);
        return x;
}

void saveFile(WINDOW *wnd, char *argv[]) {
        int lChar;
        int lLine;
        int fd;
        int my;
        int mx;
        int y = 0;
        int x = 0;
        char ch[2];

        /* Overwrite */
        fd = open(argv[0], O_RDWR | O_APPEND | O_TRUNC);

        getmaxyx(wnd, my, mx);
        lLine = getLastLine(wnd, my, mx);
        wmove(wnd, 0, 0);
        while (y <= lLine) {
                x = 0;
                lChar = getLastChar(wnd, mx);
                while (x <= lChar) {
                        ch[0] = winch(wnd);
                        write(fd, ch, 1);
                        wmove(wnd, y, ++x);
                }
                ch[0] = '\n';
                write(fd, ch, 1);
                wmove(wnd, ++y, 0);
        }
}

void commandModeDeleteLine(WINDOW *wnd)
{
	int ch;
	int i, j;
	int dy, dx;
	getyx(wnd, dy, dx);

	ch = wgetch(wnd);
	if ( ch == 'd' )		//Delete 1 line
	{
		wdeleteln(wnd);
		wmove(wnd, dy, 0);
		wrefresh(wnd);
	}
	else if ( ch == 'w' ) 		//Delete 1 word
	{
		while((i = winch(wnd)) != ' ')
		{
			wdelch(wnd);
		}
		wdelch(wnd);
		wrefresh(wnd);
	}
}

void commandModeDeleteOneChar(WINDOW *wnd)
{
	int y, x;
	
	getyx(wnd, y, x);
	wmove(wnd, y, --x);
	wdelch(wnd);
	wrefresh(wnd);
}

void commandModeInsertA(WINDOW *wndEdit, WINDOW *wndCmd)
{
	int dy, dx;
	getyx(wndEdit, dy, dx);	
	wmove(wndEdit, dy, ++dx);
	wrefresh(wndEdit);
	
	setPrompt(" -- INSERT MODE -- ", wndEdit, wndCmd);
}

void commandModeReplaceChar(WINDOW *wnd, int ch)
{
	int dy, dx;
	getyx(wnd, dy, dx);
	wmove(wnd, dy, dx);
	wdelch(wnd);
	ch = getch();
	winsch(wnd, ch);
	wrefresh(wnd);
}

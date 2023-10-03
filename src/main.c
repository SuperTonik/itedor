/* INCLUDES */

#include <ctype.h>
#include <stdio.h>
#include <unistd.h>
#include <termios.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/ioctl.h>			// For getting window size 

/* DEFINES */

#define CTRL_KEY(k) ((k) & 0x1f)	// Macro to set upper 3 bits of k to 0

/* DATA */

struct editorConfig
{
	int screenrows;
	int screencols;
	struct termios orig_termios;
};

struct editorConfig E;		// Editor state variable. 

/* TERMINAL */

void die(const char *s) {
	/*
		Function that terminates the program when an error occures.

		Parameters:
			const char * s	:	Error message.
	*/

	write(STDOUT_FILENO, "\x1b[2J", 4);
	write(STDOUT_FILENO, "\x1b[H", 3);
	
	perror(s);
 	exit(EXIT_FAILURE);
}

void disableRawMode() {
	if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &E.orig_termios) == -1)
    	die("tcsetattr");
}

void enableRawMode() {
  	if (tcgetattr(STDIN_FILENO, &E.orig_termios) == -1) die("tcgetattr");
	tcgetattr(STDIN_FILENO, &E.orig_termios);
	atexit(disableRawMode);
	
	struct termios raw = E.orig_termios;
  	raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
  	raw.c_oflag &= ~(OPOST);
  	raw.c_cflag |= (CS8);
  	raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
  	raw.c_cc[VMIN] = 0;
  	raw.c_cc[VTIME] = 1;

	if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1) die("tcsetattr");
}

char editorReadKey() {
	/*
		Function to wait for one key press and return the result.
	*/
	int nread;			
	char c;
	while ((nread = read(STDIN_FILENO, &c, 1)) != 1) {
		if (nread == -1 && errno != EAGAIN) die("read");
	}
	return c;
}

int getWindowSize(int *rows, int *cols) {
	/*
		Function to return the size of the terminal window. Initally an 
		attempt will be made to fetch the size using ioctl() function. However,
		since this might fail, there is a fallback method where escape sequences
		are used to deduce the number of rows and columns based on the location
		of the cursor.

		Parameters:
			int *rows	:	Variable where the number of rows will be stored.
			int *cols	:	Variable where the number of columns will be stored.
		
		Returns:
			-1 on failure, 0 otherwise.
	*/
	struct winsize ws;

	if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0) {
		if (write(STDOUT_FILENO, "\x1b[999C\x1b[999B", 12) != 12) return -1;
		editorReadKey();
		return -1;
	} else {
		*cols = ws.ws_col;
		*rows = ws.ws_row;
		return 0;
	}
}

/* INPUT */

void editorProcessKeypress() {
	/*
		Handle the keypress returned by editorReadKey().
	*/
	char c = editorReadKey();

	// Mapping key combinations to various functions.
	switch(c) {
		case CTRL_KEY('q'):		// In case of CTRL+Q exit program.
			write(STDOUT_FILENO, "\x1b[2J", 4);
			write(STDOUT_FILENO, "\x1b[H", 3);
			exit(EXIT_SUCCESS);		
			break;
	}
}

/* OUTPUT */

void editorDrawRows() {
	/*
		Function that draws each row of the text buffer currently being edited.
		A hashtag is drawn on a row indicates that the row is not part of the file
		and doesn't contain any text. This is similar to vim.
	*/

	for (int i = 0; i < E.screenrows; i++)
	{
		write(STDOUT_FILENO, "#\r\n", 3);
	}	
}

void editorRefreshScreen() {
	/*
		The following are VT100 escape sequences:

			"\x1b[2J" : Clear entire screen.
			"\x1b[H"  : Reposition cursor to top-left corner. 

		Alternative would be to use something like ncurses for increased support
		of different terminals.
	*/
	write(STDOUT_FILENO, "\x1b[2J", 4);
	write(STDOUT_FILENO, "\x1b[H", 3);

	editorDrawRows();				// Draw buffer.

	write(STDOUT_FILENO, "\x1b[H", 3);	// Reposition cursor to top-left corner.
}

/* INIT */

void initEditor() {
	/*
		Initalize editor, ie. initalize all fields in the editor state
		variable.
	*/
	if (getWindowSize(&E.screenrows, &E.screencols) == -1) die("getWindowSize");
}

int main() {
	enableRawMode();
	initEditor();
	
  	while (1) {
		editorProcessKeypress();
  	}
	return 0;
}

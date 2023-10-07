/* INCLUDES */

#include <ctype.h>
#include <stdio.h>
#include <unistd.h>
#include <termios.h>			// Terminal stuff.
#include <stdlib.h>
#include <errno.h>
#include <sys/ioctl.h>			// For getting window size 
#include <string.h>				// Functions for string handling. Used for dynamic string type.

/* DEFINES */

#define VERSION "0.1"				// The most important definition: version of the program.
#define CTRL_KEY(k) ((k) & 0x1f)	// Macro to set upper 3 bits of k to 0
#define ABUF_INIT {NULL, 0}			// Macro for an empty string buffer.

enum editorKey {
	// Mapping arrow keys to integer number so that 
	// they won't conflict with any other keys.
	ARROW_LEFT = 1000,
	ARROW_RIGHT,
	ARROW_UP,
	ARROW_DOWN
};

/* DATA */

struct editorConfig
{
	int cx, cy;
	int screenrows;
	int screencols;
	struct termios orig_termios;
};

struct abuf {	// Kind of dynamic string that supports only appending.
	char *b;
	int len;
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
 	exit(EXIT_FAILURE);		// Program termination with clean up.
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

int editorReadKey() {
	/*
		Function to wait for one key press and return the result.

		Returns:
			Char value of key pressed.
	*/

	int nread;			
	char c;
	while ((nread = read(STDIN_FILENO, &c, 1)) != 1) {
		if (nread == -1 && errno != EAGAIN) die("read");
	}

	if (c == '\x1b') {
		char seq[3];

		if (read(STDIN_FILENO, &seq[0], 1) != 1) return '\x1b';
		if (read(STDIN_FILENO, &seq[1], 1) != 1) return '\x1b';

		if (seq[0] == '[') {
			switch (seq[1]) {
				case 'A': return ARROW_UP;
				case 'B': return ARROW_DOWN;
				case 'C': return ARROW_RIGHT;
				case 'D': return ARROW_LEFT;
			}
		}

		return '\x1b';
	} else {
		return c;
	}

}

int getCursorPosition(int *rows, int *cols) {
	/*
		This function gets the cursor position in terminal.

		Parameters:
			rows	:	Storage for cursor row position.
			cols	:	--""-- column position.
		
		Returns:
			Error code.
	*/

	char buffer[32];
	size_t i = 0;

	// 6n is escape sequence for cursor position report.
	if (write(STDOUT_FILENO, "\x1b[6n", 4) != 4) return -1;

	while (i < sizeof(buffer) - 1) {
		if (read(STDIN_FILENO, &buffer[i], 1) != 1) break;
		if (buffer[i] == 'R') break;
		i++;
	}

	buffer[i] = '\0';		// printf() expects string to end with 0 byte.
	//printf("\r\n&buf[i]: '%s'\r\n", &buffer[1]);

	if (buffer[0] != '\x1b' || buffer[1] != '[') return -1;
	if (sscanf(&buffer[2], "%d;%d", rows, cols) != 2) return -1;

	return 0;
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
		return getCursorPosition(rows, cols);
	} else {
		*cols = ws.ws_col;
		*rows = ws.ws_row;
		return 0;
	}
}

/* APPEND BUFFER */

void abAppend(struct abuf *ab, const char *s, int len) {
	/*
		Append operation for dynamic string type. 
	
		Parameters:
			ab	: 	String buffer to append to.
			s	: 	String that will be appended to ab.
			len	:	Lenght of string s. 	 
	*/
	
	char *new = realloc(ab -> b, ab->len + len);		// Can me get some memory, plz? :3

	if (new == NULL) return;
	memcpy(&new[ab->len], s, len);		// Copy s to the end of the current data in the buffer.
	ab->b = new;						
	ab->len += len;
}

void abFree(struct abuf *ab) {
	/*
		Free buffer from memory.
		
		Parameters:
			ab	:	string buffer to be deallocated.
	*/

	free(ab->b);
}

/* INPUT */

void editorMoveCursor(int key) {
	/*
		Function to map key press to cursor movement.

		Parameters:
			key	:	Key pressed.
	*/
	switch (key) {
		case ARROW_LEFT:
			E.cx--;
			break;
		case ARROW_RIGHT:
			E.cx++;
			break;
		case ARROW_UP:
			E.cy--;
			break;
		case ARROW_DOWN:
			E.cy++;
			break;
	}
}

void editorProcessKeypress() {
	/*
		Handle the keypress returned by editorReadKey().
	*/

	int c = editorReadKey();

	// Mapping key combinations to various functions.
	switch(c) {
		case CTRL_KEY('q'):		// In case of CTRL+Q exit program.
			write(STDOUT_FILENO, "\x1b[2J", 4);
			write(STDOUT_FILENO, "\x1b[H", 3);
			exit(EXIT_SUCCESS);		
			break;
		
		case ARROW_UP:
		case ARROW_DOWN:
		case ARROW_LEFT:
		case ARROW_RIGHT:
			editorMoveCursor(c);
			break;
	}
}

/* OUTPUT */

void editorDrawRows(struct abuf *ab) {
	/*
		Function that draws each row of the text buffer currently being edited.
		A tilde is drawn on a row indicates that the row is not part of the file
		and doesn't contain any text. This is similar to vim.

		Parameters:
			ab	:	String buffer containing to which the data is appended.
	*/

	int y;
	for (y = 0; y < E.screenrows; y++) {
		if (y == E.screenrows / 3) {
			char welcome[80];
			int welcomelen = snprintf(welcome, sizeof(welcome),
				"Itedor editor -- version %s", VERSION);
			if (welcomelen > E.screencols) welcomelen = E.screencols;
			int padding = (E.screencols - welcomelen) / 2;
			if (padding) {
				abAppend(ab, "~", 1);
				padding--;	
			}
			while (padding--) abAppend(ab, " ", 1);
			abAppend(ab, welcome, welcomelen);
		} else {
			abAppend(ab, "~", 1);
		}

		abAppend(ab, "\x1b[K", 3);
		if (y < E.screenrows - 1) {
			abAppend(ab, "\r\n", 2);
		}
	}	
}

void editorRefreshScreen() {
	/*
		The following are VT100 escape sequences:
			"\x1b[H"  	: Reposition cursor to top-left corner. 
			"\x1b[?25l"	: Hide cursor
			"\x1b[?25h"	: Show cursor

		Alternative would be to use something like ncurses for increased support
		of different terminals.
	*/
	
	struct abuf ab = ABUF_INIT;

	abAppend(&ab, "\x1b[?25l", 6);		// Hide cursor before refreshing to prevent flickering.
	abAppend(&ab, "\x1b[H", 3);

	editorDrawRows(&ab);				// Draw buffer.

	// Move cursor to position stored in editorConfig struct.
	char buffer[32];
	snprintf(buffer, sizeof(buffer), "\x1b[%d;%dH", E.cy + 1, E.cx + 1);
	abAppend(&ab, buffer, strlen(buffer));

	abAppend(&ab, "\x1b[?25h", 6);		// Reveal cursor

	write(STDOUT_FILENO, ab.b, ab.len);	// Write all of buffer at once.
	abFree(&ab);
}

/* INIT */

void initEditor() {
	/*
		Initalize editor, ie. initalize all fields in the editor state
		variable.
	*/

	E.cx = 0;
	E.cy = 0;

	if (getWindowSize(&E.screenrows, &E.screencols) == -1) die("getWindowSize");
}

int main() {
	enableRawMode();
	initEditor();
	
  	while (1) {
		editorRefreshScreen();
		editorProcessKeypress();
  	}
	return 0;
}

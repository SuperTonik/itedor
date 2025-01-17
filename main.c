#include <ctype.h>
#include <stdio.h>
#include <unistd.h>
#include <termios.h>
#include <stdlib.h>
#include <errno.h>

/* DEFINES */
#define CTRL_KEY(k) ((k) & 0x1f)	// Macro to set upper 3 bits of k to 0

/* DATA */
struct termios orig_termios;

/* TERMINAL */
void die(const char *s) {
  perror(s);
  exit(1);
}

void disableRawMode() {
	if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios) == -1)
    	die("tcsetattr");
}

void enableRawMode() {
  	if (tcgetattr(STDIN_FILENO, &orig_termios) == -1) die("tcgetattr");
	tcgetattr(STDIN_FILENO, &orig_termios);
	atexit(disableRawMode);
	
	struct termios raw = orig_termios;
  	raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
  	raw.c_oflag &= ~(OPOST);
  	raw.c_cflag |= (CS8);
  	raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
  	raw.c_cc[VMIN] = 0;
  	raw.c_cc[VTIME] = 1;

	if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1) die("tcsetattr");
}

/* INIT */
int main() {
	enableRawMode();
	
  	while (1) {
    	char c = '\0';
    	if (read(STDIN_FILENO, &c, 1) == -1 && errno != EAGAIN) die("read");
    	if (iscntrl(c)) {
      		printf("%d\r\n", c);
    	} else {
      		printf("%d ('%c')\r\n", c, c);
    	}
    	if (c == 'q') break;
  	}
	return 0;
}

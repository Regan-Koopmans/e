CC = gcc
CFLAGS = -Wall -Wextra -std=c99
LIBS = -lncurses

editor: editor.c
	$(CC) $(CFLAGS) -o editor editor.c $(LIBS)

clean:
	rm -f editor

.PHONY: clean
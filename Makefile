CC = clang
CFLAGS = -Wall -Wextra -std=c99 -flto -O3
LIBS = -lncurses

editor: editor.c
	$(CC) $(CFLAGS) -o e editor.c $(LIBS)

clean:
	rm -f e

.PHONY: clean

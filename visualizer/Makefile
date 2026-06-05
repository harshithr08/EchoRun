CC     = gcc
CFLAGS = -Wall -Wextra -g

all: echovis

echovis: echovis.c trace_parser.c svg_renderer.c tui_renderer.c \
         trace_diff.c trace_reader.c echotrace_bin.c
	$(CC) $(CFLAGS) -o $@ $^

clean:
	rm -f echovis

.PHONY: all clean

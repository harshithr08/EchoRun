CC     = gcc
CFLAGS = -Wall -Wextra -g

all: echoplay

# //CHANGE
echoplay: echoplay.c replay_cursor.c replay_loop.c syscall_inject.c \
          signal_inject.c checkpoint.c time_travel.c repl.c \
          divergence.c trace_reader.c echotrace_bin.c sys_cat_info.c \
          ../../shared-CHANGE/telemetry-CHANGE.c
	$(CC) $(CFLAGS) -o $@ $^
# //CHANGE

clean:
	rm -f echoplay

.PHONY: all clean

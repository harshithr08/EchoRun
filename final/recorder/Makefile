CC     = gcc
CFLAGS = -Wall -Wextra -g

all: echorun dump_trace tracee tracee2 tracee3

# //CHANGE
echorun: echorun.c syscall_loop.c ND_syscall_handler.c \
         echotrace_bin.c sys_cat_info.c ../../shared-CHANGE/telemetry-CHANGE.c
	$(CC) $(CFLAGS) -o $@ $^

dump_trace: dump_trace.c trace_reader.c echotrace_bin.c
	$(CC) $(CFLAGS) -o $@ $^

tracee: tracee.c
	$(CC) $(CFLAGS) -o $@ $^

tracee2: tracee2.c
	$(CC) $(CFLAGS) -o $@ $^

tracee3: tracee3.c
	$(CC) $(CFLAGS) -o $@ $^
# //CHANGE

clean:
	rm -f echorun dump_trace tracee tracee2 tracee3

.PHONY: all clean

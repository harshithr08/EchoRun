#include "ND_syscall_handler.h"
#include "echotrace_bin.h"
#include <sys/ptrace.h>
#include <sys/user.h>
#include <sys/syscall.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

static void read_from_tracee(pid_t pid, unsigned long long adr, unsigned char* dest, size_t len) {
    size_t bytes_read = 0;
    while (bytes_read < len) {
        errno = 0;
        long data = ptrace(PTRACE_PEEKDATA, pid, adr + bytes_read, 0);
        if (data == -1 && errno != 0) break;
        
        size_t to_copy = (len - bytes_read < sizeof(long)) ? (len - bytes_read) : sizeof(long);
        memcpy(dest + bytes_read, &data, to_copy); 
        bytes_read += to_copy;
    }
}

void handle_non_deterministic_exit(pid_t pid, int syscallnr, uint64_t seq_idx) {
    struct user_regs_struct regs;
    if (ptrace(PTRACE_GETREGS, pid, NULL, &regs) < 0) return;

    long long retval = (long long)regs.rax;
    if (retval <= 0) {
        write_trace_event(1, syscallnr, seq_idx, retval, 0, NULL);
        return;
    }

    unsigned long long buf_addr = 0;
    if (syscallnr == SYS_read || syscallnr == SYS_recvfrom) buf_addr = regs.rsi;
    else if (syscallnr == SYS_getrandom) buf_addr = regs.rdi;

    if (buf_addr != 0) {
        unsigned char *data = malloc(retval);
        if (data) {
            read_from_tracee(pid, buf_addr, data, retval);
            write_trace_event(1, syscallnr, seq_idx, retval, retval, data);
            free(data);
        }
    }
}
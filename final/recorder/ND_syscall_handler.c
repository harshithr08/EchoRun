#include "ND_syscall_handler.h"
#include "echotrace_bin.h"
#include <sys/ptrace.h>
#include <sys/user.h>
#include <sys/syscall.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/utsname.h>
#include <time.h>
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

//CHANGE
int handle_non_deterministic_exit(pid_t pid,
                                  int syscallnr,
                                  uint64_t seq_idx,
                                  uint32_t *payload_size_out) {
    struct user_regs_struct regs;
    if (payload_size_out) *payload_size_out = 0;
    if (ptrace(PTRACE_GETREGS, pid, NULL, &regs) < 0) return 0;

    long long retval = (long long)regs.rax;
    unsigned long long buf_addr = 0;
    uint32_t payload_len = 0;

    if (syscallnr == SYS_read || syscallnr == SYS_recvfrom) {
        if (retval > 0) payload_len = (uint32_t)retval;
        buf_addr = regs.rsi;
    } else if (syscallnr == SYS_getrandom) {
        if (retval > 0) payload_len = (uint32_t)retval;
        buf_addr = regs.rdi;
    } else if (syscallnr == SYS_fstat || syscallnr == SYS_stat) {
        if (retval == 0) payload_len = (uint32_t)sizeof(struct stat);
        buf_addr = regs.rsi;
    } else if (syscallnr == SYS_uname) {
        if (retval == 0) payload_len = (uint32_t)sizeof(struct utsname);
        buf_addr = regs.rdi;
    } else if (syscallnr == SYS_gettimeofday) {
        if (retval == 0) payload_len = (uint32_t)sizeof(struct timeval);
        buf_addr = regs.rdi;
    } else if (syscallnr == SYS_clock_gettime) {
        if (retval == 0) payload_len = (uint32_t)sizeof(struct timespec);
        buf_addr = regs.rsi;
    }

    if (payload_len == 0) {
        if (retval <= 0) {
            write_trace_event(1, syscallnr, seq_idx, retval, 0, NULL);
            return 1;
        }
        return 0;
    }

    if (buf_addr != 0) {
        unsigned char *data = malloc(payload_len);
        if (data) {
            read_from_tracee(pid, buf_addr, data, payload_len);
            write_trace_event(1, syscallnr, seq_idx, retval, payload_len, data);
            if (payload_size_out) *payload_size_out = payload_len;
            free(data);
            return 1;
        }
    }

    return 0;
}
//CHANGE

#include "divergence.h"
#include <stdio.h>
#include <inttypes.h>

// Maps syscall number to a human-readable name.
// Covers all 31 syscalls in sys_cat_info.c plus common ones.
static const char *syscall_name(uint32_t no) {
    switch (no) {
        case 0:   return "read";
        case 1:   return "write";
        case 2:   return "open";
        case 3:   return "close";
        case 4:   return "stat";
        case 5:   return "fstat";
        case 6:   return "lstat";
        case 8:   return "lseek";
        case 9:   return "mmap";
        case 10:  return "mprotect";
        case 11:  return "munmap";
        case 12:  return "brk";
        case 13:  return "rt_sigaction";
        case 14:  return "rt_sigprocmask";
        case 17:  return "pread64";
        case 18:  return "pwrite64";
        case 32:  return "dup";
        case 33:  return "dup2";
        case 39:  return "getpid";
        case 41:  return "socket";
        case 42:  return "connect";
        case 43:  return "accept";
        case 44:  return "sendto";
        case 45:  return "recvfrom";
        case 56:  return "clone";
        case 57:  return "fork";
        case 59:  return "execve";
        case 60:  return "exit";
        case 61:  return "wait4";
        case 62:  return "kill";
        case 72:  return "fcntl";
        case 96:  return "gettimeofday";
        case 102: return "getuid";
        case 228: return "clock_gettime";
        case 231: return "exit_group";
        case 235: return "uname";
        case 318: return "getrandom";
        default:  return "unknown";
    }
}

int check_divergence(uint32_t expected, uint32_t actual,
                     uint64_t seq_idx, divergence_report_t *report) {
    if (expected == actual) return 0;

    report->seq_idx          = seq_idx;
    report->expected_syscall = expected;
    report->actual_syscall   = actual;
    return 1;
}

void print_divergence(const divergence_report_t *report) {
    fprintf(stderr,
        "[DIVERGENCE] seq_idx=%" PRIu64
        "  expected: %s() [%u]  got: %s() [%u]\n",
        report->seq_idx,
        syscall_name(report->expected_syscall), report->expected_syscall,
        syscall_name(report->actual_syscall),   report->actual_syscall);
}
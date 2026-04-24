#include "syscall_inject.h"
#include <sys/ptrace.h>
#include <sys/user.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>

int suppress_syscall(pid_t pid) {
    struct user_regs_struct regs;
    if (ptrace(PTRACE_GETREGS, pid, NULL, &regs) < 0) {
        perror("suppress: PTRACE_GETREGS");
        return -1;
    }
    // Setting orig_rax to -1 tells the kernel: "no syscall to run"
    regs.orig_rax = (unsigned long long)-1;
    if (ptrace(PTRACE_SETREGS, pid, NULL, &regs) < 0) {
        perror("suppress: PTRACE_SETREGS");
        return -1;
    }
    return 0;
}

int inject_retval(pid_t pid, int64_t retval) {
    struct user_regs_struct regs;
    if (ptrace(PTRACE_GETREGS, pid, NULL, &regs) < 0) {
        perror("inject_retval: PTRACE_GETREGS");
        return -1;
    }
    // rax is the return value register on x86-64
    regs.rax = (unsigned long long)retval;
    if (ptrace(PTRACE_SETREGS, pid, NULL, &regs) < 0) {
        perror("inject_retval: PTRACE_SETREGS");
        return -1;
    }
    return 0;
}

int inject_memory(pid_t pid, unsigned long long addr,
                  const uint8_t *data, uint32_t len) {
    // PTRACE_POKEDATA writes one long (8 bytes) at a time
    uint32_t offset = 0;
    while (offset < len) {
        long word = 0;
        uint32_t chunk = len - offset;
        if (chunk >= sizeof(long)) {
            // Full 8-byte write
            memcpy(&word, data + offset, sizeof(long));
        } else {
            // Partial write: read existing word first, then overlay our bytes
            errno = 0;
            word = ptrace(PTRACE_PEEKDATA, pid, addr + offset, 0);
            if (word == -1 && errno != 0) {
                perror("inject_memory: PTRACE_PEEKDATA");
                return -1;
            }
            memcpy(&word, data + offset, chunk);
        }

        if (ptrace(PTRACE_POKEDATA, pid, addr + offset, word) < 0) {
            perror("inject_memory: PTRACE_POKEDATA");
            return -1;
        }
        offset += (chunk >= (uint32_t)sizeof(long)) ? sizeof(long) : chunk;
    }
    return 0;
}

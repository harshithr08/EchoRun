#ifndef SYSCALL_INJECT_H
#define SYSCALL_INJECT_H

#include <sys/types.h>
#include <stdint.h>

// Task 2.3 ─────────────────────────────────────────────────────────────────
// Called at syscall ENTRY stop.
// Sets orig_rax = -1, which makes the kernel skip the real syscall entirely.
// On exit stop, kernel returns -ENOSYS but we overwrite rax ourselves.
int suppress_syscall(pid_t pid);

// Task 2.3 ─────────────────────────────────────────────────────────────────
// Called at syscall EXIT stop (after suppress_syscall was used at entry).
// Writes the saved return value into rax so the tracee sees the recorded result.
int inject_retval(pid_t pid, int64_t retval);

// Task 2.4 ─────────────────────────────────────────────────────────────────
// Called after inject_retval for syscalls that also returned data in memory
// (read, recv, getrandom). Writes 'len' bytes from 'data' into the tracee's
// address space at 'addr' using PTRACE_POKEDATA (8 bytes at a time).
int inject_memory(pid_t pid, unsigned long long addr,
                  const uint8_t *data, uint32_t len);

#endif

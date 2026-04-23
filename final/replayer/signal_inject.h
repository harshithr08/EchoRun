#ifndef SIGNAL_INJECT_H
#define SIGNAL_INJECT_H

#include <sys/types.h>
#include <stdint.h>

// Task 2.5
// During replay, when we encounter a SIGNAL_EVENT in the trace, we need to
// deliver that signal to the tracee at this exact sequence position.
// We do this by passing the signal number as the 4th arg to PTRACE_CONT
// (or PTRACE_SYSCALL), which causes the kernel to inject it on resume.
int inject_signal(pid_t pid, int signum);

#endif

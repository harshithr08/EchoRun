#include "signal_inject.h"
#include <sys/ptrace.h>
#include <sys/wait.h>
#include <stdio.h>

int inject_signal(pid_t pid, int signum) {
    // PTRACE_SYSCALL with signum as the 4th argument delivers the signal
    // to the tracee when it resumes. The tracee's signal handler will fire
    // at the same point in execution as it did during recording.
    if (ptrace(PTRACE_SYSCALL, pid, 0, signum) < 0) {
        perror("inject_signal: PTRACE_SYSCALL");
        return -1;
    }
    return 0;
}

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ptrace.h>
#include <sys/wait.h>
#include <signal.h>
#include "echotrace_bin.h"

// Prototype from syscall_loop.c
void syscall_iteration(pid_t pid);

int wait_for_stop(pid_t pid) {
    int status;
    if (waitpid(pid, &status, 0) == -1) {
        perror("waitpid");
        return -1;
    }
    return 0;
}

pid_t trace_launch(char *const argv[]) {
    pid_t pid = fork();
    if (pid < 0) return pid;

    if (pid == 0) {
        if (ptrace(PTRACE_TRACEME, 0, NULL, NULL) < 0) exit(1);
        raise(SIGSTOP);
        if (execvp(argv[0], argv) < 0) exit(1);
    }

    if (wait_for_stop(pid) < 0) return -1;
    ptrace(PTRACE_SETOPTIONS, pid, NULL, PTRACE_O_EXITKILL);
    return pid;
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <cmd> [args...]\n", argv[0]);
        return 1;
    }

    if (init_trace_writer("trace.bin", "trace.idx") != 0) return 1;

    pid_t pid = trace_launch(&argv[1]);
    if (pid > 0) {
        syscall_iteration(pid); 
    }

    close_trace_writer();
    return 0;
}
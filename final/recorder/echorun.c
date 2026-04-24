#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/ptrace.h>
#include <sys/wait.h>
#include <signal.h>
#include "echotrace_bin.h"
#include "syscall_loop.h"

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
    //CHANGE
    const char *events_jsonl = NULL;
    int command_index = 1;

    if (argc >= 3 && strcmp(argv[1], "--events-jsonl") == 0) {
        events_jsonl = argv[2];
        command_index = 3;
    }

    if (argc < command_index + 1) {
        fprintf(stderr, "Usage: %s [--events-jsonl path] <cmd> [args...]\n", argv[0]);
        return 1;
    }
    //CHANGE

    if (init_trace_writer("trace.bin", "trace.idx") != 0) return 1;

    //CHANGE
    TelemetryWriter telemetry = {0};
    if (telemetry_open(&telemetry, events_jsonl) != 0) {
        close_trace_writer();
        return 1;
    }
    //CHANGE

    pid_t pid = trace_launch(&argv[command_index]);
    if (pid > 0) {
        syscall_iteration(pid, &telemetry); 
    }

    //CHANGE
    telemetry_close(&telemetry);
    //CHANGE
    close_trace_writer();
    return 0;
}

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ptrace.h>
#include <sys/wait.h>
#include <signal.h>
#include <time.h>
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

// Improvement 5: Run binary natively (no ptrace) and measure wall time.
// stdin/stdout/stderr are redirected to /dev/null so it doesn't hang
// waiting for interactive input. Returns elapsed nanoseconds, or -1 on error.
static long long run_native(char *const argv[]) {
    struct timespec t0, t1;
    clock_gettime(CLOCK_MONOTONIC, &t0);

    pid_t pid = fork();
    if (pid < 0) return -1;
    if (pid == 0) {
        freopen("/dev/null", "r", stdin);
        freopen("/dev/null", "w", stdout);
        freopen("/dev/null", "w", stderr);
        execvp(argv[0], argv);
        exit(1);
    }
    int status;
    waitpid(pid, &status, 0);

    clock_gettime(CLOCK_MONOTONIC, &t1);
    long long ns = (t1.tv_sec - t0.tv_sec) * 1000000000LL
                 + (t1.tv_nsec - t0.tv_nsec);
    return ns;
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <cmd> [args...]\n", argv[0]);
        return 1;
    }

    if (init_trace_writer("trace.bin", "trace.idx") != 0) return 1;

    // Improvement 5: baseline native run BEFORE the real recording
    printf("[ECHORUN] Measuring native baseline (stdin suppressed)...\n");
    long long native_ns = run_native(&argv[1]);

    // Timed recording run (real interactive run)
    struct timespec r0, r1;
    clock_gettime(CLOCK_MONOTONIC, &r0);

    pid_t pid = trace_launch(&argv[1]);
    if (pid > 0) {
        syscall_iteration(pid);
    }

    clock_gettime(CLOCK_MONOTONIC, &r1);
    long long record_ns = (r1.tv_sec - r0.tv_sec) * 1000000000LL
                        + (r1.tv_nsec - r0.tv_nsec);

    close_trace_writer();

    // Improvement 5: print overhead report
    if (native_ns > 0 && record_ns > 0) {
        double native_ms = (double)native_ns  / 1e6;
        double record_ms = (double)record_ns  / 1e6;
        double overhead  = record_ms / native_ms;

        printf("\n[ECHORUN] Recording overhead report:\n");
        printf("  Native (no ptrace) : %.3f ms\n", native_ms);
        printf("  Recorded           : %.3f ms\n", record_ms);
        printf("  Overhead           : %.2fx\n",   overhead);
        printf("  (Note: native run used /dev/null for stdin — "
               "overhead includes all user interaction time)\n");
    }

    return 0;
}
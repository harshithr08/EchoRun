// echoplay.c — EchoRun Replayer entry point
// Usage: ./echoplay <trace.bin> <trace.idx> <original_binary> [args...]
//
// This is the mirror of echorun.c.
// echorun records → produces trace.bin + trace.idx
// echoplay consumes those files → replays deterministically

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/ptrace.h>
#include <sys/wait.h>
#include "replayer.h"
#include <inttypes.h>

// Fork the tracee and pause it before exec, same as echorun.c does
static pid_t launch_tracee(char *const argv[]) {
    pid_t pid = fork();
    if (pid < 0) { perror("fork"); return -1; }

    if (pid == 0) {
        // Child: request tracing, stop, then exec
        if (ptrace(PTRACE_TRACEME, 0, NULL, NULL) < 0) {
            perror("PTRACE_TRACEME");
            exit(1);
        }
        raise(SIGSTOP);
        execvp(argv[0], argv);
        perror("execvp");
        exit(1);
    }

    // Parent: wait for child's SIGSTOP
    int status;
    waitpid(pid, &status, 0);
    return pid;
}

int main(int argc, char *argv[]) {
    if (argc < 4) {
        fprintf(stderr,
            "Usage: %s <trace.bin> <trace.idx> <binary> [args...]\n"
            "  trace.bin  — binary trace file produced by echorun\n"
            "  trace.idx  — index file produced by echorun\n"
            "  binary     — the same program that was recorded\n",
            argv[0]);
        return 1;
    }

    const char *trace_bin = argv[1];
    const char *trace_idx = argv[2];
    char **tracee_argv    = &argv[3];

    // ── Task 2.1: Open trace and create cursor ───────────────────────────
    ReplayCursor *cursor = cursor_open(trace_bin, trace_idx);
    if (!cursor) {
        fprintf(stderr, "Failed to open trace files: %s / %s\n",
                trace_bin, trace_idx);
        return 1;
    }
    printf("[ECHOPLAY] Trace loaded. Events recorded: %" PRIu64 "\n",
           cursor->reader->header.event_count);

    // ── Launch tracee ────────────────────────────────────────────────────
    pid_t pid = launch_tracee(tracee_argv);
    if (pid < 0) {
        cursor_close(cursor);
        return 1;
    }
    printf("[ECHOPLAY] Tracee PID=%d launched\n", pid);

    // ── Build replay session ─────────────────────────────────────────────
    ReplaySession session = {
        .pid      = pid,
        .cursor   = cursor,
        .cp_count = 0,
        .cp_head  = 0,
        .running  = 1,  // start in free-running mode
    };

    // Take initial checkpoint at seq 0 (before any event replayed)
    Checkpoint *initial = checkpoint_take(pid, 0);
    if (initial) session_push_checkpoint(&session, initial);

    // ── Task 2.2: Run the replay loop ────────────────────────────────────
    divergence_report_t report = {0};
    int result = replay_loop(&session, &report);

    if (result == 0) {
        printf("[ECHOPLAY] Replay completed successfully.\n");
    } else {
        fprintf(stderr, "[ECHOPLAY] Replay DIVERGED at seq=%" PRIu64
                " expected=%u actual=%u\n",
                report.seq_idx,
                report.expected_syscall,
                report.actual_syscall);
    }

    // ── Cleanup ──────────────────────────────────────────────────────────
    for (int i = 0; i < session.cp_count; i++) {
        checkpoint_free(session.checkpoints[i]);
    }
    cursor_close(cursor);
    return (result == 0) ? 0 : 1;
}

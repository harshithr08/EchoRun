// echoplay.c — EchoRun Replayer entry point
// Usage: ./echoplay <trace.bin> <trace.idx> <original_binary> [args...]
//
// This is the mirror of echorun.c.
// echorun records → produces trace.bin + trace.idx
// echoplay consumes those files → replays deterministically

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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
    //CHANGE
    const char *events_jsonl = NULL;
    const char *repl_script  = NULL;
    int step_mode = 0;
    int argi = 1;

    while (argi < argc) {
        if (strcmp(argv[argi], "--events-jsonl") == 0 && argi + 1 < argc) {
            events_jsonl = argv[argi + 1];
            argi += 2;
        } else if (strcmp(argv[argi], "--step") == 0) {
            step_mode = 1;
            argi++;
        } else if (strcmp(argv[argi], "--repl-script") == 0 && argi + 1 < argc) {
            repl_script = argv[argi + 1];
            argi += 2;
        } else {
            break;
        }
    }

    if (argc - argi < 3) {
        fprintf(stderr,
            "Usage: %s [--events-jsonl path] [--step] [--repl-script path] <trace.bin> <trace.idx> <binary> [args...]\n"
            "  trace.bin  — binary trace file produced by echorun\n"
            "  trace.idx  — index file produced by echorun\n"
            "  binary     — the same program that was recorded\n",
            argv[0]);
        return 1;
    }
    //CHANGE

    const char *trace_bin = argv[argi];
    const char *trace_idx = argv[argi + 1];
    char **tracee_argv    = &argv[argi + 2];

    // ── Task 2.1: Open trace and create cursor ───────────────────────────
    ReplayCursor *cursor = cursor_open(trace_bin, trace_idx);
    if (!cursor) {
        fprintf(stderr, "Failed to open trace files: %s / %s\n",
                trace_bin, trace_idx);
        return 1;
    }
    printf("[ECHOPLAY] Trace loaded. Events recorded: %" PRIu64 "\n",
           cursor->reader->header.event_count);

    //CHANGE
    TelemetryWriter telemetry = {0};
    if (telemetry_open(&telemetry, events_jsonl) != 0) {
        cursor_close(cursor);
        return 1;
    }

    FILE *repl_input = NULL;
    if (repl_script) {
        repl_input = fopen(repl_script, "r");
        if (!repl_input) {
            perror("fopen repl_script");
            telemetry_close(&telemetry);
            cursor_close(cursor);
            return 1;
        }
    }
    //CHANGE

    // ── Launch tracee ────────────────────────────────────────────────────
    pid_t pid = launch_tracee(tracee_argv);
    if (pid < 0) {
        //CHANGE
        if (repl_input) fclose(repl_input);
        telemetry_close(&telemetry);
        //CHANGE
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
        .running  = step_mode ? 2 : 1,
        //CHANGE
        .should_quit = 0,
        .repl_input = repl_input,
        .telemetry = &telemetry,
        //CHANGE
    };

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
    //CHANGE
    if (session.repl_input) fclose(session.repl_input);
    telemetry_close(&telemetry);
    //CHANGE
    cursor_close(cursor);
    return (result == 0) ? 0 : 1;
}

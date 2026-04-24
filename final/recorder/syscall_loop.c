#include <sys/ptrace.h>
#include <sys/user.h>
#include <sys/wait.h>
#include <stdio.h>
#include "syscall_loop.h"
#include "sys_cat_info.h"
#include "ND_syscall_handler.h"
#include "echotrace_bin.h"
#include <signal.h>

/**
 * Handle syscall entry: Print info and log state.
 */
static void handle_entry(pid_t pid, struct user_regs_struct *regs) {
    // orig_rax holds the syscall number
    printf("[ENTRY] PID: %d, Syscall: %lld\n", pid, regs->orig_rax);
}

/**
 * Handle syscall exit: Check for non-determinism and log return values.
 */
//CHANGE
static void handle_exit(pid_t pid,
                        struct user_regs_struct *regs,
                        uint64_t *seq_idx,
                        TelemetryWriter *telemetry) {
    // rax holds the return value on exit
    printf("[EXIT]  PID: %d, Return: %lld\n", pid, regs->rax);
    
    int syscall_nr = (int)regs->orig_rax;
    SysCallCategory cat = syscall_classify(syscall_nr);
    uint32_t payload_size = 0;
    int has_trace_seq = 0;
    uint64_t trace_seq = 0;
    
    // Use the classifier to determine if we need to record data
    if (cat == NON_DET) {
        trace_seq = *seq_idx;
        has_trace_seq = handle_non_deterministic_exit(pid, syscall_nr, trace_seq, &payload_size);
        (*seq_idx)++;
    }

    telemetry_emit_syscall(telemetry,
                           pid,
                           syscall_nr,
                           cat,
                           (int64_t)regs->rax,
                           1,
                           payload_size,
                           has_trace_seq,
                           trace_seq,
                           0,
                           0,
                           0);
}
//CHANGE

/**
 * The core loop that monitors the tracee process.
 */
//CHANGE
void syscall_iteration(pid_t pid, TelemetryWriter *telemetry) {
    int status;
    int is_entry = 1; 
    uint64_t seq_idx = 0;

    // Task 1.11: Start hardware perf counter for this tracee
    setup_perf_counter(pid);

    ptrace(PTRACE_SETOPTIONS, pid, 0, 
           PTRACE_O_TRACESYSGOOD | PTRACE_O_TRACEFORK | 
           PTRACE_O_TRACEVFORK | PTRACE_O_TRACECLONE | 
           PTRACE_O_TRACEEXEC | PTRACE_O_TRACEEXIT);

    while (1) {
        if (ptrace(PTRACE_SYSCALL, pid, 0, 0) < 0) break;
        waitpid(pid, &status, 0);

        if (WIFEXITED(status)) {
            // Task 1.8: Process exit event
            write_trace_event(PROC_EVENT, 0, seq_idx++, WEXITSTATUS(status), 0, NULL); 
            telemetry_emit_process(telemetry, pid, 0, WEXITSTATUS(status), 1, 1, seq_idx - 1);
            break;
        }

        int sig = WSTOPSIG(status);

        // Task 1.7: Signal Recording
        if (WIFSTOPPED(status) && sig != (SIGTRAP | 0x80) && sig != SIGTRAP) {
            // Record the signal number and sequence position
            write_trace_event(SIGNAL_EVENT, sig, seq_idx, 0, 0, NULL);
            telemetry_emit_signal(telemetry, pid, sig, 1, seq_idx);
            seq_idx++;
            /* BUG FIX: forward the signal to the tracee (was passing 0 which
               swallowed it). Pass sig as 4th arg to PTRACE_SYSCALL. */
            ptrace(PTRACE_SYSCALL, pid, 0, sig);
            waitpid(pid, &status, 0);
            continue; 
        }

        // Task 1.8: Process Events (Fork/Exec)
        if (WIFSTOPPED(status) && sig == SIGTRAP && (status >> 16) != 0) {
            int event = status >> 16;
            write_trace_event(PROC_EVENT, event, seq_idx, 0, 0, NULL);
            telemetry_emit_process(telemetry, pid, event, 0, 0, 1, seq_idx);
            seq_idx++;
            continue;
        }

        // Existing Syscall Handling
        if (WIFSTOPPED(status) && (sig == (SIGTRAP | 0x80))) {
            struct user_regs_struct regs;
            ptrace(PTRACE_GETREGS, pid, 0, &regs);
            if (is_entry) handle_entry(pid, &regs);
            else handle_exit(pid, &regs, &seq_idx, telemetry);
            is_entry = !is_entry;
        }
    }
}
//CHANGE

// Task 2.2: Main replay event loop

#include "replay_loop.h"
#include "syscall_inject.h"
#include "signal_inject.h"
#include "checkpoint.h"
#include "repl.h"
#include "echotrace_bin.h"
#include "sys_cat_info.h"

#include <sys/ptrace.h>
#include <sys/user.h>
#include <sys/wait.h>
#include <signal.h>
#include <stdio.h>
#include <inttypes.h>
#include <stdlib.h>

int replay_loop(ReplaySession *s, divergence_report_t *report_out) {
    int status;
    int is_entry          = 1;
    int suppress_pending  = 0;
    int is_ndet_pending   = 0;   // was this syscall NON_DET at entry?
    int64_t  saved_retval = 0;
    uint64_t saved_bufaddr = 0;
    uint8_t *saved_data   = NULL;
    uint32_t saved_datalen = 0;
    uint64_t events_since_checkpoint = 0;

    // Mirror the recorder's options so we see the same PROC_EVENTs
    ptrace(PTRACE_SETOPTIONS, s->pid, 0,
           PTRACE_O_TRACESYSGOOD | PTRACE_O_TRACEEXEC | PTRACE_O_TRACEEXIT);

    while (1) {
        if (ptrace(PTRACE_SYSCALL, s->pid, 0, 0) < 0) break;
        waitpid(s->pid, &status, 0);

        if (WIFEXITED(status)) {
            printf("[REPLAY] Tracee exited with code %d\n", WEXITSTATUS(status));
            return 0;
        }

        if (!WIFSTOPPED(status)) continue;

        int sig = WSTOPSIG(status);

        // ── Ptrace event stop (exec/exit) ─────────────────────────────────
        // Recorder wrote PROC_EVENT here and incremented seq_idx.
        // We must do the same so our counter stays in sync.
        if (sig == SIGTRAP && (status >> 16) != 0) {
            struct EventRecorder *ev = cursor_peek(s->cursor);
            if (ev != NULL && s->cursor->seq_counter == ev->seq_idx
                           && ev->event_type == PROC_EVENT) {
                cursor_consume(s->cursor);
                if (s->cursor->current_data) {
                    free(s->cursor->current_data);
                    s->cursor->current_data = NULL;
                }
            }
            s->cursor->seq_counter++;
            continue;
        }

        // ── Signal stop ───────────────────────────────────────────────────
        if (sig != (SIGTRAP | 0x80) && sig != SIGTRAP) {
            struct EventRecorder *ev = cursor_peek(s->cursor);
            if (ev != NULL && s->cursor->seq_counter == ev->seq_idx
                           && ev->event_type == SIGNAL_EVENT) {
                cursor_consume(s->cursor);
                if (s->cursor->current_data) {
                    free(s->cursor->current_data);
                    s->cursor->current_data = NULL;
                }
            }
            s->cursor->seq_counter++;
            inject_signal(s->pid, sig);
            continue;
        }

        // ── Syscall stop ──────────────────────────────────────────────────
        struct user_regs_struct regs;
        if (ptrace(PTRACE_GETREGS, s->pid, 0, &regs) < 0) continue;

        if (is_entry) {
            // ── ENTRY STOP ────────────────────────────────────────────────
            uint32_t incoming_syscall = (uint32_t)regs.orig_rax;
            SysCallCategory cat = syscall_classify((long)incoming_syscall);

            is_ndet_pending  = (cat == NON_DET);
            suppress_pending = 0;

            if (cat == NON_DET) {
                // Check if this NON_DET syscall is the one in the trace
                struct EventRecorder *ev = cursor_peek(s->cursor);

                if (ev != NULL && s->cursor->seq_counter == ev->seq_idx
                               && ev->event_type == SYSCALL_EVENT) {
                    // Divergence check
                    if (check_divergence(ev->syscall_no, incoming_syscall,
                                         ev->seq_idx, report_out)) {
                        print_divergence(report_out);
                        return -1;
                    }

                    // Consume event and set up injection
                    cursor_consume(s->cursor);
                    ev = &s->cursor->current_ev;

                    suppress_syscall(s->pid);
                    suppress_pending = 1;
                    saved_retval  = ev->retval;
                    saved_data    = s->cursor->current_data;
                    saved_datalen = ev->data_len;
                    saved_bufaddr = 0;
                    s->cursor->current_data = NULL;

                    if (ev->data_len > 0) {
                        if (ev->syscall_no == 0 /* read */ ||
                            ev->syscall_no == 45 /* recvfrom */)
                            saved_bufaddr = regs.rsi;
                        else if (ev->syscall_no == 318 /* getrandom */)
                            saved_bufaddr = regs.rdi;
                    }

                    printf("[REPLAY] Entry: suppressed syscall=%u seq=%" PRIu64 "\n",
                           incoming_syscall, ev->seq_idx);

                    // Auto-checkpoint
                    events_since_checkpoint++;
                    if (events_since_checkpoint >= CHECKPOINT_INTERVAL) {
                        Checkpoint *cp = checkpoint_take(s->pid, ev->seq_idx);
                        if (cp) session_push_checkpoint(s, cp);
                        events_since_checkpoint = 0;
                    }
                }
                // else: NON_DET but not in trace at this position (mmap/brk/etc
                // that returned an address — recorder consumed the seq slot but
                // didn't write an event). Let the real syscall run; we increment
                // seq_counter at the exit stop below.
            }

            if (s->running == 2) {   // step mode
                s->running = 0;
                repl_run(s);
            }

        } else {
            // ── EXIT STOP ─────────────────────────────────────────────────
            if (suppress_pending) {
                inject_retval(s->pid, saved_retval);

                if (saved_datalen > 0 && saved_data != NULL && saved_bufaddr != 0)
                    inject_memory(s->pid, saved_bufaddr, saved_data, saved_datalen);

                if (saved_data) { free(saved_data); saved_data = NULL; }
                saved_datalen    = 0;
                saved_bufaddr    = 0;
                suppress_pending = 0;

                printf("[REPLAY] Exit: injected retval=%" PRId64 "\n", saved_retval);
            }

            // Mirror recorder: seq_idx increments on every NON_DET syscall exit
            if (is_ndet_pending) {
                s->cursor->seq_counter++;
                is_ndet_pending = 0;
            }
        }

        is_entry = !is_entry;
    }

    return 0;
}
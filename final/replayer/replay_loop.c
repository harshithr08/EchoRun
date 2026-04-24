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

//CHANGE
static void release_current_event(ReplayCursor *cursor) {
    if (!cursor || !cursor->current_data) return;
    free(cursor->current_data);
    cursor->current_data = NULL;
}
//CHANGE

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
    //CHANGE
    uint32_t pending_syscall = 0;
    SysCallCategory pending_cat = UNKNOWN;
    int pending_has_trace_seq = 0;
    uint64_t pending_trace_seq = 0;
    uint32_t pending_payload_size = 0;
    //CHANGE

    // Mirror the recorder's options so we see the same PROC_EVENTs
    ptrace(PTRACE_SETOPTIONS, s->pid, 0,
           PTRACE_O_TRACESYSGOOD | PTRACE_O_TRACEEXEC | PTRACE_O_TRACEEXIT);

    while (1) {
        if (ptrace(PTRACE_SYSCALL, s->pid, 0, 0) < 0) break;
        waitpid(s->pid, &status, 0);

        if (WIFEXITED(status)) {
            //CHANGE
            int has_trace_seq = 0;
            uint64_t trace_seq = 0;
            struct EventRecorder *ev = cursor_peek(s->cursor);
            if (ev != NULL && s->cursor->seq_counter == ev->seq_idx
                           && ev->event_type == PROC_EVENT) {
                cursor_consume(s->cursor);
                has_trace_seq = 1;
                trace_seq = ev->seq_idx;
                release_current_event(s->cursor);
                s->cursor->seq_counter++;
            }

            telemetry_emit_process(s->telemetry,
                                   s->pid,
                                   0,
                                   WEXITSTATUS(status),
                                   1,
                                   has_trace_seq,
                                   trace_seq);
            //CHANGE
            printf("[REPLAY] Tracee exited with code %d\n", WEXITSTATUS(status));
            return 0;
        }

        if (!WIFSTOPPED(status)) continue;

        int sig = WSTOPSIG(status);

        // ── Ptrace event stop (exec/exit) ─────────────────────────────────
        // Recorder wrote PROC_EVENT here and incremented seq_idx.
        // We must do the same so our counter stays in sync.
        if (sig == SIGTRAP && (status >> 16) != 0) {
            //CHANGE
            int proc_code = status >> 16;
            int has_trace_seq = 0;
            uint64_t trace_seq = 0;
            struct EventRecorder *ev = cursor_peek(s->cursor);
            if (ev != NULL && s->cursor->seq_counter == ev->seq_idx
                           && ev->event_type == PROC_EVENT) {
                cursor_consume(s->cursor);
                has_trace_seq = 1;
                trace_seq = ev->seq_idx;
                release_current_event(s->cursor);
            }
            telemetry_emit_process(s->telemetry, s->pid, proc_code, 0, 0, has_trace_seq, trace_seq);
            s->cursor->seq_counter++;
            //CHANGE
            if (proc_code == PTRACE_EVENT_EXEC && s->cp_count == 0) {
                Checkpoint *initial = checkpoint_take(s->pid, s->cursor->seq_counter);
                if (initial) session_push_checkpoint(s, initial);
            }
            //CHANGE
            continue;
        }

        // ── Signal stop ───────────────────────────────────────────────────
        if (sig != (SIGTRAP | 0x80) && sig != SIGTRAP) {
            //CHANGE
            int has_trace_seq = 0;
            uint64_t trace_seq = 0;
            struct EventRecorder *ev = cursor_peek(s->cursor);
            if (ev != NULL && s->cursor->seq_counter == ev->seq_idx
                           && ev->event_type == SIGNAL_EVENT) {
                cursor_consume(s->cursor);
                has_trace_seq = 1;
                trace_seq = ev->seq_idx;
                release_current_event(s->cursor);
            }
            telemetry_emit_signal(s->telemetry, s->pid, sig, has_trace_seq, trace_seq);
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
            //CHANGE
            pending_syscall = incoming_syscall;
            pending_cat = cat;
            pending_has_trace_seq = 0;
            pending_trace_seq = 0;
            pending_payload_size = 0;
            //CHANGE
            suppress_pending = 0;

            if (cat == NON_DET) {
                // Check if this NON_DET syscall is the one in the trace
                struct EventRecorder *ev = cursor_peek(s->cursor);

                if (ev != NULL && s->cursor->seq_counter == ev->seq_idx
                               && ev->event_type == SYSCALL_EVENT) {
                    // Divergence check
                    if (check_divergence(ev->syscall_no, incoming_syscall,
                                         ev->seq_idx, report_out)) {
                        //CHANGE
                        telemetry_emit_syscall(s->telemetry,
                                               s->pid,
                                               incoming_syscall,
                                               cat,
                                               0,
                                               0,
                                               ev->data_len,
                                               1,
                                               ev->seq_idx,
                                               1,
                                               ev->syscall_no,
                                               incoming_syscall);
                        //CHANGE
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
                    //CHANGE
                    pending_has_trace_seq = 1;
                    pending_trace_seq = ev->seq_idx;
                    pending_payload_size = ev->data_len;
                    //CHANGE

                    if (ev->data_len > 0) {
                        //CHANGE
                        if (ev->syscall_no == 0 /* read */ ||
                            ev->syscall_no == 45 /* recvfrom */ ||
                            ev->syscall_no == 4 /* stat */ ||
                            ev->syscall_no == 5 /* fstat */ ||
                            ev->syscall_no == 228 /* clock_gettime */)
                            saved_bufaddr = regs.rsi;
                        else if (ev->syscall_no == 96 /* gettimeofday */ ||
                                 ev->syscall_no == 235 /* uname */ ||
                                 ev->syscall_no == 318 /* getrandom */)
                            saved_bufaddr = regs.rdi;
                        //CHANGE
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
                //CHANGE
                if (s->should_quit) return 0;
                //CHANGE
            }

        } else {
            // ── EXIT STOP ─────────────────────────────────────────────────
            //CHANGE
            int was_suppressed = suppress_pending;
            int64_t final_retval = (int64_t)regs.rax;
            //CHANGE
            if (suppress_pending) {
                inject_retval(s->pid, saved_retval);

                if (saved_datalen > 0 && saved_data != NULL && saved_bufaddr != 0)
                    inject_memory(s->pid, saved_bufaddr, saved_data, saved_datalen);

                if (saved_data) { free(saved_data); saved_data = NULL; }
                saved_datalen    = 0;
                saved_bufaddr    = 0;
                suppress_pending = 0;
                //CHANGE
                final_retval = saved_retval;
                //CHANGE

                printf("[REPLAY] Exit: injected retval=%" PRId64 "\n", saved_retval);
            }

            //CHANGE
            telemetry_emit_syscall(s->telemetry,
                                   s->pid,
                                   pending_syscall,
                                   pending_cat,
                                   final_retval,
                                   1,
                                   pending_payload_size,
                                   pending_has_trace_seq,
                                   pending_trace_seq,
                                   0,
                                   0,
                                   0);
            if (was_suppressed) pending_payload_size = 0;
            //CHANGE

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

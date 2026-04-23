#include "time_travel.h"
#include "syscall_inject.h"
#include <sys/ptrace.h>
#include <stdio.h>
#include <errno.h>

void session_push_checkpoint(ReplaySession *s, Checkpoint *cp) {
    // Ring buffer: overwrite oldest when full
    if (s->cp_count < MAX_CHECKPOINTS) {
        s->checkpoints[s->cp_count++] = cp;
    } else {
        checkpoint_free(s->checkpoints[s->cp_head]);
        s->checkpoints[s->cp_head] = cp;
        s->cp_head = (s->cp_head + 1) % MAX_CHECKPOINTS;
    }
}

Checkpoint *session_nearest_checkpoint(ReplaySession *s, uint64_t target_seq) {
    Checkpoint *best = NULL;
    for (int i = 0; i < s->cp_count; i++) {
        Checkpoint *c = s->checkpoints[i];
        if (!c) continue;
        if (c->seq_idx <= target_seq) {
            if (!best || c->seq_idx > best->seq_idx) best = c;
        }
    }
    return best;
}

void cmd_continue(ReplaySession *s) {
    // Just flip the flag; replay_loop does the actual work
    s->running = 1;
    printf("[TT] Continuing replay...\n");
}

void cmd_step(ReplaySession *s) {
    // run for exactly one event then stop
    s->running = 2; // special value: step mode
    printf("[TT] Stepping one event...\n");
}

int cmd_goto(ReplaySession *s, uint64_t target_seq) {
    // Find the closest checkpoint before target_seq
    Checkpoint *cp = session_nearest_checkpoint(s, target_seq);
    if (!cp) {
        fprintf(stderr, "[TT] No checkpoint available before seq %lu\n", target_seq);
        return -1;
    }

    printf("[TT] Restoring checkpoint at seq=%lu, then seeking to seq=%lu\n",
           cp->seq_idx, target_seq);

    // Restore process state to checkpoint
    if (checkpoint_restore(s->pid, cp) < 0) return -1;

    // Seek the trace cursor to the target position
    if (cursor_seek(s->cursor, target_seq) < 0) {
        fprintf(stderr, "[TT] seq_idx %lu not found in trace\n", target_seq);
        return -1;
    }

    s->running = 0;
    printf("[TT] At seq=%lu. Use 'step' or 'continue'.\n", target_seq);
    return 0;
}

void cmd_peek(ReplaySession *s, unsigned long long addr) {
    errno = 0;
    long val = ptrace(PTRACE_PEEKDATA, s->pid, addr, 0);
    if (val == -1 && errno != 0) {
        perror("[TT] peek failed");
    } else {
        printf("[TT] peek 0x%llx => 0x%016lx (%ld)\n", addr, (unsigned long)val, val);
    }
}

void cmd_poke(ReplaySession *s, unsigned long long addr, long val) {
    if (ptrace(PTRACE_POKEDATA, s->pid, addr, val) < 0) {
        perror("[TT] poke failed");
    } else {
        printf("[TT] poke 0x%llx <= 0x%016lx\n", addr, (unsigned long)val);
    }
}

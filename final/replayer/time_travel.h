#ifndef TIME_TRAVEL_H
#define TIME_TRAVEL_H

#include "replay_cursor.h"
#include "checkpoint.h"
#include <sys/types.h>
#include <stdint.h>

#define MAX_CHECKPOINTS 64  // how many auto-checkpoints to keep

// All live state the time-travel commands need to operate on
typedef struct {
    pid_t          pid;            // tracee PID (paused at a syscall stop)
    ReplayCursor  *cursor;         // current trace position
    Checkpoint    *checkpoints[MAX_CHECKPOINTS]; // ring of auto-checkpoints (Task 2.9)
    int            cp_count;       // how many are stored
    int            cp_head;        // ring buffer head
    int            running;        // 0 = paused (REPL active), 1 = free running
} ReplaySession;

// Task 2.8 commands ────────────────────────────────────────────────────────

// 'continue' — resume replay until next divergence or trace end
void cmd_continue(ReplaySession *s);

// 'step' — advance exactly one NON_DET event, then pause
void cmd_step(ReplaySession *s);

// 'goto <seq_idx>' — seek to that trace position, restore nearest checkpoint
int  cmd_goto(ReplaySession *s, uint64_t target_seq);

// 'peek <addr>' — read 8 bytes from tracee memory and print them
void cmd_peek(ReplaySession *s, unsigned long long addr);

// 'poke <addr> <val>' — write a 64-bit value into tracee memory
void cmd_poke(ReplaySession *s, unsigned long long addr, long val);

// Save a checkpoint into the session's ring buffer (called by replay_loop)
void session_push_checkpoint(ReplaySession *s, Checkpoint *cp);

// Find the closest checkpoint whose seq_idx <= target
Checkpoint *session_nearest_checkpoint(ReplaySession *s, uint64_t target_seq);

#endif

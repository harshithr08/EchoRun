#ifndef REPLAY_LOOP_H
#define REPLAY_LOOP_H

#include "time_travel.h"   // ReplaySession
#include "divergence.h"    // divergence_report_t

// Task 2.2: The core replay loop.
// Runs the tracee under ptrace, intercepts every syscall stop,
// matches against the trace, suppresses real syscalls, injects
// saved return values and memory, detects divergence.
//
// Returns 0 on clean replay completion, -1 on divergence or error.
int replay_loop(ReplaySession *s, divergence_report_t *report_out);

#endif

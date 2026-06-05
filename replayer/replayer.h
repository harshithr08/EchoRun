#ifndef REPLAYER_H
#define REPLAYER_H

// Public interface for the EchoRun replayer module.
// Include this in echoplay.c and any integration tests.

#include "replay_cursor.h"   // Task 2.1 — cursor struct + API
#include "replay_loop.h"     // Task 2.2 — main replay loop
#include "syscall_inject.h"  // Tasks 2.3+2.4 — suppress + inject
#include "signal_inject.h"   // Task 2.5 — signal delivery
#include "divergence.h"      // Task 2.6 — divergence_report_t
#include "checkpoint.h"      // Tasks 2.7+2.9 — snapshot/restore
#include "time_travel.h"     // Task 2.8 — REPL commands
#include "repl.h"            // REPL stdin loop

#endif

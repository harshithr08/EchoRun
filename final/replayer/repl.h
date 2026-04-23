#ifndef REPL_H
#define REPL_H

#include "time_travel.h"

// Starts the interactive REPL. Blocks reading stdin.
// Commands: continue | step | goto <seq> | peek <addr> | poke <addr> <val> | quit
void repl_run(ReplaySession *s);

#endif

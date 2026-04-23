#ifndef SYSCALL_LOOP_H
#define SYSCALL_LOOP_H

#include <sys/types.h>

/**
 * Starts the syscall monitoring loop for the given process ID.
 * Differentiates between entry and exit stops to record non-deterministic data.
 */
void syscall_iteration(pid_t pid);

#endif
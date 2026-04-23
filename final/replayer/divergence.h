#ifndef DIVERGENCE_H
#define DIVERGENCE_H

#include <stdint.h>

// Struct consumed by Person 3 (Visualizer) to mark red tick on timeline
typedef struct {
    uint64_t seq_idx;           // which event position diverged
    uint32_t expected_syscall;  // what the trace said should happen
    uint32_t actual_syscall;    // what the tracee actually did
} divergence_report_t;

// Returns 1 if diverged (fills report), 0 if match
int check_divergence(uint32_t expected, uint32_t actual,
                     uint64_t seq_idx, divergence_report_t *report);

// Print the divergence report to stderr
void print_divergence(const divergence_report_t *report);

#endif

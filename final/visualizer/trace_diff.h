#ifndef TRACE_DIFF_H
#define TRACE_DIFF_H

#include "echotrace_bin.h"
#include <stdint.h>

// One divergence record found by the diff tool (Task 3.9)
typedef struct {
    uint64_t seq_idx;
    uint8_t  event_type;
    uint32_t a_syscall;
    int64_t  a_retval;
    uint32_t b_syscall;
    int64_t  b_retval;
    int      found;  // 1 if a divergence was found
} DiffRecord;

// Task 3.8+3.9: Walk two traces in lockstep and find first divergence.
// Returns 0 if identical, 1 if diverged (fills record), -1 on error.
int trace_diff(const char *trace_a_bin, const char *trace_a_idx,
               const char *trace_b_bin, const char *trace_b_idx,
               DiffRecord *record);

// Task 3.10: Print the diff record in human-readable form
void print_diff_record(const DiffRecord *rec);

#endif

#include "divergence.h"
#include <stdio.h>
#include <inttypes.h>

int check_divergence(uint32_t expected, uint32_t actual,
                     uint64_t seq_idx, divergence_report_t *report) {
    if (expected == actual) return 0;

    report->seq_idx          = seq_idx;
    report->expected_syscall = expected;
    report->actual_syscall   = actual;
    return 1;
}

void print_divergence(const divergence_report_t *report) {
    fprintf(stderr,
        "[DIVERGENCE] seq_idx=%" PRIu64
        " expected_syscall=%u actual_syscall=%u\n",
        report->seq_idx,
        report->expected_syscall,
        report->actual_syscall);
}

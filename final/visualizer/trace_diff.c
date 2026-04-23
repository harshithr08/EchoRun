#include "trace_diff.h"
#include "trace_reader.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <inttypes.h>

// Simple FNV-1a hash over a byte buffer (Task 3.9 payload comparison)
static uint64_t fnv1a(const uint8_t *data, uint32_t len) {
    uint64_t hash = 0xcbf29ce484222325ULL;
    for (uint32_t i = 0; i < len; i++) {
        hash ^= data[i];
        hash *= 0x100000001b3ULL;
    }
    return hash;
}

int trace_diff(const char *trace_a_bin, const char *trace_a_idx,
               const char *trace_b_bin, const char *trace_b_idx,
               DiffRecord *record) {
    record->found = 0;

    TraceReader *ta = open_trace(trace_a_bin, trace_a_idx);
    TraceReader *tb = open_trace(trace_b_bin, trace_b_idx);
    if (!ta || !tb) {
        fprintf(stderr, "[DIFF] Failed to open trace files\n");
        if (ta) close_trace(ta);
        if (tb) close_trace(tb);
        return -1;
    }

    struct EventRecorder ea, eb;
    uint8_t *da = NULL, *db = NULL;

    while (1) {
        int ra = read_event(ta, &ea, &da);
        int rb = read_event(tb, &eb, &db);

        // Both exhausted — traces are identical
        if (ra != 0 && rb != 0) break;

        // One ended before the other — length divergence
        if (ra != rb) {
            record->found      = 1;
            record->seq_idx    = (ra == 0) ? ea.seq_idx : eb.seq_idx;
            record->event_type = (ra == 0) ? ea.event_type : eb.event_type;
            record->a_syscall  = (ra == 0) ? ea.syscall_no : 0;
            record->a_retval   = (ra == 0) ? ea.retval     : 0;
            record->b_syscall  = (rb == 0) ? eb.syscall_no : 0;
            record->b_retval   = (rb == 0) ? eb.retval     : 0;
            if (da) { free(da); da = NULL; }
            if (db) { free(db); db = NULL; }
            break;
        }

        // Task 3.9: Compare syscall_no, retval, data hash
        int diverged = 0;
        if (ea.syscall_no != eb.syscall_no) diverged = 1;
        if (ea.retval     != eb.retval)     diverged = 1;
        if (ea.data_len > 0 || eb.data_len > 0) {
            uint64_t ha = fnv1a(da ? da : (uint8_t*)"", ea.data_len);
            uint64_t hb = fnv1a(db ? db : (uint8_t*)"", eb.data_len);
            if (ha != hb) diverged = 1;
        }

        if (diverged) {
            record->found      = 1;
            record->seq_idx    = ea.seq_idx;
            record->event_type = ea.event_type;
            record->a_syscall  = ea.syscall_no;
            record->a_retval   = ea.retval;
            record->b_syscall  = eb.syscall_no;
            record->b_retval   = eb.retval;
            if (da) { free(da); da = NULL; }
            if (db) { free(db); db = NULL; }
            break;
        }

        if (da) { free(da); da = NULL; }
        if (db) { free(db); db = NULL; }
    }

    close_trace(ta);
    close_trace(tb);
    return record->found ? 1 : 0;
}

// Improvement 2: Full syscall name table — matches all 31 entries in sys_cat_info.c
// plus additional common syscalls for complete coverage.
static const char *sname(uint32_t no) {
    switch (no) {
        case 0:   return "read";
        case 1:   return "write";
        case 2:   return "open";
        case 3:   return "close";
        case 4:   return "stat";
        case 5:   return "fstat";
        case 6:   return "lstat";
        case 8:   return "lseek";
        case 9:   return "mmap";
        case 10:  return "mprotect";
        case 11:  return "munmap";
        case 12:  return "brk";
        case 13:  return "rt_sigaction";
        case 14:  return "rt_sigprocmask";
        case 17:  return "pread64";
        case 18:  return "pwrite64";
        case 32:  return "dup";
        case 33:  return "dup2";
        case 39:  return "getpid";
        case 41:  return "socket";
        case 42:  return "connect";
        case 43:  return "accept";
        case 44:  return "sendto";
        case 45:  return "recvfrom";
        case 56:  return "clone";
        case 57:  return "fork";
        case 59:  return "execve";
        case 60:  return "exit";
        case 61:  return "wait4";
        case 62:  return "kill";
        case 72:  return "fcntl";
        case 96:  return "gettimeofday";
        case 102: return "getuid";
        case 228: return "clock_gettime";
        case 231: return "exit_group";
        case 235: return "uname";
        case 318: return "getrandom";
        default:  return "unknown";
    }
}

void print_diff_record(const DiffRecord *rec) {
    if (!rec->found) {
        printf("[DIFF] Traces are IDENTICAL.\n");
        return;
    }
    printf("[DIFF] First divergence at seq_idx=%" PRIu64 "\n", rec->seq_idx);
    printf("  Expected : %s() -> %" PRId64 "\n",
           sname(rec->a_syscall), rec->a_retval);
    printf("  Got      : %s() -> %" PRId64 "\n",
           sname(rec->b_syscall), rec->b_retval);
}
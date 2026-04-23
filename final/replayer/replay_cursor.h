#ifndef REPLAY_CURSOR_H
#define REPLAY_CURSOR_H

#include "trace_reader.h"   // open_trace, read_event, seek_to_seq_idx, close_trace
#include "echotrace_bin.h"  // EventRecorder, TraceHeader, SYSCALL_EVENT etc.
#include <stdint.h>

typedef struct {
    TraceReader        *reader;
    struct EventRecorder current_ev;
    uint8_t            *current_data;

    // Lookahead: next event pre-loaded so we can check seq_idx before consuming
    struct EventRecorder peek_ev;
    uint8_t            *peek_data;
    int                 has_peek;

    uint64_t            position;
    int                 exhausted;

    // Counts every ptrace syscall-entry stop (0-indexed).
    // Compared against peek_ev.seq_idx to decide whether to act.
    uint64_t            seq_counter;
} ReplayCursor;

ReplayCursor *cursor_open(const char *trace_path, const char *idx_path);
int           cursor_next(ReplayCursor *cursor);
struct EventRecorder *cursor_peek(ReplayCursor *cursor);
int           cursor_consume(ReplayCursor *cursor);
int           cursor_seek(ReplayCursor *cursor, uint64_t target_seq);
void          cursor_close(ReplayCursor *cursor);

#endif
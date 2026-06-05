#ifndef TRACE_READER_H
#define TRACE_READER_H

#include "echotrace_bin.h"

typedef struct {
    FILE *fp;
    FILE *idx_fp;
    struct TraceHeader header;
} TraceReader;

TraceReader* open_trace(const char *path, const char *idx_path);
int read_event(TraceReader *tr, struct EventRecorder *ev, uint8_t **data);
int seek_to_seq_idx(TraceReader *tr, uint64_t seq_idx);
void close_trace(TraceReader *tr);

#endif
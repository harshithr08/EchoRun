#include "replay_cursor.h"
#include <stdlib.h>
#include <stdio.h>

ReplayCursor *cursor_open(const char *trace_path, const char *idx_path) {
    ReplayCursor *c = calloc(1, sizeof(ReplayCursor));
    if (!c) return NULL;

    c->reader = open_trace(trace_path, idx_path);
    if (!c->reader) { free(c); return NULL; }

    c->position    = 0;
    c->exhausted   = 0;
    c->seq_counter = 0;
    c->has_peek    = 0;
    c->peek_data   = NULL;
    c->current_data = NULL;
    return c;
}

// Peek at next event without consuming. Returns &peek_ev or NULL if exhausted.
struct EventRecorder *cursor_peek(ReplayCursor *cursor) {
    if (cursor->exhausted) return NULL;
    if (cursor->has_peek)  return &cursor->peek_ev;

    if (cursor->peek_data) { free(cursor->peek_data); cursor->peek_data = NULL; }

    int ret = read_event(cursor->reader, &cursor->peek_ev, &cursor->peek_data);
    if (ret != 0) { cursor->exhausted = 1; return NULL; }

    cursor->has_peek = 1;
    return &cursor->peek_ev;
}

// Consume the peeked event — moves peek -> current.
int cursor_consume(ReplayCursor *cursor) {
    if (!cursor->has_peek) return -1;

    if (cursor->current_data) { free(cursor->current_data); cursor->current_data = NULL; }

    cursor->current_ev   = cursor->peek_ev;
    cursor->current_data = cursor->peek_data;
    cursor->peek_data    = NULL;
    cursor->has_peek     = 0;
    cursor->position++;
    return 0;
}

// Legacy cursor_next: peek then consume in one shot.
int cursor_next(ReplayCursor *cursor) {
    if (!cursor_peek(cursor)) return -1;
    return cursor_consume(cursor);
}

// Seek to a specific seq_idx via linear scan of the index file.
int cursor_seek(ReplayCursor *cursor, uint64_t target_seq) {
    FILE *idx = cursor->reader->idx_fp;
    FILE *fp  = cursor->reader->fp;

    rewind(idx);
    struct IndexRecord rec;
    while (fread(&rec, sizeof(struct IndexRecord), 1, idx) == 1) {
        if (rec.seq_idx == target_seq) {
            if (fseek(fp, (long)rec.byte_offset, SEEK_SET) != 0) return -1;
            // Invalidate peek cache since we jumped
            if (cursor->peek_data) { free(cursor->peek_data); cursor->peek_data = NULL; }
            cursor->has_peek  = 0;
            cursor->exhausted = 0;
            return 0;
        }
    }
    return -1;
}

void cursor_close(ReplayCursor *cursor) {
    if (!cursor) return;
    if (cursor->current_data) free(cursor->current_data);
    if (cursor->peek_data)    free(cursor->peek_data);
    if (cursor->reader)       close_trace(cursor->reader);
    free(cursor);
}
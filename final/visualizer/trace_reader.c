#include "trace_reader.h"
#include <stdlib.h>

TraceReader* open_trace(const char *path, const char *idx_path) {
    TraceReader *tr = malloc(sizeof(TraceReader));
    if (!tr) return NULL;
    tr->fp     = fopen(path,     "rb");
    tr->idx_fp = fopen(idx_path, "rb");
    if (!tr->fp || !tr->idx_fp) {
        /* BUG FIX: free tr before returning NULL to avoid memory leak */
        if (tr->fp)     fclose(tr->fp);
        if (tr->idx_fp) fclose(tr->idx_fp);
        free(tr);
        return NULL;
    }
    fread(&tr->header, sizeof(struct TraceHeader), 1, tr->fp);
    return tr;
}

int read_event(TraceReader *tr, struct EventRecorder *ev, uint8_t **data) {
    if (fread(ev, sizeof(struct EventRecorder), 1, tr->fp) != 1) return -1;
    if (ev->data_len > 0) {
        *data = malloc(ev->data_len);
        fread(*data, ev->data_len, 1, tr->fp);
    } else {
        *data = NULL;
    }
    return 0;
}

int seek_to_seq_idx(TraceReader *tr, uint64_t seq_idx) {
    /* BUG FIX: seq_idx values are NOT contiguous (only NON_DET events are
       recorded, so seq values skip e.g. 0,3,5,6,13...).
       Must linear-scan the index file for the matching entry. */
    struct IndexRecord rec;
    rewind(tr->idx_fp);
    while (fread(&rec, sizeof(struct IndexRecord), 1, tr->idx_fp) == 1) {
        if (rec.seq_idx == seq_idx)
            return fseek(tr->fp, (long)rec.byte_offset, SEEK_SET);
    }
    return -1; /* seq_idx not found */
}

void close_trace(TraceReader *tr) {
    fclose(tr->fp);
    fclose(tr->idx_fp);
    free(tr);
}
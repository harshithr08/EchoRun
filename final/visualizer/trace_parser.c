#include "visualizer.h"
#include "trace_reader.h"
#include <stdlib.h>
#include <stdio.h>

EventList *load_events(const char *trace_path, const char *idx_path) {
    TraceReader *tr = open_trace(trace_path, idx_path);
    if (!tr) {
        fprintf(stderr, "[VIS] Failed to open trace: %s\n", trace_path);
        return NULL;
    }

    uint64_t total = tr->header.event_count;
    if (total == 0) {
        fprintf(stderr, "[VIS] Trace has 0 events.\n");
        close_trace(tr);
        return NULL;
    }

    EventList *el = malloc(sizeof(EventList));
    el->events = malloc(sizeof(VisEvent) * total);
    el->count  = 0;

    struct EventRecorder ev;
    uint8_t *data = NULL;

    while (read_event(tr, &ev, &data) == 0 && el->count < total) {
        VisEvent *ve = &el->events[el->count++];
        ve->seq_idx   = ev.seq_idx;
        ve->event_type = ev.event_type;
        ve->syscall_no = ev.syscall_no;
        ve->retval     = ev.retval;
        if (data) { free(data); data = NULL; }
    }

    close_trace(tr);
    return el;
}

void free_events(EventList *el) {
    if (!el) return;
    free(el->events);
    free(el);
}

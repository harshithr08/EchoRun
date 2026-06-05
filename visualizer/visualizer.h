#ifndef VISUALIZER_H
#define VISUALIZER_H

#include "echotrace_bin.h"
#include <stdint.h>

// ── Event list ────────────────────────────────────────────────────────────
// In-memory record built from the trace file (Task 3.1)
typedef struct {
    uint64_t seq_idx;
    uint8_t  event_type;   // SYSCALL_EVENT=1, SIGNAL_EVENT=2, PROC_EVENT=3
    uint32_t syscall_no;
    int64_t  retval;
} VisEvent;

typedef struct {
    VisEvent *events;
    uint64_t  count;
} EventList;

// ── Divergence report (from Person 2 / replayer) ─────────────────────────
typedef struct {
    uint64_t seq_idx;
    uint32_t expected_syscall;
    uint32_t actual_syscall;
    int      valid;   // 0 = no divergence to overlay
} DivergenceReport;

// ── Output mode ───────────────────────────────────────────────────────────
typedef enum { OUT_SVG, OUT_TUI } OutputMode;

// ── Public API ────────────────────────────────────────────────────────────

// Task 3.1: Load trace file into EventList
EventList *load_events(const char *trace_path, const char *idx_path);
void       free_events(EventList *el);

// Task 3.2-3.4: Render SVG timeline to file (or stdout if path is NULL)
int render_svg(const EventList *el, const DivergenceReport *div,
               const char *out_path);

// Task 3.5: Render compact TUI timeline to terminal
void render_tui(const EventList *el, const DivergenceReport *div);

// Task 3.6: Top-level CLI dispatcher
int cmd_visualise(int argc, char *argv[]);

#endif

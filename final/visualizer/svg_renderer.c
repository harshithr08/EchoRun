#include "visualizer.h"
#include <stdio.h>
#include <string.h>
#include <inttypes.h>

#define SVG_WIDTH      1200
#define SVG_HEIGHT     300
#define LANE_Y_SYSCALL  60
#define LANE_Y_SIGNAL  130
#define LANE_Y_PROC    200
#define TICK_H          30
#define MARGIN_LEFT     80
#define DRAW_WIDTH    (SVG_WIDTH - MARGIN_LEFT - 20)

static const char *event_color(uint8_t etype) {
    switch (etype) {
        case 1: return "#4A90D9";
        case 2: return "#E8A838";
        case 3: return "#4CAF50";
        default: return "#888888";
    }
}

static int lane_y(uint8_t etype) {
    switch (etype) {
        case 1: return LANE_Y_SYSCALL;
        case 2: return LANE_Y_SIGNAL;
        case 3: return LANE_Y_PROC;
        default: return LANE_Y_PROC;
    }
}

static const char *syscall_name(uint32_t no) {
    switch (no) {
        case 0:  return "read";      case 1:  return "write";
        case 2:  return "open";      case 3:  return "close";
        case 4:  return "stat";      case 5:  return "fstat";
        case 8:  return "lseek";     case 9:  return "mmap";
        case 11: return "munmap";    case 12: return "brk";
        case 39: return "getpid";    case 45: return "recvfrom";
        case 60: return "exit";      case 318:return "getrandom";
        default: return "syscall";
    }
}

int render_svg(const EventList *el, const DivergenceReport *div,
               const char *out_path) {
    FILE *f = out_path ? fopen(out_path, "w") : stdout;
    if (!f) { perror("render_svg: fopen"); return -1; }

    uint64_t total = el->count;
    if (total == 0) { fprintf(f, "<!-- empty trace -->\n"); if (out_path) fclose(f); return 0; }

    uint64_t max_seq = el->events[total - 1].seq_idx;
    if (max_seq == 0) max_seq = 1;

    fprintf(f,
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
        "<svg xmlns=\"http://www.w3.org/2000/svg\" "
             "width=\"%d\" height=\"%d\" "
             "style=\"background:#1a1a2e;font-family:monospace\">\n",
        SVG_WIDTH, SVG_HEIGHT);

    /* Lane background strips */
    int lane_tops[3]  = {LANE_Y_SYSCALL-TICK_H, LANE_Y_SIGNAL-TICK_H, LANE_Y_PROC-TICK_H};
    int lane_bots[3]  = {LANE_Y_SYSCALL+TICK_H, LANE_Y_SIGNAL+TICK_H, LANE_Y_PROC+TICK_H};
    const char *bg[3] = {"#16213e", "#0f3460", "#16213e"};
    for (int i = 0; i < 3; i++)
        fprintf(f, "<rect x=\"%d\" y=\"%d\" width=\"%d\" height=\"%d\" fill=\"%s\" opacity=\"0.6\"/>\n",
                MARGIN_LEFT, lane_tops[i], DRAW_WIDTH, lane_bots[i]-lane_tops[i], bg[i]);

    /* Lane labels */
    const char *llabels[3] = {"SYSCALL","SIGNAL","PROC"};
    int         lys[3]     = {LANE_Y_SYSCALL, LANE_Y_SIGNAL, LANE_Y_PROC};
    for (int i = 0; i < 3; i++)
        fprintf(f, "<text x=\"5\" y=\"%d\" fill=\"#aaaaaa\" font-size=\"11\" "
                   "dominant-baseline=\"middle\">%s</text>\n", lys[i], llabels[i]);

    /* Title */
    fprintf(f, "<text x=\"%d\" y=\"20\" fill=\"#e0e0e0\" font-size=\"14\" "
               "text-anchor=\"middle\">EchoRun Trace Timeline (%" PRIu64 " events)</text>\n",
               SVG_WIDTH / 2, total);

    /* Event ticks */
    for (uint64_t i = 0; i < total; i++) {
        const VisEvent *ve = &el->events[i];
        int x = MARGIN_LEFT + (int)((double)ve->seq_idx / (double)max_seq * DRAW_WIDTH);
        int y = lane_y(ve->event_type);
        char tip[128];
        snprintf(tip, sizeof(tip), "%s() seq=%" PRIu64 " ret=%" PRId64,
                 syscall_name(ve->syscall_no), ve->seq_idx, ve->retval);
        fprintf(f,
            "<line x1=\"%d\" y1=\"%d\" x2=\"%d\" y2=\"%d\" "
                  "stroke=\"%s\" stroke-width=\"2\" opacity=\"0.85\">"
                "<title>%s</title></line>\n",
            x, y - TICK_H/2, x, y + TICK_H/2, event_color(ve->event_type), tip);
    }

    /* Divergence overlay */
    if (div && div->valid) {
        int dx = MARGIN_LEFT + (int)((double)div->seq_idx / (double)max_seq * DRAW_WIDTH);
        fprintf(f,
            "<line x1=\"%d\" y1=\"30\" x2=\"%d\" y2=\"%d\" "
                  "stroke=\"#FF4444\" stroke-width=\"3\" stroke-dasharray=\"6,3\">"
                "<title>DIVERGENCE seq=%" PRIu64 " expected=%u actual=%u</title>"
            "</line>\n"
            "<text x=\"%d\" y=\"28\" fill=\"#FF4444\" font-size=\"11\" "
                  "text-anchor=\"middle\">DIV</text>\n",
            dx, dx, SVG_HEIGHT - 10,
            div->seq_idx, div->expected_syscall, div->actual_syscall, dx);
    }

    /* X-axis */
    fprintf(f, "<line x1=\"%d\" y1=\"%d\" x2=\"%d\" y2=\"%d\" stroke=\"#444\" stroke-width=\"1\"/>\n",
            MARGIN_LEFT, SVG_HEIGHT-30, SVG_WIDTH-20, SVG_HEIGHT-30);
    for (int t = 0; t <= 4; t++) {
        uint64_t sl = (uint64_t)((double)max_seq * t / 4.0);
        int tx = MARGIN_LEFT + (int)((double)sl / (double)max_seq * DRAW_WIDTH);
        fprintf(f, "<text x=\"%d\" y=\"%d\" fill=\"#666\" font-size=\"10\" "
                   "text-anchor=\"middle\">%" PRIu64 "</text>\n",
                tx, SVG_HEIGHT-15, sl);
    }

    /* Legend */
    int lx = SVG_WIDTH - 200, ly = 50;
    const char *lc[4] = {"#4A90D9","#E8A838","#4CAF50","#FF4444"};
    const char *ll[4] = {"SYSCALL","SIGNAL","PROC","DIVERGENCE"};
    for (int i = 0; i < 4; i++) {
        fprintf(f, "<rect x=\"%d\" y=\"%d\" width=\"12\" height=\"12\" fill=\"%s\"/>\n"
                   "<text x=\"%d\" y=\"%d\" fill=\"#ccc\" font-size=\"11\">%s</text>\n",
                lx, ly+i*18, lc[i], lx+16, ly+i*18+11, ll[i]);
    }

    fprintf(f, "</svg>\n");
    if (out_path) fclose(f);
    return 0;
}

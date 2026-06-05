#include "visualizer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <sys/ioctl.h>
#include <unistd.h>

#define ANSI_RESET  "\033[0m"
#define ANSI_BLUE   "\033[34m"
#define ANSI_ORANGE "\033[33m"
#define ANSI_GREEN  "\033[32m"
#define ANSI_RED    "\033[31m"
#define ANSI_GRAY   "\033[90m"
#define ANSI_BOLD   "\033[1m"
#define BLOCK_FULL  "\xe2\x96\x88"
#define BLOCK_HALF  "\xe2\x96\x84"
#define BLOCK_LIGHT "\xe2\x96\x91"

static int term_width(void) {
    struct winsize w;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) == 0 && w.ws_col > 20) return w.ws_col;
    return 80;
}

void render_tui(const EventList *el, const DivergenceReport *div) {
    if (!el || el->count == 0) { printf("[VIS] No events.\n"); return; }

    int width = term_width() - 12;
    if (width < 20)  width = 20;
    if (width > 200) width = 200;

    uint64_t total   = el->count;
    uint64_t max_seq = el->events[total - 1].seq_idx;
    if (max_seq == 0) max_seq = 1;

    int *sb = calloc(width, sizeof(int)); // syscall
    int *gb = calloc(width, sizeof(int)); // signal
    int *pb = calloc(width, sizeof(int)); // proc

    for (uint64_t i = 0; i < total; i++) {
        const VisEvent *ve = &el->events[i];
        int col = (int)((double)ve->seq_idx / (double)max_seq * (width - 1));
        if (col >= width) col = width - 1;
        switch (ve->event_type) {
            case 1: sb[col]++; break;
            case 2: gb[col]++; break;
            case 3: pb[col]++; break;
        }
    }

    int div_col = -1;
    if (div && div->valid)
        div_col = (int)((double)div->seq_idx / (double)max_seq * (width - 1));

    printf("\n" ANSI_BOLD "  EchoRun Trace Timeline  (%" PRIu64 " events)\n" ANSI_RESET, total);
    printf("  seq: 0");
    for (int i = 0; i < width - 12; i++) printf(" ");
    printf("%" PRIu64 "\n", max_seq);

    const char *labels[3] = {"SYSCALL", "SIGNAL ", "PROC   "};
    int *buckets[3] = {sb, gb, pb};
    const char *colors[3] = {ANSI_BLUE, ANSI_ORANGE, ANSI_GREEN};

    for (int lane = 0; lane < 3; lane++) {
        printf("  %s%s" ANSI_RESET " |", colors[lane], labels[lane]);
        for (int col = 0; col < width; col++) {
            if (col == div_col) { printf(ANSI_RED "|" ANSI_RESET); continue; }
            int cnt = buckets[lane][col];
            if (cnt == 0)      printf(ANSI_GRAY " " ANSI_RESET);
            else if (cnt == 1) printf("%s" BLOCK_LIGHT ANSI_RESET, colors[lane]);
            else if (cnt < 5)  printf("%s" BLOCK_HALF  ANSI_RESET, colors[lane]);
            else               printf("%s" BLOCK_FULL  ANSI_RESET, colors[lane]);
        }
        printf("|\n");
    }
    printf("  %*s  ", 7, "");
    for (int col = 0; col < width; col++) printf("-");
    printf("\n");

    if (div && div->valid)
        printf(ANSI_RED "  DIVERGENCE seq=%" PRIu64 " expected=%u actual=%u\n" ANSI_RESET,
               div->seq_idx, div->expected_syscall, div->actual_syscall);

    free(sb); free(gb); free(pb);
}

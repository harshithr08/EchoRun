// echovis.c — EchoRun Visualizer + Diff CLI
// Usage:
//   ./echovis visualise <trace.bin> <trace.idx> [--output out.svg] [--tui]
//                       [--divergence <seq> <exp_syscall> <act_syscall>]
//   ./echovis diff <a.bin> <a.idx> <b.bin> <b.idx>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include "visualizer.h"
#include "trace_diff.h"

static void usage(const char *prog) {
    fprintf(stderr,
        "Usage:\n"
        "  %s visualise <trace.bin> <trace.idx> [options]\n"
        "     --output <file.svg>    write SVG to file (default: stdout)\n"
        "     --tui                  render TUI block-char timeline\n"
        "     --divergence <seq> <expected_syscall> <actual_syscall>\n"
        "                            overlay a divergence marker\n\n"
        "  %s diff <a.bin> <a.idx> <b.bin> <b.idx>\n"
        "     Compare two traces, print first divergence.\n",
        prog, prog);
}

// ── visualise subcommand ─────────────────────────────────────────────────
static int subcmd_visualise(int argc, char *argv[]) {
    // argv[0] = "visualise", argv[1] = trace.bin, argv[2] = trace.idx
    if (argc < 3) { fprintf(stderr, "visualise needs <trace.bin> <trace.idx>\n"); return 1; }

    const char *bin_path = argv[1];
    const char *idx_path = argv[2];
    const char *out_path = NULL;
    int tui_mode         = 0;
    DivergenceReport div = {0};

    for (int i = 3; i < argc; i++) {
        if (strcmp(argv[i], "--output") == 0 && i+1 < argc) {
            out_path = argv[++i];
        } else if (strcmp(argv[i], "--tui") == 0) {
            tui_mode = 1;
        } else if (strcmp(argv[i], "--divergence") == 0 && i+3 < argc) {
            div.seq_idx          = (uint64_t)strtoull(argv[++i], NULL, 10);
            div.expected_syscall = (uint32_t)strtoul(argv[++i],  NULL, 10);
            div.actual_syscall   = (uint32_t)strtoul(argv[++i],  NULL, 10);
            div.valid            = 1;
        }
    }

    EventList *el = load_events(bin_path, idx_path);
    if (!el) return 1;

    printf("[VIS] Loaded %" PRIu64 " events from %s\n", el->count, bin_path);

    if (tui_mode) {
        render_tui(el, div.valid ? &div : NULL);
    } else {
        int r = render_svg(el, div.valid ? &div : NULL, out_path);
        if (r == 0 && out_path)
            printf("[VIS] SVG written to %s\n", out_path);
    }

    free_events(el);
    return 0;
}

// ── diff subcommand ──────────────────────────────────────────────────────
static int subcmd_diff(int argc, char *argv[]) {
    if (argc < 5) {
        fprintf(stderr, "diff needs <a.bin> <a.idx> <b.bin> <b.idx>\n");
        return 1;
    }
    DiffRecord rec = {0};
    int r = trace_diff(argv[1], argv[2], argv[3], argv[4], &rec);
    if (r < 0) return 1;
    print_diff_record(&rec);
    return (rec.found) ? 1 : 0;
}

// ── main ─────────────────────────────────────────────────────────────────
int main(int argc, char *argv[]) {
    if (argc < 2) { usage(argv[0]); return 1; }

    if (strcmp(argv[1], "visualise") == 0)
        return subcmd_visualise(argc - 1, argv + 1);
    if (strcmp(argv[1], "diff") == 0)
        return subcmd_diff(argc - 1, argv + 1);

    fprintf(stderr, "Unknown subcommand: %s\n", argv[1]);
    usage(argv[0]);
    return 1;
}

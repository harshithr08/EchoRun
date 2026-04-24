// echovis.c — EchoRun Visualizer + Diff CLI
// Usage:
//   ./echovis visualise <trace.bin> <trace.idx> [--output out.svg] [--tui]
//                       [--divergence <seq> <exp_syscall> <act_syscall>]
//   ./echovis diff <a.bin> <a.idx> <b.bin> <b.idx> [--output diff.svg]

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
        "  %s diff <a.bin> <a.idx> <b.bin> <b.idx> [options]\n"
        "     --output <file.svg>    also render SVG of trace A with divergence\n"
        "                            marker overlaid at the found divergence point\n",
        prog, prog);
}

// Improvement 3: print event breakdown stats after loading a trace
static void print_event_stats(const EventList *el) {
    if (!el || el->count == 0) return;

    uint64_t syscall_count = 0, signal_count = 0, proc_count = 0;

    // Count most frequent syscall
    uint32_t freq[400] = {0};  // covers syscall numbers 0-399
    for (uint64_t i = 0; i < el->count; i++) {
        switch (el->events[i].event_type) {
            case 1: syscall_count++;
                    if (el->events[i].syscall_no < 400)
                        freq[el->events[i].syscall_no]++;
                    break;
            case 2: signal_count++; break;
            case 3: proc_count++;   break;
        }
    }

    // Find most frequent syscall
    uint32_t top_no = 0;
    uint32_t top_count = 0;
    for (int i = 0; i < 400; i++) {
        if (freq[i] > top_count) { top_count = freq[i]; top_no = (uint32_t)i; }
    }

    // Resolve top syscall name using the same table as trace_diff
    static const char *sname_map[] = {
        [0]="read",[1]="write",[2]="open",[3]="close",[4]="stat",
        [5]="fstat",[9]="mmap",[11]="munmap",[12]="brk",[39]="getpid",
        [45]="recvfrom",[60]="exit",[62]="kill",[318]="getrandom"
    };
    const char *top_name = (top_no < 400 && sname_map[top_no])
                           ? sname_map[top_no] : "syscall";

    printf("[VIS] Event breakdown:\n");
    printf("      SYSCALL : %" PRIu64 "\n", syscall_count);
    printf("      SIGNAL  : %" PRIu64 "\n", signal_count);
    printf("      PROC    : %" PRIu64 "\n", proc_count);
    if (top_count > 0)
        printf("      Most frequent syscall: %s() x%u\n", top_name, top_count);
}

// ── visualise subcommand ─────────────────────────────────────────────────
static int subcmd_visualise(int argc, char *argv[]) {
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

    // Improvement 3: always show stats
    print_event_stats(el);

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
    // argv[0]="diff" argv[1]=a.bin argv[2]=a.idx argv[3]=b.bin argv[4]=b.idx
    if (argc < 5) {
        fprintf(stderr, "diff needs <a.bin> <a.idx> <b.bin> <b.idx>\n");
        return 1;
    }

    // Improvement 1: optional --output flag to auto-render SVG with divergence
    const char *svg_out = NULL;
    for (int i = 5; i < argc; i++) {
        if (strcmp(argv[i], "--output") == 0 && i+1 < argc) {
            svg_out = argv[++i];
        }
    }

    DiffRecord rec = {0};
    int r = trace_diff(argv[1], argv[2], argv[3], argv[4], &rec);
    if (r < 0) return 1;

    print_diff_record(&rec);

    // Improvement 1: if divergence found and --output given, render SVG of
    // trace A with the divergence marker overlaid automatically.
    if (rec.found && svg_out) {
        EventList *el = load_events(argv[1], argv[2]);
        if (el) {
            printf("[VIS] Loaded %" PRIu64 " events from %s\n", el->count, argv[1]);
            print_event_stats(el);

            DivergenceReport div = {
                .seq_idx          = rec.seq_idx,
                .expected_syscall = rec.a_syscall,
                .actual_syscall   = rec.b_syscall,
                .valid            = 1
            };
            if (render_svg(el, &div, svg_out) == 0)
                printf("[VIS] SVG with divergence marker written to %s\n", svg_out);
            free_events(el);
        }
    } else if (!rec.found && svg_out) {
        // Traces identical — render clean SVG anyway
        EventList *el = load_events(argv[1], argv[2]);
        if (el) {
            print_event_stats(el);
            if (render_svg(el, NULL, svg_out) == 0)
                printf("[VIS] SVG (no divergence) written to %s\n", svg_out);
            free_events(el);
        }
    }

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
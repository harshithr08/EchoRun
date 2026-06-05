// echovis.c — EchoRun Visualizer + Diff + Summarise CLI
// Usage:
//   ./echovis visualise <trace.bin> <trace.idx> [--output out.svg] [--tui]
//                       [--divergence <seq> <exp_syscall> <act_syscall>]
//   ./echovis diff      <a.bin> <a.idx> <b.bin> <b.idx> [--output diff.svg]
//   ./echovis summarise <trace.bin> <trace.idx>

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
        "     --output <file.svg>    render SVG of trace A with divergence marker\n\n"
        "  %s summarise <trace.bin> <trace.idx>\n"
        "     print a human-readable narrative of what the program did\n",
        prog, prog, prog);
}

// Improvement 3: print event breakdown stats after loading a trace
static void print_event_stats(const EventList *el) {
    if (!el || el->count == 0) return;

    uint64_t syscall_count = 0, signal_count = 0, proc_count = 0;
    uint32_t freq[400] = {0};

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

    uint32_t top_no = 0, top_count = 0;
    for (int i = 0; i < 400; i++) {
        if (freq[i] > top_count) { top_count = freq[i]; top_no = (uint32_t)i; }
    }

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

// ── Improvement 3: summarise subcommand ──────────────────────────────────
// Pattern-matches on the event stream to produce a human-readable narrative
// of what the program did, grouped into phases.


static int subcmd_summarise(int argc, char *argv[]) {
    if (argc < 3) {
        fprintf(stderr, "summarise needs <trace.bin> <trace.idx>\n");
        return 1;
    }

    EventList *el = load_events(argv[1], argv[2]);
    if (!el) return 1;

    printf("\n[SUMMARY] Program execution narrative (%" PRIu64 " events)\n",
           el->count);
    printf("──────────────────────────────────────────────────────\n");

    // Phase counters
    uint32_t mmap_count   = 0, fstat_count  = 0;
    uint32_t open_count   = 0, close_count  = 0;
    uint32_t read_count   = 0, write_count  = 0;
    uint32_t rng_count    = 0, net_count    = 0;
    uint32_t signal_count = 0, proc_count   = 0;
    uint32_t time_count   = 0, other_count  = 0;

    int64_t  total_bytes_read    = 0;
    int64_t  total_bytes_written = 0;
    int      stdin_read          = 0;
    int      stdout_write        = 0;
    int      exit_code           = -1;
    int      exec_seen           = 0;
    int      fork_seen           = 0;

    for (uint64_t i = 0; i < el->count; i++) {
        const VisEvent *ve = &el->events[i];

        if (ve->event_type == 2) { signal_count++; continue; }
        if (ve->event_type == 3) {
            proc_count++;
            // PROC_EVENT with syscall_no == 59 = execve
            // syscall_no == 57 or 56 = fork/clone
            // retval used as exit code when syscall_no == 0 (exit)
            if (ve->syscall_no == 59) exec_seen = 1;
            if (ve->syscall_no == 57 || ve->syscall_no == 56) fork_seen = 1;
            if (ve->syscall_no == 0 && ve->retval >= 0)
                exit_code = (int)ve->retval;
            continue;
        }

        // SYSCALL_EVENT
        switch (ve->syscall_no) {
            case 9:  mmap_count++;  break;
            case 5:  fstat_count++; break;
            case 2:  open_count++;  break;
            case 3:  close_count++; break;
            case 0:  // read
                read_count++;
                if (ve->retval > 0) total_bytes_read += ve->retval;
                // fd stored in rdi during record but not in trace —
                // heuristic: early reads with large retval are loader,
                // later smaller reads with retval <= 256 tend to be user I/O
                if (ve->retval > 0 && ve->retval <= 256 && read_count > 3)
                    stdin_read = 1;
                break;
            case 1:  // write
                write_count++;
                if (ve->retval > 0) total_bytes_written += ve->retval;
                stdout_write = 1;
                break;
            case 318: rng_count++;  break;
            case 41: case 42: case 43:
            case 44: case 45: net_count++; break;
            case 96: case 228: time_count++; break;
            default: other_count++; break;
        }
    }

    int step = 1;

    // Phase 1: loader activity
    if (mmap_count > 0 || fstat_count > 0 || exec_seen) {
        printf("  %d. [Startup] Loaded dynamic libraries and mapped binary\n", step++);
        printf("     (%u mmap, %u fstat/stat calls — typical ELF loader activity)\n",
               mmap_count, fstat_count);
    }

    // Phase 2: process events
    if (fork_seen)
        printf("  %d. [Process] Forked a child process\n", step++);

    // Phase 3: randomness
    if (rng_count > 0)
        printf("  %d. [Entropy] Called getrandom() %u time(s) — "
               "used hardware RNG\n", step++, rng_count);

    // Phase 4: time
    if (time_count > 0)
        printf("  %d. [Time] Called gettimeofday/clock_gettime %u time(s)\n",
               step++, time_count);

    // Phase 5: file I/O
    if (open_count > 0 || read_count > 0) {
        printf("  %d. [File I/O] Opened %u file(s), read %" PRId64 " bytes total "
               "across %u read() call(s)\n",
               step++, open_count, total_bytes_read, read_count);
    }

    // Phase 6: stdin
    if (stdin_read)
        printf("  %d. [Input] Read from stdin (interactive or pipe input)\n",
               step++);

    // Phase 7: network
    if (net_count > 0)
        printf("  %d. [Network] Made %u network-related call(s) "
               "(socket/connect/send/recv)\n", step++, net_count);

    // Phase 8: output
    if (write_count > 0 && stdout_write)
        printf("  %d. [Output] Wrote %" PRId64 " bytes to stdout/stderr "
               "across %u write() call(s)\n",
               step++, total_bytes_written, write_count);

    // Phase 9: signals
    if (signal_count > 0)
        printf("  %d. [Signals] %u signal event(s) recorded during execution\n",
               step++, signal_count);

    // Phase 10: exit
    if (exit_code >= 0)
        printf("  %d. [Exit] Exited cleanly with code %d\n", step++, exit_code);
    else
        printf("  %d. [Exit] Process terminated\n", step++);

    printf("──────────────────────────────────────────────────────\n");

    // Misc unknown syscalls
    if (other_count > 0)
        printf("  (%u other syscall(s) not categorized above)\n", other_count);

    free_events(el);
    return 0;
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
    if (argc < 5) {
        fprintf(stderr, "diff needs <a.bin> <a.idx> <b.bin> <b.idx>\n");
        return 1;
    }

    // Improvement 1: optional --output to auto-render SVG with divergence marker
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
    if (strcmp(argv[1], "summarise") == 0)
        return subcmd_summarise(argc - 1, argv + 1);

    fprintf(stderr, "Unknown subcommand: %s\n", argv[1]);
    usage(argv[0]);
    return 1;
}
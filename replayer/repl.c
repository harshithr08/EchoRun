#include "repl.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <inttypes.h>

void repl_run(ReplaySession *s) {
    char line[256];
    printf("\n[EchoRun REPL] Commands: continue | step | goto <seq> | peek <addr> | poke <addr> <val> | checkpoints | quit\n");

    while (1) {
        printf("echoplay> ");
        fflush(stdout);

        if (!fgets(line, sizeof(line), stdin)) break;

        // Strip trailing newline
        line[strcspn(line, "\n")] = '\0';

        if (strcmp(line, "continue") == 0 || strcmp(line, "c") == 0) {
            cmd_continue(s);
            return; // hand control back to replay_loop

        } else if (strcmp(line, "step") == 0 || strcmp(line, "s") == 0) {
            cmd_step(s);
            return;

        } else if (strncmp(line, "goto ", 5) == 0) {
            uint64_t seq = (uint64_t)strtoull(line + 5, NULL, 10);
            cmd_goto(s, seq);
            // stay in REPL after goto — user may want to step from there

        } else if (strncmp(line, "peek ", 5) == 0) {
            unsigned long long addr = strtoull(line + 5, NULL, 16);
            cmd_peek(s, addr);

        } else if (strncmp(line, "poke ", 5) == 0) {
            unsigned long long addr;
            long val;
            if (sscanf(line + 5, "%llx %ld", &addr, &val) == 2) {
                cmd_poke(s, addr, val);
            } else {
                printf("Usage: poke <hex_addr> <decimal_val>\n");
            }

        } else if (strcmp(line, "checkpoints") == 0 || strcmp(line, "cp") == 0) {
            // Improvement 4: list all saved checkpoint positions
            if (s->cp_count == 0) {
                printf("[REPL] No checkpoints saved yet.\n");
            } else {
                printf("[REPL] %d checkpoint(s) saved:\n", s->cp_count);
                for (int i = 0; i < s->cp_count; i++) {
                    Checkpoint *cp = s->checkpoints[i];
                    if (cp) {
                        printf("  [%d] seq=%" PRIu64 "  regions=%d\n",
                               i, cp->seq_idx, cp->region_count);
                    }
                }
                printf("  Use: goto <seq> to jump to any of these.\n");
            }

        } else if (strcmp(line, "quit") == 0 || strcmp(line, "q") == 0) {
            printf("[EchoRun] Exiting.\n");
            exit(0);

        } else if (line[0] != '\0') {
            printf("Unknown command: '%s'\n", line);
            printf("Commands: continue | step | goto <seq> | peek <addr> | poke <addr> <val> | checkpoints | quit\n");
        }
    }
}
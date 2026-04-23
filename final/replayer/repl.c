#include "repl.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

void repl_run(ReplaySession *s) {
    char line[256];
    printf("\n[EchoRun REPL] Commands: continue | step | goto <seq> | peek <addr> | poke <addr> <val> | quit\n");

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

        } else if (strcmp(line, "quit") == 0 || strcmp(line, "q") == 0) {
            printf("[EchoRun] Exiting.\n");
            exit(0);

        } else if (line[0] != '\0') {
            printf("Unknown command: '%s'\n", line);
        }
    }
}

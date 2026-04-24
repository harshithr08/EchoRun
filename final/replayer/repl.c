#include "repl.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <inttypes.h>
#include <sys/ptrace.h>
#include <sys/wait.h>

//CHANGE
static int read_repl_line(ReplaySession *s, char *line, size_t size) {
    if (s->repl_input) {
        if (!fgets(line, size, s->repl_input)) {
            fclose(s->repl_input);
            s->repl_input = NULL;
            printf("[EchoRun REPL] Script exhausted. Continuing replay.\n");
            cmd_continue(s);
            return 0;
        }

        printf("echoplay> %s", line);
        if (strchr(line, '\n') == NULL) printf("\n");
        return 1;
    }

    printf("echoplay> ");
    fflush(stdout);
    return fgets(line, size, stdin) != NULL;
}
//CHANGE

void repl_run(ReplaySession *s) {
    char line[256];
    printf("\n[EchoRun REPL] Commands: continue | step | goto <seq> | peek <addr> | poke <addr> <val> | checkpoints | quit\n");

    while (1) {
        //CHANGE
        if (!read_repl_line(s, line, sizeof(line))) return;
        //CHANGE

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
            //CHANGE
            s->should_quit = 1;
            if (s->pid > 0) {
                ptrace(PTRACE_KILL, s->pid, 0, 0);
                waitpid(s->pid, NULL, 0);
            }
            return;
            //CHANGE

        } else if (line[0] != '\0') {
            printf("Unknown command: '%s'\n", line);
            printf("Commands: continue | step | goto <seq> | peek <addr> | poke <addr> <val> | checkpoints | quit\n");
        }
    }
}

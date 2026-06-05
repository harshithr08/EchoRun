#include "checkpoint.h"
#include <sys/ptrace.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

// Read 'size' bytes from tracee at 'addr' into 'dest' using PTRACE_PEEKDATA
static int peek_region(pid_t pid, unsigned long addr,
                        uint8_t *dest, size_t size) {
    size_t offset = 0;
    while (offset < size) {
        errno = 0;
        long word = ptrace(PTRACE_PEEKDATA, pid, addr + offset, 0);
        if (word == -1 && errno != 0) return -1;

        size_t chunk = size - offset;
        if (chunk > sizeof(long)) chunk = sizeof(long);
        memcpy(dest + offset, &word, chunk);
        offset += chunk;
    }
    return 0;
}

// Write 'size' bytes from 'src' into tracee at 'addr' using PTRACE_POKEDATA
static int poke_region(pid_t pid, unsigned long addr,
                        const uint8_t *src, size_t size) {
    size_t offset = 0;
    while (offset < size) {
        long word = 0;
        size_t chunk = size - offset;
        if (chunk >= sizeof(long)) {
            memcpy(&word, src + offset, sizeof(long));
        } else {
            errno = 0;
            word = ptrace(PTRACE_PEEKDATA, pid, addr + offset, 0);
            if (word == -1 && errno != 0) return -1;
            memcpy(&word, src + offset, chunk);
        }
        if (ptrace(PTRACE_POKEDATA, pid, addr + offset, word) < 0) return -1;
        offset += (chunk >= sizeof(long)) ? sizeof(long) : chunk;
    }
    return 0;
}

Checkpoint *checkpoint_take(pid_t pid, uint64_t seq_idx) {
    Checkpoint *cp = calloc(1, sizeof(Checkpoint));
    if (!cp) return NULL;

    cp->seq_idx = seq_idx;

    // Save all CPU registers
    if (ptrace(PTRACE_GETREGS, pid, NULL, &cp->regs) < 0) {
        perror("checkpoint_take: PTRACE_GETREGS");
        free(cp);
        return NULL;
    }

    // Open /proc/pid/maps to find writable anonymous segments
    char maps_path[64];
    snprintf(maps_path, sizeof(maps_path), "/proc/%d/maps", pid);
    FILE *maps = fopen(maps_path, "r");
    if (!maps) {
        perror("checkpoint_take: fopen maps");
        free(cp);
        return NULL;
    }

    char line[256];
    cp->region_count = 0;

    while (fgets(line, sizeof(line), maps) && cp->region_count < MAX_MEMORY_REGIONS) {
        unsigned long start, end;
        char perms[5];
        char name[128] = "";

        // Format: start-end perms offset dev inode [name]
        sscanf(line, "%lx-%lx %4s %*s %*s %*s %127s", &start, &end, perms, name);

        // We only snapshot writable, anonymous regions (heap, stack, bss).
        // Skip file-backed regions (they don't change).
        if (perms[1] != 'w') continue;              // not writable
        if (name[0] != '\0' && name[0] != '[') continue; // file-backed

        size_t size = end - start;
        uint8_t *buf = malloc(size);
        if (!buf) continue;

        if (peek_region(pid, start, buf, size) < 0) {
            free(buf);
            continue;
        }

        cp->regions[cp->region_count].start = start;
        cp->regions[cp->region_count].end   = end;
        cp->regions[cp->region_count].data  = buf;
        cp->regions[cp->region_count].size  = size;
        cp->region_count++;
    }

    fclose(maps);
    return cp;
}

int checkpoint_restore(pid_t pid, const Checkpoint *cp) {
    // Restore CPU registers
    if (ptrace(PTRACE_SETREGS, pid, NULL, &cp->regs) < 0) {
        perror("checkpoint_restore: PTRACE_SETREGS");
        return -1;
    }

    // Restore all saved memory regions
    for (int i = 0; i < cp->region_count; i++) {
        const MemRegion *r = &cp->regions[i];
        if (poke_region(pid, r->start, r->data, r->size) < 0) {
            fprintf(stderr, "checkpoint_restore: failed region 0x%lx\n", r->start);
            return -1;
        }
    }
    return 0;
}

void checkpoint_free(Checkpoint *cp) {
    if (!cp) return;
    for (int i = 0; i < cp->region_count; i++) {
        free(cp->regions[i].data);
    }
    free(cp);
}

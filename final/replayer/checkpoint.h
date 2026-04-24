#ifndef CHECKPOINT_H
#define CHECKPOINT_H

#include <sys/types.h>
#include <sys/user.h>
#include <stdint.h>

#define MAX_MEMORY_REGIONS 64   // max writable segments we snapshot
#define CHECKPOINT_INTERVAL 500 // auto-checkpoint every N NON_DET events (Task 2.9)

// One contiguous writable memory region from /proc/pid/maps
typedef struct {
    unsigned long start;
    unsigned long end;
    uint8_t      *data; // saved bytes (heap allocated)
    size_t        size;
} MemRegion;

// Task 2.7: Full process state snapshot
typedef struct {
    struct user_regs_struct regs;          // all CPU registers
    MemRegion regions[MAX_MEMORY_REGIONS]; // writable anonymous segments
    int       region_count;
    uint64_t  seq_idx;                     // trace position when snapshot was taken
} Checkpoint;

// Take a snapshot of pid's register state + all writable anonymous memory
Checkpoint *checkpoint_take(pid_t pid, uint64_t seq_idx);

// Restore a previously taken checkpoint into pid
int checkpoint_restore(pid_t pid, const Checkpoint *cp);

// Free heap memory inside a checkpoint
void checkpoint_free(Checkpoint *cp);

#endif

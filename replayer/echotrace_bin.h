#ifndef ECHOTRACE_BIN_H
#define ECHOTRACE_BIN_H

#include <stdint.h>
#include <stdio.h>
#include <sys/types.h>

#define ECHOTRACE_MAGIC 0x4F484345
#define ECHOTRACE_VERSION 1
#define ARCH_X86_64 1

// Added for Task 1.8 / 1.7
#define SYSCALL_EVENT 1
#define SIGNAL_EVENT  2
#define PROC_EVENT    3

// Added for Task 1.6: Circular Buffer capacity
#define CIRCULAR_BUFFER_SIZE 10240 

#pragma pack(push, 1)
struct TraceHeader {
    uint32_t magic;
    uint32_t version;
    uint32_t arch;
    uint64_t event_count;
};

struct EventRecorder {
    uint8_t event_type;
    uint32_t syscall_no;
    uint64_t seq_idx;
    int64_t retval; 
    uint32_t data_len;
    uint64_t instruction_count; // Added for Task 1.11
};

struct IndexRecord {
    uint64_t seq_idx;
    uint64_t byte_offset;
};
#pragma pack(pop)

int init_trace_writer(const char *trace_path, const char *idx_path);
void write_trace_event(uint8_t type, uint32_t syscall_no, uint64_t seq_idx, int64_t retval, uint32_t data_len, const uint8_t *data);
void close_trace_writer(void);

// Added for Task 1.11
int setup_perf_counter(pid_t pid);

#endif
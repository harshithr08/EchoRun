#include "echotrace_bin.h"
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <linux/perf_event.h>
#include <sys/ioctl.h>
#include <asm/unistd.h>

static FILE *trace_file = NULL;
static FILE *index_file = NULL;
static uint64_t current_event_count = 0;

// Task 1.6: Circular Buffer State
static struct EventRecorder ring_buffer[CIRCULAR_BUFFER_SIZE];
static uint8_t* data_buffer[CIRCULAR_BUFFER_SIZE]; 
static int head = 0;
static int is_full = 0;

// Task 1.11: Perf Counter State
static int perf_fd = -1;

int init_trace_writer(const char *trace_path, const char *idx_path) {
    trace_file = fopen(trace_path, "wb");
    index_file = fopen(idx_path, "wb");

    if (!trace_file || !index_file) {
        perror("Failed to open trace files");
        return -1;
    }

    struct TraceHeader header = {
        .magic = ECHOTRACE_MAGIC,
        .version = ECHOTRACE_VERSION,
        .arch = ARCH_X86_64,
        .event_count = 0 
    };
    fwrite(&header, sizeof(header), 1, trace_file);
    return 0;
}

// Task 1.11: Initialize hardware performance counter
int setup_perf_counter(pid_t pid) {
    struct perf_event_attr pe;
    memset(&pe, 0, sizeof(struct perf_event_attr));
    pe.type = PERF_TYPE_HARDWARE;
    pe.size = sizeof(struct perf_event_attr);
    pe.config = PERF_COUNT_HW_INSTRUCTIONS;
    pe.disabled = 1;
    pe.exclude_kernel = 1;
    pe.exclude_hv = 1;

    perf_fd = syscall(__NR_perf_event_open, &pe, pid, -1, -1, 0);
    if (perf_fd != -1) {
        ioctl(perf_fd, PERF_EVENT_IOC_RESET, 0);
        ioctl(perf_fd, PERF_EVENT_IOC_ENABLE, 0);
    }
    return perf_fd;
}

void write_trace_event(uint8_t type, uint32_t syscall_no, uint64_t seq_idx, int64_t retval, uint32_t data_len, const uint8_t *data) {
    uint64_t insn_count = 0;
    
    // Read hardware instruction count if available
    if (perf_fd != -1) {
        read(perf_fd, &insn_count, sizeof(insn_count));
    }

    // Task 1.6: If overwriting old events in the buffer, free old data
    if (is_full && data_buffer[head] != NULL) {
        free(data_buffer[head]);
        data_buffer[head] = NULL;
    }

    // Write to ring buffer
    ring_buffer[head] = (struct EventRecorder){
        .event_type = type,
        .syscall_no = syscall_no,
        .seq_idx = seq_idx,
        .retval = retval,
        .data_len = data_len,
        .instruction_count = insn_count
    };

    if (data_len > 0 && data != NULL) {
        data_buffer[head] = malloc(data_len);
        if (data_buffer[head]) {
            memcpy(data_buffer[head], data, data_len);
        }
    } else {
        data_buffer[head] = NULL;
    }

    head = (head + 1) % CIRCULAR_BUFFER_SIZE;
    if (head == 0) is_full = 1;
}

void close_trace_writer() {
    if (!trace_file) return;

    // Task 1.6: Flush ring buffer to disk linearly
    int start = is_full ? head : 0;
    int count = is_full ? CIRCULAR_BUFFER_SIZE : head;

    for (int i = 0; i < count; i++) {
        int idx = (start + i) % CIRCULAR_BUFFER_SIZE;
        struct EventRecorder *ev = &ring_buffer[idx];

        // Write index record
        uint64_t current_offset = ftell(trace_file);
        struct IndexRecord idx_rec = { .seq_idx = ev->seq_idx, .byte_offset = current_offset };
        if (index_file) {
            fwrite(&idx_rec, sizeof(idx_rec), 1, index_file);
        }

        // Write event record
        fwrite(ev, sizeof(struct EventRecorder), 1, trace_file);
        
        // Write event data
        if (ev->data_len > 0 && data_buffer[idx] != NULL) {
            fwrite(data_buffer[idx], ev->data_len, 1, trace_file);
            free(data_buffer[idx]); // Cleanup memory during flush
            data_buffer[idx] = NULL;
        }
        current_event_count++;
    }

    // Finalize header event count
    fseek(trace_file, offsetof(struct TraceHeader, event_count), SEEK_SET);
    fwrite(&current_event_count, sizeof(current_event_count), 1, trace_file);
    
    fclose(trace_file);
    if (index_file) fclose(index_file);
    
    // Task 1.11 cleanup
    if (perf_fd != -1) {
        ioctl(perf_fd, PERF_EVENT_IOC_DISABLE, 0);
        close(perf_fd);
    }
}
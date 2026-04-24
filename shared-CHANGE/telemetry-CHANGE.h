#ifndef TELEMETRY_CHANGE_H
#define TELEMETRY_CHANGE_H

#include <stdint.h>
#include <stdio.h>
#include <sys/types.h>

typedef struct {
    FILE    *fp;
    uint64_t seq;
} TelemetryWriter;

int telemetry_open(TelemetryWriter *writer, const char *path);
void telemetry_close(TelemetryWriter *writer);

const char *telemetry_category_name(int category);
const char *telemetry_lane_for_syscall(long syscall_no, int category);
const char *telemetry_syscall_name(long syscall_no);
const char *telemetry_signal_name(int signum);
const char *telemetry_process_name(int proc_code);

void telemetry_emit_syscall(TelemetryWriter *writer,
                            pid_t pid,
                            long syscall_no,
                            int category,
                            int64_t retval,
                            int has_retval,
                            uint32_t payload_size,
                            int has_trace_seq,
                            uint64_t trace_seq,
                            int diverged,
                            uint32_t expected_syscall,
                            uint32_t actual_syscall);

void telemetry_emit_signal(TelemetryWriter *writer,
                           pid_t pid,
                           int signum,
                           int has_trace_seq,
                           uint64_t trace_seq);

void telemetry_emit_process(TelemetryWriter *writer,
                            pid_t pid,
                            int proc_code,
                            int64_t retval,
                            int has_retval,
                            int has_trace_seq,
                            uint64_t trace_seq);

#endif

#include "telemetry-CHANGE.h"

#include <inttypes.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>

static void emit_json_string(FILE *fp, const char *value) {
    fputc('"', fp);
    if (!value) {
        fputc('"', fp);
        return;
    }

    for (const unsigned char *p = (const unsigned char *)value; *p; ++p) {
        switch (*p) {
            case '\\': fputs("\\\\", fp); break;
            case '"':  fputs("\\\"", fp); break;
            case '\n': fputs("\\n", fp); break;
            case '\r': fputs("\\r", fp); break;
            case '\t': fputs("\\t", fp); break;
            default:
                if (*p < 0x20) fprintf(fp, "\\u%04x", *p);
                else fputc(*p, fp);
                break;
        }
    }

    fputc('"', fp);
}

int telemetry_open(TelemetryWriter *writer, const char *path) {
    if (!writer) return -1;

    writer->fp  = NULL;
    writer->seq = 0;

    if (!path) return 0;

    writer->fp = fopen(path, "w");
    if (!writer->fp) {
        perror("telemetry_open");
        return -1;
    }

    return 0;
}

void telemetry_close(TelemetryWriter *writer) {
    if (!writer || !writer->fp) return;
    fclose(writer->fp);
    writer->fp = NULL;
}

const char *telemetry_category_name(int category) {
    switch (category) {
        case 0: return "deterministic";
        case 1: return "non_deterministic";
        case 2: return "side_effect";
        //CHANGE
        default: return "deterministic";
        //CHANGE
    }
}

const char *telemetry_syscall_name(long no) {
    switch (no) {
        case 0:   return "read";
        case 1:   return "write";
        case 2:   return "open";
        case 3:   return "close";
        case 4:   return "stat";
        case 5:   return "fstat";
        case 6:   return "lstat";
        case 8:   return "lseek";
        case 9:   return "mmap";
        case 10:  return "mprotect";
        case 11:  return "munmap";
        case 12:  return "brk";
        case 13:  return "rt_sigaction";
        case 14:  return "rt_sigprocmask";
        case 17:  return "pread64";
        case 18:  return "pwrite64";
        case 21:  return "access";
        case 22:  return "pipe";
        case 32:  return "dup";
        case 33:  return "dup2";
        case 39:  return "getpid";
        case 41:  return "socket";
        case 42:  return "connect";
        case 43:  return "accept";
        case 44:  return "sendto";
        case 45:  return "recvfrom";
        case 47:  return "recvmsg";
        case 49:  return "bind";
        case 50:  return "listen";
        case 53:  return "socketpair";
        case 54:  return "setsockopt";
        case 56:  return "clone";
        case 57:  return "fork";
        case 59:  return "execve";
        case 60:  return "exit";
        case 61:  return "wait4";
        case 62:  return "kill";
        case 72:  return "fcntl";
        case 96:  return "gettimeofday";
        case 102: return "getuid";
        case 158: return "arch_prctl";
        case 218: return "set_tid_address";
        case 228: return "clock_gettime";
        case 231: return "exit_group";
        case 235: return "uname";
        case 257: return "openat";
        case 273: return "set_robust_list";
        case 293: return "pipe2";
        case 302: return "prlimit64";
        case 318: return "getrandom";
        case 334: return "rseq";
        default:  return "unknown";
    }
}

const char *telemetry_signal_name(int signum) {
    switch (signum) {
        case SIGINT:  return "SIGINT";
        case SIGILL:  return "SIGILL";
        case SIGABRT: return "SIGABRT";
        case SIGBUS:  return "SIGBUS";
        case SIGFPE:  return "SIGFPE";
        case SIGKILL: return "SIGKILL";
        case SIGSEGV: return "SIGSEGV";
        case SIGPIPE: return "SIGPIPE";
        case SIGALRM: return "SIGALRM";
        case SIGTERM: return "SIGTERM";
        case SIGCHLD: return "SIGCHLD";
        case SIGCONT: return "SIGCONT";
        case SIGSTOP: return "SIGSTOP";
        case SIGTSTP: return "SIGTSTP";
        case SIGTTIN: return "SIGTTIN";
        case SIGTTOU: return "SIGTTOU";
        case SIGTRAP: return "SIGTRAP";
        default:      return "SIGNAL";
    }
}

const char *telemetry_process_name(int proc_code) {
    switch (proc_code) {
        case 0: return "exit";
        case 1: return "fork";
        case 2: return "vfork";
        case 3: return "clone";
        case 4: return "exec";
        case 6: return "exit";
        default: return "process";
    }
}

static int is_ipc_syscall(long syscall_no) {
    switch (syscall_no) {
        case 22:
        case 32:
        case 33:
        case 41:
        case 42:
        case 43:
        case 44:
        case 45:
        case 47:
        case 49:
        case 50:
        case 53:
        case 54:
        case 293:
            return 1;
        default:
            return 0;
    }
}

const char *telemetry_lane_for_syscall(long syscall_no, int category) {
    if (is_ipc_syscall(syscall_no)) return "ipc";

    switch (category) {
        case 0: return "deterministic";
        case 1: return "non_deterministic";
        case 2: return "side_effect";
        default: return "deterministic";
    }
}

static void emit_common_prefix(TelemetryWriter *writer,
                               const char *kind,
                               const char *lane,
                               const char *category,
                               pid_t pid,
                               int has_trace_seq,
                               uint64_t trace_seq) {
    fprintf(writer->fp,
            "{\"seq\":%" PRIu64 ",\"traceSeq\":",
            writer->seq++);

    if (has_trace_seq) fprintf(writer->fp, "%" PRIu64, trace_seq);
    else fputs("null", writer->fp);

    fputs(",\"kind\":", writer->fp);
    emit_json_string(writer->fp, kind);
    fputs(",\"lane\":", writer->fp);
    emit_json_string(writer->fp, lane);
    fputs(",\"category\":", writer->fp);
    emit_json_string(writer->fp, category);
    fprintf(writer->fp, ",\"pid\":%d", (int)pid);
}

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
                            uint32_t actual_syscall) {
    if (!writer || !writer->fp) return;

    emit_common_prefix(writer,
                       "syscall",
                       telemetry_lane_for_syscall(syscall_no, category),
                       telemetry_category_name(category),
                       pid,
                       has_trace_seq,
                       trace_seq);

    fprintf(writer->fp, ",\"syscallNo\":%ld", syscall_no);
    fputs(",\"syscallName\":", writer->fp);
    emit_json_string(writer->fp, telemetry_syscall_name(syscall_no));

    fputs(",\"eventName\":", writer->fp);
    emit_json_string(writer->fp, telemetry_syscall_name(syscall_no));

    if (has_retval) fprintf(writer->fp, ",\"retval\":%" PRId64, retval);
    else fputs(",\"retval\":null", writer->fp);

    fprintf(writer->fp, ",\"payloadSize\":%u", payload_size);
    fprintf(writer->fp, ",\"diverged\":%s", diverged ? "true" : "false");

    if (diverged) {
        fprintf(writer->fp,
                ",\"expectedSyscallNo\":%u,\"actualSyscallNo\":%u",
                expected_syscall,
                actual_syscall);
        fputs(",\"expectedSyscallName\":", writer->fp);
        emit_json_string(writer->fp, telemetry_syscall_name(expected_syscall));
        fputs(",\"actualSyscallName\":", writer->fp);
        emit_json_string(writer->fp, telemetry_syscall_name(actual_syscall));
    }

    fputs("}\n", writer->fp);
    fflush(writer->fp);
}

void telemetry_emit_signal(TelemetryWriter *writer,
                           pid_t pid,
                           int signum,
                           int has_trace_seq,
                           uint64_t trace_seq) {
    if (!writer || !writer->fp) return;

    emit_common_prefix(writer,
                       "signal",
                       "signals",
                       "signal",
                       pid,
                       has_trace_seq,
                       trace_seq);

    fprintf(writer->fp, ",\"syscallNo\":%d", signum);
    fputs(",\"syscallName\":", writer->fp);
    emit_json_string(writer->fp, telemetry_signal_name(signum));
    fputs(",\"eventName\":", writer->fp);
    emit_json_string(writer->fp, telemetry_signal_name(signum));
    fputs(",\"retval\":null,\"payloadSize\":0,\"diverged\":false}\n", writer->fp);
    fflush(writer->fp);
}

void telemetry_emit_process(TelemetryWriter *writer,
                            pid_t pid,
                            int proc_code,
                            int64_t retval,
                            int has_retval,
                            int has_trace_seq,
                            uint64_t trace_seq) {
    if (!writer || !writer->fp) return;

    emit_common_prefix(writer,
                       "process",
                       "process",
                       "process",
                       pid,
                       has_trace_seq,
                       trace_seq);

    fprintf(writer->fp, ",\"syscallNo\":%d", proc_code);
    fputs(",\"syscallName\":", writer->fp);
    emit_json_string(writer->fp, telemetry_process_name(proc_code));
    fputs(",\"eventName\":", writer->fp);
    emit_json_string(writer->fp, telemetry_process_name(proc_code));

    if (has_retval) fprintf(writer->fp, ",\"retval\":%" PRId64, retval);
    else fputs(",\"retval\":null", writer->fp);

    fputs(",\"payloadSize\":0,\"diverged\":false}\n", writer->fp);
    fflush(writer->fp);
}

#include "sys_cat_info.h"
#include <sys/syscall.h>
#include <stddef.h>

typedef struct {
    long syscall_number;
    const char* name;
    SysCallCategory cat;
} system_info;

static const system_info syscall_cat[] = {
    { SYS_read, "read", NON_DET },
    { SYS_write, "write", SIDE_EFFECT },
    { SYS_open, "open", SIDE_EFFECT },
    { SYS_close, "close", SIDE_EFFECT },
    { SYS_stat, "stat", NON_DET },
    { SYS_fstat, "fstat", NON_DET },
    { SYS_lseek, "lseek", DET },
    { SYS_dup, "dup", SIDE_EFFECT },
    { SYS_fcntl, "fcntl", SIDE_EFFECT },
    { SYS_mmap, "mmap", NON_DET },
    { SYS_mprotect, "mprotect", SIDE_EFFECT },
    { SYS_munmap, "munmap", SIDE_EFFECT },
    { SYS_brk, "brk", NON_DET },
    { SYS_clone, "clone", SIDE_EFFECT },
    { SYS_execve, "execve", SIDE_EFFECT },
    { SYS_exit, "exit", SIDE_EFFECT },
    { SYS_exit_group, "exit_group", SIDE_EFFECT },
    { SYS_wait4, "wait4", NON_DET },
    { SYS_kill, "kill", SIDE_EFFECT },
    { SYS_getpid, "getpid", NON_DET },
    { SYS_getuid, "getuid", NON_DET },
    //CHANGE
    { SYS_getrandom, "getrandom", NON_DET },
    //CHANGE
    { SYS_socket, "socket", SIDE_EFFECT },
    { SYS_connect, "connect", SIDE_EFFECT },
    { SYS_accept, "accept", NON_DET },
    { SYS_sendto, "sendto", SIDE_EFFECT },
    { SYS_recvfrom, "recvfrom", NON_DET },
    { SYS_gettimeofday, "gettimeofday", NON_DET },
    { SYS_clock_gettime, "clock_gettime", NON_DET},
    { SYS_uname, "uname", NON_DET },
    { SYS_rt_sigaction, "rt_sigaction", SIDE_EFFECT },
    { SYS_rt_sigprocmask, "rt_sigprocmask", SIDE_EFFECT }
};

#define TABLE_SIZE (sizeof(syscall_cat)/sizeof(syscall_cat[0]))

SysCallCategory syscall_classify(long orig_rax) {
    for(size_t i = 0; i < TABLE_SIZE; i++) {
        if(syscall_cat[i].syscall_number == orig_rax) return syscall_cat[i].cat;
    }
    return UNKNOWN;
}

const char* get_cat_name(SysCallCategory cat) {
    switch(cat) {
        case DET: return "DET";
        case NON_DET: return "NON_DET";
        case SIDE_EFFECT: return "SIDE_EFFECT";
        default: return "UNKNOWN";
    }
}

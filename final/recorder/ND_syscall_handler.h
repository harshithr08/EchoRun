#ifndef ND_SYSCALL_HANDLER_H
#define ND_SYSCALL_HANDLER_H

#include <sys/types.h>
#include <stdint.h>

//CHANGE
int handle_non_deterministic_exit(pid_t pid,
                                  int syscallnr,
                                  uint64_t seq_idx,
                                  uint32_t *payload_size_out);
//CHANGE

#endif

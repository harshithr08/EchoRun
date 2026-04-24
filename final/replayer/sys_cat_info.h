#ifndef SYS_CAT_INFO_H
#define SYS_CAT_INFO_H

typedef enum {
    DET,
    NON_DET,
    SIDE_EFFECT,
    UNKNOWN
} SysCallCategory;

// Returns the category of a given syscall number
SysCallCategory syscall_classify(long orig_rax);

// Returns a string representation of the category
const char* get_cat_name(SysCallCategory cat);

#endif
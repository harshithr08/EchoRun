#include <stdio.h>
#include <stdlib.h>
#include "trace_reader.h"
#include "echotrace_bin.h"
#include <inttypes.h>


int main() {
    // Opens both the bin and idx files
    TraceReader *tr = open_trace("trace.bin", "trace.idx");
    if (!tr) {
        printf("Failed to open trace files.\n");
        return 1;
    }

    printf("Trace loaded successfully.\n");
    printf("Total Events Recorded: %" PRIu64 "\n", tr->header.event_count);
    printf("--------------------------------------------------\n");

    struct EventRecorder ev;
    uint8_t *data = NULL;
    
    // Loop through and read all events
    while (read_event(tr, &ev, &data) == 0) {
        printf("Seq Idx: %" PRIu64 " | Event Type: %u | Syscall No: %u | Retval: %" PRId64 " | Data Len: %u\n",
       ev.seq_idx, ev.event_type, ev.syscall_no, ev.retval, ev.data_len);
               
        // If data was saved (like from your SYS_read intercept), print it
        if (ev.data_len > 0 && data != NULL) {
            printf("   -> Intercepted Data: ");
            for (uint32_t i = 0; i < ev.data_len; i++) {
                // Print alphanumeric characters, else print a dot
                if (data[i] >= 32 && data[i] <= 126) printf("%c", data[i]);
                else printf(".");
            }
            printf("\n");
            free(data); 
        }
    }
    
    close_trace(tr);
    return 0;
}
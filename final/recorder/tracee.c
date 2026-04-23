#include <stdio.h>
#include <unistd.h>

int main() {
    printf("Starting tracee...\n");
    char buf[16];
    
    // This will trigger SYS_read, which is NON_DET and will be intercepted
    printf("Enter some text: ");
    ssize_t bytes = read(STDIN_FILENO, buf, 10); 
    
    printf("\nTracee read %zd bytes.\n", bytes);
    return 0;
}
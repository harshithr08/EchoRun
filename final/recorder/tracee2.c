#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <sys/random.h>

int main() {
    // 1. Read random bytes (getrandom syscall — NON_DET)
    unsigned char rng[4];
    ssize_t r = getrandom(rng, sizeof(rng), 0);
    printf("getrandom returned %zd bytes: %02x %02x %02x %02x\n",
           r, rng[0], rng[1], rng[2], rng[3]);

    // 2. Read from a file (read syscall — NON_DET)
    int fd = open("/etc/hostname", O_RDONLY);
    if (fd >= 0) {
        char buf[64] = {0};
        ssize_t n = read(fd, buf, sizeof(buf) - 1);
        if (n > 0) {
            buf[n] = '\0';
            // strip newline
            if (buf[n-1] == '\n') buf[n-1] = '\0';
            printf("hostname: %s\n", buf);
        }
        close(fd);
    }

    // 3. Read from stdin (read syscall — NON_DET)
    printf("Enter a word: ");
    fflush(stdout);
    char input[32] = {0};
    ssize_t n = read(STDIN_FILENO, input, sizeof(input) - 1);
    if (n > 0) {
        input[n] = '\0';
        printf("You typed: %s", input);
    }

    printf("Done.\n");
    return 0;
}
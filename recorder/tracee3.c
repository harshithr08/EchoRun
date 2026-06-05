// TEST CASE 3 — File I/O
// Tests: open, write, read, close chain
// All read() calls are NON_DET — recorder saves them, replayer injects them.
// During replay, no real file I/O happens — data comes from trace.

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>

int main() {
    const char *path = "/tmp/echorun_test.txt";

    // 1. Write something to a file
    int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) { perror("open write"); return 1; }
    const char *msg = "EchoRun file IO test\n";
    write(fd, msg, strlen(msg));
    close(fd);

    // 2. Read it back (NON_DET — this is what gets recorded)
    fd = open(path, O_RDONLY);
    if (fd < 0) { perror("open read"); return 1; }
    char buf[64] = {0};
    ssize_t n = read(fd, buf, sizeof(buf) - 1);
    if (n > 0) {
        buf[n] = '\0';
        printf("Read %zd bytes from file: %s", n, buf);
    }
    close(fd);

    // 3. Read a second file (NON_DET — tests multiple read injections)
    fd = open("/etc/os-release", O_RDONLY);
    if (fd >= 0) {
        char info[128] = {0};
        n = read(fd, info, sizeof(info) - 1);
        if (n > 0) {
            // just print first line
            char *newline = strchr(info, '\n');
            if (newline) *newline = '\0';
            printf("OS info: %s\n", info);
        }
        close(fd);
    }

    printf("File IO test done.\n");
    return 0;
}
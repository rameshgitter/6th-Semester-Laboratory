#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>

#define TOTAL_BLOCKS (8 + 2048)
#define BLOCK_SIZE 4096

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s dd1\n", argv[0]);
        exit(1);
    }

    const char *dd1 = argv[1];
    int fd = open(dd1, O_RDWR | O_CREAT | O_TRUNC, 0644);
    if (fd == -1) {
        perror("open");
        exit(1);
    }

    off_t size = TOTAL_BLOCKS * BLOCK_SIZE;
    if (ftruncate(fd, size) == -1) {
        perror("ftruncate");
        close(fd);
        exit(1);
    }

    close(fd);
    return 0;
}

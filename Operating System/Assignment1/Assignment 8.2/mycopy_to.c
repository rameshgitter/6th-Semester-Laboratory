#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <stdint.h>

#define METADATA_BLOCKS 8
#define BLOCK_SIZE 4096
#define MAX_FILENAME_LEN 12
#define METADATA_ENTRIES_PER_BLOCK (BLOCK_SIZE / 16)
#define TOTAL_METADATA_ENTRIES (METADATA_BLOCKS * METADATA_ENTRIES_PER_BLOCK)

typedef struct __attribute__((packed)) {
    char name[MAX_FILENAME_LEN];
    uint32_t size;
} MetadataEntry;

int find_empty_entry(int fd) {
    MetadataEntry entry;
    for (int i = 0; i < TOTAL_METADATA_ENTRIES; i++) {
        off_t block = i / METADATA_ENTRIES_PER_BLOCK;
        off_t offset_in_block = (i % METADATA_ENTRIES_PER_BLOCK) * sizeof(MetadataEntry);
        off_t offset = block * BLOCK_SIZE + offset_in_block;
        if (pread(fd, &entry, sizeof(entry), offset) != sizeof(entry)) {
            perror("pread");
            return -1;
        }
        if (entry.name[0] == '\0') {
            return i;
        }
    }
    return -1;
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s sourcefile dd1\n", argv[0]);
        exit(1);
    }

    const char *sourcefile = argv[1];
    const char *dd1 = argv[2];

    FILE *src = fopen(sourcefile, "rb");
    if (!src) {
        perror("fopen");
        exit(1);
    }

    fseek(src, 0, SEEK_END);
    long file_size = ftell(src);
    if (file_size > BLOCK_SIZE) {
        fprintf(stderr, "File too large\n");
        fclose(src);
        exit(1);
    }
    fseek(src, 0, SEEK_SET);

    char buffer[BLOCK_SIZE] = {0};
    if (fread(buffer, 1, file_size, src) != (size_t)file_size) {
        perror("fread");
        fclose(src);
        exit(1);
    }
    fclose(src);

    int fd = open(dd1, O_RDWR);
    if (fd == -1) {
        perror("open");
        exit(1);
    }

    int entry_index = find_empty_entry(fd);
    if (entry_index == -1) {
        fprintf(stderr, "No space in dd1\n");
        close(fd);
        exit(1);
    }

    const char *filename = strrchr(sourcefile, '/');
    filename = filename ? filename + 1 : sourcefile;

    if (strlen(filename) > MAX_FILENAME_LEN) {
        fprintf(stderr, "Filename too long\n");
        close(fd);
        exit(1);
    }

    MetadataEntry entry;
    memset(&entry, 0, sizeof(entry));
    strncpy(entry.name, filename, MAX_FILENAME_LEN);
    entry.size = file_size;

    off_t metadata_offset = (entry_index / METADATA_ENTRIES_PER_BLOCK) * BLOCK_SIZE;
    metadata_offset += (entry_index % METADATA_ENTRIES_PER_BLOCK) * sizeof(entry);
    if (pwrite(fd, &entry, sizeof(entry), metadata_offset) != sizeof(entry)) {
        perror("pwrite metadata");
        close(fd);
        exit(1);
    }

    off_t data_offset = (METADATA_BLOCKS + entry_index) * BLOCK_SIZE;
    if (pwrite(fd, buffer, BLOCK_SIZE, data_offset) != BLOCK_SIZE) {
        perror("pwrite data");
        close(fd);
        exit(1);
    }

    close(fd);
    return 0;
}

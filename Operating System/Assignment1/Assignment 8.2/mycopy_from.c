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

int find_entry_by_name(int fd, const char *filename, MetadataEntry *out_entry, int *out_index) {
    char target_name[MAX_FILENAME_LEN] = {0};
    strncpy(target_name, filename, MAX_FILENAME_LEN);

    MetadataEntry entry;
    for (int i = 0; i < TOTAL_METADATA_ENTRIES; i++) {
        off_t block = i / METADATA_ENTRIES_PER_BLOCK;
        off_t offset_in_block = (i % METADATA_ENTRIES_PER_BLOCK) * sizeof(MetadataEntry);
        off_t offset = block * BLOCK_SIZE + offset_in_block;
        if (pread(fd, &entry, sizeof(entry), offset) != sizeof(entry)) {
            perror("pread");
            return -1;
        }
        if (entry.name[0] != '\0' && memcmp(entry.name, target_name, MAX_FILENAME_LEN) == 0) {
            *out_entry = entry;
            *out_index = i;
            return 0;
        }
    }
    return -1;
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s filename dd1\n", argv[0]);
        exit(1);
    }

    const char *filename = argv[1];
    const char *dd1 = argv[2];

    int fd = open(dd1, O_RDONLY);
    if (fd == -1) {
        perror("open");
        exit(1);
    }

    MetadataEntry entry;
    int entry_index;
    if (find_entry_by_name(fd, filename, &entry, &entry_index) == -1) {
        fprintf(stderr, "File not found\n");
        close(fd);
        exit(1);
    }

    char buffer[BLOCK_SIZE];
    off_t data_offset = (METADATA_BLOCKS + entry_index) * BLOCK_SIZE;
    if (pread(fd, buffer, entry.size, data_offset) != entry.size) {
        perror("pread data");
        close(fd);
        exit(1);
    }

    FILE *dest = fopen(entry.name, "wb");
    if (!dest) {
        perror("fopen");
        close(fd);
        exit(1);
    }
    if (fwrite(buffer, 1, entry.size, dest) != entry.size) {
        perror("fwrite");
        fclose(dest);
        close(fd);
        exit(1);
    }
    fclose(dest);
    close(fd);
    return 0;
}

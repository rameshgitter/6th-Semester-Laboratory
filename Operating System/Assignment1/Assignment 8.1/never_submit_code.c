#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define HEADER_SIZE 4096

// Function prototypes
int init_File_dd(const char *fname, int bsize, int bno);
int get_freeblock(const char *fname);
int free_block(const char *fname, int bno);
int check_fs(const char *fname);

// Helper function to count the number of set bits in the bitmap
static int count_set_bits(const unsigned char *ub, int n) {
    int count = 0;
    for (int i = 0; i < n; i++) {
        int byte_idx = i / 8;
        int bit_idx = i % 8;
        if (ub[byte_idx] & (1 << bit_idx)) {
            count++;
        }
    }
    return count;
}

int init_File_dd(const char *fname, int bsize, int bno) {
    // Check if bitmap can accommodate bno blocks
    if (bno > 32640) { // 4080 bytes * 8 bits = 32640 bits
        return -1;
    }

    // Create and open the file
    FILE *fp = fopen(fname, "wb+");
    if (!fp) {
        return -1;
    }

    // Initialize header
    unsigned char header[HEADER_SIZE] = {0};
    int *meta = (int *)header;
    meta[0] = bno; // n
    meta[1] = bsize; // s
    meta[2] = 0; // ubn
    meta[3] = bno; // fbn

    // Write header to file
    if (fwrite(header, 1, HEADER_SIZE, fp) != HEADER_SIZE) {
        fclose(fp);
        return -1;
    }

    // Calculate data size and write zeros to data blocks
    size_t data_size = (size_t)bsize * bno;
    unsigned char *zeros = (unsigned char *)calloc(data_size, 1);
    if (!zeros) {
        fclose(fp);
        return -1;
    }
    if (fwrite(zeros, 1, data_size, fp) != data_size) {
        free(zeros);
        fclose(fp);
        return -1;
    }
    free(zeros);

    fclose(fp);
    return 0;
}

int get_freeblock(const char *fname) {
    FILE *fp = fopen(fname, "rb+");
    if (!fp) {
        return -1;
    }

    // Read header
    unsigned char header[HEADER_SIZE];
    if (fread(header, 1, HEADER_SIZE, fp) != HEADER_SIZE) {
        fclose(fp);
        return -1;
    }

    int *meta = (int *)header;
    int n = meta[0];
    int s = meta[1];
    int ubn = meta[2];
    int fbn = meta[3];

    if (ubn >= n) {
        fclose(fp);
        return -1;
    }

    // Find first free block
    int block = -1;
    for (int i = 0; i < n; i++) {
        int byte_idx = i / 8;
        int bit_idx = i % 8;
        if (!(header[16 + byte_idx] & (1 << bit_idx))) {
            block = i;
            break;
        }
    }

    if (block == -1) {
        fclose(fp);
        return -1;
    }

    // Update bitmap
    int byte_idx = block / 8;
    int bit_idx = block % 8;
    header[16 + byte_idx] |= (1 << bit_idx);

    // Update metadata
    meta[2] = ubn + 1;
    meta[3] = fbn - 1;

    // Write back header
    rewind(fp);
    if (fwrite(header, 1, HEADER_SIZE, fp) != HEADER_SIZE) {
        fclose(fp);
        return -1;
    }

    // Fill the block with 1's
    if (fseek(fp, HEADER_SIZE + block * s, SEEK_SET) != 0) {
        fclose(fp);
        return -1;
    }

    unsigned char *ones = (unsigned char *)malloc(s);
    if (!ones) {
        fclose(fp);
        return -1;
    }
    memset(ones, 0xFF, s);
    if (fwrite(ones, 1, s, fp) != s) {
        free(ones);
        fclose(fp);
        return -1;
    }
    free(ones);

    fclose(fp);
    return block;
}

int free_block(const char *fname, int bno) {
    FILE *fp = fopen(fname, "rb+");
    if (!fp) {
        return 0;
    }

    unsigned char header[HEADER_SIZE];
    if (fread(header, 1, HEADER_SIZE, fp) != HEADER_SIZE) {
        fclose(fp);
        return 0;
    }

    int *meta = (int *)header;
    int n = meta[0];
    int s = meta[1];
    int ubn = meta[2];
    int fbn = meta[3];

    if (bno < 0 || bno >= n) {
        fclose(fp);
        return 0;
    }

    int byte_idx = bno / 8;
    int bit_idx = bno % 8;
    unsigned char *ub = header + 16;
    if (!(ub[byte_idx] & (1 << bit_idx))) {
        fclose(fp);
        return 0;
    }

    // Update bitmap
    ub[byte_idx] &= ~(1 << bit_idx);
    meta[2] = ubn - 1;
    meta[3] = fbn + 1;

    // Write back header
    rewind(fp);
    if (fwrite(header, 1, HEADER_SIZE, fp) != HEADER_SIZE) {
        fclose(fp);
        return 0;
    }

    // Fill the block with 0's
    if (fseek(fp, HEADER_SIZE + bno * s, SEEK_SET) != 0) {
        fclose(fp);
        return 0;
    }

    unsigned char *zeros = (unsigned char *)malloc(s);
    if (!zeros) {
        fclose(fp);
        return 0;
    }
    memset(zeros, 0, s);
    if (fwrite(zeros, 1, s, fp) != s) {
        free(zeros);
        fclose(fp);
        return 0;
    }
    free(zeros);

    fclose(fp);
    return 1;
}

int check_fs(const char *fname) {
    FILE *fp = fopen(fname, "rb");
    if (!fp) {
        return 1;
    }

    unsigned char header[HEADER_SIZE];
    if (fread(header, 1, HEADER_SIZE, fp) != HEADER_SIZE) {
        fclose(fp);
        return 1;
    }

    int *meta = (int *)header;
    int n = meta[0];
    int s = meta[1];
    int ubn = meta[2];
    int fbn = meta[3];

    // Check ubn + fbn == n
    if (ubn + fbn != n) {
        fclose(fp);
        return 1;
    }

    // Check number of set bits in ub matches ubn
    unsigned char *ub = header + 16;
    int count = count_set_bits(ub, n);
    if (count != ubn) {
        fclose(fp);
        return 1;
    }

    // Check each block's content
    unsigned char *block = (unsigned char *)malloc(s);
    if (!block) {
        fclose(fp);
        return 1;
    }

    for (int i = 0; i < n; i++) {
        if (fseek(fp, HEADER_SIZE + i * s, SEEK_SET) != 0) {
            free(block);
            fclose(fp);
            return 1;
        }

        if (fread(block, 1, s, fp) != s) {
            free(block);
            fclose(fp);
            return 1;
        }

        // Determine expected value based on bitmap
        int byte_idx = i / 8;
        int bit_idx = i % 8;
        int is_used = (ub[byte_idx] >> bit_idx) & 1;
        unsigned char expected = is_used ? 0xFF : 0x00;

        for (int j = 0; j < s; j++) {
            if (block[j] != expected) {
                free(block);
                fclose(fp);
                return 1;
            }
        }
    }

    free(block);
    fclose(fp);
    return 0;
}

int main() {
    const char *filename = "test.dd";
    int block_size = 4096;
    int num_blocks = 10;

    // Initialize the file
    if (init_File_dd(filename, block_size, num_blocks) != 0) {
        printf("Initialization failed.\n");
        return 1;
    }
    printf("File initialized successfully.\n");

    // Check file system integrity
    if (check_fs(filename) != 0) {
        printf("Integrity check failed after initialization.\n");
        return 1;
    }
    printf("Integrity check passed after initialization.\n");

    // Allocate a free block
    int block_num = get_freeblock(filename);
    if (block_num == -1) {
        printf("Failed to allocate a free block.\n");
        return 1;
    }
    printf("Allocated block number: %d\n", block_num);

    // Check integrity after allocation
    if (check_fs(filename) != 0) {
        printf("Integrity check failed after allocation.\n");
        return 1;
    }
    printf("Integrity check passed after allocation.\n");

    // Free the allocated block
    if (!free_block(filename, block_num)) {
        printf("Failed to free block %d.\n", block_num);
        return 1;
    }
    printf("Freed block number: %d\n", block_num);

    // Check integrity after freeing
    if (check_fs(filename) != 0) {
        printf("Integrity check failed after freeing block.\n");
        return 1;
    }
    printf("Integrity check passed after freeing block.\n");

    printf("All operations completed successfully.\n");
    return 0;
}

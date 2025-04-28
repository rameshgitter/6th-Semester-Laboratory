#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>

// -----------------------------------------------
// Constants
// -----------------------------------------------
#define METADATA_SIZE 4096      // First 4096 bytes reserved for superblock
#define MAX_BLOCKS    2048      // For example, we allow up to 2048 blocks
                                // (You can adapt this if needed)

// -----------------------------------------------
// Data Structures
// -----------------------------------------------
#pragma pack(push, 1)
typedef struct {
    int n;      // total number of data blocks
    int s;      // size of each data block (bytes)
    int ubn;    // number of used blocks
    int fbn;    // number of free blocks
    // 'ub' is a bitmap array for up to MAX_BLOCKS
    // Each bit in ub corresponds to one block: 1 = used, 0 = free
    unsigned char ub[MAX_BLOCKS/8]; // 2048 blocks => 2048 bits => 256 bytes
} superblock_t;
#pragma pack(pop)

// -----------------------------------------------
// Function Prototypes
// -----------------------------------------------
int init_File_dd(const char *fname, int bsize, int bno);
int get_freeblock(const char *fname);
int free_block(const char *fname, int bno);
int check_fs(const char *fname);

// Helper functions
static int read_superblock(FILE *fp, superblock_t *sb);
static int write_superblock(FILE *fp, const superblock_t *sb);
static void set_bit(unsigned char *bitmap, int bno);
static void clear_bit(unsigned char *bitmap, int bno);
static int test_bit(const unsigned char *bitmap, int bno);
static int count_used_bits(const unsigned char *bitmap, int total_blocks);

// -----------------------------------------------
// 1) init_File_dd
// -----------------------------------------------
int init_File_dd(const char *fname, int bsize, int bno)
{
    // Compute total file size
    // first 4096 bytes (metadata) + (bno * bsize)
    long total_size = METADATA_SIZE + (long)bno * bsize;

    // Open file (create if not exist, truncate to 0 length, then set size)
    FILE *fp = fopen(fname, "wb+");
    if (!fp) {
        perror("fopen");
        return -1;
    }

    // Attempt to set the file to the required total size
    // One approach: Seek to (total_size-1) and write a single 0 byte.
    if (fseek(fp, total_size - 1, SEEK_SET) != 0) {
        perror("fseek");
        fclose(fp);
        return -1;
    }
    if (fputc(0, fp) == EOF) {
        perror("fputc");
        fclose(fp);
        return -1;
    }

    // Now file has the required size. Next, fill in the superblock.
    superblock_t sb;
    memset(&sb, 0, sizeof(sb));
    sb.n   = bno;
    sb.s   = bsize;
    sb.ubn = 0;     // used blocks = 0
    sb.fbn = bno;   // free blocks = n initially
    // ub[] is already zeroed by memset, so all bits = 0 => all blocks free

    // Write the superblock to the first 4096 bytes
    if (fseek(fp, 0, SEEK_SET) != 0) {
        perror("fseek");
        fclose(fp);
        return -1;
    }
    if (fwrite(&sb, sizeof(sb), 1, fp) != 1) {
        perror("fwrite");
        fclose(fp);
        return -1;
    }

    // Done
    fclose(fp);
    return 0;
}

// -----------------------------------------------
// 2) get_freeblock
// -----------------------------------------------
int get_freeblock(const char *fname)
{
    FILE *fp = fopen(fname, "r+b");
    if (!fp) {
        perror("fopen");
        return -1;
    }

    // Read superblock
    superblock_t sb;
    if (read_superblock(fp, &sb) < 0) {
        fclose(fp);
        return -1;
    }

    // Find the first free block in the bitmap
    int free_bno = -1;
    for (int i = 0; i < sb.n; i++) {
        if (!test_bit(sb.ub, i)) { // bit == 0 => free
            free_bno = i;
            break;
        }
    }

    if (free_bno == -1) {
        // No free block found
        fclose(fp);
        return -1;
    }

    // Mark that block as used
    set_bit(sb.ub, free_bno);
    sb.ubn += 1;
    sb.fbn -= 1;

    // Write back the updated superblock
    if (write_superblock(fp, &sb) < 0) {
        fclose(fp);
        return -1;
    }

    // Fill that block with 1's (0xFF) to show it's now used
    // Calculate offset: 4096 + free_bno * s
    long block_offset = METADATA_SIZE + (long)free_bno * sb.s;
    if (fseek(fp, block_offset, SEEK_SET) != 0) {
        perror("fseek");
        fclose(fp);
        return -1;
    }

    // Create a buffer of size s, filled with 0xFF
    unsigned char *buf = (unsigned char*)malloc(sb.s);
    if (!buf) {
        fprintf(stderr, "Memory allocation failed\n");
        fclose(fp);
        return -1;
    }
    memset(buf, 0xFF, sb.s);

    if (fwrite(buf, sb.s, 1, fp) != 1) {
        perror("fwrite block");
        free(buf);
        fclose(fp);
        return -1;
    }

    free(buf);
    fclose(fp);

    // Return the block number that was allocated
    return free_bno;
}

// -----------------------------------------------
// 3) free_block
// -----------------------------------------------
int free_block(const char *fname, int bno)
{
    FILE *fp = fopen(fname, "r+b");
    if (!fp) {
        perror("fopen");
        return -1;
    }

    // Read superblock
    superblock_t sb;
    if (read_superblock(fp, &sb) < 0) {
        fclose(fp);
        return -1;
    }

    // Check if bno is valid
    if (bno < 0 || bno >= sb.n) {
        fprintf(stderr, "Invalid block number\n");
        fclose(fp);
        return 0; // or -1
    }

    // Check if this block is currently used
    if (!test_bit(sb.ub, bno)) {
        // It's already free, so do nothing
        fclose(fp);
        return 0;
    }

    // Mark the block as free
    clear_bit(sb.ub, bno);
    sb.ubn -= 1;
    sb.fbn += 1;

    // Update superblock on disk
    if (write_superblock(fp, &sb) < 0) {
        fclose(fp);
        return -1;
    }

    // Fill the block with zeros
    long block_offset = METADATA_SIZE + (long)bno * sb.s;
    if (fseek(fp, block_offset, SEEK_SET) != 0) {
        perror("fseek");
        fclose(fp);
        return -1;
    }

    unsigned char *buf = (unsigned char*)calloc(sb.s, 1); // s bytes of 0x00
    if (!buf) {
        fprintf(stderr, "Memory allocation failed\n");
        fclose(fp);
        return -1;
    }

    if (fwrite(buf, sb.s, 1, fp) != 1) {
        perror("fwrite block");
        free(buf);
        fclose(fp);
        return -1;
    }

    free(buf);
    fclose(fp);

    return 1; // success
}

// -----------------------------------------------
// 4) check_fs
// -----------------------------------------------
int check_fs(const char *fname)
{
    FILE *fp = fopen(fname, "rb");
    if (!fp) {
        perror("fopen");
        return 1; // can't open => fail
    }

    superblock_t sb;
    if (read_superblock(fp, &sb) < 0) {
        fclose(fp);
        return 1;
    }

    // Basic consistency checks
    if ((sb.ubn + sb.fbn) != sb.n) {
        fclose(fp);
        return 1; // mismatch in used+free vs total
    }

    // Count how many bits are set in sb.ub
    int used_count = count_used_bits(sb.ub, sb.n);
    if (used_count != sb.ubn) {
        fclose(fp);
        return 1; // mismatch in actual used bits
    }

    // If we pass all checks, we consider it consistent
    fclose(fp);
    return 0; // 0 => no inconsistency
}

// -----------------------------------------------
// HELPER FUNCTIONS
// -----------------------------------------------
static int read_superblock(FILE *fp, superblock_t *sb)
{
    if (fseek(fp, 0, SEEK_SET) != 0) {
        perror("fseek");
        return -1;
    }
    if (fread(sb, sizeof(*sb), 1, fp) != 1) {
        perror("fread superblock");
        return -1;
    }
    return 0;
}

static int write_superblock(FILE *fp, const superblock_t *sb)
{
    if (fseek(fp, 0, SEEK_SET) != 0) {
        perror("fseek");
        return -1;
    }
    if (fwrite(sb, sizeof(*sb), 1, fp) != 1) {
        perror("fwrite superblock");
        return -1;
    }
    return 0;
}

// bit manipulation
static void set_bit(unsigned char *bitmap, int bno)
{
    bitmap[bno / 8] |= (1 << (bno % 8));
}

static void clear_bit(unsigned char *bitmap, int bno)
{
    bitmap[bno / 8] &= ~(1 << (bno % 8));
}

static int test_bit(const unsigned char *bitmap, int bno)
{
    return (bitmap[bno / 8] & (1 << (bno % 8))) != 0;
}

// Count how many bits are set to 1 in the first 'total_blocks' bits
static int count_used_bits(const unsigned char *bitmap, int total_blocks)
{
    int count = 0;
    for (int i = 0; i < total_blocks; i++) {
        if (test_bit(bitmap, i)) {
            count++;
        }
    }
    return count;
}

// -----------------------------------------------
// DEMO main()
// -----------------------------------------------
int main(void)
{
    // Example usage and demonstration
    const char *fname = "dd1";
    int block_size = 4096;  // s
    int block_count = 8;    // n (for a small demo, let's do 8 blocks)

    // 1) Initialize the file
    if (init_File_dd(fname, block_size, block_count) != 0) {
        fprintf(stderr, "init_File_dd failed.\n");
        return 1;
    }
    printf("File %s initialized with %d blocks of %d bytes each.\n",
           fname, block_count, block_size);

    // 2) Allocate a free block
    int bno = get_freeblock(fname);
    if (bno < 0) {
        fprintf(stderr, "get_freeblock failed.\n");
        return 1;
    }
    printf("Allocated block number: %d\n", bno);

    // 3) Allocate another free block
    int bno2 = get_freeblock(fname);
    printf("Allocated another block: %d\n", bno2);

    // 4) Free the first block
    int freed = free_block(fname, bno);
    printf("Freeing block %d => returned %d\n", bno, freed);

    // 5) Check file system consistency
    int check = check_fs(fname);
    if (check == 0) {
        printf("Filesystem check: OK\n");
    } else {
        printf("Filesystem check: INCONSISTENT\n");
    }

    return 0;
}


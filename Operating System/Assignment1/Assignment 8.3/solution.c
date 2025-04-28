#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

// Constants
#define BLOCK_SIZE 4096
// Last 4 bytes reserved for pointer to next block; effective data bytes per block:
#define DATA_SIZE (BLOCK_SIZE - sizeof(uint32_t))
#define MAX_NAME_LEN 12

// File types
#define TYPE_FILE 'f'
#define TYPE_FOLDER 'd'

// Superblock structure stored in block 0 of dd1.
typedef struct {
    uint32_t total_blocks;      // Total blocks in our file system
    uint32_t free_block_count;  // Number of free blocks
    uint32_t root_dir_block;    // Block number where the root directory is stored (here, block 1)
} SuperBlock;

// Directory entry (or file descriptor) is exactly 21 bytes.
// We use packing to prevent any compiler-added padding.
#pragma pack(push, 1)
typedef struct {
    char name[MAX_NAME_LEN];  // 12 bytes: file/folder name (if shorter, unused bytes are 0)
    uint8_t type;             // 1 byte: 'f' for file, 'd' for folder
    uint32_t start_block;     // 4 bytes: starting block number of file/folder data
    uint32_t size;            // 4 bytes: size in bytes (for file) or total size of descriptor list (for folder)
} MyFSEntry;
#pragma pack(pop)

// Function prototypes for our four commands.
int mymkfs(const char *linuxfile);
int mycopyto(const char *linuxfile, const char *myfspath);
int mycopyfrom(const char *myfspath, const char *linuxfile);
int myrm(const char *myfspath);

// Helper functions for block-level I/O.
uint32_t allocate_block(FILE *fp, SuperBlock *sb);
void free_block(FILE *fp, SuperBlock *sb, uint32_t block_num);
int read_block(FILE *fp, uint32_t block_num, void *buffer);
int write_block(FILE *fp, uint32_t block_num, const void *buffer);

// Main parses command-line arguments and calls the appropriate function.
int main(int argc, char *argv[]) {
    if (argc < 3) {
        fprintf(stderr, "Usage: <command> <args>\n");
        exit(1);
    }
    
    if (strcmp(argv[1], "mymkfs") == 0) {
        return mymkfs(argv[2]);
    } else if (strcmp(argv[1], "mycopyto") == 0) {
        if (argc < 4) {
            fprintf(stderr, "Usage: mycopyto <linuxfile> <myfspath>\n");
            exit(1);
        }
        return mycopyto(argv[2], argv[3]);
    } else if (strcmp(argv[1], "mycopyfrom") == 0) {
        if (argc < 4) {
            fprintf(stderr, "Usage: mycopyfrom <myfspath> <linuxfile>\n");
            exit(1);
        }
        return mycopyfrom(argv[2], argv[3]);
    } else if (strcmp(argv[1], "myrm") == 0) {
        return myrm(argv[2]);
    } else {
        fprintf(stderr, "Unknown command: %s\n", argv[1]);
        exit(1);
    }
    
    return 0;
}

/*
 * mymkfs: Formats the given Linux file (dd1) as a new myfs.
 * - Creates a file system with a fixed number of blocks (here, 1000 as an example).
 * - Writes the superblock in block 0 and initializes an empty root directory in block 1.
 */
int mymkfs(const char *linuxfile) {
    FILE *fp = fopen(linuxfile, "w+b");
    if (!fp) {
        perror("fopen");
        return -1;
    }
    
    // For demonstration, we create a file system with 1000 blocks.
    uint32_t total_blocks = 1000;
    
    SuperBlock sb;
    sb.total_blocks = total_blocks;
    // Reserve block 0 (superblock) and block 1 (root directory).
    sb.free_block_count = total_blocks - 2;
    sb.root_dir_block = 1;
    
    // Write the superblock to block 0.
    fseek(fp, 0, SEEK_SET);
    if (fwrite(&sb, sizeof(SuperBlock), 1, fp) != 1) {
        perror("fwrite");
        fclose(fp);
        return -1;
    }
    
    // Initialize an empty root directory block (all bytes zero).
    char root_block[BLOCK_SIZE];
    memset(root_block, 0, BLOCK_SIZE);
    fseek(fp, BLOCK_SIZE, SEEK_SET);  // Block 1
    if (fwrite(root_block, BLOCK_SIZE, 1, fp) != 1) {
        perror("fwrite");
        fclose(fp);
        return -1;
    }
    
    fclose(fp);
    printf("File system created with %u blocks.\n", total_blocks);
    return 0;
}

/*
 * read_block: Reads block number 'block_num' from dd1 into the provided buffer.
 */
int read_block(FILE *fp, uint32_t block_num, void *buffer) {
    if (fseek(fp, block_num * BLOCK_SIZE, SEEK_SET) != 0) {
        perror("fseek");
        return -1;
    }
    if (fread(buffer, BLOCK_SIZE, 1, fp) != 1) {
        perror("fread");
        return -1;
    }
    return 0;
}

/*
 * write_block: Writes the provided buffer to block number 'block_num' in dd1.
 */
int write_block(FILE *fp, uint32_t block_num, const void *buffer) {
    if (fseek(fp, block_num * BLOCK_SIZE, SEEK_SET) != 0) {
        perror("fseek");
        return -1;
    }
    if (fwrite(buffer, BLOCK_SIZE, 1, fp) != 1) {
        perror("fwrite");
        return -1;
    }
    return 0;
}

/*
 * allocate_block: Naively searches for a free block (all bytes zero)
 * starting from block 2 (since blocks 0 and 1 are reserved).
 * When found, it “allocates” the block by initializing it (and sets its
 * next pointer to 0) then updates the superblock's free count.
 */
uint32_t allocate_block(FILE *fp, SuperBlock *sb) {
    char buffer[BLOCK_SIZE];
    for (uint32_t i = 2; i < sb->total_blocks; i++) {
        if (read_block(fp, i, buffer) != 0)
            continue;
        int free = 1;
        for (int j = 0; j < BLOCK_SIZE; j++) {
            if (buffer[j] != 0) {
                free = 0;
                break;
            }
        }
        if (free) {
            memset(buffer, 0, BLOCK_SIZE);
            uint32_t next = 0;
            memcpy(buffer + BLOCK_SIZE - sizeof(uint32_t), &next, sizeof(uint32_t));
            write_block(fp, i, buffer);
            sb->free_block_count--;
            // Update the superblock on disk.
            fseek(fp, 0, SEEK_SET);
            fwrite(sb, sizeof(SuperBlock), 1, fp);
            return i;
        }
    }
    return 0; // Return 0 on failure (block 0 is reserved, so 0 is invalid)
}

/*
 * free_block: Releases a block by zeroing it and updating the free block count.
 */
void free_block(FILE *fp, SuperBlock *sb, uint32_t block_num) {
    char buffer[BLOCK_SIZE];
    memset(buffer, 0, BLOCK_SIZE);
    write_block(fp, block_num, buffer);
    sb->free_block_count++;
    fseek(fp, 0, SEEK_SET);
    fwrite(sb, sizeof(SuperBlock), 1, fp);
}

/*
 * mycopyto: Copies a Linux file into myfs.
 * Steps:
 * 1. Open the source Linux file and determine its size.
 * 2. Open the dd1 file (our myfs) and read the superblock.
 * 3. Allocate as many blocks as needed to store the file.
 *    For each block, write up to DATA_SIZE bytes from the file.
 *    At the end of each block (last 4 bytes), store the block number of the next block.
 * 4. Create a MyFSEntry descriptor (with name, type, starting block, and size)
 *    and add it to the root directory (for simplicity, we assume the file is added
 *    to the root folder).
 */
int mycopyto(const char *linuxfile, const char *myfspath) {
    FILE *src = fopen(linuxfile, "rb");
    if (!src) {
        perror("fopen src");
        return -1;
    }
    
    // Here we assume the myfs file is always named "dd1"
    FILE *fp = fopen("dd1", "r+b");
    if (!fp) {
        perror("fopen dd1");
        fclose(src);
        return -1;
    }
    
    // Read superblock.
    SuperBlock sb;
    fseek(fp, 0, SEEK_SET);
    fread(&sb, sizeof(SuperBlock), 1, fp);
    
    // Determine source file size.
    fseek(src, 0, SEEK_END);
    uint32_t filesize = ftell(src);
    fseek(src, 0, SEEK_SET);
    
    uint32_t first_block = 0, current_block = 0;
    char block_data[BLOCK_SIZE];
    uint32_t bytes_remaining = filesize;
    
    // Allocate and write blocks, chaining them via the last 4 bytes.
    while (bytes_remaining > 0) {
        uint32_t new_block = allocate_block(fp, &sb);
        if (new_block == 0) {
            fprintf(stderr, "No free block available.\n");
            fclose(src);
            fclose(fp);
            return -1;
        }
        if (first_block == 0) {
            first_block = new_block;
            current_block = new_block;
        } else {
            // Update previous block's last 4 bytes with new_block number.
            memcpy(block_data + BLOCK_SIZE - sizeof(uint32_t), &new_block, sizeof(uint32_t));
            write_block(fp, current_block, block_data);
            current_block = new_block;
        }
        
        memset(block_data, 0, BLOCK_SIZE);
        uint32_t to_read = (bytes_remaining > DATA_SIZE) ? DATA_SIZE : bytes_remaining;
        fread(block_data, 1, to_read, src);
        bytes_remaining -= to_read;
        // Set the next pointer to 0 for now.
        uint32_t next = 0;
        memcpy(block_data + BLOCK_SIZE - sizeof(uint32_t), &next, sizeof(uint32_t));
        write_block(fp, current_block, block_data);
    }
    
    // Create a new directory entry for this file.
    MyFSEntry entry;
    memset(&entry, 0, sizeof(MyFSEntry));
    strncpy(entry.name, myfspath, MAX_NAME_LEN);
    entry.type = TYPE_FILE;
    entry.start_block = first_block;
    entry.size = filesize;
    
    // Add the entry into the root directory block (block 1).
    char root_dir[BLOCK_SIZE];
    read_block(fp, sb.root_dir_block, root_dir);
    int found = 0;
    for (int i = 0; i < BLOCK_SIZE / sizeof(MyFSEntry); i++) {
        MyFSEntry *e = (MyFSEntry *)(root_dir + i * sizeof(MyFSEntry));
        if (e->name[0] == '\0') {  // empty slot found
            memcpy(e, &entry, sizeof(MyFSEntry));
            found = 1;
            break;
        }
    }
    if (!found) {
        fprintf(stderr, "Root directory is full.\n");
        fclose(src);
        fclose(fp);
        return -1;
    }
    write_block(fp, sb.root_dir_block, root_dir);
    
    fclose(src);
    fclose(fp);
    printf("File '%s' copied to myfs as '%s'\n", linuxfile, myfspath);
    return 0;
}

/*
 * mycopyfrom: Copies a file from myfs to a Linux file.
 * Steps:
 * 1. Open dd1 and read the superblock and root directory.
 * 2. Locate the file’s MyFSEntry by matching its name.
 * 3. Using the start_block and the file size, traverse the block chain,
 *    reading up to DATA_SIZE bytes per block and writing to the destination file.
 */
int mycopyfrom(const char *myfspath, const char *linuxfile) {
    FILE *fp = fopen("dd1", "rb");
    if (!fp) {
        perror("fopen dd1");
        return -1;
    }
    
    SuperBlock sb;
    fseek(fp, 0, SEEK_SET);
    fread(&sb, sizeof(SuperBlock), 1, fp);
    
    char root_dir[BLOCK_SIZE];
    read_block(fp, sb.root_dir_block, root_dir);
    
    MyFSEntry *entry = NULL;
    for (int i = 0; i < BLOCK_SIZE / sizeof(MyFSEntry); i++) {
        MyFSEntry *e = (MyFSEntry *)(root_dir + i * sizeof(MyFSEntry));
        if (strncmp(e->name, myfspath, MAX_NAME_LEN) == 0) {
            entry = e;
            break;
        }
    }
    if (entry == NULL) {
        fprintf(stderr, "File not found in myfs.\n");
        fclose(fp);
        return -1;
    }
    
    if (entry->type != TYPE_FILE) {
        fprintf(stderr, "Specified path is not a file.\n");
        fclose(fp);
        return -1;
    }
    
    FILE *dst = fopen(linuxfile, "wb");
    if (!dst) {
        perror("fopen dst");
        fclose(fp);
        return -1;
    }
    
    uint32_t filesize = entry->size;
    uint32_t current_block = entry->start_block;
    char block_data[BLOCK_SIZE];
    while (filesize > 0 && current_block != 0) {
        read_block(fp, current_block, block_data);
        uint32_t to_write = (filesize > DATA_SIZE) ? DATA_SIZE : filesize;
        fwrite(block_data, 1, to_write, dst);
        filesize -= to_write;
        // Get the next block pointer from the last 4 bytes.
        memcpy(&current_block, block_data + BLOCK_SIZE - sizeof(uint32_t), sizeof(uint32_t));
    }
    
    fclose(dst);
    fclose(fp);
    printf("File '%s' copied from myfs to '%s'\n", myfspath, linuxfile);
    return 0;
}

/*
 * myrm: Removes a file from myfs.
 * Steps:
 * 1. Open dd1 and read the superblock and root directory.
 * 2. Locate the file’s directory entry in the root directory.
 * 3. Traverse the chain of blocks used by the file, freeing each block.
 * 4. Remove the directory entry (clear it) from the root directory.
 */
int myrm(const char *myfspath) {
    FILE *fp = fopen("dd1", "r+b");
    if (!fp) {
        perror("fopen dd1");
        return -1;
    }
    
    SuperBlock sb;
    fseek(fp, 0, SEEK_SET);
    fread(&sb, sizeof(SuperBlock), 1, fp);
    
    char root_dir[BLOCK_SIZE];
    read_block(fp, sb.root_dir_block, root_dir);
    
    int found_index = -1;
    MyFSEntry entry;
    for (int i = 0; i < BLOCK_SIZE / sizeof(MyFSEntry); i++) {
        MyFSEntry *e = (MyFSEntry *)(root_dir + i * sizeof(MyFSEntry));
        if (strncmp(e->name, myfspath, MAX_NAME_LEN) == 0) {
            found_index = i;
            memcpy(&entry, e, sizeof(MyFSEntry));
            break;
        }
    }
    if (found_index == -1) {
        fprintf(stderr, "File not found in myfs.\n");
        fclose(fp);
        return -1;
    }
    
    if (entry.type != TYPE_FILE) {
        fprintf(stderr, "Specified path is not a file.\n");
        fclose(fp);
        return -1;
    }
    
    // Free the chain of blocks.
    uint32_t current_block = entry.start_block;
    char block_data[BLOCK_SIZE];
    while (current_block != 0) {
        read_block(fp, current_block, block_data);
        uint32_t next_block = 0;
        memcpy(&next_block, block_data + BLOCK_SIZE - sizeof(uint32_t), sizeof(uint32_t));
        free_block(fp, &sb, current_block);
        current_block = next_block;
    }
    
    // Remove the directory entry.
    memset(root_dir + found_index * sizeof(MyFSEntry), 0, sizeof(MyFSEntry));
    write_block(fp, sb.root_dir_block, root_dir);
    
    fclose(fp);
    printf("File '%s' removed from myfs.\n", myfspath);
    return 0;
}


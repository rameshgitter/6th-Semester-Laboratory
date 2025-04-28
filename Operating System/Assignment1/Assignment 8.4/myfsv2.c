#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <libgen.h>

// ----------------------------------------------------------------
// Constants
// ----------------------------------------------------------------
#define MAX_NAME_LEN 12           // Maximum length for file/dir name
#define DESCRIPTOR_SIZE 21        // Fixed size for each directory entry (12+1+4+4)
// We use dynamic block sizes; block size is stored in the superblock.
#define ENTRY_PER_BLOCK(bs) ((bs - sizeof(uint32_t)) / sizeof(MyFSEntry))
// Last 4 bytes of any block are reserved as "next block" pointer for chaining

// File/directory type definitions
#define FILE_TYPE 1
#define DIR_TYPE  2

// ----------------------------------------------------------------
// Data Structures
// ----------------------------------------------------------------

// Superblock is stored in block 0.
typedef struct {
    uint32_t block_size;       // Block size in bytes
    uint32_t total_blocks;     // Total number of blocks in fs
    uint32_t first_free_block; // Free block chain pointer
    uint32_t root_dir_block;   // Block number for root directory (we use block 1)
} SuperBlock;

// Directory entry (MyFSEntry) is exactly 21 bytes.
#pragma pack(push, 1)
typedef struct {
    char name[MAX_NAME_LEN];   // 12 bytes: name (padded with 0 if needed)
    uint8_t type;              // 1 byte: FILE_TYPE or DIR_TYPE
    uint32_t start_block;      // 4 bytes: pointer to first data block (or dir block)
    uint32_t size;             // 4 bytes: file size (or for dir: total bytes used for descriptors)
} MyFSEntry;
#pragma pack(pop)

// ----------------------------------------------------------------
// Helper Functions
// ----------------------------------------------------------------

/*
 * parse_path:
 * Splits a string of the form "<path>@<fsfile>" into two allocated strings.
 * Caller must free the returned strings.
 */
int parse_path(const char *fullpath, char **fsname, char **path) {
    const char *at = strchr(fullpath, '@');
    if (!at) {
        fprintf(stderr, "parse_path: '@' missing in '%s'\n", fullpath);
        return -1;
    }
    size_t len = at - fullpath;
    *path = strndup(fullpath, len);
    *fsname = strdup(at + 1);
    if (!*path || !*fsname) return -1;
    return 0;
}

/*
 * read_superblock: Reads superblock (block 0) from fd.
 */
int read_superblock(int fd, SuperBlock *sb) {
    if (pread(fd, sb, sizeof(SuperBlock), 0) != sizeof(SuperBlock)) {
        perror("read_superblock");
        return -1;
    }
    return 0;
}

/*
 * write_superblock: Writes superblock to block 0.
 */
int write_superblock(int fd, SuperBlock *sb) {
    if (pwrite(fd, sb, sizeof(SuperBlock), 0) != sizeof(SuperBlock)) {
        perror("write_superblock");
        return -1;
    }
    return 0;
}

/*
 * read_block: Reads a block (by number) into buffer.
 */
int read_block(int fd, uint32_t block_num, void *buffer, uint32_t bs) {
    off_t offset = block_num * bs;
    if (pread(fd, buffer, bs, offset) != bs) {
        perror("read_block");
        return -1;
    }
    return 0;
}

/*
 * write_block: Writes buffer to block number block_num.
 */
int write_block(int fd, uint32_t block_num, const void *buffer, uint32_t bs) {
    off_t offset = block_num * bs;
    if (pwrite(fd, buffer, bs, offset) != bs) {
        perror("write_block");
        return -1;
    }
    return 0;
}

/*
 * allocate_block: Allocates a free block from the free-chain.
 * In a free block, the last 4 bytes store the next free block pointer.
 */
uint32_t allocate_block(int fd, SuperBlock *sb) {
    if (sb->first_free_block == 0) {
        fprintf(stderr, "allocate_block: No free block available\n");
        return 0;
    }
    uint32_t alloc = sb->first_free_block;
    char *buffer = malloc(sb->block_size);
    if (!buffer) return 0;
    if (read_block(fd, alloc, buffer, sb->block_size) < 0) {
        free(buffer);
        return 0;
    }
    // Next free block is stored in last 4 bytes.
    memcpy(&sb->first_free_block, buffer + sb->block_size - sizeof(uint32_t), sizeof(uint32_t));
    // Mark allocated block: zero it and set next pointer to 0.
    memset(buffer, 0, sb->block_size);
    uint32_t next = 0;
    memcpy(buffer + sb->block_size - sizeof(uint32_t), &next, sizeof(uint32_t));
    write_block(fd, alloc, buffer, sb->block_size);
    free(buffer);
    write_superblock(fd, sb);
    return alloc;
}

/*
 * free_block: Frees a block by linking it into the free-chain.
 */
void free_block(int fd, SuperBlock *sb, uint32_t block) {
    char *buffer = malloc(sb->block_size);
    if (!buffer) return;
    memset(buffer, 0, sb->block_size);
    // Link freed block to current free-chain head.
    memcpy(buffer + sb->block_size - sizeof(uint32_t), &sb->first_free_block, sizeof(uint32_t));
    write_block(fd, block, buffer, sb->block_size);
    sb->first_free_block = block;
    write_superblock(fd, sb);
    free(buffer);
}

/*
 * dir_find_entry: Searches a directory (possibly spanning multiple blocks)
 * for an entry with name. Returns 0 on success (entry found) and sets *entry,
 * *block_found (the block number where the entry resides) and *entry_index.
 * Returns -1 if not found.
 */
int dir_find_entry(int fd, SuperBlock *sb, uint32_t dir_block, const char *name,
                     MyFSEntry *entry, uint32_t *block_found, int *entry_index) {
    uint32_t current = dir_block;
    char *buffer = malloc(sb->block_size);
    if (!buffer) return -1;
    while (current != 0) {
        if (read_block(fd, current, buffer, sb->block_size) < 0) {
            free(buffer);
            return -1;
        }
        int n = ENTRY_PER_BLOCK(sb->block_size);
        MyFSEntry *entries = (MyFSEntry *)buffer;
        for (int i = 0; i < n; i++) {
            if (entries[i].name[0] && strcmp(entries[i].name, name) == 0) {
                *entry = entries[i];
                *block_found = current;
                *entry_index = i;
                free(buffer);
                return 0;
            }
        }
        // Read next directory block pointer from last 4 bytes.
        uint32_t next;
        memcpy(&next, buffer + sb->block_size - sizeof(uint32_t), sizeof(uint32_t));
        current = next;
    }
    free(buffer);
    return -1;
}

/*
 * dir_insert_entry: Inserts a new entry into a directory.
 * It traverses the directory chain starting at dir_block.
 * If no free slot is found in the existing blocks, it allocates a new directory block,
 * chains it to the end, and inserts the entry there.
 * Returns 0 on success, -1 on failure.
 */
int dir_insert_entry(int fd, SuperBlock *sb, uint32_t dir_block, MyFSEntry *new_entry) {
    uint32_t current = dir_block;
    char *buffer = malloc(sb->block_size);
    if (!buffer) return -1;
    uint32_t prev = 0;
    while (current != 0) {
        if (read_block(fd, current, buffer, sb->block_size) < 0) {
            free(buffer);
            return -1;
        }
        int n = ENTRY_PER_BLOCK(sb->block_size);
        MyFSEntry *entries = (MyFSEntry *)buffer;
        for (int i = 0; i < n; i++) {
            if (entries[i].name[0] == '\0') {
                // Found free slot.
                entries[i] = *new_entry;
                if (write_block(fd, current, buffer, sb->block_size) < 0) {
                    free(buffer);
                    return -1;
                }
                free(buffer);
                return 0;
            }
        }
        // No free slot: go to next directory block.
        memcpy(&prev, buffer + sb->block_size - sizeof(uint32_t), sizeof(uint32_t));
        if (prev == 0) break;
        current = prev;
    }
    // No free slot in current chain; allocate new directory block.
    uint32_t new_block = allocate_block(fd, sb);
    if (new_block == 0) {
        free(buffer);
        return -1;
    }
    // Initialize new block as directory block (zeroed, with next pointer 0)
    char *newbuf = calloc(1, sb->block_size);
    if (!newbuf) {
        free(buffer);
        return -1;
    }
    uint32_t zero = 0;
    memcpy(newbuf + sb->block_size - sizeof(uint32_t), &zero, sizeof(uint32_t));
    if (write_block(fd, new_block, newbuf, sb->block_size) < 0) {
        free(newbuf);
        free(buffer);
        return -1;
    }
    free(newbuf);
    // Link the last block in the chain to new_block.
    if (current != 0) {
        memcpy(buffer + sb->block_size - sizeof(uint32_t), &new_block, sizeof(uint32_t));
        if (write_block(fd, current, buffer, sb->block_size) < 0) {
            free(buffer);
            return -1;
        }
    }
    // Insert new entry into new_block.
    memset(buffer, 0, sb->block_size);
    MyFSEntry *entries = (MyFSEntry *)buffer;
    entries[0] = *new_entry;
    if (write_block(fd, new_block, buffer, sb->block_size) < 0) {
        free(buffer);
        return -1;
    }
    free(buffer);
    return 0;
}

/*
 * traverse_path: Given a full path like "/dir1/dir2/file", traverse the directory tree.
 * On success, returns 0 and sets *parent_block to the block number of the parent directory
 * and *final_token to the last component (which is not traversed).
 * If the full path refers to a directory (and ends with a '/'), then *final_token is set to NULL.
 */
int traverse_path(int fd, SuperBlock *sb, const char *full_path, uint32_t *parent_block, char **final_token) {
    // Duplicate and tokenize the path.
    char *pathdup = strdup(full_path);
    if (!pathdup) return -1;
    // Remove leading '/'
    char *p = pathdup;
    while (*p == '/') p++;
    char *saveptr;
    char *token = strtok_r(p, "/", &saveptr);
    uint32_t current = sb->root_dir_block;
    uint32_t prev = current;
    char *next_token = NULL;
    while (token != NULL) {
        next_token = strtok_r(NULL, "/", &saveptr);
        if (next_token == NULL) {
            // Last token: this is the entry to be created/located in current directory.
            *parent_block = current;
            *final_token = strdup(token);
            free(pathdup);
            return 0;
        }
        // Traverse into directory token.
        MyFSEntry found;
        if (dir_find_entry(fd, sb, current, token, &found, &current, NULL) < 0) {
            // Directory not found.
            free(pathdup);
            return -1;
        }
        if (found.type != DIR_TYPE) {
            free(pathdup);
            return -1;
        }
    }
    // If path was empty or ends with '/', then final_token is NULL.
    *parent_block = current;
    *final_token = NULL;
    free(pathdup);
    return 0;
}

// ----------------------------------------------------------------
// Core System Call Implementations
// ----------------------------------------------------------------

/*
 * mymkfs: Creates a myfsv2 filesystem on file fname.
 * Usage: ./myfs mymkfs <fsfile> <block_size> <no_of_blocks>
 */
int mymkfs(const char *fname, int block_size, int no_of_blocks) {
    int fd = open(fname, O_RDWR | O_CREAT | O_TRUNC, 0666);
    if (fd == -1) {
        perror("mymkfs: open");
        return -1;
    }
    off_t total_size = block_size * no_of_blocks;
    if (ftruncate(fd, total_size) == -1) {
        perror("mymkfs: ftruncate");
        close(fd);
        return -1;
    }
    SuperBlock sb;
    sb.block_size = block_size;
    sb.total_blocks = no_of_blocks;
    sb.first_free_block = 2; // Block 0 is superblock, block 1 is root dir.
    sb.root_dir_block = 1;
    if (write_superblock(fd, &sb) < 0) {
        close(fd);
        return -1;
    }
    // Initialize root directory block (block 1) as empty.
    char *zero = calloc(1, block_size);
    if (!zero) { close(fd); return -1; }
    if (pwrite(fd, zero, block_size, block_size) != block_size) {
        perror("mymkfs: writing root dir");
        free(zero);
        close(fd);
        return -1;
    }
    free(zero);
    // Initialize free chain for blocks 2 to total_blocks-1.
    char *buf = calloc(1, block_size);
    if (!buf) { close(fd); return -1; }
    for (uint32_t i = 2; i < (uint32_t)no_of_blocks; i++) {
        uint32_t next = (i < no_of_blocks - 1) ? i + 1 : 0;
        memcpy(buf + block_size - sizeof(uint32_t), &next, sizeof(uint32_t));
        if (pwrite(fd, buf, block_size, i * block_size) != block_size) {
            perror("mymkfs: initializing free chain");
            free(buf);
            close(fd);
            return -1;
        }
    }
    free(buf);
    close(fd);
    printf("Filesystem '%s' created: block size = %d, total blocks = %d\n", fname, block_size, no_of_blocks);
    return 0;
}

/*
 * mycopyTo: Copies a Linux file into myfs.
 * Target specification is of the form <path>@<fsfile>.
 * For example: ./myfs mycopyTo resume.txt /docs/reports/cv.txt@dd1
 * This implementation traverses the directory tree, and requires that intermediate directories exist.
 */
int mycopyTo(const char *srcfile, char *destspec) {
    char *fsname = NULL, *path = NULL;
    if (parse_path(destspec, &fsname, &path) < 0)
        return -1;
    int sfd = open(srcfile, O_RDONLY);
    if (sfd == -1) {
        perror("mycopyTo: open srcfile");
        free(fsname); free(path);
        return -1;
    }
    int fd = open(fsname, O_RDWR);
    if (fd == -1) {
        perror("mycopyTo: open fsfile");
        free(fsname); free(path);
        close(sfd);
        return -1;
    }
    SuperBlock sb;
    if (read_superblock(fd, &sb) < 0) {
        close(sfd); close(fd); free(fsname); free(path);
        return -1;
    }
    // Resolve parent directory from the path.
    uint32_t parent_block;
    char *final_token;
    if (traverse_path(fd, &sb, path, &parent_block, &final_token) < 0) {
        fprintf(stderr, "mycopyTo: Could not resolve path '%s'\n", path);
        close(sfd); close(fd); free(fsname); free(path);
        return -1;
    }
    if (!final_token) {
        fprintf(stderr, "mycopyTo: Target must be a file, not a directory\n");
        close(sfd); close(fd); free(fsname); free(path);
        return -1;
    }
    // Read source file size.
    struct stat st;
    if (fstat(sfd, &st) == -1) {
        perror("mycopyTo: fstat");
        close(sfd); close(fd); free(fsname); free(path); free(final_token);
        return -1;
    }
    uint32_t filesize = st.st_size;
    // Allocate blocks for file data.
    uint32_t first_block = 0, current_block = 0;
    uint32_t bytes_remaining = filesize;
    char *data_buf = malloc(sb.block_size);
    if (!data_buf) {
        close(sfd); close(fd); free(fsname); free(path); free(final_token);
        return -1;
    }
    while (bytes_remaining > 0) {
        uint32_t new_block = allocate_block(fd, &sb);
        if (new_block == 0) {
            fprintf(stderr, "mycopyTo: No free block available\n");
            free(data_buf); close(sfd); close(fd);
            free(fsname); free(path); free(final_token);
            return -1;
        }
        if (first_block == 0) {
            first_block = new_block;
            current_block = new_block;
        } else {
            // Update previous block's next pointer.
            memcpy(data_buf + sb.block_size - sizeof(uint32_t), &new_block, sizeof(uint32_t));
            write_block(fd, current_block, data_buf, sb.block_size);
            current_block = new_block;
        }
        memset(data_buf, 0, sb.block_size);
        uint32_t to_read = (bytes_remaining > (sb.block_size - sizeof(uint32_t))) ?
                           (sb.block_size - sizeof(uint32_t)) : bytes_remaining;
        if (read(sfd, data_buf, to_read) != to_read) {
            perror("mycopyTo: read");
            free(data_buf); close(sfd); close(fd);
            free(fsname); free(path); free(final_token);
            return -1;
        }
        bytes_remaining -= to_read;
        uint32_t next = 0;
        memcpy(data_buf + sb.block_size - sizeof(uint32_t), &next, sizeof(uint32_t));
        write_block(fd, current_block, data_buf, sb.block_size);
    }
    free(data_buf);
    close(sfd);
    // Create file descriptor entry.
    MyFSEntry new_entry;
    memset(&new_entry, 0, sizeof(MyFSEntry));
    // Use basename of final_token for file name.
    strncpy(new_entry.name, final_token, MAX_NAME_LEN);
    new_entry.type = FILE_TYPE;
    new_entry.start_block = first_block;
    new_entry.size = filesize;
    // Insert into parent directory.
    if (dir_insert_entry(fd, &sb, parent_block, &new_entry) < 0) {
        fprintf(stderr, "mycopyTo: Failed to insert entry\n");
        close(fd);
        free(fsname); free(path); free(final_token);
        return -1;
    }
    printf("File '%s' copied to myfs as '%s' under directory (block %u) in filesystem '%s'.\n",
           srcfile, final_token, parent_block, fsname);
    free(fsname); free(path); free(final_token);
    close(fd);
    return 0;
}

/*
 * mycopyFrom: Copies a file from myfs to a Linux file.
 * Source specification is of the form <path>@<fsfile>.
 */
int mycopyFrom(char *myfname, const char *linuxfile) {
    char *fsname = NULL, *path = NULL;
    if (parse_path(myfname, &fsname, &path) < 0)
        return -1;
    int fd = open(fsname, O_RDONLY);
    if (fd == -1) {
        perror("mycopyFrom: open fsfile");
        free(fsname); free(path);
        return -1;
    }
    SuperBlock sb;
    if (read_superblock(fd, &sb) < 0) {
        close(fd); free(fsname); free(path);
        return -1;
    }
    // Resolve full path to file. Traverse path and get the final entry.
    uint32_t parent_block;
    char *final_token;
    if (traverse_path(fd, &sb, path, &parent_block, &final_token) < 0) {
        fprintf(stderr, "mycopyFrom: Could not resolve path '%s'\n", path);
        close(fd); free(fsname); free(path);
        return -1;
    }
    // Now, final_token is the file name. Search for it in parent directory.
    MyFSEntry fileEntry;
    uint32_t found_block;
    int entry_index;
    if (dir_find_entry(fd, &sb, parent_block, final_token, &fileEntry, &found_block, &entry_index) < 0) {
        fprintf(stderr, "mycopyFrom: File '%s' not found\n", final_token);
        close(fd); free(fsname); free(path); free(final_token);
        return -1;
    }
    if (fileEntry.type != FILE_TYPE) {
        fprintf(stderr, "mycopyFrom: Specified path is not a file\n");
        close(fd); free(fsname); free(path); free(final_token);
        return -1;
    }
    // Open destination Linux file.
    int sfd = open(linuxfile, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (sfd == -1) {
        perror("mycopyFrom: open dest file");
        close(fd); free(fsname); free(path); free(final_token);
        return -1;
    }
    uint32_t filesize = fileEntry.size;
    uint32_t current_block = fileEntry.start_block;
    char *data_buf = malloc(sb.block_size);
    if (!data_buf) {
        close(sfd); close(fd); free(fsname); free(path); free(final_token);
        return -1;
    }
    while (filesize > 0 && current_block != 0) {
        if (read_block(fd, current_block, data_buf, sb.block_size) < 0) break;
        uint32_t to_write = (filesize > (sb.block_size - sizeof(uint32_t))) ?
                            (sb.block_size - sizeof(uint32_t)) : filesize;
        write(sfd, data_buf, to_write);
        filesize -= to_write;
        memcpy(&current_block, data_buf + sb.block_size - sizeof(uint32_t), sizeof(uint32_t));
    }
    free(data_buf);
    close(sfd);
    close(fd);
    printf("File '%s' copied from myfs to '%s' from filesystem '%s'.\n", final_token, linuxfile, fsname);
    free(fsname); free(path); free(final_token);
    return 0;
}

/*
 * myrm: Removes a file from myfs.
 * Specification: <path>@<fsfile>
 */
int myrm(char *myfname) {
    char *fsname = NULL, *path = NULL;
    if (parse_path(myfname, &fsname, &path) < 0)
        return -1;
    int fd = open(fsname, O_RDWR);
    if (fd == -1) {
        perror("myrm: open fsfile");
        free(fsname); free(path);
        return -1;
    }
    SuperBlock sb;
    if (read_superblock(fd, &sb) < 0) {
        close(fd); free(fsname); free(path);
        return -1;
    }
    uint32_t parent_block;
    char *final_token;
    if (traverse_path(fd, &sb, path, &parent_block, &final_token) < 0) {
        fprintf(stderr, "myrm: Could not resolve path '%s'\n", path);
        close(fd); free(fsname); free(path);
        return -1;
    }
    MyFSEntry fileEntry;
    uint32_t found_block;
    int entry_index;
    if (dir_find_entry(fd, &sb, parent_block, final_token, &fileEntry, &found_block, &entry_index) < 0) {
        fprintf(stderr, "myrm: File '%s' not found\n", final_token);
        close(fd); free(fsname); free(path); free(final_token);
        return -1;
    }
    if (fileEntry.type != FILE_TYPE) {
        fprintf(stderr, "myrm: Specified path is not a file\n");
        close(fd); free(fsname); free(path); free(final_token);
        return -1;
    }
    // Free the chain of blocks used by the file.
    uint32_t current_block = fileEntry.start_block;
    char *buf = malloc(sb.block_size);
    if (!buf) {
        close(fd); free(fsname); free(path); free(final_token);
        return -1;
    }
    while (current_block != 0) {
        if (read_block(fd, current_block, buf, sb.block_size) < 0) break;
        uint32_t next_block = 0;
        memcpy(&next_block, buf + sb.block_size - sizeof(uint32_t), sizeof(uint32_t));
        free_block(fd, &sb, current_block);
        current_block = next_block;
    }
    free(buf);
    // Remove entry from directory.
    char *dir_buf = malloc(sb.block_size);
    if (!dir_buf) {
        close(fd); free(fsname); free(path); free(final_token);
        return -1;
    }
    if (read_block(fd, parent_block, dir_buf, sb.block_size) < 0) {
        free(dir_buf); close(fd); free(fsname); free(path); free(final_token);
        return -1;
    }
    MyFSEntry *entries = (MyFSEntry *)dir_buf;
    entries[entry_index].name[0] = '\0';
    write_block(fd, parent_block, dir_buf, sb.block_size);
    free(dir_buf);
    printf("File '%s' removed from filesystem '%s'.\n", final_token, fsname);
    close(fd);
    free(fsname); free(path); free(final_token);
    return 0;
}

/*
 * mymkdir: Creates a directory in myfs.
 * Specification: <dir path>@<fsfile>. Intermediate directories must already exist.
 */
int mymkdir(char *mydirname) {
    char *fsname = NULL, *path = NULL;
    if (parse_path(mydirname, &fsname, &path) < 0)
        return -1;
    int fd = open(fsname, O_RDWR);
    if (fd == -1) {
        perror("mymkdir: open fsfile");
        free(fsname); free(path);
        return -1;
    }
    SuperBlock sb;
    if (read_superblock(fd, &sb) < 0) {
        close(fd); free(fsname); free(path);
        return -1;
    }
    // Traverse path to get parent directory and final token.
    uint32_t parent_block;
    char *final_token;
    if (traverse_path(fd, &sb, path, &parent_block, &final_token) < 0) {
        fprintf(stderr, "mymkdir: Could not resolve path '%s'\n", path);
        close(fd); free(fsname); free(path);
        return -1;
    }
    // final_token is the name of the new directory.
    MyFSEntry new_entry;
    memset(&new_entry, 0, sizeof(MyFSEntry));
    strncpy(new_entry.name, final_token, MAX_NAME_LEN);
    new_entry.type = DIR_TYPE;
    // Allocate a block for the new directory.
    uint32_t new_dir_block = allocate_block(fd, &sb);
    if (new_dir_block == 0) {
        fprintf(stderr, "mymkdir: No free block available\n");
        close(fd); free(fsname); free(path); free(final_token);
        return -1;
    }
    new_entry.start_block = new_dir_block;
    new_entry.size = 0; // Initially empty
    // Initialize the new directory block.
    char *zero = calloc(1, sb.block_size);
    if (!zero) {
        close(fd); free(fsname); free(path); free(final_token);
        return -1;
    }
    uint32_t next = 0;
    memcpy(zero + sb.block_size - sizeof(uint32_t), &next, sizeof(uint32_t));
    write_block(fd, new_dir_block, zero, sb.block_size);
    free(zero);
    // Insert the new directory entry into the parent directory.
    if (dir_insert_entry(fd, &sb, parent_block, &new_entry) < 0) {
        fprintf(stderr, "mymkdir: Failed to insert directory entry\n");
        close(fd); free(fsname); free(path); free(final_token);
        return -1;
    }
    printf("Directory '%s' created under parent block %u in filesystem '%s' (new block %u).\n",
           final_token, parent_block, fsname, new_dir_block);
    close(fd);
    free(fsname); free(path); free(final_token);
    return 0;
}

/*
 * myrmdir: Removes a directory from myfs.
 * Specification: <dir path>@<fsfile>. The directory must be empty.
 */
int myrmdir(char *mydirname) {
    char *fsname = NULL, *path = NULL;
    if (parse_path(mydirname, &fsname, &path) < 0)
        return -1;
    int fd = open(fsname, O_RDWR);
    if (fd == -1) {
        perror("myrmdir: open fsfile");
        free(fsname); free(path);
        return -1;
    }
    SuperBlock sb;
    if (read_superblock(fd, &sb) < 0) {
        close(fd); free(fsname); free(path);
        return -1;
    }
    // Traverse to parent directory and get final token.
    uint32_t parent_block;
    char *final_token;
    if (traverse_path(fd, &sb, path, &parent_block, &final_token) < 0) {
        fprintf(stderr, "myrmdir: Could not resolve path '%s'\n", path);
        close(fd); free(fsname); free(path);
        return -1;
    }
    MyFSEntry dirEntry;
    uint32_t found_block;
    int entry_index;
    if (dir_find_entry(fd, &sb, parent_block, final_token, &dirEntry, &found_block, &entry_index) < 0) {
        fprintf(stderr, "myrmdir: Directory '%s' not found\n", final_token);
        close(fd); free(fsname); free(path); free(final_token);
        return -1;
    }
    if (dirEntry.type != DIR_TYPE) {
        fprintf(stderr, "myrmdir: Specified path is not a directory\n");
        close(fd); free(fsname); free(path); free(final_token);
        return -1;
    }
    // Check if directory is empty.
    // For simplicity, we assume if the directory block contains no entries, it is empty.
    char *dir_buf = malloc(sb.block_size);
    if (!dir_buf) {
        close(fd); free(fsname); free(path); free(final_token);
        return -1;
    }
    if (read_block(fd, dirEntry.start_block, dir_buf, sb.block_size) < 0) {
        free(dir_buf); close(fd); free(fsname); free(path); free(final_token);
        return -1;
    }
    int n = ENTRY_PER_BLOCK(sb.block_size);
    int empty = 1;
    MyFSEntry *dentries = (MyFSEntry *)dir_buf;
    for (int i = 0; i < n; i++) {
        if (dentries[i].name[0] != '\0') { empty = 0; break; }
    }
    free(dir_buf);
    if (!empty) {
        fprintf(stderr, "myrmdir: Directory '%s' is not empty\n", final_token);
        close(fd); free(fsname); free(path); free(final_token);
        return -1;
    }
    // Free the directory block.
    free_block(fd, &sb, dirEntry.start_block);
    // Remove the directory entry from the parent directory.
    char *parent_buf = malloc(sb.block_size);
    if (!parent_buf) {
        close(fd); free(fsname); free(path); free(final_token);
        return -1;
    }
    if (read_block(fd, parent_block, parent_buf, sb.block_size) < 0) {
        free(parent_buf); close(fd); free(fsname); free(path); free(final_token);
        return -1;
    }
    MyFSEntry *pentries = (MyFSEntry *)parent_buf;
    pentries[entry_index].name[0] = '\0';
    write_block(fd, parent_block, parent_buf, sb.block_size);
    free(parent_buf);
    printf("Directory '%s' removed from filesystem '%s'.\n", final_token, fsname);
    close(fd);
    free(fsname); free(path); free(final_token);
    return 0;
}

/*
 * myreadBlock: Reads the block_no-th block of a file into buf.
 * myfname is of the form <file path>@<fsfile>.
 */
int myreadBlock(char *myfname, char *buf, int block_no) {
    char *fsname = NULL, *path = NULL;
    if (parse_path(myfname, &fsname, &path) < 0)
        return -1;
    int fd = open(fsname, O_RDONLY);
    if (fd == -1) {
        perror("myreadBlock: open fsfile");
        free(fsname); free(path);
        return -1;
    }
    SuperBlock sb;
    if (read_superblock(fd, &sb) < 0) {
        close(fd); free(fsname); free(path);
        return -1;
    }
    // Traverse to parent directory and get file name.
    uint32_t parent_block;
    char *final_token;
    if (traverse_path(fd, &sb, path, &parent_block, &final_token) < 0) {
        fprintf(stderr, "myreadBlock: Could not resolve path '%s'\n", path);
        close(fd); free(fsname); free(path);
        return -1;
    }
    MyFSEntry fileEntry;
    uint32_t found_block;
    int entry_index;
    if (dir_find_entry(fd, &sb, parent_block, final_token, &fileEntry, &found_block, &entry_index) < 0) {
        fprintf(stderr, "myreadBlock: File '%s' not found\n", final_token);
        close(fd); free(fsname); free(path); free(final_token);
        return -1;
    }
    if (fileEntry.type != FILE_TYPE) {
        fprintf(stderr, "myreadBlock: '%s' is not a file\n", final_token);
        close(fd); free(fsname); free(path); free(final_token);
        return -1;
    }
    uint32_t current = fileEntry.start_block;
    char *temp_buf = malloc(sb.block_size);
    if (!temp_buf) {
        close(fd); free(fsname); free(path); free(final_token);
        return -1;
    }
    for (int i = 0; i < block_no; i++) {
        if (read_block(fd, current, temp_buf, sb.block_size) < 0) break;
        memcpy(&current, temp_buf + sb.block_size - sizeof(uint32_t), sizeof(uint32_t));
        if (current == 0) {
            fprintf(stderr, "myreadBlock: Block chain ended before block %d\n", block_no);
            free(temp_buf); close(fd); free(fsname); free(path); free(final_token);
            return -1;
        }
    }
    if (read_block(fd, current, buf, sb.block_size) < 0) {
        free(temp_buf); close(fd); free(fsname); free(path); free(final_token);
        return -1;
    }
    free(temp_buf);
    close(fd);
    free(fsname); free(path); free(final_token);
    return 0;
}

/*
 * mystat: Stores metadata for a file or directory in buf.
 * myname is of the form <path>@<fsfile>.
 */
int mystat(char *myname, char *buf) {
    char *fsname = NULL, *path = NULL;
    if (parse_path(myname, &fsname, &path) < 0)
        return -1;
    int fd = open(fsname, O_RDONLY);
    if (fd == -1) {
        perror("mystat: open fsfile");
        free(fsname); free(path);
        return -1;
    }
    SuperBlock sb;
    if (read_superblock(fd, &sb) < 0) {
        close(fd); free(fsname); free(path);
        return -1;
    }
    uint32_t parent_block;
    char *final_token;
    if (traverse_path(fd, &sb, path, &parent_block, &final_token) < 0) {
        fprintf(stderr, "mystat: Could not resolve path '%s'\n", path);
        close(fd); free(fsname); free(path);
        return -1;
    }
    MyFSEntry entry;
    uint32_t found;
    int idx;
    if (dir_find_entry(fd, &sb, parent_block, final_token, &entry, &found, &idx) < 0) {
        snprintf(buf, 256, "mystat: Entry '%s' not found in filesystem '%s'.", final_token, fsname);
        close(fd); free(fsname); free(path); free(final_token);
        return -1;
    }
    snprintf(buf, 256, "Name: %s\nType: %s\nStart Block: %u\nSize: %u bytes",
             entry.name, (entry.type == FILE_TYPE) ? "File" : "Directory", entry.start_block, entry.size);
    close(fd);
    free(fsname); free(path); free(final_token);
    return 0;
}

// ----------------------------------------------------------------
// Main: Command Dispatch
// ----------------------------------------------------------------
int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr,
        "Usage:\n"
        "  %s mymkfs <fsfile> <block_size> <no_of_blocks>\n"
        "  %s mycopyTo <linuxfile> <myfile_path>@<fsfile>\n"
        "  %s mycopyFrom <myfile_path>@<fsfile> <linuxfile>\n"
        "  %s myrm <myfile_path>@<fsfile>\n"
        "  %s mymkdir <dir_path>@<fsfile>\n"
        "  %s myrmdir <dir_path>@<fsfile>\n"
        "  %s myreadBlock <myfile_path>@<fsfile> <buf> <block_no>\n"
        "  %s mystat <path>@<fsfile>\n",
        argv[0], argv[0], argv[0], argv[0], argv[0], argv[0], argv[0], argv[0]);
        exit(1);
    }
    
    if (strcmp(argv[1], "mymkfs") == 0) {
        if (argc != 5) {
            fprintf(stderr, "Usage: %s mymkfs <fsfile> <block_size> <no_of_blocks>\n", argv[0]);
            exit(1);
        }
        int bs = atoi(argv[3]);
        int nblocks = atoi(argv[4]);
        return mymkfs(argv[2], bs, nblocks);
    }
    else if (strcmp(argv[1], "mycopyTo") == 0) {
        if (argc != 4) {
            fprintf(stderr, "Usage: %s mycopyTo <linuxfile> <myfile_path>@<fsfile>\n", argv[0]);
            exit(1);
        }
        return mycopyTo(argv[2], argv[3]);
    }
    else if (strcmp(argv[1], "mycopyFrom") == 0) {
        if (argc != 4) {
            fprintf(stderr, "Usage: %s mycopyFrom <myfile_path>@<fsfile> <linuxfile>\n", argv[0]);
            exit(1);
        }
        return mycopyFrom(argv[2], argv[3]);
    }
    else if (strcmp(argv[1], "myrm") == 0) {
        if (argc != 3) {
            fprintf(stderr, "Usage: %s myrm <myfile_path>@<fsfile>\n", argv[0]);
            exit(1);
        }
        return myrm(argv[2]);
    }
    else if (strcmp(argv[1], "mymkdir") == 0) {
        if (argc != 3) {
            fprintf(stderr, "Usage: %s mymkdir <dir_path>@<fsfile>\n", argv[0]);
            exit(1);
        }
        return mymkdir(argv[2]);
    }
    else if (strcmp(argv[1], "myrmdir") == 0) {
        if (argc != 3) {
            fprintf(stderr, "Usage: %s myrmdir <dir_path>@<fsfile>\n", argv[0]);
            exit(1);
        }
        return myrmdir(argv[2]);
    }
    else if (strcmp(argv[1], "myreadBlock") == 0) {
        if (argc != 4) {
            fprintf(stderr, "Usage: %s myreadBlock <myfile_path>@<fsfile> <block_no>\n", argv[0]);
            exit(1);
        }
        int bno = atoi(argv[3]);
        char *readbuf = malloc(4096); // For simplicity; ideally, use the fs block size.
        if (!readbuf) exit(1);
        if (myreadBlock(argv[2], readbuf, bno) == 0) {
            printf("Block %d data (first 64 bytes):\n", bno);
            for (int i = 0; i < 64; i++) {
                printf("%02X ", (unsigned char)readbuf[i]);
                if ((i+1)%16 == 0) printf("\n");
            }
            printf("\n");
        }
        free(readbuf);
        return 0;
    }
    else if (strcmp(argv[1], "mystat") == 0) {
        if (argc != 3) {
            fprintf(stderr, "Usage: %s mystat <path>@<fsfile>\n", argv[0]);
            exit(1);
        }
        char info[256];
        if (mystat(argv[2], info) == 0) {
            printf("%s\n", info);
        }
        return 0;
    }
    else {
        fprintf(stderr, "Unknown command: %s\n", argv[1]);
        exit(1);
    }
    
    return 0;
}



/*
 * (C) 2026, Cornell University
 * All rights reserved.
 *
 * Description: generate disk image (disk.img) and QEMU ROM image (qemuROM.bin)
 * The disk image should be exactly 4MB:
 *     2MB holds the executables of EGOS and system servers;
 *     2MB is managed by a file system.
 * This disk image should be programmed to the microSD card.
 *
 * The QEMU ROM image should be exactly 32MB with 16 x 2MB blocks.
 */

#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>
#include <stdlib.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>
#include "inode.h"
#include "fs.h"

#undef printf  // cancel my_printf definition
#include <stdio.h>

char* egos_binaries[] = {"./egos.bin",
                         "../build/release/sys_proc.elf",
                         "../build/release/sys_terminal.elf",
                         "../build/release/sys_file.elf",
                         "../build/release/sys_net.elf",
                         "../build/release/sys_shell.elf"
                         /* "./images/Bohr.bmp" - for video demo (student TODO) */};
#define EGOS_BIN_NUM ((sizeof(egos_binaries) / sizeof(char*)))

char bin_dir[256] = "./   6 ../   0 ";
char* contents[]  = {
    "./   0 ../   0 home/   1 bin/   6 ",
    "./   1 ../   0 cs6640/ 2 ta/    3 fs/       4 ",
    "./   2 ../   1 README  5 rwfs/  128 ",
    "./   3 ../   1 ",
    "./   4 ../   1 ",
    "Welcome to CS6640 labs. \nThis OS is tailored from egos-2000 (https://github.com/yhzhang0128/egos-2000).\n",
    bin_dir};
#define BIN_DIR_INODE ((sizeof(contents) / sizeof(char*)) - 1)

char inode[SIZE_2MB], tmp[512];
char exec[SIZE_2MB], fs[SIZE_2MB];
char rwfs[SIZE_2MB];

int load_file(char* file_name, char* dst) {
    struct stat st;
    stat(file_name, &st);
    int fd = open(file_name, O_RDONLY);
    for (uint nread = 0; nread < st.st_size;) {
        nread += read(fd, dst + nread, st.st_size - nread);
    }
    close(fd);

    return st.st_size;
}

int getsize(inode_intf bs, uint ino) {
    return FILE_SYS_DISK_SIZE / BLOCK_SIZE;
}

int setsize(inode_intf bs, uint ino, uint newsize) {
    assert(0);
}

int ramread(inode_intf bs, uint ino, uint offset, block_t* block) {
    memcpy(block, fs + offset * BLOCK_SIZE, BLOCK_SIZE);
    return 0;
}

int ramwrite(inode_intf bs, uint ino, uint offset, block_t* block) {
    memcpy(fs + offset * BLOCK_SIZE, block, BLOCK_SIZE);
    return 0;
}

void mkrwfs();

int main() {
    /* Write the kernel and system server binaries into exec[]. */
    printf("[INFO] Load %ld kernel binary files\n", EGOS_BIN_NUM);
    for (uint i = 0; i < EGOS_BIN_NUM; i++) {
        int sz = load_file(egos_binaries[i], exec + i * EGOS_BIN_MAX_NBYTE);
        printf("[INFO] Load %s: %d bytes\n", egos_binaries[i], sz);
    }

    /* Initialize the file system using the fs[] buffer as a ramdisk. */
    printf("MKFS is using *%s*\n", FILESYS == 0 ? "mydisk" : "treedisk");
    struct inode_store ramdisk = (struct inode_store){.read    = ramread,
                                                      .write   = ramwrite,
                                                      .getsize = getsize,
                                                      .setsize = setsize};
    (FILESYS == 0) ? assert(mydisk_create(&ramdisk, 0, NINODES) >= 0)
                   : assert(treedisk_create(&ramdisk, 0, NINODES) >= 0);
    inode_intf filesys =
        (FILESYS == 0) ? mydisk_init(&ramdisk, 0) : treedisk_init(&ramdisk, 0);

    /* Write to inode 0..BIN_DIR_INODE-1 in the file system. */
    for (uint ino = 0; ino < BIN_DIR_INODE; ino++) {
        printf("[INFO] Load ino=%d, %ld bytes\n", ino, strlen(contents[ino]));
        strncpy(inode, contents[ino], BLOCK_SIZE);
        filesys->write(filesys, ino, 0, (void*)inode);
    }

    /* Write to one inode for each user application. */
    uint app_ino = BIN_DIR_INODE + 1;
    DIR* dp      = opendir("../build/release/user");
    assert(dp != NULL);
    for (struct dirent* ep = readdir(dp); ep != NULL; ep = readdir(dp)) {
        if (strstr(ep->d_name, ".elf")) {
            sprintf(tmp, "../build/release/user/%s", ep->d_name);
            int file_size = load_file(tmp, inode);
            printf("[INFO] Load ino=%d, %s: %d bytes\n", app_ino, ep->d_name,
                   file_size);

            /* Write the ELF format application binary into inode app_ino. */
            for (uint b = 0; b * BLOCK_SIZE < file_size; b++)
                filesys->write(filesys, app_ino, b,
                               (void*)(inode + b * BLOCK_SIZE));

            /* Add the corresponding file entry into the /bin directory. */
            ep->d_name[strlen(ep->d_name) - 4] = 0;
            sprintf(tmp, "%s%4d ", ep->d_name, app_ino++);
            strcat(bin_dir, tmp);
        }
    }
    closedir(dp);
    filesys->write(filesys, BIN_DIR_INODE, 0, (void*)bin_dir);
    printf("[INFO] Load ino=%ld, %s\n", BIN_DIR_INODE, bin_dir);

#ifdef RWFSON
    mkrwfs();
#endif

    /* Generate the disk image file. */
    int fd  = open("disk.img", O_CREAT | O_WRONLY, 0666);
    int sz1 = write(fd, exec, SIZE_2MB);
    sz1 += write(fd, fs, SIZE_2MB);
    sz1 += write(fd, rwfs, SIZE_2MB); // write rwfs
    for (uint i = 0; i < 13; i++) sz1 += write(fd, fs, SIZE_2MB);
    /* Pad the image to 32MB */
    close(fd);

    /* Generate the QEMU ROM image file. */
    fd      = open("qemuROM.bin", O_CREAT | O_WRONLY, 0666);
    int sz2 = write(fd, exec, SIZE_2MB);
    for (uint i = 0; i < 15; i++) sz2 += write(fd, fs, SIZE_2MB);
    /* Simply pad the image to 32MB which is required by QEMU. */
    close(fd);

    assert(sz1 == SIZE_2MB * 16 && sz2 == SIZE_2MB * 16);
    printf("[INFO] Finish making the image files\n");
    return 0;
}

static void mk_entry(dirent_t* entries, int i, int inum, char *name) {
    entries[i].valid = 1;
    entries[i].inum = inum;
    strcpy(entries[i].name, name);
}

void mkrwfs() {
    super_t *super = (super_t*)rwfs;
    super->magic = 0x6640;
    super->total_blks = sizeof(rwfs) / BLOCK_SIZE;

    unsigned char *map = (unsigned char*) &rwfs[BLOCK_SIZE];
    // 0: super block
    // 1: bitmap
    // 2--11: inodes
    // 12+0: root data block
    // +1: file1 data block
    // +2: dir1 data block
    // +3: file2 indirect block
    // +4-+13: file2 data block1--10
    // +14: file2 first indirect data block (12+14=26)
    // total: 27
    for (int i=0; i<27; i++) {
        map[i/8] |= (1 << (i%8));
    }

    inode_t *inodes = (inode_t*) &rwfs[INODEARR_BLOCK_START * BLOCK_SIZE];

    int root_ino = 0;  /* root-> /home/cs6640/rwfs/ */
    int root_data_blk = DATA_BLOCK_START;
    int file1_ino = 1;
    int file1_data_blk = DATA_BLOCK_START + 1;
    int dir1_ino = 2;
    int dir1_data_blk = DATA_BLOCK_START + 2;
    int file2_ino = 3;
    int file2_indirect_blk = DATA_BLOCK_START + 3;
    int file2_data_blks[11] = {
        DATA_BLOCK_START + 4,
        DATA_BLOCK_START + 5,
        DATA_BLOCK_START + 6,
        DATA_BLOCK_START + 7,
        DATA_BLOCK_START + 8,
        DATA_BLOCK_START + 9,
        DATA_BLOCK_START + 10,
        DATA_BLOCK_START + 11,
        DATA_BLOCK_START + 12,
        DATA_BLOCK_START + 13,
        DATA_BLOCK_START + 14,
    };
    dirent_t *entries;
    char *data;

    // root
    inode_t *root = &inodes[root_ino];
    root->mode = MODE_D | MODE_ALL;
    root->pads[0] = 0xdeadbeef;
    root->ptrs[0] = root_data_blk;
    entries = (dirent_t*) &rwfs[BLOCK_SIZE*root_data_blk];
    mk_entry(entries, 0, root_ino, ".");
    mk_entry(entries, 1, 2, "..");  /* ..-> /home/cs6640/ */
    //   root/file1.txt
    mk_entry(entries, 2, file1_ino, "file1.txt");
    //   root/dir1
    mk_entry(entries, 3, dir1_ino, "dir1");
    root->size = 4 * sizeof(dirent_t);

    // dir1
    inode_t *dir1 = &inodes[dir1_ino];
    dir1->mode = MODE_D | MODE_ALL;
    dir1->pads[0] = 0xdeadbeef;
    dir1->ptrs[0] = dir1_data_blk;
    //   dir1/file2.txt
    entries = (dirent_t*) &rwfs[BLOCK_SIZE*dir1_data_blk];
    mk_entry(entries, 0, dir1_ino, ".");
    mk_entry(entries, 1, root_ino, "..");
    mk_entry(entries, 2, file2_ino, "file2.txt");
    dir1->size = 3 * sizeof(dirent_t);

    // file1.txt
    inode_t *file1 = &inodes[file1_ino];
    file1->mode = MODE_F | MODE_ALL;
    file1->pads[0] = 0xdeadbeef;
    file1->ptrs[0] = file1_data_blk;
    // contents
    data= (char*) &rwfs[BLOCK_SIZE*file1_data_blk];
    strcpy(data, "hello world");
    file1->size = strlen("hello world");

    // file2.txt
    inode_t *file2 = &inodes[file2_ino];
    file2->mode = MODE_F | MODE_ALL;
    file2->pads[0] = 0xdeadbeef;
    file2->indirect_ptr = file2_indirect_blk;
    for (int i=0; i<NUM_PTRS; i++) { // direct data block
        file2->ptrs[i] = file2_data_blks[i];
    }
    // indirect data blocks
    * ((uint*) &rwfs[BLOCK_SIZE*file2_indirect_blk]) = file2_data_blks[10];

    // contents
    char hex[] = "0123456789abcdef,";
    int fsz = BLOCK_SIZE * 11;

    char *start = (char*) &rwfs[BLOCK_SIZE*file2_data_blks[0]];
    for (int i=0; i<fsz/17; i++) {
        memcpy(start, hex, 17);
        start += 17;
    }
    file2->size = fsz;
}

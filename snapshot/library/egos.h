#pragma once

typedef unsigned char uchar;
typedef unsigned short ushort;
typedef unsigned int uint;
typedef unsigned long long ulonglong;

struct earth {
    uint (*mmu_alloc)();
    void (*mmu_free)(int pid);
    void (*mmu_flush_cache)();
    void (*timer_reset)(uint core_id);
    ulonglong (*gettime)();

    void (*mmu_map)(int pid, uint vpage_no, uint ppage_id);
    uint (*mmu_translate)(int pid, uint vaddr);
    void (*mmu_switch)(int pid);

    void (*tty_read)(char* c);
    void (*tty_write)(char c);
    uint (*tty_input_empty)();
    void (*disk_read)(uint block_no, uint nblocks, char* dst);
    void (*disk_write)(uint block_no, uint nblocks, char* src);
    void (*net_send)(int length, void* packet);
    int (*net_recv)(void* buffer);

    enum { PAGE_TABLE, SOFT_TLB } translation;
};

struct grass {
    int (*proc_alloc)();
    void (*proc_free)(int pid);
    void (*proc_set_ready)(int pid);

    void (*sys_send)(int receiver, char* msg, uint size);
    void (*sys_recv)(int from, int* sender, char* buf, uint size);
    /* Student's code goes here (System Call | Multicore & Locks). */

    /* Add interface functions for process sleep and multicore information. */

    /* Student's code ends here. */
};

extern struct earth* earth;
extern struct grass* grass;

/* Below is the physical memory layout in egos-2k+. */
#define RAM_END         0x80800000UL /* 8MB memory starting at RAM_START */
#define APPS_PAGES_BASE 0x80400000UL /* 4MB free for mmu_alloc           */
#define APPS_STACK_TOP  0x80400000UL /* 1MB app stack (growing down)     */
#define SHELL_WORK_DIR  0x80302000UL /* current work directory for shell */
#define SYSCALL_ARG     0x80301000UL /* struct syscall                   */
#define APPS_ARG        0x80300000UL /* main() arguments (argc and argv) */
#define APPS_ENTRY      0x80200000UL /* 1MB app code and data            */
#define EGOS_STACK_TOP  0x80200000UL /* 1MB egos stack (growing down)    */
#define GRASS_STRUCT    0x80101000UL /* struct grass                     */
#define EARTH_STRUCT    0x80100000UL /* struct earth                     */
#define RAM_START       0x80000000UL /* 1MB egos code and data           */

#define PAGE_SIZE          4096
#define APPS_PAGES_CNT     ((RAM_END - APPS_PAGES_BASE) / PAGE_SIZE)

/* ticks for each time slice */
#define QUANTUM       6000000UL

/* Below is the memory-mapped I/O layout for QEMU virt machine. */
#define SDHCI_PCI_ECAM     0x30008000UL
#define SDHCI_BASE         0x40000000UL
#define ETH_PCI_ECAM       0x30018000UL
#define ETH_CTL_BASE       0x41000000UL
#define UART_BASE          0x10000000UL
#define CLINT_BASE         0x02000000UL
#define CLINT_MSIP(hartid) (CLINT_BASE + 4UL*(hartid))
#define FLASH_ROM_BASE     0x22000000UL
#define VIDEO_FRAME_BASE   0x42000000UL

/* Below are some common macros/declarations for I/O, multicore and printing. */
#define ACCESS(x)          (*(__typeof__(*x) volatile*)(x))
#define REGW(base, offset) (ACCESS((uint*)(base + offset)))
#define REGB(base, offset) (ACCESS((uchar*)(base + offset)))

#define NCORES     4
#define release(x) __sync_lock_release(&x);
#define acquire(x) while (__sync_lock_test_and_set(&x, 1) != 0);
extern int boot_lock, kernel_lock, booted_core_cnt;

#define printf my_printf
int INFO(const char* format, ...);
int FATAL(const char* format, ...);
int SUCCESS(const char* format, ...);
int CRITICAL(const char* format, ...);
int my_printf(const char* format, ...);

#define ASSERT(cond, msg)                         \
  do {                                            \
    if (!(cond)) {                                \
      FATAL("ASSERT failed: %s (%s:%d: %s)\n\t%s",\
        #cond, __FILE__, __LINE__, __func__, msg);\
    }                                             \
  } while (0)

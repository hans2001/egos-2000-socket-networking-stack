/* Author: CS6640 23fall staff
 * Description: virtual memory management
 * Updated by CS6640 26spring staff
 */

#include "egos.h"
#include "servers.h"
#include "string.h"

/* from grass/process.h */
#define MAX_NPROCESS 16
#define USER_PID_START GPID_USER_START

/* Page Table Translation
 *
 * The code below creates an identity mapping using RISC-V Sv32;
 * Read section4.3 of RISC-V privileged spec manual.
 */

#define FLAG_NEXT_LEVEL 0x1
#define PTE_V 0x001
#define PTE_R 0x002
#define PTE_W 0x004
#define PTE_X 0x008
#define PTE_U 0x010
#define PTE_A 0x040
#define PTE_D 0x080
#define LEAF_FLAG (PTE_V | PTE_R | PTE_W | PTE_X | PTE_U | PTE_A | PTE_D)

/* Interface to allocate/free a physical page (from mem_alloc.c) */
void* pmalloc(int clear_page);
void  pfree(void *paddr);
int   num_free_pages();

/* a mapping from pid to page table root */
static uint* pid_to_pagetable_base[MAX_NPROCESS+1];

void fence() {
    asm("sfence.vma zero,zero");
}


/* Helper function: establish an identity mapping
 * from virtual addresses to the same physical addresses.
 *
 * Configure virtual memory mappings for process `pid` by mapping
 *      VA [addr, addr + npages * PAGE_SIZE)
 * to
 *      PA [addr, addr + npages * PAGE_SIZE)
 * with permission bits specified by `flag`.
 */
void setup_identity_region(int pid, uint addr, int npages, uint flag) {
    ASSERT(npages <= 1024, "npages is larger than 1024");
    ASSERT((addr & 0xFFF) == 0, "addr is not 4K aligned");

    uint* root = pid_to_pagetable_base[pid];
    ASSERT(root != NULL, "pagetable root is NULL");

    /* Setup the entry in the root page table */
    int l1_idx = addr >> 22;
    uint* l2_pa = 0;
    if (root[l1_idx] == 0) { // if L1 PTE doesn't exist
        /* Allocate the L2 page table page */
        l2_pa = pmalloc(1);
        uint ppn = ((uint)l2_pa >> 12);
        root[l1_idx] =  (ppn << 10) | FLAG_NEXT_LEVEL;
    } else {  // if L1 PTE exists
        ASSERT(root[l1_idx] & FLAG_NEXT_LEVEL, "invalide l1 PTE");
        l2_pa = (void*) ((root[l1_idx] << 2) & ~0xfff);
    }

    /* Setup the PTE in the L2 page table page */
    uint l2_idx = (addr >> 12) & 0x3FF;
    for (uint i = 0; i < npages; i++) {
        ASSERT(l2_pa[l2_idx + i] == 0, "identity region: non-empty l2 PTE");
        l2_pa[l2_idx + i] = ((addr + i * PAGE_SIZE) >> 2) | flag;
    }
}


/* [lab5-ex2]
 * Walk the page table rooted at `root` to translate virtual address `va`,
 * and return the address of the L2 PTE corresponding to `va`.
 *
 * If `alloc` is 1 (i.e., non-zero):
 *   allocate intermediate page table pages when they are missing.
 * If `alloc` is 0:
 *   trigger FATAL if the required mapping does not exist.
 *
 * Hints:
 *  - The return value is a pointer to the L2 page table page PTE.
 *    With this pointer, you can directly modify the PTE permission bits.
 *  - Use pmalloc() to allocate new page table pages when necessary.
 */
uint* walk(uint* root, uint va, int alloc) {
    ASSERT(root != NULL, "root is null");

    /* TODO: your code here */
    uint *l2_pte = NULL;
    int l1_idx = va >> 22;
    uint* l2_pa = 0;

    if (root[l1_idx] == 0) { 
        if (alloc) {
            l2_pa = pmalloc(1);
            uint ppn = ((uint)l2_pa >> 12);
            root[l1_idx] =  (ppn << 10) | FLAG_NEXT_LEVEL;
        } else { 
            FATAL("required mapping does not exist.");
        }
    } 

    ASSERT(root[l1_idx] & FLAG_NEXT_LEVEL, "invalid l1 PTE");
    l2_pa = (void*) ((root[l1_idx] << 2) & ~0xfff);
    uint l2_idx = (va >> 12) & 0x3FF;
    l2_pte = &l2_pa[l2_idx];
    return l2_pte;
}


/* mapping a virtual page to a physical page for pid */
void page_table_map(int pid, uint vpage_no, uint ppage_id) {
    ASSERT(pid > 0 && pid <= MAX_NPROCESS, "pid is illegal");
    // calculate the virtual address and physical address
    uint va = vpage_no * PAGE_SIZE;
    uint pa = ppage_id * PAGE_SIZE + APPS_PAGES_BASE;

    uint *root = pid_to_pagetable_base[pid];
    /* [lab5-ex2]
     * Establish a mapping from virtual address `va` to physical address `pa`
     * for process `pid`.
     *
     * Implement virtual-to-physical address translation as follows:
     * (1) If the page table for `pid` does not exist, allocate and initialize it.
     *     (1.a) If `pid` corresponds to a system process (pid < USER_PID_START),
     *           map the predefined kernel memory regions into the process
     *           address space using identity mapping.
     *           The predefined regions include:
     *
     *     | Start Address   | # Pages | Size  | Explanation                              |
     *     +-----------------+---------+-------+------------------------------------------+
     *     | RAM_START       | 512     | 2 MB  | EGOS region (code+data+heap+stack)       |
     *     | APPS_PAGES_BASE | 1024    | 4 MB  | free memory for physical page allocation |
     *     | CLINT_BASE      | 16      | 64 KB | Memory-mapped registers for timer        |
     *     | UART_BASE       | 1       | 4 KB  | Memory-mapped registers for TTY          |
     *     | SDHCI_BASE      | 1       | 4 KB  | Memory-mapped registers for SD           |
     *     | ETH_CTL_BASE    | 32      | 128KB | Memory-mapped registers for e1000        |
     *     | FLASH_ROM_BASE  | 1024    | 4 MB  | Flash storage for kernel image           |
     *     | SHELL_WORK_DIR  | 1       | 4 KB  | Work dir (see apps/app.h)                |
     *
     *     Hint: you should use "setup_identity_region()"
     *
     *     (1.b) If the process is a user process (pid >= USER_PID_START),
     *       you need only to map:
     *
     *     | Start Address   | # Pages | Size  | Explanation                              |
     *     +-----------------+---------+-------+------------------------------------------+
     *     | SHELL_WORK_DIR  | 1       | 4 KB  | Work dir (see apps/app.h)                |
     *
     *     (Why? Try without this mapping)
     *
     * (2) If the page table already exists,
     *     invoke `walk()` to obtain the pointer to the target PTE,
     *     and update the PTE accordingly.
     *
     *     Note:
     *     Both system processes and user processes execute in user mode.
     *     Set the PTE permission bits to reflect the corresponding privilege level.
     */

    /* TODO: your code here */
    if (root == NULL) {
        root = pmalloc(1);
        pid_to_pagetable_base[pid] = root;
    
        if (pid < USER_PID_START) {
            setup_identity_region(pid, RAM_START, 512, LEAF_FLAG);
            setup_identity_region(pid, APPS_PAGES_BASE, 1024, LEAF_FLAG);
            setup_identity_region(pid, CLINT_BASE, 16, LEAF_FLAG);
            setup_identity_region(pid, UART_BASE, 1, LEAF_FLAG);
            setup_identity_region(pid, SDHCI_BASE, 1, LEAF_FLAG);
            setup_identity_region(pid, ETH_CTL_BASE, 32, LEAF_FLAG);
            setup_identity_region(pid, FLASH_ROM_BASE, 1024, LEAF_FLAG);
            setup_identity_region(pid, SHELL_WORK_DIR, 1, LEAF_FLAG);
        } else { 
            setup_identity_region(pid, SHELL_WORK_DIR, 1, LEAF_FLAG);
        }
    }

    uint* l2_pte = walk(root, va, 1);
    ASSERT(*l2_pte == 0, "l2_pte already mapped");
    *l2_pte = (pa >> 2) | LEAF_FLAG;
}

/* [lab5-ex3]
 * Switch the active address space to process `pid`.
 *
 * Hints: * Use assembly (`asm`) to update the `satp` CSR.
 */
void page_table_switch(int pid) {
    ASSERT(pid > 0 && pid <= MAX_NPROCESS, "pid is invalid");
    /* ensure all updates to page table are settled */
    fence();

    /* TODO: your code here */
    uint* root = pid_to_pagetable_base[pid];
    ASSERT(root != NULL, "pagetable root is NULL");

    uint satp = (1U << 31) | (((uint)root) >> 12);
    asm("csrw satp, %0" ::"r"(satp));

    /* wait flushing TLB entries */
    fence();
}

/* [lab5-ex4]
 * Translate the virtual address `va` to its corresponding physical address
 * and return the resulting physical address.
 */
uint page_table_translate(int pid, uint va) {

    /* TODO: your code here */
    ASSERT(pid > 0 && pid <= MAX_NPROCESS, "pid is invalid");
    uint* root = pid_to_pagetable_base[pid];
    ASSERT(root != NULL, "root is null");

    uint* l2_pte = walk(root, va, 0);
    ASSERT((*l2_pte & PTE_V) != 0, "invalid l2_pte");
    uint pa_base = (*l2_pte << 2) & ~0xFFF;
    uint pa = pa_base | (va & 0xFFF);
    return pa;
}



/* [lab5-ex5]
 * Release the page table and free all associated pages.
 */
void page_table_free(int pid) {
    ASSERT(pid > 0 && pid <= MAX_NPROCESS, "pid is invalid");

     /* To release the page table:
     * (1) Free all pages reachable from the page table root
     *     using pfree():
     *       (a) iterate over each L1 PTE;
     *           (b) for each valid L2 page table, iterate over its PTEs;
     *               (c) free each mapped data page;
     *           (b) free the L2 page table page;
     *       (a) free the L1 page table page referenced by the root.
     *
     * (2) Reset pid_to_pagetable_base[pid] to 0.
     *
     * Note:
     * - Use pfree() to release pages.
     * - Do not free shared pages that were not allocated by your implementation.
     * - Use num_free_pages() to see number of ree pages, which is helpful to
     *   check memory leakage
     */

    /* TODO: your code here */
    uint* root = pid_to_pagetable_base[pid];
    if (root == NULL) return;
    for (uint l1_idx = 0; l1_idx < 1024; l1_idx++) {
        uint l1_pte = root[l1_idx];

        if (!(l1_pte & PTE_V)) continue;

        uint* l2_pa = (void*) ((l1_pte << 2) & ~0xfff);

        for (uint l2_idx = 0; l2_idx < 1024; l2_idx++) {
            uint l2_pte = l2_pa[l2_idx];

            if (!(l2_pte & PTE_V)) continue;

            uint pa = (l2_pte << 2) & ~0xFFF;

            uint va = (l1_idx << 22) | (l2_idx << 12);

            if (pa != va && pa >= APPS_PAGES_BASE && pa < RAM_END) {
                pfree((void*)pa);
            }
        }

        pfree(l2_pa);
    }

    pfree(root);
    pid_to_pagetable_base[pid] = 0;
}


void vm_init() {
    // essentially disable PMP checking
    // FIXME: Can we do better on PMP protection for egos?
    asm("csrw pmpaddr0,%0" :: "r" (~0UL));
    asm("csrw pmpcfg0,%0" :: "r"(0x1 << 3 /*A*/ | 0x7 /*R/W/X*/ ));

    earth->mmu_map = page_table_map;
    earth->mmu_free = page_table_free;
    earth->mmu_switch = page_table_switch;
    earth->mmu_translate = page_table_translate;
}

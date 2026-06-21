/*
 * (C) 2026, Cornell University
 * All rights reserved.
 *
 * Description: bootloader
 * Initialize the tty device, disk device, MMU, and CPU interrupts.
 */

#include "egos.h"

void tty_init();
void disk_init();
void net_init();
void mmu_init();
void intr_init(uint core_id);
void timer_init(uint core_id);
void grass_entry(uint core_id);

struct grass* grass = (void*)GRASS_STRUCT;
struct earth* earth = (void*)EARTH_STRUCT;

void boot() {
    uint core_id;
    asm("csrr %0, mhartid" : "=r"(core_id));

    if (booted_core_cnt++ == 0) {
        /* The first booted core needs to do some more work. */
        tty_init();
        CRITICAL("--- Booting on QEMU with core #%d ---", core_id);

        disk_init();
        SUCCESS("Finished initializing the tty and disk devices");

        net_init();
        SUCCESS("Finished initializing the network interface hooks");

        mmu_init();
        timer_init(core_id);
        intr_init(core_id);
        SUCCESS("Finished initializing the MMU, timer and interrupts");

        grass_entry(core_id);
    } else {
        SUCCESS("--- Core #%d starts running ---", core_id);
    }
}

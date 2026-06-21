/*
 * (C) 2026, Cornell University
 * All rights reserved.
 *
 * Description: earth-level network device hooks
 */

#include "egos.h"
#include <string.h>

#define PCI_VENDOR_DEVICE 0x00
#define PCI_COMMAND       0x04
#define PCI_BAR0          0x10

#define PCI_COMMAND_IO_EN      (1u << 0)
#define PCI_COMMAND_MEM_EN     (1u << 1)
#define PCI_COMMAND_BUSMASTER  (1u << 2)

#define E1000_VENDOR_ID 0x8086
#define E1000_DEVICE_ID 0x100e
#define E1000_CTRL      0x00000
#define E1000_EECD      0x00010
#define E1000_STATUS    0x00008

#define E1000_TCTL   0x00400
#define E1000_TIPG   0x00410
#define E1000_TDBAL  0x03800
#define E1000_TDBAH  0x03804
#define E1000_TDLEN  0x03808
#define E1000_TDH    0x03810
#define E1000_TDT    0x03818

#define E1000_RCTL   0x00100
#define E1000_RDBAL  0x02800
#define E1000_RDBAH  0x02804
#define E1000_RDLEN  0x02808
#define E1000_RDH    0x02810
#define E1000_RDT    0x02818
#define E1000_RDTR   0x02820
#define E1000_RXDCTL 0x02828
#define E1000_RADV   0x0282C
#define E1000_MTA    0x05200
#define E1000_RAL    0x05400
#define E1000_RAH    0x05404

#define E1000_MPC    0x04010
#define E1000_GPRC   0x04074
#define E1000_BPRC   0x04078
#define E1000_MPRC   0x0407C
#define E1000_RNBC   0x040A0
#define E1000_TPR    0x040D0
#define E1000_GPTC   0x04080

#define E1000_CTRL_RST         (1u << 26)
#define E1000_CTRL_SLU         (1u << 6)
#define E1000_CTRL_ASDE        (1u << 5)
#define E1000_EECD_AUTO_RD     (1u << 9)

#define E1000_TXD_STAT_DD      (1u << 0)
#define E1000_TXD_CMD_EOP      (1u << 0)
#define E1000_TXD_CMD_IFCS     (1u << 1)
#define E1000_TCTL_EN          (1u << 1)
#define E1000_TXD_CMD_RS       (1u << 3)
#define E1000_TCTL_PSP         (1u << 3)
#define E1000_TCTL_CT_SHIFT    4
#define E1000_TCTL_COLD_SHIFT  12

#define E1000_RXD_STAT_DD      (1u << 0)
#define E1000_RCTL_EN          (1u << 1)
#define E1000_RCTL_SBP         (1u << 2)
#define E1000_RCTL_UPE         (1u << 3)
#define E1000_RCTL_MPE         (1u << 4)
#define E1000_RXD_STAT_EOP     (1u << 1)
#define E1000_RCTL_BAM         (1u << 15)
#define E1000_RCTL_BSIZE_2048  (0u << 16)
#define E1000_RCTL_SECRC       (1u << 26)
#define E1000_RAH_AV           (1u << 31)

#define E1000_TX_RING_SIZE  16
#define E1000_RX_RING_SIZE  16
#define E1000_TX_BUF_SIZE   2048
#define E1000_RX_BUF_SIZE   2048
#define E1000_TX_WAIT_SPINS (1u << 24)

struct e1000_tx_desc {
    ulonglong buffer_addr;
    ushort length;
    uchar cso;
    uchar cmd;
    uchar status;
    uchar css;
    ushort special;
} __attribute__((packed));

struct e1000_rx_desc {
    ulonglong buffer_addr;
    ushort length;
    ushort csum;
    uchar status;
    uchar errors;
    ushort special;
} __attribute__((packed));

static uint net_mmio_base;

static volatile struct e1000_tx_desc tx_ring[E1000_TX_RING_SIZE] __attribute__((aligned(16)));
static volatile struct e1000_rx_desc rx_ring[E1000_RX_RING_SIZE] __attribute__((aligned(16)));

static uchar tx_bufs[E1000_TX_RING_SIZE][E1000_TX_BUF_SIZE] __attribute__((aligned(64)));
static uchar rx_bufs[E1000_RX_RING_SIZE][E1000_RX_BUF_SIZE] __attribute__((aligned(64)));

static uint tx_index;
static uint rx_index;
static const uchar egos_mac[6] = {0x52, 0x54, 0x00, 0x00, 0x00, 0x01};

static inline ulonglong e1000_dma_addr(const volatile void* ptr) {
    return (ulonglong)(uint)ptr;
}

static inline uint e1000_read(uint reg) {
    return REGW(net_mmio_base, reg);
}

static inline void e1000_write(uint reg, uint value) {
    REGW(net_mmio_base, reg) = value;
}

static inline void e1000_dma_sync(void) {
    asm volatile("fence rw, rw" ::: "memory");
}

static void e1000_reset() {
    uint ctrl = e1000_read(E1000_CTRL);
    e1000_write(E1000_CTRL, ctrl | E1000_CTRL_RST);

    int timeout = 1000000;
    while (timeout-- > 0) {
        if (e1000_read(E1000_EECD) & E1000_EECD_AUTO_RD) {
            return;
        }
    }

    INFO("e1000 reset did not report AUTO_RD; continuing with QEMU fallback");
}

static void e1000_set_mac() {
    uint ral = egos_mac[0] |
               (egos_mac[1] << 8) |
               (egos_mac[2] << 16) |
               (egos_mac[3] << 24);
    uint rah = egos_mac[4] |
               (egos_mac[5] << 8) |
               E1000_RAH_AV;

    e1000_write(E1000_RAL, ral);
    e1000_write(E1000_RAH, rah);
}

static void e1000_clear_mta() {
    int i;

    for (i = 0; i < 128; i++) {
        e1000_write(E1000_MTA + i * sizeof(uint), 0);
    }
}

static void e1000_configure_ctrl() {
    uint ctrl = e1000_read(E1000_CTRL);
    ctrl |= E1000_CTRL_SLU | E1000_CTRL_ASDE;
    e1000_write(E1000_CTRL, ctrl);
}

static void e1000_tx_init() {
    int i;

    for (i = 0; i < E1000_TX_RING_SIZE; i++) {
        tx_ring[i].buffer_addr = e1000_dma_addr(&tx_bufs[i][0]);
        tx_ring[i].length = 0;
        tx_ring[i].cso = 0;
        tx_ring[i].cmd = 0;
        tx_ring[i].status = E1000_TXD_STAT_DD;
        tx_ring[i].css = 0;
        tx_ring[i].special = 0;
    }

    tx_index = 0;

    e1000_write(E1000_TDBAL, (uint)e1000_dma_addr(tx_ring));
    e1000_write(E1000_TDBAH, 0);
    e1000_write(E1000_TDLEN, sizeof(tx_ring));

    e1000_write(E1000_TDH, 0);
    e1000_write(E1000_TDT, 0);

    e1000_write(E1000_TCTL,
        E1000_TCTL_EN |
        E1000_TCTL_PSP |
        (0x10u << E1000_TCTL_CT_SHIFT) |
        (0x40u << E1000_TCTL_COLD_SHIFT));
    e1000_write(E1000_TIPG, 0x0060200Au);
}

static void e1000_rx_init() {
    int i;

    for (i = 0; i < E1000_RX_RING_SIZE; i++) {
        rx_ring[i].buffer_addr = e1000_dma_addr(&rx_bufs[i][0]);
        rx_ring[i].length = 0;
        rx_ring[i].csum = 0;
        rx_ring[i].status = 0;
        rx_ring[i].errors = 0;
        rx_ring[i].special = 0;
    }

    rx_index = 0;

    e1000_write(E1000_RDBAL, (uint)e1000_dma_addr(rx_ring));
    e1000_write(E1000_RDBAH, 0);
    e1000_write(E1000_RDLEN, sizeof(rx_ring));

    e1000_write(E1000_RDH, 0);
    e1000_write(E1000_RDT, E1000_RX_RING_SIZE - 1);

    e1000_write(E1000_RCTL,
        E1000_RCTL_EN |
        E1000_RCTL_SBP |
        E1000_RCTL_UPE |
        E1000_RCTL_MPE |
        E1000_RCTL_BAM |
        E1000_RCTL_SECRC |
        E1000_RCTL_BSIZE_2048);

}

static uint pci_cfg_read(uint offset) {
    return REGW(ETH_PCI_ECAM, offset);
}

static void pci_cfg_write(uint offset, uint value) {
    REGW(ETH_PCI_ECAM, offset) = value;
}

static void net_send(int length, void* packet) {
    volatile struct e1000_tx_desc* desc = &tx_ring[tx_index];
    uint next_index;
    uint spin_count;

    ASSERT(length > 0, "invalid transmit length");
    ASSERT(length <= E1000_TX_BUF_SIZE, "packet too large for tx buffer");

    spin_count = 0;
    while ((desc->status & E1000_TXD_STAT_DD) == 0) {
        if (++spin_count == E1000_TX_WAIT_SPINS) {
            FATAL("tx descriptor stuck: slot=%d status=0x%x", tx_index, desc->status);
        }
    }

    memcpy(tx_bufs[tx_index], packet, length);

    desc->buffer_addr = e1000_dma_addr(&tx_bufs[tx_index][0]);
    desc->length = length;
    desc->cso = 0;
    desc->cmd = E1000_TXD_CMD_EOP | E1000_TXD_CMD_IFCS | E1000_TXD_CMD_RS;
    desc->status = 0;
    desc->css = 0;
    desc->special = 0;

    next_index = (tx_index + 1) % E1000_TX_RING_SIZE;
    e1000_dma_sync();
    e1000_write(E1000_TDT, next_index);
    tx_index = next_index;
}

static int net_recv(void* buffer) {
    volatile struct e1000_rx_desc* desc = &rx_ring[rx_index];

    if ((desc->status & E1000_RXD_STAT_DD) == 0) {
        return 0;
    }

    if ((desc->status & E1000_RXD_STAT_EOP) == 0) {
        FATAL("multi-descriptor packets not supported yet");
    }

    int len = desc->length;
    ASSERT(len <= E1000_RX_BUF_SIZE, "received packet too large");
    memcpy(buffer, rx_bufs[rx_index], len);

    desc->length = 0;
    desc->csum = 0;
    desc->status = 0;
    desc->errors = 0;
    desc->special = 0;

    e1000_dma_sync();
    e1000_write(E1000_RDT, rx_index);
    rx_index = (rx_index + 1) % E1000_RX_RING_SIZE;

    return len;
}

void net_init() {
    uint vendor_device = pci_cfg_read(PCI_VENDOR_DEVICE);
    ushort vendor_id = vendor_device & 0xffff;
    ushort device_id = vendor_device >> 16;

    ASSERT(vendor_id != 0xffff, "no PCI network device present at ETH_PCI_ECAM");
    ASSERT(vendor_id == E1000_VENDOR_ID, "unexpected PCI network vendor");
    ASSERT(device_id == E1000_DEVICE_ID, "unexpected PCI network device");

    uint pci_command = pci_cfg_read(PCI_COMMAND);
    pci_command |= PCI_COMMAND_MEM_EN | PCI_COMMAND_BUSMASTER;
    pci_command &= ~PCI_COMMAND_IO_EN;
    pci_cfg_write(PCI_COMMAND, pci_command);
    pci_cfg_write(PCI_BAR0, ETH_CTL_BASE);

    net_mmio_base = pci_cfg_read(PCI_BAR0) & ~0xfu;
    ASSERT(net_mmio_base != 0, "invalid e1000 MMIO base");

    INFO("Detected e1000 at PCI cfg 0x%x with MMIO base 0x%x",
         ETH_PCI_ECAM, net_mmio_base);

    e1000_reset();
    e1000_configure_ctrl();
    e1000_set_mac();
    e1000_clear_mta();
    e1000_rx_init();
    e1000_tx_init();

    earth->net_send = net_send;
    earth->net_recv = net_recv;
}

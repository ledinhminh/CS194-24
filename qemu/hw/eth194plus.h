#ifndef HW_ETH194PLUS_H
#define HW_ETH194PLUS_H 1

#define ETH194PLUS_PMEM_SIZE    (16*1024)
#define ETH194PLUS_PMEM_START   (16*1024)
#define ETH194PLUS_PMEM_END     (ETH194plus_PMEM_SIZE+ETH194plus_PMEM_START)
#define ETH194PLUS_MEM_SIZE     ETH194plus_PMEM_END

#define ETH194PLUS_MAX_FRAME_SIZE 1514
struct eth194plus_fb {
    uint8_t df, hf;
    uint32_t nphy;
    uint16_t cnt;
    uint8_t d[ETH194PLUS_MAX_FRAME_SIZE];
} __attribute__((packed));

typedef struct ETH194PlusState {
    MemoryRegion io;
    uint8_t cmd;
    uint8_t tsr;
    uint16_t tcnt;
    uint16_t rcnt;
    uint8_t rsr;
    uint8_t rxcr;
    uint8_t isr;
    uint8_t imr;
    uint8_t phys[6]; /* mac address */
    uint8_t mult[8]; /* multicast mask array */
    uint32_t curr;
    uint32_t curw;
    uint32_t tblw;
    uint8_t rv;
    uint8_t wv;
    qemu_irq irq;
    NICState *nic;
    NICConf c;
} ETH194PlusState;

void eth194plus_setup_io(ETH194PlusState *s, unsigned size);
extern const VMStateDescription vmstate_eth194plus;
void eth194plus_reset(ETH194PlusState *s);
int eth194plus_can_receive(NetClientState *nc);
ssize_t eth194plus_receive(NetClientState *nc, const uint8_t *buf, size_t size_);
uint32_t eth194plus_chain_for_mac(ETH194PlusState* s, uint8_t* src);

#endif

#ifndef HW_ETH194_H
#define HW_ETH194_H 1

#define ETH194_PMEM_SIZE    (16*1024)
#define ETH194_PMEM_START   (16*1024)
#define ETH194_PMEM_END     (ETH194_PMEM_SIZE+ETH194_PMEM_START)
#define ETH194_MEM_SIZE     ETH194_PMEM_END

#define ETH194_MAX_FRAME_SIZE 1514
struct eth194_fb {
    uint8_t df, hf;
    uint32_t nphy;
    uint16_t cnt;
    uint8_t d[ETH194_MAX_FRAME_SIZE];
} __attribute__((packed));

typedef struct ETH194State {
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
    uint8_t rv;
    uint8_t wv;
    qemu_irq irq;
    NICState *nic;
    NICConf c;
} ETH194State;

void eth194_setup_io(ETH194State *s, unsigned size);
extern const VMStateDescription vmstate_eth194;
void eth194_reset(ETH194State *s);
int eth194_can_receive(NetClientState *nc);
ssize_t eth194_receive(NetClientState *nc, const uint8_t *buf, size_t size_);

#endif

/* ne2k-pci.c: A NE2000 clone on PCI bus driver for Linux. */
/*
	A Linux device driver for PCI NE2000 clones.

	Authors and other copyright holders:
	1992-2000 by Donald Becker, NE2000 core and various modifications.
	1995-1998 by Paul Gortmaker, core modifications and PCI support.
	Copyright 1993 assigned to the United States Government as represented
	by the Director, National Security Agency.

	This software may be used and distributed according to the terms of
	the GNU General Public License (GPL), incorporated herein by reference.
	Drivers based on or derived from this code fall under the GPL and must
	retain the authorship, copyright and license notice.  This file is not
	a complete program and may only be used when the entire operating
	system is licensed under the GPL.

	The author may be reached as becker@scyld.com, or C/O
	Scyld Computing Corporation
	410 Severn Ave., Suite 210
	Annapolis MD 21403

	Issues remaining:
	People are making PCI ne2000 clones! Oh the horror, the horror...
	Limited full-duplex support.
*/

#define DRV_NAME	"eth194"
#define DRV_VERSION	"TERRIBLE"
#define DRV_RELDATE	"1/1/2014"
#define ETH194_MAX_FRAME_SIZE 1514


/* The user-configurable values.
   These may be modified when a driver module is loaded.*/

static int debug = 2;			/* 1 normal messages, 0 quiet .. 7 verbose. */

#define MAX_UNITS 8				/* More are supported, limit only on options */
/* Used to pass the full-duplex flag, etc. */
static int full_duplex[MAX_UNITS];
static int options[MAX_UNITS];

/* Force a non std. amount of memory.  Units are 256 byte pages. */
/* #define PACKETBUF_MEMSIZE	0x40 */


#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/pci.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/ethtool.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>

#include <asm/io.h>
#include <asm/irq.h>
#include <asm/uaccess.h>

#include "8390.h"

/* These identify the driver base version and may not be removed. */
static const char version[] __devinitconst =
	KERN_INFO DRV_NAME ".c:v" DRV_VERSION " " DRV_RELDATE
	" CS194-gh\n";

#if defined(__powerpc__)
#define inl_le(addr)  le32_to_cpu(inl(addr))
#define inw_le(addr)  le16_to_cpu(inw(addr))
#endif

#define PFX DRV_NAME ": "

MODULE_AUTHOR("CS194-gh");
MODULE_DESCRIPTION("ETH194 something or other driver");
MODULE_LICENSE("GPL");

module_param(debug, int, 0);
module_param_array(options, int, NULL, 0);
module_param_array(full_duplex, int, NULL, 0);
MODULE_PARM_DESC(debug, "debug level (1-2)");
MODULE_PARM_DESC(options, "Bit 5: full duplex");
MODULE_PARM_DESC(full_duplex, "full duplex setting(s) (1)");

/* Some defines that people can play with if so inclined. */

/* Use 32 bit data-movement operations instead of 16 bit. */
#define USE_LONGIO

/* Do we implement the read before write bugfix ? */
/* #define NE_RW_BUGFIX */

/* Flags.  We rename an existing ei_status field to store flags! */
/* Thus only the low 8 bits are usable for non-init-time flags. */
#define ne2k_flags reg0
enum {
	ONLY_16BIT_IO=8, ONLY_32BIT_IO=4,	/* Chip can do only 16/32-bit xfers. */
	FORCE_FDX=0x20,						/* User override. */
	REALTEK_FDX=0x40, HOLTEK_FDX=0x80,
	STOP_PG_0x60=0x100,
};

enum ne2k_pci_chipsets {
	CH_RealTek_RTL_8029 = 0,
	CH_Winbond_89C940,
	CH_Compex_RL2000,
	CH_KTI_ET32P2,
	CH_NetVin_NV5000SC,
	CH_Via_86C926,
	CH_SureCom_NE34,
	CH_Winbond_W89C940F,
	CH_Holtek_HT80232,
	CH_Holtek_HT80229,
	CH_Winbond_89C940_8c4a,
    CH_194,
};


static struct {
	char *name;
	int flags;
} pci_clone_list[] __devinitdata = {
	{"RealTek RTL-8029", REALTEK_FDX},
	{"Winbond 89C940", 0},
	{"Compex RL2000", 0},
	{"KTI ET32P2", 0},
	{"NetVin NV5000SC", 0},
	{"Via 86C926", ONLY_16BIT_IO},
	{"SureCom NE34", 0},
	{"Winbond W89C940F", 0},
	{"Holtek HT80232", ONLY_16BIT_IO | HOLTEK_FDX},
	{"Holtek HT80229", ONLY_32BIT_IO | HOLTEK_FDX | STOP_PG_0x60 },
	{"Winbond W89C940(misprogrammed)", 0},
    {"Berkeley ETH194", 0}, // What is 0?
    {"Berkeley ETH194", 0}, // What is 0?
	{NULL,}
};


static DEFINE_PCI_DEVICE_TABLE(ne2k_pci_tbl) = {
	{ 0x10ec, 0x8029, PCI_ANY_ID, PCI_ANY_ID, 0, 0, CH_RealTek_RTL_8029 },
	{ 0x1050, 0x0940, PCI_ANY_ID, PCI_ANY_ID, 0, 0, CH_Winbond_89C940 },
	{ 0x11f6, 0x1401, PCI_ANY_ID, PCI_ANY_ID, 0, 0, CH_Compex_RL2000 },
	{ 0x8e2e, 0x3000, PCI_ANY_ID, PCI_ANY_ID, 0, 0, CH_KTI_ET32P2 },
	{ 0x4a14, 0x5000, PCI_ANY_ID, PCI_ANY_ID, 0, 0, CH_NetVin_NV5000SC },
	{ 0x1106, 0x0926, PCI_ANY_ID, PCI_ANY_ID, 0, 0, CH_Via_86C926 },
	{ 0x10bd, 0x0e34, PCI_ANY_ID, PCI_ANY_ID, 0, 0, CH_SureCom_NE34 },
	{ 0x1050, 0x5a5a, PCI_ANY_ID, PCI_ANY_ID, 0, 0, CH_Winbond_W89C940F },
	{ 0x12c3, 0x0058, PCI_ANY_ID, PCI_ANY_ID, 0, 0, CH_Holtek_HT80232 },
	{ 0x12c3, 0x5598, PCI_ANY_ID, PCI_ANY_ID, 0, 0, CH_Holtek_HT80229 },
	{ 0x8c4a, 0x1980, PCI_ANY_ID, PCI_ANY_ID, 0, 0, CH_Winbond_89C940_8c4a },
    { 0x0ca1, 0xe195, PCI_ANY_ID, PCI_ANY_ID, 0, 0, CH_194 },
    { 0x0ca1, 0xe194, PCI_ANY_ID, PCI_ANY_ID, 0, 0, CH_194 },
	{ 0, }
};
MODULE_DEVICE_TABLE(pci, ne2k_pci_tbl);

// ETH194 stuff
struct e194_buffer {
    uint8_t df, hf;//device and host flags
    uint32_t nphy;//pointer to next buffer
    uint16_t cnt;//how big the d is
    uint8_t d[ETH194_MAX_FRAME_SIZE]; //the actual data
} __attribute__((packed));

// Let's not screw around with e194_buffer
struct e194_list_node {
    struct e194_chain_head* this;
    struct e194_list_node* next;
};

struct e194_chain_head {
	uint32_t curw;
	struct e194_buffer *write;
	struct e194_buffer *chain;
} __attribute__((packed));

#define WRITE_CHAIN_SIZE 1024 
#define MAC_BUFFER_CHAIN_SIZE 32 
// #define E8390_PAGE0     0x00    /* Select page chip registers */
// #define E8390_PAGE1     0x40    /* using the two high-order bits */
// #define E8390_PAGE2     0x80    /* Page 3 is invalid. */
// Yeah, sure.
#define E8390_PAGE3     0xC0

// We must set page by CR, so constants are not as they seem qemu-side

#define EN3_CURR0       0x02
#define EN3_CURR1       0x04
#define EN3_CURR2       0x07
#define EN3_CURR3       0x08
#define EN3_CURW0       0x0A
#define EN3_CURW1       0x0B
#define EN3_CURW2       0x0C
#define EN3_CURW3       0x0D
#define EN3_TBLW0       0x01
#define EN3_TBLW1       0x09
#define EN3_TBLW2       0x0E
#define EN3_TBLW3       0x0F

// Function declaration for the unwashed masses
static irqreturn_t __eth194_ei_interrupt(int irq, void *dev_id);
static void __eth194_ei_receive(struct net_device *dev);
static void __eth194_tx_intr(struct net_device *dev);

#define INTR_HANDLER __eth194_ei_interrupt

// Ugh...
// For some reason achieving the linkage is tricky.
// Just give up and include the damn thing.
#include "lib8390.c"

/* ---- No user-serviceable parts below ---- */

#define NE_BASE	 (dev->base_addr)
#define NE_CMD	 	0x00
#define NE_DATAPORT	0x10	/* NatSemi-defined port window offset. */
#define NE_RESET	0x1f	/* Issue a read to reset, a write to clear. */
#define NE_IO_EXTENT	0x20

#define NESM_START_PG	0x40	/* First page of TX buffer */
#define NESM_STOP_PG	0x80	/* Last page +1 of RX ring */


static int ne2k_pci_open(struct net_device *dev);
static int ne2k_pci_close(struct net_device *dev);

static void ne2k_pci_reset_8390(struct net_device *dev);
static void ne2k_pci_get_8390_hdr(struct net_device *dev, struct e8390_pkt_hdr *hdr,
			  int ring_page);
static void ne2k_pci_block_input(struct net_device *dev, int count,
			  struct sk_buff *skb, int ring_offset);
static void ne2k_pci_block_output(struct net_device *dev, const int count,
		const unsigned char *buf, const int start_page);
static const struct ethtool_ops ne2k_pci_ethtool_ops;



/* There is no room in the standard 8390 structure for extra info we need,
   so we build a meta/outer-wrapper structure.. */
struct ne2k_pci_card {
	struct net_device *dev;
	struct pci_dev *pci_dev;
};



/*
  NEx000-clone boards have a Station Address (SA) PROM (SAPROM) in the packet
  buffer memory space.  By-the-spec NE2000 clones have 0x57,0x57 in bytes
  0x0e,0x0f of the SAPROM, while other supposed NE2000 clones must be
  detected by their SA prefix.

  Reading the SAPROM from a word-wide card with the 8390 set in byte-wide
  mode results in doubled values, which can be detected and compensated for.

  The probe is also responsible for initializing the card and filling
  in the 'dev' and 'ei_status' structures.
*/

static const struct net_device_ops ne2k_netdev_ops = {
	.ndo_open		= ne2k_pci_open,
	.ndo_stop		= ne2k_pci_close,
	.ndo_start_xmit		= ei_start_xmit,
	.ndo_tx_timeout		= ei_tx_timeout,
	.ndo_get_stats		= ei_get_stats,
	.ndo_set_rx_mode	= ei_set_multicast_list,
	.ndo_validate_addr	= eth_validate_addr,
	.ndo_set_mac_address 	= eth_mac_addr,
	.ndo_change_mtu		= eth_change_mtu,
#ifdef CONFIG_NET_POLL_CONTROLLER
	.ndo_poll_controller = ei_poll,
#endif
};

static int __devinit ne2k_pci_init_one (struct pci_dev *pdev,
				     const struct pci_device_id *ent)
{
	struct net_device *dev;
	int i;
	unsigned char SA_prom[32];
	int start_page, stop_page;
	int irq, reg0, chip_idx = ent->driver_data;
	static unsigned int fnd_cnt;
	long ioaddr;
	int flags = pci_clone_list[chip_idx].flags;

	struct e194_buffer *read;
	struct e194_buffer *write;
	struct e194_buffer *temp;
  	uint32_t wpoint, tbl_ptr;

/* when built into the kernel, we only print version if device is found */
#ifndef MODULE
	static int printed_version;
	if (!printed_version++)
		printk(version);
#endif

	fnd_cnt++;

	i = pci_enable_device (pdev);
	if (i)
		return i;

	ioaddr = pci_resource_start (pdev, 0);
	irq = pdev->irq;

	if (!ioaddr || ((pci_resource_flags (pdev, 0) & IORESOURCE_IO) == 0)) {
		dev_err(&pdev->dev, "no I/O resource at PCI BAR #0\n");
		return -ENODEV;
	}

	if (request_region (ioaddr, NE_IO_EXTENT, DRV_NAME) == NULL) {
		dev_err(&pdev->dev, "I/O resource 0x%x @ 0x%lx busy\n",
			NE_IO_EXTENT, ioaddr);
		return -EBUSY;
	}

	reg0 = inb(ioaddr);
	if (reg0 == 0xFF)
		goto err_out_free_res;

	/* Do a preliminary verification that we have a 8390. */
	{
		int regd;
		outb(E8390_NODMA+E8390_PAGE1+E8390_STOP, ioaddr + E8390_CMD);
		regd = inb(ioaddr + 0x0d);
		outb(0xff, ioaddr + 0x0d);
		outb(E8390_NODMA+E8390_PAGE0, ioaddr + E8390_CMD);
		inb(ioaddr + EN0_COUNTER0); /* Clear the counter by reading. */
		if (inb(ioaddr + EN0_COUNTER0) != 0) {
			outb(reg0, ioaddr);
			outb(regd, ioaddr + 0x0d);	/* Restore the old values. */
			goto err_out_free_res;
		}
	}

	/* Allocate net_device, dev->priv; fill in 8390 specific dev fields. */
	dev = alloc_ei_netdev();
	if (!dev) {
		dev_err(&pdev->dev, "cannoti allocate ethernet device\n");
		goto err_out_free_res;
	}
	dev->netdev_ops = &ne2k_netdev_ops;

	SET_NETDEV_DEV(dev, &pdev->dev);

	/* Reset card. Who knows what dain-bramaged state it was left in. */
	{
		unsigned long reset_start_time = jiffies;

		outb(inb(ioaddr + NE_RESET), ioaddr + NE_RESET);

		/* This looks like a horrible timing loop, but it should never take
		   more than a few cycles.
		*/
		while ((inb(ioaddr + EN0_ISR) & ENISR_RESET) == 0)
			/* Limit wait: '2' avoids jiffy roll-over. */
			if (jiffies - reset_start_time > 2) {
				dev_err(&pdev->dev,
					"Card failure (no reset ack).\n");
				goto err_out_free_netdev;
			}

		outb(0xff, ioaddr + EN0_ISR);		/* Ack all intr. */
	}

	/* Read the 16 bytes of station address PROM.
	   We must first initialize registers, similar to NS8390_init(eifdev, 0).
	   We can't reliably read the SAPROM address without this.
	   (I learned the hard way!). */
	{
		struct {unsigned char value, offset; } program_seq[] = {
			{E8390_NODMA+E8390_PAGE0+E8390_STOP, E8390_CMD}, /* Select page 0*/
			{0x49,	EN0_DCFG},	/* Set word-wide access. */
			{0x00,	EN0_RCNTLO},	/* Clear the count regs. */
			{0x00,	EN0_RCNTHI},
			{0x00,	EN0_IMR},	/* Mask completion irq. */
			{0xFF,	EN0_ISR},
			{E8390_RXOFF, EN0_RXCR},	/* 0x20  Set to monitor */
			{E8390_TXOFF, EN0_TXCR},	/* 0x02  and loopback mode. */
			{32,	EN0_RCNTLO},
			{0x00,	EN0_RCNTHI},
			{0x00,	EN0_RSARLO},	/* DMA starting at 0x0000. */
			{0x00,	EN0_RSARHI},
			{E8390_RREAD+E8390_START, E8390_CMD},
		};
		for (i = 0; i < ARRAY_SIZE(program_seq); i++)
			outb(program_seq[i].value, ioaddr + program_seq[i].offset);

	}

	/* Note: all PCI cards have at least 16 bit access, so we don't have
	   to check for 8 bit cards.  Most cards permit 32 bit access. */
	if (flags & ONLY_32BIT_IO) {
		for (i = 0; i < 4 ; i++)
			((u32 *)SA_prom)[i] = le32_to_cpu(inl(ioaddr + NE_DATAPORT));
	} else
		for(i = 0; i < 32 /*sizeof(SA_prom)*/; i++)
			SA_prom[i] = inb(ioaddr + NE_DATAPORT);

	/* We always set the 8390 registers for word mode. */
	outb(0x49, ioaddr + EN0_DCFG);
	start_page = NESM_START_PG;

	stop_page = flags & STOP_PG_0x60 ? 0x60 : NESM_STOP_PG;

	/* Set up the rest of the parameters. */
	dev->irq = irq;
	dev->base_addr = ioaddr;
	pci_set_drvdata(pdev, dev);

	ei_status.name = pci_clone_list[chip_idx].name;
	ei_status.tx_start_page = start_page;
	ei_status.stop_page = stop_page;
	ei_status.word16 = 1;
	ei_status.ne2k_flags = flags;
	if (fnd_cnt < MAX_UNITS) {
		if (full_duplex[fnd_cnt] > 0  ||  (options[fnd_cnt] & FORCE_FDX))
			ei_status.ne2k_flags |= FORCE_FDX;
	}

	ei_status.rx_start_page = start_page + TX_PAGES;
#ifdef PACKETBUF_MEMSIZE
	/* Allow the packet buffer size to be overridden by know-it-alls. */
	ei_status.stop_page = ei_status.tx_start_page + PACKETBUF_MEMSIZE;
#endif

	ei_status.reset_8390 = &ne2k_pci_reset_8390;
	ei_status.block_input = &ne2k_pci_block_input;
	ei_status.block_output = &ne2k_pci_block_output;
	ei_status.get_8390_hdr = &ne2k_pci_get_8390_hdr;
	ei_status.priv = (unsigned long) pdev;

	dev->ethtool_ops = &ne2k_pci_ethtool_ops;
	NS8390_init(dev, 0);

	memcpy(dev->dev_addr, SA_prom, dev->addr_len);
	memcpy(dev->perm_addr, dev->dev_addr, dev->addr_len);

	i = register_netdev(dev);
	if (i)
		goto err_out_free_netdev;

	printk("%s: %s found at %#lx, IRQ %d, %pM.\n",
	       dev->name, pci_clone_list[chip_idx].name, ioaddr, dev->irq,
	       dev->dev_addr);
    printk(KERN_INFO "\tThat MAC address isn't right but that's cool.\n");

    printk(KERN_INFO "%s: initializing write buffer chain...\n", dev->name);
    
    // read = kmalloc(sizeof(struct e194_buffer) * 20, GFP_DMA | GFP_KERNEL);
    // memset(read, 0, sizeof(struct e194_buffer)*20);
    write = kmalloc(sizeof(struct e194_buffer) * WRITE_CHAIN_SIZE, GFP_DMA | GFP_KERNEL);
    printk(KERN_INFO "%s: got %zu bytes actually\n", dev->name, ksize(write));
    memset(write, 0, sizeof(struct e194_buffer) * WRITE_CHAIN_SIZE);
/*    for (i = 0; i < 19; i++){
    	temp = (read + sizeof(struct e194_buffer)*i);
    	temp->nphy =  virt_to_bus(temp + sizeof(struct e194_buffer));
    }*/

    for (i = 0; i < WRITE_CHAIN_SIZE - 1; i++){
    	temp = write + i; //because type is implied
    	temp->nphy = virt_to_bus(temp + 1);
        printk(KERN_INFO "%s: buffer at 0x%x has nphy=0x%x\n", dev->name, (uint32_t) virt_to_bus(temp), (uint32_t) temp->nphy);
    }

    printk(KERN_INFO "%s: write->npy=%X", dev->name, write->nphy);

    // ei_status.read = read;
    ei_status.write = write;
    ei_status.write_free = write;
    ei_status.write_end = write + WRITE_CHAIN_SIZE - 1;
    printk(KERN_INFO "%s: MAKING SURE ITS NULL write_end=0x%X\n", dev->name, (uint32_t) ei_status.write_end->nphy);

    // printk(KERN_INFO "%s: read.cnt=%d\n", dev->name, ei_status.read->cnt);

    // TODO: Check allocations didn't fail...
    
    printk(KERN_INFO "%s: we have write=%i\n", dev->name, (uint32_t) virt_to_bus(ei_status.write));
    
    // printk(KERN_INFO "%s: writing CURR[0..3]...\n", dev->name);
    
    // Switch to page 3.
    outb(E8390_PAGE3, ioaddr + E8390_CMD);
    
    // Write values
    
    printk(KERN_INFO "%s: writing CURW[0..3]...\n", dev->name);
    
  	wpoint = virt_to_bus(ei_status.write);
    outb(wpoint >> 0, ioaddr + EN3_CURW0);
    outb(wpoint >> 8, ioaddr + EN3_CURW1);
    outb(wpoint >> 16, ioaddr + EN3_CURW2);
    outb(wpoint >> 24, ioaddr + EN3_CURW3);
    
    printk(KERN_INFO "%s: wrote CURW[0..3]... curw=0x%X\n", dev->name, wpoint);

    //Allocate mac_buffer
    ei_status.mac_table = kmalloc(sizeof(uint32_t) * 256, GFP_DMA | GFP_KERNEL);
    memset(ei_status.mac_table, 0, sizeof(uint32_t) * 256);
    printk(KERN_INFO "%s: ei_status = 0x%X", dev->name, (void *) &ei_status);
    printk(KERN_INFO "%s: MAC TABLE IS AT 0x%X\n", dev->name, (void *) ei_status.mac_table);
    
    printk(KERN_INFO "%s: writing TBLW[0..3]...\n", dev->name);
    
  	tbl_ptr = virt_to_bus(ei_status.mac_table);
    outb(tbl_ptr >> 0, ioaddr + EN3_TBLW0);
    outb(tbl_ptr >> 8, ioaddr + EN3_TBLW1);
    outb(tbl_ptr >> 16, ioaddr + EN3_TBLW2);
    outb(tbl_ptr >> 24, ioaddr + EN3_TBLW3);
    
    printk(KERN_INFO "%s: wrote TBLW[0..3]... tblw=0x%X\n", dev->name, wpoint);

	return 0;

err_out_free_netdev:
	free_netdev (dev);
err_out_free_res:
	release_region (ioaddr, NE_IO_EXTENT);
	pci_set_drvdata (pdev, NULL);
	return -ENODEV;

}

/*
 * Magic incantation sequence for full duplex on the supported cards.
 */
static inline int set_realtek_fdx(struct net_device *dev)
{
	long ioaddr = dev->base_addr;

	outb(0xC0 + E8390_NODMA, ioaddr + NE_CMD); /* Page 3 */
	outb(0xC0, ioaddr + 0x01); /* Enable writes to CONFIG3 */
	outb(0x40, ioaddr + 0x06); /* Enable full duplex */
	outb(0x00, ioaddr + 0x01); /* Disable writes to CONFIG3 */
	outb(E8390_PAGE0 + E8390_NODMA, ioaddr + NE_CMD); /* Page 0 */
	return 0;
}

static inline int set_holtek_fdx(struct net_device *dev)
{
	long ioaddr = dev->base_addr;

	outb(inb(ioaddr + 0x20) | 0x80, ioaddr + 0x20);
	return 0;
}

static int ne2k_pci_set_fdx(struct net_device *dev)
{
	if (ei_status.ne2k_flags & REALTEK_FDX)
		return set_realtek_fdx(dev);
	else if (ei_status.ne2k_flags & HOLTEK_FDX)
		return set_holtek_fdx(dev);

	return -EOPNOTSUPP;
}

static int ne2k_pci_open(struct net_device *dev)
{
	int ret = request_irq(dev->irq, INTR_HANDLER, IRQF_SHARED, dev->name, dev);
	if (ret)
		return ret;

	if (ei_status.ne2k_flags & FORCE_FDX)
		ne2k_pci_set_fdx(dev);

	ei_open(dev);
	return 0;
}

static int ne2k_pci_close(struct net_device *dev)
{
	ei_close(dev);
	free_irq(dev->irq, dev);
	return 0;
}

/* Hard reset the card.  This used to pause for the same period that a
   8390 reset command required, but that shouldn't be necessary. */
static void ne2k_pci_reset_8390(struct net_device *dev)
{
	unsigned long reset_start_time = jiffies;

	if (debug > 1) printk("%s: Resetting the 8390 t=%ld...\n",
						  dev->name, jiffies);

	outb(inb(NE_BASE + NE_RESET), NE_BASE + NE_RESET);

	ei_status.txing = 0;
	ei_status.dmaing = 0;

	/* This check _should_not_ be necessary, omit eventually. */
	while ((inb(NE_BASE+EN0_ISR) & ENISR_RESET) == 0)
		if (jiffies - reset_start_time > 2) {
			printk("%s: ne2k_pci_reset_8390() did not complete.\n", dev->name);
			break;
		}
	outb(ENISR_RESET, NE_BASE + EN0_ISR);	/* Ack intr. */
}

/* Grab the 8390 specific header. Similar to the block_input routine, but
   we don't need to be concerned with ring wrap as the header will be at
   the start of a page, so we optimize accordingly. */

static void ne2k_pci_get_8390_hdr(struct net_device *dev, struct e8390_pkt_hdr *hdr, int ring_page)
{
	long nic_base = dev->base_addr;
    
    

	/* This *shouldn't* happen. If it does, it's the last thing you'll see */
	if (ei_status.dmaing) {
		printk("%s: DMAing conflict in ne2k_pci_get_8390_hdr "
			   "[DMAstat:%d][irqlock:%d].\n",
			   dev->name, ei_status.dmaing, ei_status.irqlock);
		return;
	}

	ei_status.dmaing |= 0x01;
	outb(E8390_NODMA+E8390_PAGE0+E8390_START, nic_base+ NE_CMD);
	outb(sizeof(struct e8390_pkt_hdr), nic_base + EN0_RCNTLO);
	outb(0, nic_base + EN0_RCNTHI);
	outb(0, nic_base + EN0_RSARLO);		/* On page boundary */
	outb(ring_page, nic_base + EN0_RSARHI);
	outb(E8390_RREAD+E8390_START, nic_base + NE_CMD);

	if (ei_status.ne2k_flags & ONLY_16BIT_IO) {
		insw(NE_BASE + NE_DATAPORT, hdr, sizeof(struct e8390_pkt_hdr)>>1);
	} else {
		*(u32*)hdr = le32_to_cpu(inl(NE_BASE + NE_DATAPORT));
		le16_to_cpus(&hdr->count);
	}

	outb(ENISR_RDC, nic_base + EN0_ISR);	/* Ack intr. */
	ei_status.dmaing &= ~0x01;
}

/* Block input and output, similar to the Crynwr packet driver.  If you
   are porting to a new ethercard, look at the packet driver source for hints.
   The NEx000 doesn't share the on-board packet memory -- you have to put
   the packet out through the "remote DMA" dataport using outb. */

static void ne2k_pci_block_input(struct net_device *dev, int count,
				 struct sk_buff *skb, int ring_offset)
{
	long nic_base = dev->base_addr;
	char *buf = skb->data;

	printk(KERN_INFO "BLOCK INPUT");
	/* This *shouldn't* happen. If it does, it's the last thing you'll see */
	if (ei_status.dmaing) {
		printk("%s: DMAing conflict in ne2k_pci_block_input "
			   "[DMAstat:%d][irqlock:%d].\n",
			   dev->name, ei_status.dmaing, ei_status.irqlock);
		return;
	}
	ei_status.dmaing |= 0x01;
	if (ei_status.ne2k_flags & ONLY_32BIT_IO)
		count = (count + 3) & 0xFFFC;
	outb(E8390_NODMA+E8390_PAGE0+E8390_START, nic_base+ NE_CMD);
	outb(count & 0xff, nic_base + EN0_RCNTLO);
	outb(count >> 8, nic_base + EN0_RCNTHI);
	outb(ring_offset & 0xff, nic_base + EN0_RSARLO);
	outb(ring_offset >> 8, nic_base + EN0_RSARHI);
	outb(E8390_RREAD+E8390_START, nic_base + NE_CMD);

	if (ei_status.ne2k_flags & ONLY_16BIT_IO) {
		insw(NE_BASE + NE_DATAPORT,buf,count>>1);
		if (count & 0x01) {
			buf[count-1] = inb(NE_BASE + NE_DATAPORT);
		}
	} else {
		insl(NE_BASE + NE_DATAPORT, buf, count>>2);
		if (count & 3) {
			buf += count & ~3;
			if (count & 2) {
				__le16 *b = (__le16 *)buf;

				*b++ = cpu_to_le16(inw(NE_BASE + NE_DATAPORT));
				buf = (char *)b;
			}
			if (count & 1)
				*buf = inb(NE_BASE + NE_DATAPORT);
		}
	}

	outb(ENISR_RDC, nic_base + EN0_ISR);	/* Ack intr. */
	ei_status.dmaing &= ~0x01;
}

static void ne2k_pci_block_output(struct net_device *dev, int count,
				  const unsigned char *buf, const int start_page)
{
	long nic_base;
	unsigned curr;
	// unsigned long dma_start;
	nic_base = NE_BASE;

	// Is there a CURR buffer chain already?
	// Set page 3 first.
	outb(E8390_PAGE3+E8390_NODMA, nic_base + E8390_CMD);

	curr = 0;
	curr += inb(nic_base + EN3_CURR0);
	curr += inb(nic_base + EN3_CURR1) << 8;
	curr += inb(nic_base + EN3_CURR2) << 16;
	curr += inb(nic_base + EN3_CURR3) << 24;

	printk(KERN_INFO "%s: CURR = %d\n", dev->name, curr);
	if (!(inb(nic_base + EN3_CURR0) | inb(nic_base + EN3_CURR1) | inb(nic_base + EN3_CURR2) | inb(nic_base + EN3_CURR3))) {
		// CURR is null. Give it a buffer.
		printk(KERN_INFO "%s: CURR is null...\n", dev->name);
		if (ei_status.read == 0x0) { // Nothing in our buffer chain yet.
			printk(KERN_INFO "\tOur read is null too. Allocing buffer...\n");
			ei_status.read = kmalloc(sizeof(struct e194_buffer), GFP_DMA | GFP_KERNEL);
    		memset(ei_status.read, 0, sizeof(struct e194_buffer));
		} else { // There was already stuff in buffer chain. Pick the first one off, fix it up, and assign CURR to it.
			printk(KERN_INFO "\tWe have some buffers already. Using the first one\n");
			ei_status.read_free = bus_to_virt(ei_status.read->nphy);
			ei_status.read->nphy = 0x0;
		}

		printk(KERN_INFO "%s: Copying in buffer...%i bytes to 0x%x at 0x%x.\n", dev->name, (uint16_t) count, (uint32_t) virt_to_bus(ei_status.read->d), (uint32_t)virt_to_bus(ei_status.read));
	    ei_status.read->cnt = (u16) count;
	    ei_status.read->df = 0x00;
	    ei_status.read->hf = 0x00;
		memcpy(ei_status.read->d, buf, (u16) count);

		printk(KERN_INFO "%s: Updating CURR\n", dev->name);

		unsigned read_bus_addr = virt_to_bus(ei_status.read);

	    outb(read_bus_addr, nic_base + EN3_CURR0);
	    outb(read_bus_addr >> 8, nic_base + EN3_CURR1);
	    outb(read_bus_addr >> 16, nic_base + EN3_CURR2);
	    outb(read_bus_addr >> 24, nic_base + EN3_CURR3);

	} else { // CURR is not null.
		struct e194_buffer* new_frame;
		struct e194_buffer* last = ei_status.read;

		printk(KERN_INFO "%s: CURR is not null...\n", dev->name);
		if (ei_status.read_free == 0x0) {
			printk(KERN_INFO "\tNothing on free list. Allocing buffer...\n");
			new_frame = kmalloc(sizeof(struct e194_buffer), GFP_DMA | GFP_KERNEL);
    		memset(new_frame, 0, sizeof(struct e194_buffer));
		} else {
			printk(KERN_INFO "\tFree list not null. Picking from free list\n");
			new_frame = ei_status.read_free;
			ei_status.read_free = bus_to_virt(new_frame->nphy);
			new_frame->nphy = (uint32_t) virt_to_bus(NULL);
		}

		while (true) {
			if (last->nphy == 0x0)
				break;
			last = bus_to_virt(last->nphy);
		}

		last->nphy = (uint32_t) virt_to_bus(new_frame);

		printk(KERN_INFO "%s: Copying in buffer...%i bytes to %X.\n", dev->name, (u16) count, (unsigned int) virt_to_bus(new_frame->d));
	    new_frame->cnt = (u16) count;
		memcpy(new_frame->d, buf, count);
	}

	/* On little-endian it's always safe to round the count up for
	   word writes. */
	if (ei_status.ne2k_flags & ONLY_32BIT_IO)
		count = (count + 3) & 0xFFFC;
	else
		if (count & 0x01)
			count++;

	/* This *shouldn't* happen. If it does, it's the last thing you'll see */
	if (ei_status.dmaing) {
		printk("%s: DMAing conflict in ne2k_pci_block_output."
			   "[DMAstat:%d][irqlock:%d]\n",
			   dev->name, ei_status.dmaing, ei_status.irqlock);
		return;
	}
	ei_status.dmaing |= 0x01;
	/* We should already be in page 0, but to be safe... */
	outb(E8390_PAGE0+E8390_START+E8390_NODMA, nic_base + NE_CMD);

#ifdef NE8390_RW_BUGFIX
	/* Handle the read-before-write bug the same way as the
	   Crynwr packet driver -- the NatSemi method doesn't work.
	   Actually this doesn't always work either, but if you have
	   problems with your NEx000 this is better than nothing! */
	outb(0x42, nic_base + EN0_RCNTLO);
	outb(0x00, nic_base + EN0_RCNTHI);
	outb(0x42, nic_base + EN0_RSARLO);
	outb(0x00, nic_base + EN0_RSARHI);
	outb(E8390_RREAD+E8390_START, nic_base + NE_CMD);
#endif
	outb(ENISR_RDC, nic_base + EN0_ISR);

	/* Now the normal output. */

	/* Remote byte count reg WR */
	outb(count & 0xff, nic_base + EN0_RCNTLO);
	
	/* High byte of tx byte count WR */
	outb(count >> 8,   nic_base + EN0_RCNTHI);

	/* Remote start address reg 0 */
	outb(0x00, nic_base + EN0_RSARLO);

	/* Remote start address reg 1 */
	outb(start_page, nic_base + EN0_RSARHI);

	/* Command Register 0x00 */
	outb(E8390_RWRITE+E8390_START, nic_base + NE_CMD);

	// if (ei_status.ne2k_flags & ONLY_16BIT_IO) {
	// 	outsw(NE_BASE + NE_DATAPORT, buf, count>>1);
	// } else {
	// 	outsl(NE_BASE + NE_DATAPORT, buf, count>>2);
	// 	if (count & 3) {
	// 		buf += count & ~3;
	// 		if (count & 2) {
	// 			__le16 *b = (__le16 *)buf;

	// 			outw(le16_to_cpu(*b++), NE_BASE + NE_DATAPORT);
	// 			buf = (char *)b;
	// 		}
	// 	}
	// }

	// dma_start = jiffies;

	// //while we're not done dma'ing
	// while ((inb(nic_base + EN0_ISR) & ENISR_RDC) == 0)
	// 	if (jiffies - dma_start > 2) {			 
	//		//Avoid clock roll-over. 
	// 		printk(KERN_WARNING "%s: timeout waiting for Tx RDC.\n", dev->name);
	// 		ne2k_pci_reset_8390(dev);
	// 		NS8390_init(dev,1);
	// 		break;
	// 	}

	// outb(ENISR_RDC, nic_base + EN0_ISR);	 Ack intr. 
	ei_status.dmaing &= ~0x01;
}

/**
 * ei_interrupt - handle the interrupts from an 8390
 * @irq: interrupt number
 * @dev_id: a pointer to the net_device
 *
 * Handle the ether interface interrupts. We pull packets from
 * the 8390 via the card specific functions and fire them at the networking
 * stack. We also handle transmit completions and wake the transmit path if
 * necessary. We also update the counters and do other housekeeping as
 * needed.
 * 
 * This copy of __ei_interrupt calls __eth194_ei_receive instead.
 *
 */

static irqreturn_t __eth194_ei_interrupt(int irq, void *dev_id)
{
	struct net_device *dev = dev_id;
	unsigned long e8390_base = dev->base_addr;
	int interrupts, nr_serviced = 0;
	struct ei_device *ei_local = netdev_priv(dev);
    
    printk(KERN_INFO "%s: INTERRUPT\n", dev->name);

	/*
	 *	Protect the irq test too.
	 */

	spin_lock(&ei_local->page_lock);

	if (ei_local->irqlock) {
		/*
		 * This might just be an interrupt for a PCI device sharing
		 * this line
		 */
		netdev_err(dev, "Interrupted while interrupts are masked! isr=%#2x imr=%#2x\n",
			   ei_inb_p(e8390_base + EN0_ISR),
			   ei_inb_p(e8390_base + EN0_IMR));
		spin_unlock(&ei_local->page_lock);
		return IRQ_NONE;
	}

	/* Change to page 0 and read the intr status reg. */
	ei_outb_p(E8390_NODMA+E8390_PAGE0, e8390_base + E8390_CMD);
	if (ei_debug > 3)
		netdev_dbg(dev, "interrupt(isr=%#2.2x)\n",
			   ei_inb_p(e8390_base + EN0_ISR));

	/* !!Assumption!! -- we stay in page 0.	 Don't break this. */
	// printk(KERN_INFO "%s: WHILE INTERRUPT HANDLER START", dev->name);
	while ((interrupts = ei_inb_p(e8390_base + EN0_ISR)) != 0 &&
	       ++nr_serviced < MAX_SERVICE) {
		if (!netif_running(dev)) {
			netdev_warn(dev, "interrupt from stopped card\n");
			/* rmk - acknowledge the interrupts */
			ei_outb_p(interrupts, e8390_base + EN0_ISR);
			interrupts = 0;
			break;
		}
		if (interrupts & ENISR_OVER)
			ei_rx_overrun(dev);
		else if (interrupts & (ENISR_RX+ENISR_RX_ERR)) {
			/* Got a good (?) packet. */
			printk(KERN_INFO "%s: \tRECEIVE START interrupts=0x%X\n", dev->name, ei_inb_p(e8390_base + EN0_ISR));
			__eth194_ei_receive(dev);
			// printk(KERN_INFO "%s: RECEIVE FIN interrupts=0x%X\n", dev->name, ei_inb_p(e8390_base + EN0_ISR));
		}
		/* Push the next to-transmit packet through. */
		if (interrupts & ENISR_TX){
			__eth194_tx_intr(dev);
		} else if (interrupts & ENISR_TX_ERR){
			ei_tx_err(dev);
		}


		if (interrupts & ENISR_COUNTERS) {
			dev->stats.rx_frame_errors += ei_inb_p(e8390_base + EN0_COUNTER0);
			dev->stats.rx_crc_errors   += ei_inb_p(e8390_base + EN0_COUNTER1);
			dev->stats.rx_missed_errors += ei_inb_p(e8390_base + EN0_COUNTER2);
			ei_outb_p(ENISR_COUNTERS, e8390_base + EN0_ISR); /* Ack intr. */
		}

		/* Ignore any RDC interrupts that make it back to here. */
		if (interrupts & ENISR_RDC)
			ei_outb_p(ENISR_RDC, e8390_base + EN0_ISR);

		ei_outb_p(E8390_NODMA+E8390_PAGE0, e8390_base + E8390_CMD);
	}
	// printk(KERN_INFO "%s: WHILE INTERRUPT HANDLER FIN", dev->name);

	if (interrupts && ei_debug) {
		ei_outb_p(E8390_NODMA+E8390_PAGE0+E8390_START, e8390_base + E8390_CMD);
		if (nr_serviced >= MAX_SERVICE) {
			/* 0xFF is valid for a card removal */
			if (interrupts != 0xFF)
				netdev_warn(dev, "Too much work at interrupt, status %#2.2x\n",
					    interrupts);
			ei_outb_p(ENISR_ALL, e8390_base + EN0_ISR); /* Ack. most intrs. */
		} else {
			netdev_warn(dev, "unknown interrupt %#2x\n", interrupts);
			ei_outb_p(0xff, e8390_base + EN0_ISR); /* Ack. all intrs. */
		}
	}
	spin_unlock(&ei_local->page_lock);
	return IRQ_RETVAL(nr_serviced > 0);
}
/**
 * ei_tx_intr - transmit interrupt handler
 * @dev: network device for which tx intr is handled
 *
 * We have finished a transmit: check for errors and then trigger the next
 * packet to be sent. Called with lock held.
 */

static void __eth194_tx_intr(struct net_device *dev)
{
	uint32_t curr;

	unsigned long e8390_base = dev->base_addr;
	struct ei_device *ei_local = netdev_priv(dev);
	int status = ei_inb(e8390_base + EN0_TSR);

	outb(E8390_PAGE0, e8390_base + E8390_CMD);
	outb(ENISR_TX, e8390_base + EN0_ISR); /* Ack intr. */
	
	/*
	 * There are two Tx buffers, see which one finished, and trigger
	 * the send of another one if it exists.
	 */
	ei_local->txqueue--;

	if (ei_local->tx1 < 0) {
		if (ei_local->lasttx != 1 && ei_local->lasttx != -1)
			pr_err("%s: bogus last_tx_buffer %d, tx1=%d\n",
			       ei_local->name, ei_local->lasttx, ei_local->tx1);
		ei_local->tx1 = 0;
		if (ei_local->tx2 > 0) {
			ei_local->txing = 1;
			NS8390_trigger_send(dev, ei_local->tx2, ei_local->tx_start_page + 6);
			dev->trans_start = jiffies;
			ei_local->tx2 = -1,
			ei_local->lasttx = 2;
		} else
			ei_local->lasttx = 20, ei_local->txing = 0;
	} else if (ei_local->tx2 < 0) {
		if (ei_local->lasttx != 2  &&  ei_local->lasttx != -2)
			pr_err("%s: bogus last_tx_buffer %d, tx2=%d\n",
			       ei_local->name, ei_local->lasttx, ei_local->tx2);
		ei_local->tx2 = 0;
		if (ei_local->tx1 > 0) {
			ei_local->txing = 1;
			NS8390_trigger_send(dev, ei_local->tx1, ei_local->tx_start_page);
			dev->trans_start = jiffies;
			ei_local->tx1 = -1;
			ei_local->lasttx = 1;
		} else
			ei_local->lasttx = 10, ei_local->txing = 0;
	}

	//we should set it to be page 3 and the change it back to page 0
	outb(E8390_PAGE3, e8390_base + E8390_CMD);
	curr = 0;
	curr += inb(e8390_base + EN3_CURR0);
	curr += inb(e8390_base + EN3_CURR1) << 8;
	curr += inb(e8390_base + EN3_CURR2) << 16;
	curr += inb(e8390_base + EN3_CURR3) << 24;
	outb(E8390_PAGE0, e8390_base + E8390_CMD);

	// if(curr == 0x0){
	// 	printk(KERN_INFO "%s: NULL CURR=0x%X\n", dev->name, curr);
	// 	//curr in device is null, lets check if read's next is null
	// 	if (ei_local->read->nphy != 0x0){
	// 		printk(KERN_INFO "%s: READ'S NEXT IS NOT NULL... we need to tell it to do some stuff\n", dev->name);
	// 	}
	// } else {
	// 	printk(KERN_INFO "%s: NOT NULL CURR=0x%X\n", dev->name, curr);
	// }

	/* Minimize Tx latency: update the statistics after we restart TXing. */
	if (status & ENTSR_COL)
		dev->stats.collisions++;
	if (status & ENTSR_PTX)
		dev->stats.tx_packets++;
	else {
		dev->stats.tx_errors++;
		if (status & ENTSR_ABT) {
			dev->stats.tx_aborted_errors++;
			dev->stats.collisions += 16;
		}
		if (status & ENTSR_CRS)
			dev->stats.tx_carrier_errors++;
		if (status & ENTSR_FU)
			dev->stats.tx_fifo_errors++;
		if (status & ENTSR_CDH)
			dev->stats.tx_heartbeat_errors++;
		if (status & ENTSR_OWC)
			dev->stats.tx_window_errors++;
	}
	// printk(KERN_INFO "INTERRUPTED TRANSMIT FIN\n");
	netif_wake_queue(dev);
}

/**
 * ei_receive - receive some packets
 * @dev: network device with which receive will be run
 *
 * We have a good packet(s), get it/them out of the buffers.
 * Called with lock held.
 * 
 * This copy of ei_receive knows about how ETH194 memory works. 
 *
 */

static void __eth194_ei_receive(struct net_device *dev)
{
	unsigned long e8390_base = dev->base_addr;
	struct ei_device *ei_local = netdev_priv(dev);
	unsigned char rxing_page, this_frame, next_frame;
	unsigned short current_offset;
	int rx_pkt_count = 0;
	struct e8390_pkt_hdr rx_frame;
	int num_rx_pages = ei_local->stop_page-ei_local->rx_start_page;
    
	outb(E8390_PAGE3, e8390_base + E8390_CMD);
    
	while (++rx_pkt_count < 10) {
		int pkt_len;
        unsigned pkt_stat;
	    struct e194_buffer *temp;

        printk(KERN_INFO "%s: \t\twrite bus_head=0x%X virt_head=0x%X\n", dev->name, (unsigned int) virt_to_bus(ei_local->write), (void *) ei_local->write);
                
        // If we're done, bail
		// THIS IS BAD WE SHOULDN'T BE RUNNING OUT OF BUFFERS
        if (ei_local->write == 0x0) {
            printk(KERN_INFO "%s: buffer chain ended. \n", dev->name);
            break;
        }

        // If we hit a not ready buffer, we're dont, bail
        // we should also shuffle buffers
        if(!(ei_local->write->df & 0x03)){
        	printk(KERN_INFO "%s: \t\tbuffer isn't ready...", dev->name);
        	if(ei_local->write == ei_local->write_free){
        		//there's no free buffers... sad
        	} else {
        		// printk(KERN_INFO "%s: SHUFFLE TIME! write_end->nphy=0x%X (should be null)\n", dev->name, (uint32_t) ei_local->write_end->nphy);
        		//they're not the same, we have some free buffers, shuffle time
        		temp = ei_local->write_free;
        		while(bus_to_virt(temp->nphy) != ei_local->write){
        			temp->df = 0x00;
        			temp = bus_to_virt(temp->nphy);
        		}
        		//temp points to the one before whatever isn't ready

        		temp->nphy = virt_to_bus(NULL);
        		ei_local->write_end->nphy = virt_to_bus(ei_local->write_free);
        		printk(KERN_INFO "%s: SHUFFLE TIME! write_end->nphy=0x%X (should not be null)\n", dev->name, (uint32_t) ei_local->write_end->nphy);
        		ei_local->write_end = temp;
        		ei_local->write_free = ei_local->write;
        		// printk(KERN_INFO "%s: SHUFFLE TIME! write_end->nphy=0x%X (should be null)\n", dev->name, (uint32_t) ei_local->write_end->nphy);
        	}
			ei_outb_p(ENISR_RDC+ENISR_RX+ENISR_RX_ERR, e8390_base+EN0_ISR);
        	break;
        }
        
        pkt_len = ei_local->write->cnt;
        // Ahh, this is the RSR.
        // Switch to page 0 just in case.
        outb(E8390_PAGE0, dev->base_addr + E8390_CMD);
        pkt_stat = inb(dev->base_addr + EN0_RSR);
        // printk(KERN_INFO "%s: pkt_len=%d, pkt_stat=%d\n", dev->name, pkt_len, pkt_stat);

		// next_frame = this_frame + 1 + ((pkt_len+4)>>8);

		/* Check for bogosity warned by 3c503 book: the status byte is never
		   written.  This happened a lot during testing! This code should be
		   cleaned up someday. */
        /*
		if (rx_frame.next != next_frame &&
		    rx_frame.next != next_frame + 1 &&
		    rx_frame.next != next_frame - num_rx_pages &&
		    rx_frame.next != next_frame + 1 - num_rx_pages) {
			ei_local->current_page = rxing_page;
			ei_outb(ei_local->current_page-1, e8390_base+EN0_BOUNDARY);
			dev->stats.rx_errors++;
			continue;
		}
        */

		if (pkt_len < 60  ||  pkt_len > 1518) {
			// if (ei_debug)
				netdev_dbg(dev, "bogus packet size: %d, status=%#2x nxpg=%#2x\n",
					   rx_frame.count, rx_frame.status,
					   rx_frame.next);
			dev->stats.rx_errors++;
			dev->stats.rx_length_errors++;
		} else if ((pkt_stat & 0x0F) == ENRSR_RXOK) {
			struct sk_buff *skb;
            
            printk(KERN_INFO "%s: \t\tpacket looks good, proceeding...\n", dev->name);

			skb = netdev_alloc_skb(dev, pkt_len + 2);
			if (skb == NULL) {
				if (ei_debug > 1)
					netdev_dbg(dev, "Couldn't allocate a sk_buff of size %d\n",
						   pkt_len);
				dev->stats.rx_dropped++;
				break;
			} else {
				skb_reserve(skb, 2);	/* IP headers on 16 byte boundaries */
				skb_put(skb, pkt_len);	/* Make room */
				// ei_block_input(dev, pkt_len, skb, current_offset + sizeof(rx_frame));
                printk(KERN_INFO "%s: \t\tmemcpy from d=0x%x to skb at %p...\n", dev->name, ei_local->write->d, skb->data);
                memcpy(skb->data, ei_local->write->d, pkt_len);

                //clear out flags
                printk(KERN_INFO "%s: \t\tclearing out flags\n", dev->name);
                ei_local->write->df = 0x00;

                // Throw it away. TODO: ...don't throw it away
                printk(KERN_INFO "%s: \t\tmoving to nphy=0x%x\n", dev->name, ei_local->write->nphy);
                ei_local->write = bus_to_virt(ei_local->write->nphy);
                
				skb->protocol = eth_type_trans(skb, dev);
				if (!skb_defer_rx_timestamp(skb))
					netif_rx(skb);
				dev->stats.rx_packets++;
				dev->stats.rx_bytes += pkt_len;
				if (pkt_stat & ENRSR_PHY)
					dev->stats.multicast++;
			}
		} else {
			// if (ei_debug)
				netdev_dbg(dev, "bogus packet: status=%#2x nxpg=%#2x size=%d\n",
					   rx_frame.status, rx_frame.next,
					   rx_frame.count);
			dev->stats.rx_errors++;
			/* NB: The NIC counts CRC, frame and missed errors. */
			if (pkt_stat & ENRSR_FO)
				dev->stats.rx_fifo_errors++;
		}
        // Why do they do this all the way here?
		// next_frame = rx_frame.next;

		/* This _should_ never happen: it's here for avoiding bad clones. */
		if (next_frame >= ei_local->stop_page) {
			netdev_notice(dev, "next frame inconsistency, %#2x\n",
				      next_frame);
			next_frame = ei_local->rx_start_page;
		}
		// ei_local->current_page = next_frame;
		// ei_outb_p(next_frame-1, e8390_base+EN0_BOUNDARY);
		printk(KERN_INFO "%s\n", dev->name);
	}

	// comment: combined this with last outb
    // outb(ENISR_RDC, dev->base_addr + EN0_ISR);	/* Ack intr. */


	/* We used to also ack ENISR_OVER here, but that would sometimes mask
	   a real overrun, leaving the 8390 in a stopped state with rec'vr off. */
	ei_outb_p(ENISR_RDC+ENISR_RX+ENISR_RX_ERR, e8390_base+EN0_ISR);

}

static void ne2k_pci_get_drvinfo(struct net_device *dev,
				 struct ethtool_drvinfo *info)
{
	struct ei_device *ei = netdev_priv(dev);
	struct pci_dev *pci_dev = (struct pci_dev *) ei->priv;

	strlcpy(info->driver, DRV_NAME, sizeof(info->driver));
	strlcpy(info->version, DRV_VERSION, sizeof(info->version));
	strlcpy(info->bus_info, pci_name(pci_dev), sizeof(info->bus_info));
}

static const struct ethtool_ops ne2k_pci_ethtool_ops = {
	.get_drvinfo		= ne2k_pci_get_drvinfo,
};

static void __devexit ne2k_pci_remove_one (struct pci_dev *pdev)
{
	struct net_device *dev = pci_get_drvdata(pdev);

	BUG_ON(!dev);
	unregister_netdev(dev);
	release_region(dev->base_addr, NE_IO_EXTENT);
	free_netdev(dev);
	pci_disable_device(pdev);
	pci_set_drvdata(pdev, NULL);
}

#ifdef CONFIG_PM
static int ne2k_pci_suspend (struct pci_dev *pdev, pm_message_t state)
{
	struct net_device *dev = pci_get_drvdata (pdev);

	netif_device_detach(dev);
	pci_save_state(pdev);
	pci_disable_device(pdev);
	pci_set_power_state(pdev, pci_choose_state(pdev, state));

	return 0;
}

static int ne2k_pci_resume (struct pci_dev *pdev)
{
	struct net_device *dev = pci_get_drvdata (pdev);
	int rc;

	pci_set_power_state(pdev, 0);
	pci_restore_state(pdev);

	rc = pci_enable_device(pdev);
	if (rc)
		return rc;

	NS8390_init(dev, 1);
	netif_device_attach(dev);

	return 0;
}

#endif /* CONFIG_PM */


static struct pci_driver ne2k_driver = {
	.name		= DRV_NAME,
	.probe		= ne2k_pci_init_one,
	.remove		= __devexit_p(ne2k_pci_remove_one),
	.id_table	= ne2k_pci_tbl,
#ifdef CONFIG_PM
	.suspend	= ne2k_pci_suspend,
	.resume		= ne2k_pci_resume,
#endif /* CONFIG_PM */

};


static int __init ne2k_pci_init(void)
{
/* when a module, this is printed whether or not devices are found in probe */
#ifdef MODULE
	printk(version);
#endif
	return pci_register_driver(&ne2k_driver);
}


static void __exit ne2k_pci_cleanup(void)
{
	pci_unregister_driver (&ne2k_driver);
}

module_init(ne2k_pci_init);
module_exit(ne2k_pci_cleanup);


/* Cramming Proc Interface in here too
 * Only cuz linking it is easier
 */
#include <linux/proc_fs.h>

struct ei_device* get_eth194(){

	struct ei_device *ei_local;
	struct net_device *dev;

	//gotta find the net device....
	dev = first_net_device(&init_net);
	while (dev) {
		ei_local = netdev_priv(dev);
		// printk(KERN_INFO "got [%s]\n", ei_local->name);
		if (ei_local->name == NULL) {
			dev = next_net_device(dev);
			continue;
		}

		if (strcmp("Berkeley ETH194", ei_local->name) == 0) {
			// printk("FOUND IT: ei_local=0x%X\n", ei_local);
			break;
		} else {
			dev = next_net_device(dev);
		}
	}
	
	return ei_local;    
}


int eth_read(char *buf, char **start, off_t offset, int count, int *eof, void *data){
	int len, a, b, c, d, e, f;
	struct ei_device *eth194;
	uint32_t* mac_1;
	uint32_t* mac_2;
	uint32_t* mac_3;
	uint32_t* mac_4;
	uint32_t* mac_5;
	uint32_t* mac_6;

	len = 0;
	len += sprintf(buf+len, "MAC addresses currently tracked:\n");

	//get the device
	eth194 = get_eth194();

	//go through the tables and find non-null entries
	mac_1 = eth194->mac_table;
	for (a = 0; a < 256; a++){
		//start going through the layers....
		if (bus_to_virt(*mac_1) != bus_to_virt(NULL)){
			mac_2 = bus_to_virt(*mac_1);
			
			for (b = 0; b < 256; b++){
				if (bus_to_virt(*mac_2) != bus_to_virt(NULL)){
					mac_3 = bus_to_virt(*mac_2);

					for(c = 0; c < 256; c++){
						if(bus_to_virt(*mac_3) != bus_to_virt(NULL)){
							mac_4 = bus_to_virt(*mac_3);

							for(d = 0; d < 256; d++){
								if(bus_to_virt(*mac_4) != bus_to_virt(NULL)){

									mac_5 = bus_to_virt(*mac_4);
									for (e = 0; e < 256; e++){
										if(bus_to_virt(*mac_5) != bus_to_virt(NULL)){

											mac_6 = bus_to_virt(*mac_5);
											for (f = 0; f< 256; f++){
												if(bus_to_virt(*mac_6) != bus_to_virt(NULL)){
													// printk("MAC=%X:%X:%X:%X:%X:%X\n", a, b, c, d, e, f);
													len += sprintf(buf+len, "MAC=%X:%X:%X:%X:%X:%X\n", a, b, c, d, e, f);
												}

												mac_6 += 1;
											}

										}
										mac_5 += 1;
									}

								}
								mac_4 += 1;
							}

						}
						mac_3 += 1;
					}
				}
				mac_2 += 1;
			}
		}
		mac_1 += 1;
	}

	return len;
}

int eth_write(struct file *file, const char *buf, int count, void *data) {
    // Is that even possible?
	char proc_data[count + 1];
    unsigned char addr[6] = {0, 0, 0, 0, 0, 0};
	unsigned long long addr_long;
    int i = 0;
	int ret;
	// int mask_addr;
	uint32_t *mac_temp; // An array(pointer) of pointers
    uint32_t *mac_temp_next;
	struct net_device *dev;
	struct ei_device *ei_local;

    struct e194_buffer *temp;
    struct e194_chain_head *chain_head;
	// check to make sure there's only 12
	// count will be 13 because of the newline from echo
	if (count != 13) {
		printk("count is %i which is not 13", count);
		return -1;
	}

	if (copy_from_user(proc_data, buf, count))
		return -EFAULT;

	// add a null character at the end
	proc_data[count] = '\0';

	// try to get the integer out
	ret = kstrtoull(&proc_data, 16, &addr_long);
	if (ret != 0)
		return ret;

	/*  SO WHAT NEEDS TO HAPPEN HERE.....
	 *  Actually need to create the buffer
	 */

    // MAC addresses come off the wire MSB first. So we do it too.
    for (i = 0; i < 6; i++)
        addr[5 - i] = ((addr_long & (0xFFUL << (8 * i))) >> (8 * i));
        
    printk(KERN_INFO "got mac %x:%x:%x:%x:%x:%x\n", addr[0], addr[1], addr[2], addr[3], addr[4], addr[5]);

	//gotta find the net device....
	dev = first_net_device(&init_net);
	while (dev) {
		ei_local = netdev_priv(dev);
		printk(KERN_INFO "got [%s]\n", ei_local->name);
		if (ei_local->name == NULL) {
			dev = next_net_device(dev);	
			continue;
		}
		if (strcmp("Berkeley ETH194", ei_local->name) == 0) {
			printk("FOUND IT: ei_local=0x%X\n", ei_local);
			break;
		} else {
			dev = next_net_device(dev);
		}
	}
    
	ei_local = netdev_priv(dev);

    
    printk("size of uint32_t is %u\n", sizeof(uint32_t));
	mac_temp = ei_local->mac_table;
    printk(KERN_INFO "%s: mac_table at 0x%x\n", dev->name, mac_temp);
    
    for (i = 0; i < 5; i++) { 
        mac_temp_next = mac_temp[addr[i]];
        
        printk(KERN_INFO "%s: next for byte #%d 0x%02x is 0x%x\n", dev->name, i, addr[i], mac_temp_next);
        
        if (mac_temp_next == 0) {
            mac_temp_next = kmalloc(sizeof(uint32_t) * 256, GFP_DMA | GFP_KERNEL);
            mac_temp[addr[i]] = virt_to_bus(mac_temp_next);
            memset(mac_temp_next, 0, sizeof(uint32_t) * 256);
            printk(KERN_INFO "%s: allocated next table at 0x%x (bus address 0x%x): 0x%x = mac_temp[0x%x] = 0x%x\n", dev->name, mac_temp_next, virt_to_bus(mac_temp_next), mac_temp + addr[i], addr[i], mac_temp[addr[i]]);
        	mac_temp = mac_temp_next;
        } else {
        	mac_temp = bus_to_virt(mac_temp_next);
            printk(KERN_INFO "%s: table already allocated\n", dev->name);
        }
        
    }
    
    if (mac_temp[addr[5]] == 0) {

    	//allocate a buffer, add it to the chain head
        struct e194_buffer *right = kmalloc(sizeof(struct e194_buffer) * MAC_BUFFER_CHAIN_SIZE, GFP_DMA | GFP_KERNEL);
        memset(right, 0, sizeof(struct e194_buffer) * MAC_BUFFER_CHAIN_SIZE);

        //set the nphys
        for (i = 0; i < MAC_BUFFER_CHAIN_SIZE - 1; i++){
	    	temp = right + i; //because type is implied
	    	temp->nphy = virt_to_bus(temp + 1);
	    	printk(KERN_INFO "%s: current=0x%x\tnext=0x%x\n", dev->name, virt_to_bus(temp), temp->nphy);
	    }

    	//allocate a chain head
    	chain_head = kmalloc(sizeof(struct e194_chain_head), GFP_DMA | GFP_KERNEL);
    	memset(chain_head, 0, sizeof(struct e194_chain_head));

    	//set the write
        chain_head->write = right;

        //set the curw
        uint32_t curw = virt_to_bus(right);
        chain_head->curw = curw;

        //set the head of the buffer
        chain_head->chain = right;

        //add chain_head to tlb
        mac_temp[addr[5]] = chain_head;
        printk(KERN_INFO "%s: allocated buffer chain for byte 5 %02x at 0x%x\n", dev->name, addr[5], (void*) right);
        
        //add it onto the active right chain
        struct e194_list_node *to_add;
        struct e194_list_node *node;
        if(ei_local->active_write_chains == NULL){
        	//there's no active_chain yet, lets make one
        	node = kmalloc(sizeof(struct e194_list_node), GFP_DMA | GFP_KERNEL);
        	memset(node, 0, sizeof(struct e194_list_node));
        	ei_local->active_write_chains = node;

        	//and add the chain_head into the node
        	node->this = chain_head;
        } else {
        	//already have an active chain, find the last one
	        node = ei_local->active_write_chains;
	        while (node->next != NULL)
	        	node = node->next;
	        to_add = kmalloc(sizeof(struct e194_list_node), GFP_DMA | GFP_KERNEL);
	        memset(to_add , 0, sizeof(struct e194_list_node));
	        to_add->this = chain_head;
	        node->next = to_add;
        }
    } else {
        printk(KERN_INFO "%s: buffer chain already allocated\n", dev->name);
    }
    
    printk(KERN_INFO "mac_table[0x%x] = 0x%x\n", addr[0], ei_local->mac_table[addr[0]]);
    
	return count;
}

int eth_init(void){
	struct proc_dir_entry *e;

	e = create_proc_entry("eth194", 0, NULL);
	e->read_proc = eth_read;
	e->write_proc = eth_write;

	return 0;
}

void eth_cleanup(void){
	remove_proc_entry("eth194", NULL);

}

module_init(eth_init);
module_exit(eth_cleanup);
#define DRV_NAME	"eth194"
#define DRV_VERSION	"1.0"
#define DRV_RELDATE	"4/19/2013"


/* The user-configurable values.
   These may be modified when a driver module is loaded.*/

static int debug = 1;			/* 1 normal messages, 0 quiet .. 7 verbose. */

#define MAX_UNITS 8	

/* Used to pass the full-duplex flag, etc. */
// NOT SURE WHAT THIS IS
static int full_duplex[MAX_UNITS];
static int options[MAX_UNITS];

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

#define PFX DRV_NAME ": "

MODULE_AUTHOR("CS194-gh");
MODULE_DESCRIPTION("ETH194 Something-or-other driver");
MODULE_LICENSE("GPL");

module_param(debug, int, 0);
MODULE_PARM_DESC(debug, "debug level (1-2)");
// DON'T KNOW WHAT THIS IS ---------------------
module_param_array(options, int, NULL, 0);
module_param_array(full_duplex, int, NULL, 0);
MODULE_PARM_DESC(options, "Bit 5: full duplex");
MODULE_PARM_DESC(full_duplex, "full duplex setting(s) (1)");
// ---------------------------------------------

static int __devinit ne2k_pci_init_one (struct pci_dev *pdev,
				     const struct pci_device_id *ent)
{
	struct net_device *dev; //a net_device struct
	int i; //used as return integer for a bunch of stuff
	unsigned char SA_prom[32];
	int start_page, stop_page;
	int irq, reg0, chip_idx = ent->driver_data;
	static unsigned int fnd_cnt;
	long ioaddr;
	int flags = pci_clone_list[chip_idx].flags;

	/* when built into the kernel, we only print version if device is found */
	#ifndef MODULE
		static int printed_version;
		if (!printed_version++)
			printk(version);
	#endif

	fnd_cnt++;

	//initialize the device
	//  asks low-level code to endable IO and memory, wakes up device if suspended
	//  you need to check its return
	i = pci_enable_device (pdev);
	if (i)
		return i;


	//#define pci_resource_start(dev, bar)    ((dev)->resource[(bar)].start)
	ioaddr = pci_resource_start (pdev, 0);


	//get the irq number, i guess
	irq = pdev->irq;

	
	//check the ioaddr that we got, make sure its legit
	if (!ioaddr || ((pci_resource_flags (pdev, 0) & IORESOURCE_IO) == 0)) {
		dev_err(&pdev->dev, "no I/O resource at PCI BAR #0\n");
		return -ENODEV;
	}



	//device is busy... not sure what that means
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
		dev_err(&pdev->dev, "cannot allocate ethernet device\n");
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

	return 0;

err_out_free_netdev:
	free_netdev (dev);
err_out_free_res:
	release_region (ioaddr, NE_IO_EXTENT);
	pci_set_drvdata (pdev, NULL);
	return -ENODEV;

}

// ---------------------------------------------
// DRIVER DEFINITION
// ---------------------------------------------
static struct pci_driver ne2k_driver = {
	.name		= DRV_NAME,
	.probe		= ne2k_pci_init_one,
	.remove		= __devexit_p(ne2k_pci_remove_one),
	.id_table	= ne2k_pci_tbl,
};


// ---------------------------------------------
// DRIVER INITIALIZATION and CLEANUP
// ---------------------------------------------
static int __init eth194_pci_init(void)
{
/* when a module, this is printed whether or not devices are found in probe */
	#ifdef MODULE
		printk(version);
	#endif
		return pci_register_driver(&ne2k_driver);
}

static void __exit eth194_pci_cleanup(void)
{
	pci_unregister_driver (&ne2k_driver);
}


// ---------------------------------------------
// DRIVER REGISTRATION
// ---------------------------------------------
module_init(eth194_pci_init);
module_exit(eth194_pci_cleanup);

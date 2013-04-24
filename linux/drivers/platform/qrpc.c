/*
 *  qrpc.c: QEMU RPC device
 *
 *  Copyright 2013 Palmer Dabbelt <palmer@dabbelt.com>
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/pci.h>

#include <linux/major.h>
#include <linux/fs.h>
#include <linux/blkdev.h>
#include <linux/genhd.h>
#include <linux/hdreg.h>
#include <linux/vmalloc.h>

#define DRV_NAME      "qrpc"
#define DRV_VERSION   "1.0.0"
#define MAX_DEVICES   8

static int qrpc_init_one(struct pci_dev *pdev, const struct pci_device_id *ent);
static void qrpc_remove_one(struct pci_dev *pdev);

static const struct pci_device_id qrpc_pci_tbl[] = {
    { 0xCA1, 0xF194, PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0 },
    { } /* terminate list */
};

static struct pci_driver qrpc_pci_driver = {
	.name			= DRV_NAME,
	.id_table		= qrpc_pci_tbl,
	.probe			= qrpc_init_one,
	.remove			= qrpc_remove_one,
};

// This allows me to keep around a mapping from block devices to PCI
// devices, which is necessary as otherwise I won't be able to find
// the IO region.
struct qrpc_dev {
    struct gendisk *disk;
    struct pci_dev *pdev;
    spinlock_t lock;
};
static struct qrpc_dev *valid_devices[MAX_DEVICES];

// Just error out on any normal IO anyone tries to do to this device,
// as it doesn't really mean anything anyway.
static void qrpc_request(struct request_queue *q) {
    struct request *req;
    while ((req = blk_fetch_request(q)) != NULL) {
        __blk_end_request_all(req, -EIO);
    }
}

// This isn't a proper block device, it's just necessary to use
// something to give it a name inside Linux.  As such I'm going to
// just pretend that it has only a single block.
int qrpc_getgeo(struct block_device *block_device, struct hd_geometry *geo) {
    geo->cylinders = 1;
    geo->heads = 1;
    geo->sectors = 1;
    geo->start = 1;
    return 0;
}

static struct block_device_operations qrpc_ops = {
    .owner  = THIS_MODULE,
    .getgeo = &qrpc_getgeo,
};

// This gets called once for every time a QRPC PCI device is detected.
int qrpc_init_one(struct pci_dev *pdev, const struct pci_device_id *ent)
{
    int err;
    struct qrpc_dev *dev;
    long ioaddr;
    int i;
    struct qrpc_dev **vdo;
    
    vdo = NULL;
    for (i = 0; i < MAX_DEVICES && vdo == NULL; i++)
        if (valid_devices[i] == NULL)
            vdo = &valid_devices[i];

    if (vdo == NULL)
        panic("Too many QEMU RPC devices!");

    if ((err = pci_enable_device(pdev)) != 0)
        return err;

    ioaddr = pci_resource_start(pdev, 0);
    request_region(ioaddr, 2048, DRV_NAME);

    if (ioaddr == 0)
        panic("QRPC wasn't able to figure out ioaddr");

    // Sends the init command
    outb(0, ioaddr);
    if (inb(ioaddr + 1) != 0)
        panic("QRPC didn't initialize correctly");

    dev = vmalloc(sizeof(*dev));
    if (dev == NULL)
        panic("QRPC wasn't able to allocate memory");

    dev->pdev = pdev;
    spin_lock_init(&dev->lock);

    if (register_blkdev(QRPC_MAJOR, DRV_NAME) != 0)
        panic("QRPC wasn't able to register major number");

    dev->disk = alloc_disk(1);
    if (dev->disk == NULL)
        panic("QRPC wasn't able to allocate disk");

    dev->disk->major = QRPC_MAJOR;
    dev->disk->first_minor = 0;
    dev->disk->fops = &qrpc_ops;
    dev->disk->private_data = dev;
    strcpy(dev->disk->disk_name, "qrpc0");
    set_capacity(dev->disk, 2);

    dev->disk->queue = blk_init_queue(&qrpc_request, &dev->lock);
    dev->disk->queue->queuedata = dev;
    blk_queue_logical_block_size(dev->disk->queue, 1024);

    add_disk(dev->disk);

    *vdo = dev;

    return 0;
}

void qrpc_remove_one(struct pci_dev *pdev)
{
}

// This is the module initialization code
static int __init qrpc_init(void)
{
    int i;

    for (i = 0; i < MAX_DEVICES; i++)
        valid_devices[i] = NULL;

    return pci_register_driver(&qrpc_pci_driver);
}

static void __exit qrpc_cleanup(void)
{
    pci_unregister_driver(&qrpc_pci_driver);
}

module_init(qrpc_init);
module_exit(qrpc_cleanup);

// Looks a block device in the table of QEMU devices.  This is
// necessary because we don't use the proper block device API because
// we want to avoid the block cache entirely.
static struct qrpc_dev *bdev_to_qdev(struct block_device *bdev)
{
    int i;

    if (bdev == NULL)
        return NULL;

    if (bdev->bd_disk == NULL)
        return NULL;

    for (i = 0; i < MAX_DEVICES; i++)
        if (valid_devices[i] != NULL)
            if (valid_devices[i]->disk == bdev->bd_disk)
                return valid_devices[i];

    return NULL;
}

// This is the special external interface to that QFS uses.  The
// prototypes of these functions can't change without making a
// cooresponding change in fs/qfs.c.
int qrpc_check_bdev(struct block_device *bdev)
{
    return bdev_to_qdev(bdev) != NULL;
}

void qrpc_transfer(struct block_device *bdev, u8 *data, int count)
{
    int i;
    struct qrpc_dev *qdev;
    long ioaddr;

    qdev = bdev_to_qdev(bdev);
    if (qdev == NULL)
        panic("Not a QRPC device!");

    ioaddr = pci_resource_start(qdev->pdev, 0);
    request_region(ioaddr, 2048, DRV_NAME);

    for (i = count-1; i >= 0; i--)
        outb(data[i], ioaddr + i);

    for (i = 0; i < count; i++)
        data[i] = inb(ioaddr + i);
}

#include "hw.h"
#include "pci/pci.h"
#include "qrpc.h"
#include "loader.h"
#include "dirent.h"

#define DEBUG_PORTS 0

static uint64_t qrpc_read(void *v, hwaddr a, unsigned w)
{
    QRPCState *s = v;

    // This might be useful for debugging... :)
#if DEBUG_PORTS
    fprintf(stderr, "qrpc_read(%p, %lX, %u) => %d\n", v, a, w,
            ((uint8_t *)(&s->frame))[a]);
#endif

    // Silently drop everything that's not a inb
    if (w != 1)
        return -1;

    // Throw away all addresses that are too large
    if (a > sizeof(QRPCFrame))
        return -1;

    return ((uint8_t *)(&s->frame))[a];
}

static void qrpc_write(void *v, hwaddr a, uint64_t d, unsigned w)
{
    QRPCState *s = v;

    // This might be useful for debugging... :)
#if DEBUG_PORTS
    fprintf(stderr, "qrpc_write(%p, %lX, %lx, %u)\n", v, a, d, w);
#endif

    // Silently drop everything that's not a outb
    if (w != 1)
        return;

    // Throw away all addresses that are too large
    if (a > sizeof(QRPCFrame))
        return;

    ((uint8_t *)(&s->frame))[a] = d;

    if (a != 0)
        return;

    switch (d)
    {
    case QRPC_CMD_INIT:
        fprintf(stderr, "QEMU RPC interface initialized by Linux\n");
        s->frame.ret = QRPC_RET_OK;
        break;
    case QRPC_CMD_MOUNT:
        fprintf(stderr, "QFS mounted by Linux %s\n", s->path);
        //return an ok
        s->frame.ret = QRPC_RET_OK;
        break;
    case QRPC_CMD_OPENDIR:
        {
        struct dirent *ent;
        fprintf(stderr, "Size of a dirent=%i bytes\n", sizeof(struct dirent) );
        DIR *dir;
        if ((dir = opendir(s->path)) != NULL){
            while ((ent = readdir(dir)) != NULL){
                fprintf(stderr, "%s\n", ent->d_name);
            }
            closedir(dir);
        }else {
            fprintf(stderr, "Couldn't open %s\n", s->path);
        }

        //return and ok
        s->frame.ret = QRPC_RET_OK;
        break;
    }
    default:
        // Silently drop all unknown commands
        break;
    }
}

// Defines an operation table for memory IO
static const MemoryRegionOps qrpc_ops = {
    .read = qrpc_read,
    .write = qrpc_write,
    .endianness = DEVICE_NATIVE_ENDIAN,
};

// FIXME: I'm not  actually sure what this does... -palmer April 23, 2013
static const VMStateDescription vmstate_qrpc = {
    .name = "qrpc",
    .version_id = 3,
    .minimum_version_id = 3,
    .minimum_version_id_old = 3,
    .fields      = (VMStateField []) {
        VMSTATE_PCI_DEVICE(dev, QRPCState),
        VMSTATE_END_OF_LIST()
    }
};

// Defines the properties that this device supports.  These properties
// are essentially arguments to the "-device" code that QEMU, and this
// structure defines exactly how they're set inside the device
// structure.
static Property qrpc_properties[] = {
    DEFINE_PROP_STRING("path", QRPCState, path),
    DEFINE_PROP_END_OF_LIST(),
};

// This is the constructor
static int qrpc_init(PCIDevice *pci_dev)
{
    int size;
    QRPCState *d = DO_UPCAST(QRPCState, dev, pci_dev);

    // FIXME: This can probably be done very cleaverly...
    size = 1;
    while (size < sizeof(QRPCFrame))
        size *= 2;

    fprintf(stderr, "qrpc_init()\n");
    memory_region_init_io(&d->io, &qrpc_ops, d, "qrpc", size);
    pci_register_bar(pci_dev, 0, PCI_BASE_ADDRESS_SPACE_IO, &d->io);

    return 0;
}

static void qrpc_exit(PCIDevice *pci_dev)
{
}

// Initializes this device class.  This gets run once when QEMU loads
// and defines an operation table for this device.  This is where
// pointers to the constructor and destructor get set, for example.
static void qrpc_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);
    PCIDeviceClass *k = PCI_DEVICE_CLASS(klass);

    k->init = qrpc_init;
    k->exit = &qrpc_exit;
    k->romfile = "pxe-ne2k_pci.rom";
    k->vendor_id = 0xCA1;
    k->device_id = 0xF194;
    k->class_id = PCI_CLASS_NETWORK_ETHERNET;
    
    dc->vmsd = &vmstate_qrpc;
    dc->props = qrpc_properties;
}

// FIXME: I'm not  actually sure what this does... -palmer April 23, 2013
static const TypeInfo qrpc_info = {
    .name          = "qrpc",
    .parent        = TYPE_PCI_DEVICE,
    .instance_size = sizeof(QRPCState),
    .class_init    = qrpc_class_init,
};

// Registers a new type.  This just gets called when QEMU is loaded,
// it's probably mapped to some sort of library constructor?
static void qrpc_register_types(void)
{
    type_register_static(&qrpc_info);
}
type_init(qrpc_register_types)

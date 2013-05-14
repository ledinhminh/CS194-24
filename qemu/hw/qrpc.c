#include "hw.h"
#include "pci/pci.h"
#include "qrpc.h"
#include "loader.h"
#include "dirent.h"

#define DEBUG_PORTS 0


//takes care of maintenance stuff
static void add_frame_to_buf(QRPCState *s, QRPCFrame *frame){
    memcpy(s->buffer + s->buf_size, frame, sizeof(QRPCFrame));
    s->buf_size += 1;
    return;
}

static uint64_t qrpc_read(void *v, hwaddr a, unsigned w)
{
    uint8_t ret;
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

    // ret = ((uint8_t *)(&s->frame))[a];
    QRPCFrame *frame = &(s->buffer[s->buf_read]);
    ret = ((uint8_t *) frame)[a];
    //check to see if we're at the end of the frame
    if (a == (sizeof(QRPCFrame) - 1)){
        s->buf_read += 1;
    }

    return ret;
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

    //ONLY COMMANDS GET TO THIS POINT
    if (d != QRPC_CMD_CONTINUE){
        //initialize the list
        //there shouldn't be any frames that haven't been read out anyway
        fprintf(stderr, "clearing out buffer...\n");
        s->buf_size = 0;
        s->buf_read = 0;
        memset(s->buffer, 0, sizeof(QRPCFrame) * QRPC_BUFFER_LEN);
    }

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
        struct stat st;
        DIR *dir;
        int ret;

        int path_size = strlen(s->path) + strlen(s->frame.data) + 1;
        char *path[MAX_PATH_LEN];
        sprintf(path, "%s/%s", s->path, s->frame.data);
        fprintf(stderr, "openddir path: %s\n", path);
        if ((dir = opendir(path)) != NULL){
            while ((ent = readdir(dir)) != NULL){

                //lets not pass this stuff
                if(strcmp(ent->d_name, ".") == 0)
                    continue;

                if(strcmp(ent->d_name, "..") == 0)
                    continue;

                int size = strlen(path) + strlen(ent->d_name);
                char full_path[size+1];
                struct qrpc_file_info finfo;

                sprintf(full_path, "%s/%s", path, ent->d_name);
                fprintf(stderr, "...stat path: %s\n", full_path);
                ret = stat(full_path, &st);

                //if we can't stat the file, we probably shouldn't keep going
                if (ret == -1)
                    continue;


                finfo.name_len = sprintf(&finfo.name, "%s", (char *) ent->d_name);
                finfo.type = ent->d_type;
                finfo.mode = st.st_mode;
                finfo.size = st.st_size;
                finfo.atime = st.st_atime;
                finfo.mtime = st.st_mtime;
                finfo.ctime = st.st_ctime;

                fprintf(stderr, "sizeof finfo: %i\n", sizeof(struct qrpc_file_info));
                // fprintf(stderr, "name: %s\n", finfo.name);
                memcpy(&(s->frame.data), &finfo, sizeof(struct qrpc_file_info));

                QRPCFrame frame;
                memset(&frame, 0, sizeof(QRPCFrame));
                memcpy(&(frame.data), &finfo, sizeof(struct qrpc_file_info));
                frame.ret = QRPC_RET_CONTINUE;

                add_frame_to_buf(s, &frame);
                fprintf(stderr, "..Added %s to buffer\n", finfo.name);
            }
            closedir(dir);
        }else {
            fprintf(stderr, "Couldn't open %s\n", s->path);
        }

        //return an ok
        s->frame.ret = QRPC_RET_OK;

        break;
        }
    case QRPC_CMD_CREATE:
        // Handle for both create and mkdir via mknod
        {
        unsigned short mode;
        int path_size;
        int fd = -1;
        char path[MAX_PATH_LEN];
        QRPCFrame frame;

        memcpy(&mode, s->frame.data, sizeof(short));

        // We need the full path.
        path_size = strlen(s->path) + strlen(s->frame.data + sizeof(short)) + 1;
        sprintf(path, "%s/%s", s->path, s->frame.data + sizeof(short));
        // We should not have to do this!
        if ((mode & S_IFREG) == S_IFREG)
            fd = creat(path, mode);
        else if ((mode & S_IFDIR) == S_IFDIR)
            fd = mkdir(path, mode); // One doesn't open a directory.
        else
            printf("mknod: invalid mode bits\n");

        printf("path? mode=%u, %s, fd = %d\n", mode, path, fd);

        if (fd == -1) {
            printf("errno=%u (%s)\n", errno, strerror(errno));
            fd = -errno;
        }

        memcpy(frame.data, &fd, sizeof(int));
        add_frame_to_buf(s, &frame);
        break;
        }
    case QRPC_CMD_REVALIDATE:
        // Just check if it exists.
        {
        int path_size;
        short ret;
        char path[MAX_PATH_LEN];
        QRPCFrame frame;

        sprintf(path, "%s/%s", s->path, s->frame.data);

        ret = access(path, F_OK) != -1 ? 1 : 0;

        printf("revalidate: %s is %d (0 = bad, 1 = ok)\n", path, ret);

        // free(path);

        memcpy(frame.data, &ret, sizeof(short));
        add_frame_to_buf(s, &frame);
        break;
        }
    case QRPC_CMD_RENAME:
        {
        int ret;
        // int old_path_len, new_path_len;
        char* old_path;
        char* old_path_abs;
        char* new_path;
        char* new_path_abs;
        QRPCFrame frame;

        old_path = strdup(s->frame.data);
        old_path_abs = malloc((strlen(s->path) + strlen(old_path) + 1) * sizeof(char));
        sprintf(old_path_abs, "%s/%s", s->path, old_path);

        new_path = strdup(s->frame.data + strlen(old_path) + 1);
        new_path_abs = malloc((strlen(s->path) + strlen(new_path) + 1) * sizeof(char));
        sprintf(new_path_abs, "%s/%s", s->path, new_path);

        ret = rename(old_path_abs, new_path_abs);
        memcpy(frame.data, &ret, sizeof(int));

        printf("old path=%s, new path=%s, ret=%u\n", old_path_abs, new_path_abs, ret);

        // We should probably do some error checking.
        if (ret == -1) {
            printf("errno=%u (%s)\n", errno, strerror(errno));
            ret = -errno;
        }

        free(old_path);
        free(old_path_abs);
        free(new_path);
        free(new_path_abs);

        add_frame_to_buf(s, &frame);
        break;
        }
    case QRPC_CMD_UNLINK:
        {
        int ret;
        char* path;
        QRPCFrame frame;

        path = malloc(strlen(s->path) + strlen(s->frame.data) + 1 * sizeof(char));
        sprintf(path, "%s/%s", s->path, s->frame.data);

        printf("unlink: %s\n", path);
        ret = unlink(path);

        if (ret == -1) {
            printf("errno=%u (%s)\n", errno, strerror(errno));
            ret = -errno;
        }

        memcpy(frame.data, &ret, sizeof(int));
        add_frame_to_buf(s, &frame);
        break;
        }
    case QRPC_CMD_RMDIR:
    {
        //delete a directory
        printf("Time to unlink");
        QRPCFrame frame;
        char path[MAX_PATH_LEN];
        int ret;

        memset(path, 0, sizeof(char)*MAX_PATH_LEN);
        sprintf(path, "%s/%s", s->path, s->frame.data);
        printf("...unlinke path %s\n", path);
        ret = rmdir(path);

        if (ret < 0){
            printf("errno = %u (%s)\n", errno, strerror(errno));
            ret = errno;
        }

        memcpy(frame.data, &ret, sizeof(int));
        add_frame_to_buf(s, &frame);
        printf("deleted %s with return %i\n", path, ret);
        break;

    }
    case QRPC_CMD_OPEN_FILE:
    {
      const char* path = strdup(s->frame.data);
      const char* path_abs = malloc((strlen(s->path) + strlen(path) + 1) * sizeof(char));
      int fd = -1;
      QRPCFrame frame;

      sprintf(path_abs, "%s/%s", s->path, path);
      fd = open(path_abs, O_RDWR);

      fprintf(stderr, "Opening file %s: fd = %ld\n", path_abs, fd);
      memcpy(frame.data, &fd, sizeof(int));

      free(path);
      free(path_abs);

      add_frame_to_buf(s, &frame);
      break;
    }
    case QRPC_CMD_READ_FILE:
    {
    struct {
        int fd;
        size_t count;
        loff_t offset;
    } data;
    unsigned int total_read = 0, read = 0;
    FILE *fp;
    
    memcpy(&data, s->frame.data, sizeof(data));
    fprintf(stderr, "Reading from fd: %d, %d bytes\n", data.fd, data.count);
    fp = fdopen(data.fd, "r");
    do {
        QRPCFrame frame;
        struct qrpc_inflight inflight;
        int data_size = QRPC_DATA_SIZE - sizeof(uint32_t) - sizeof(uint16_t) - sizeof(uint64_t);
        int this_count = data.count - total_read > data_size ? data_size : data.count - total_read;

        read = fread(inflight.data, 1, this_count, fp);
        inflight.len = read;
        inflight.backing_fd = data.fd;
        total_read += read;

        // total_read += read;
        memcpy(&frame.data, &inflight, sizeof(struct qrpc_inflight));
        frame.ret = QRPC_RET_CONTINUE;

        add_frame_to_buf(s, &frame);
        fprintf(stderr, "Read %d bytes from fd %d\n", read, data.fd);
    } while(total_read < data.count && read == QRPC_DATA_SIZE - sizeof(uint32_t) - sizeof(uint16_t) - sizeof(uint64_t));
    
    free(fp);
    QRPCFrame frame;
    memcpy(frame.data, &total_read, sizeof(unsigned int));
    add_frame_to_buf(s, &frame);
    
    break;
    }
    case QRPC_CMD_RELEASE_FILE:
    {
      int fd;
      QRPCFrame frame;
      FILE* fp;

      memcpy(&fd, s->frame.data, sizeof(int));
      fp = fdopen(fd, "r");
      fprintf(stderr, "Closing fd %d\n", fd);
      frame.ret = fclose(fp);
      //free(fp); // fclose frees fp
      add_frame_to_buf(s, &frame);
    }
    case QRPC_CMD_WRITE_START:
    {
        add_frame_to_buf(s, &s->frame);
        break;
    }
    case QRPC_CMD_WRITE_END:
    {   
        QRPCFrame frame;
        struct qrpc_inflight inflight;
        unsigned int i = 0;
        int written = 0;
        FILE *fp;
        fp = NULL;
        add_frame_to_buf(s, &s->frame);
        
        for (i = 0; i < s->buf_size; i++) {
            frame = s->buffer[i];
            memcpy(&inflight, &frame.data, sizeof(struct qrpc_inflight));
            printf("write: this frame: backing_fd=%d, len=%d, offset=%lu\n", inflight.backing_fd, inflight.len, inflight.offset);
            
            printf("%s\n", inflight.data); 
            // We don't know the fd until now.
            if (fp == NULL) {
                fprintf(stderr, "fp is null, creating one\n");
                fp = fdopen(inflight.backing_fd, "w");
                fprintf(stderr, "fp has been created\n");
                if (fseek(fp, inflight.offset, SEEK_SET) != 0) { // fseek failed
                    written = -errno;
                    fprintf(stderr, "fseek failed %s\n", strerror(errno));
                    goto done;
                }
            }
            
            fprintf(stderr, "fp is not null, we do fwrite\n");
            written += fwrite(inflight.data, 1, inflight.len, fp);
            fprintf(stderr, "loop  written %i\n", written);
            if (written != inflight.len){
                fprintf(stderr, "error: %s\n", strerror(errno));
            }
        }
        
        done:
        // Clear the buffers
        fflush(fp);
        i = ftruncate(inflight.backing_fd, written);
        if (i != 0){
            fprintf(stderr, "truncate error: %s\n", strerror(errno));
        }
        s->buf_size = 0;
        s->buf_read = 0;
        memset(s->buffer, 0, sizeof(QRPCFrame) * QRPC_BUFFER_LEN);
        
        struct stat st;
        struct qrpc_file_info finfo;
        fstat(inflight.backing_fd, &st);
        finfo.size = st.st_size;
        finfo.name_len = written;

        printf("write: new size is %d\n", st.st_size);
        memcpy(frame.data, &finfo, sizeof(struct qrpc_file_info));

        add_frame_to_buf(s, &frame);
        break;
    }
    default:
        // Silently drop all unknown commands
        break;
    }
    //set the last frame buffer to OK
    s->buffer[s->buf_size - 1].ret = QRPC_RET_OK;
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

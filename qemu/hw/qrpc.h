#ifndef HW_QRPC_H
#define HW_QRPC_H 1

#define QRPC_CMD_INIT   0
#define QRPC_CMD_MOUNT  1
#define QRPC_CMD_UMOUNT 2
#define QRPC_CMD_OPENDIR 3
#define QRPC_CMD_CREATE 4
#define QRPC_CMD_CONTINUE 9
#define QRPC_CMD_REVALIDATE 15
#define QRPC_CMD_RENAME 20
#define QRPC_CMD_UNLINK 25
#define QRPC_CMD_STAT 30
#define QRPC_CMD_RMDIR 23

#define QRPC_CMD_OPEN_FILE 100
#define QRPC_CMD_READ_FILE 101
#define QRPC_CMD_RELEASE_FILE 102

#define QRPC_RET_OK  0
#define QRPC_RET_ERR 1
#define QRPC_RET_CONTINUE 2
#define MAX_PATH_LEN 1024

#define QRPC_DATA_SIZE 1024
#define QRPC_BUFFER_LEN 256


struct qrpc_file_info {
    char name[256];
    int name_len;
    char type;
    mode_t mode;
} __attribute__((packed));

struct qrpc_inflight {
    uint32_t backing_fd;
    uint16_t len;
    uint8_t data[QRPC_DATA_SIZE - sizeof(uint32_t) - sizeof(uint16_t)];
} __attribute__((packed));

// Holds a single frame of IO
typedef struct QRPCFrame {
    // These are special memory-mapped registers.  First, the entire
    // data structure must be setup with the proper arguments for a
    // command.  Then, the CMD register should be written with the
    // cooresponding command.  QEMU will then perform the proper
    // filesystem operation, blocking reads from the RET register
    // until the operation completes.  When the RET register is read,
    // the rest of the data will contain the response.
    uint8_t cmd;
    uint8_t ret;

    uint8_t data[QRPC_DATA_SIZE];
} __attribute__((packed)) QRPCFrame;

// This represents a single QRPC device running inside of QEMU.
typedef struct QRPCState {
    // Holds the PCI device that QEMU uses to link this into the PCI bus
    PCIDevice dev;

    // Represents the path that is the root of this filesystem.
    char *path;

    // Represents an IO region, which allows this device to indicate
    // to QEMU that it wants a region of memory to be mapped.  The
    // QRPC interface
    MemoryRegion io;

    // Stores the current RPC data frame.
    QRPCFrame frame;

    QRPCFrame buffer[QRPC_BUFFER_LEN];
    int buf_size;
    int buf_read;
} QRPCState;

#endif

/*
 *  qfs.c: QEMU RPC filesystem
 *
 *  Copyright 2013 Palmer Dabbelt <palmer@dabbelt.com>
 */

#include <linux/module.h>
#include <linux/version.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/kernel.h>
#include <linux/statfs.h>

// FIXME: These should really live in a header somewhere (note: I'm
// extremely evil here, one of these parameters _doesn't_ match, but
// that's done purposefully)
extern int qrpc_check_bdev(struct block_device *bdev);
extern void qrpc_transfer(struct block_device *bdev, void *data, int count);

// FIXME: This is all included from QEMU, make sure it doesn't get out
// of sync!
#define QRPC_CMD_INIT   0
#define QRPC_CMD_MOUNT  1
#define QRPC_CMD_UMOUNT 2

#define QRPC_RET_OK  0
#define QRPC_RET_ERR 1

#define QRPC_DATA_SIZE 1024

struct qrpc_frame {
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
} __attribute__((packed));

// Here starts the actual implementation of QFS.

static int qfs_readdir(struct file *file, void * dirent, filldir_t filldir) {
    printk("QFS READDIR\n");
    return 0;

}

// INODE OPERATIONS
static int qfs_create(struct inode *inode, struct dentry *dentry, umode_t mode, bool boolean){
    printk("QFS INODE CREATE\n");
    return 0;
}

static struct dentry* qfs_lookup(struct inode *inode, struct dentry *dentry, unsigned int flags){
    printk("QFS INODE LOOKUP\n");
    return NULL;
}

static int qfs_link(struct dentry *old_dentry, struct inode *indoe, struct dentry *dentry){
    printk("QFS INODE LINK\n");
    return 0;
}

static int qfs_unlink(struct inode *dir, struct dentry *dentry){
    printk("QFS INDOE UNLINK\n");
    return 0;
}

static int qfs_symlink(struct inode *dir, struct dentry *dentry, const char *symname){
    printk("QFS INODE SYMLINK\n");
    return 0;
}

static int qfs_mkdir(struct inode *dir, struct dentry *dentry, int mode){
    printk("QFS INODE MKDIR\n");
    return 0;
}

static int qfs_rmdir(struct inode *dir, struct dentry *dentry){
    printk("QFS INODE RMDIR\n");
    return 0;
}

static int qfs_rename(struct inode *old_dir, struct dentry *old_dentry, struct inode *new_dir, struct dentry *new_dentry){
    printk("QFS INODE RENAME\n");
    return 0;
}

static int qfs_permission(struct inode *inode, int mask){
    printk("QFS INDOE PERMISSION\n");
    return 0;
}

static int qfs_getattr(struct vfsmount *mnt, struct dentry *dentry, struct kstat *stat){
    //invoked by VFS when it notices an inode needs to be refreshed from disk
    printk("QFS INODE GETATTR\n");
    return 0;
}

static int qfs_setattr(struct dentry *dentry, struct iattr *attr){
    //caled from notify_change() to notify a 'change event' after an inode has been modified
    printk("QFS INODE SETATTR\n");
    return 0;
}

// FIXME: You'll want a whole bunch of these, every field needs to be
// filled in (though some can use generic functionality).
static int qfs_statfs(struct dentry *dentry, struct kstatfs *buf)
{
    buf->f_type    = dentry->d_sb->s_magic;
    buf->f_bsize   = 2048;
    buf->f_namelen = 255;
    buf->f_blocks  = 1;
    buf->f_bavail  = 0;
    buf->f_bfree   = 0;

    return 0;
}

static const struct super_operations qfs_super_ops = {
    .statfs        = &qfs_statfs,
    .alloc_inode   = NULL,
    .drop_inode	   = NULL,
    .destroy_inode = NULL,
    .evict_inode   = NULL,
    .show_options  = NULL,
};

const struct dentry_operations qfs_dentry_operations = {
    .d_revalidate = NULL,
    .d_delete	  = NULL,
    .d_release	  = NULL,
    .d_automount  = NULL,
};


const struct file_operations qfs_dir_operations = {
    .open    = NULL,
    .release = NULL,
    .readdir = qfs_readdir,
    .lock    = NULL,
    .llseek  = NULL,
};

const struct file_operations qfs_file_operations = {
    .open        = NULL,
    .release     = NULL,
    .llseek      = NULL,
    .read        = NULL,
    .write       = NULL,
    .aio_read    = generic_file_aio_read,
    .aio_write   = generic_file_aio_write,
    .mmap        = generic_file_readonly_mmap,
    .splice_read = generic_file_splice_read,
    .fsync       = NULL,
    .lock        = NULL,
    .flock       = NULL,
};

const struct inode_operations qfs_inode_operations = {
    .create     = qfs_create,
    .lookup     = qfs_lookup,
    .link       = qfs_link,
    .unlink     = qfs_unlink,
    .symlink    = qfs_symlink,
    .mkdir      = qfs_mkdir,
    .rmdir      = qfs_rmdir,
    .rename     = qfs_rename,
    .permission = qfs_permission,
    .getattr    = qfs_getattr,
    .setattr    = qfs_setattr,
};

// An extra structure we can tag into the superblock, currently not
// used.
struct qfs_super_info
{
};

// I have absolutely no idea what this one does...
static int qfs_test_super(struct super_block *s, void *data)
{
    return 1;
}

static int qfs_set_super(struct super_block *s, void *data)
{
	s->s_fs_info = data;
	return set_anon_super(s, NULL);
}

// This is a combination of fs/super.c:mount_bdev and
// fs/afs/super.c:afs_mount.  While it might be cleaner to call them,
// I just hacked them apart... :)
struct dentry *qfs_mount(struct file_system_type *fs_type,
                         int flags, const char *dev_name, void *data)
{
    struct block_device *bdev;
    fmode_t mode;
    struct super_block *sb;
    struct qfs_super_info *qs;
    struct inode *inode;
    struct qrpc_frame f;

    mode = FMODE_READ | FMODE_EXCL;
    if (!(flags & MS_RDONLY))
        mode |= FMODE_WRITE;

    bdev = blkdev_get_by_path(dev_name, mode, fs_type);
    if (IS_ERR(bdev))
        panic("IS_ERR(bdev)");

    // FIXME: This probably shouldn't panic...
    if (!qrpc_check_bdev(bdev))
        panic("Not QRPC block device");

    qs = kmalloc(sizeof(struct qfs_super_info), GFP_KERNEL);
    if (qs == NULL)
        panic("Unable to allocate qs");

    sb = sget(fs_type, qfs_test_super, qfs_set_super, flags, qs);
    if (IS_ERR(sb))
        panic("IS_ERR(sb)");

    if (sb->s_root != NULL)
        panic("reuse");

    sb->s_blocksize = 2048;
    sb->s_blocksize_bits = 11;
    sb->s_magic = ((int *)"QRFS")[0];
    sb->s_op = &qfs_super_ops;
    sb->s_d_op = &qfs_dentry_operations;

    inode = iget_locked(sb, 1);
    inode->i_mode = S_IFDIR;
    inode->i_op   = &qfs_inode_operations;
    inode->i_fop  = &qfs_dir_operations;
    unlock_new_inode(inode);

    sb->s_root = d_make_root(inode);

    f.cmd = QRPC_CMD_MOUNT;
    f.ret = QRPC_RET_ERR;
    qrpc_transfer(bdev, &f, sizeof(f));
    if (f.ret != QRPC_RET_OK)
        panic("QRPC mount failed!");

    return dget(sb->s_root);
}

// Informs the kernel of how to mount QFS.
static struct file_system_type qfs_type =
{
    .owner    = THIS_MODULE,
    .name     = "qfs",
    .mount    = &qfs_mount,
    .kill_sb  = &kill_anon_super,
    .fs_flags = 0,
};

int __init qfs_init(void)
{
    register_filesystem(&qfs_type);
    return 0;
}

void __exit qfs_exit(void)
{
}

module_init(qfs_init);
module_exit(qfs_exit);

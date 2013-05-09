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
#define QRPC_CMD_OPENDIR 3
#define QRPC_CMD_CREATE 4
#define QRPC_CMD_CONTINUE 9

#define QRPC_RET_OK  0
#define QRPC_RET_ERR 1
#define QRPC_RET_CONTINUE 2

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

// Guesswork here taken from http://en.wikipedia.org/wiki/Read-copy-update.

static struct kmem_cache *qfs_inode_cachep;

static const struct super_operations qfs_super_ops;
const struct dentry_operations qfs_dentry_operations;
const struct file_operations qfs_dir_operations;
const struct file_operations qfs_file_operations;
const struct inode_operations qfs_inode_operations;

struct qfs_inode {
    struct inode inode;
    struct list_head list;
    unsigned long backing_fd;
    spinlock_t lock;
};

struct qrpc_file_info {
    char name[256];
    int name_len;
    char type;
    mode_t mode;
} __attribute__((packed));

LIST_HEAD(list);

spinlock_t list_mutex;
struct qfs_inode head;


//because getting the block device SUCKS~!!!
static void qtransfer(struct inode *inode, uint8_t cmd, struct qrpc_frame *f){
	struct block_device *bdev;

	//from the inode, get the superblock, then the block dev
	bdev = inode->i_sb->s_bdev;
 	f->cmd = cmd;
	f->ret = QRPC_RET_ERR;
	qrpc_transfer(bdev, f, sizeof(struct qrpc_frame));
}

// SUPER OPERATIONS
static inline struct qfs_inode* qnode_of_inode(struct inode* inode) {
    return container_of(inode, struct qfs_inode, inode);
}


//for the kmemcache which is a slab allocator. So it'll make a bunch of
//these at the start
static void qfs_inode_init(void* _inode) {
    struct qfs_inode* inode;
    printk(KERN_INFO "qfs_inode_init: initing an qfs_inode at %p...\n", _inode);

    // Only generic initialization stuff. Anything per-inode, we do when we really need to press the inode into service.
    inode = _inode;
    memset(inode, 0, sizeof(*inode));
    
    // What does this mean?!
    inode_init_once(&inode->inode);
    
    spin_lock_init(&inode->lock);
}

static struct inode* qfs_alloc_inode(struct super_block *sb) {
    struct qfs_inode *qnode;

    printk("QFS SUPER ALLOC_INODE START\n");
    qnode = kmem_cache_alloc(qfs_inode_cachep, GFP_KERNEL);
    if (!qnode) {
        printk(KERN_INFO "qfs_alloc_inode: alloc for *inode failed\n");
        return NULL;
    }
    
    // I don't know the difference between inode_init_once and this.
    // And why are there always so many allocations?
    inode_init_always(sb, &qnode->inode);

    // Add qfs_inode to list
    printk(KERN_INFO "qfs_alloc_inode: adding inode to list\n");
    list_add(&qnode->list, &list);

    return &qnode->inode;
}

// drop_inode doesn't seem to be used???
// static void qfs_drop_inode(struct inode *inode){
//     printk("QFS SUPER DROP INODE\n");
//     return;
// }

// DENTRY OPERATIONS
static int qfs_revalidate(struct dentry *dentry, unsigned int flags){
    printk("QFS DENTRY REVALIDATE\n");
    return 0;
}

static int qfs_delete(const struct dentry *dentry){
    printk("QFS DENTRY DELETE\n---dentry: 0x%p\n", dentry);
    return 0;
}

static void qfs_release(struct dentry *dentry){
    printk("QFS DENTRY RELEASE\n---dentry: 0x%p\n", dentry);
    return;
}

static void qfs_destroy_inode(struct inode *inode){
	printk("QFS SUPER DESTROY INODE\n");
	return;
}

static void qfs_evict_inode(struct inode *inode){
	printk("QFS SUPER EVICT INODE\n");
	return;
}

static int qfs_show_options(struct seq_file *file, struct dentry *dentry){
	printk("QFS SUPER SHOW OPTIONS\n");
	return 0;
}

static struct vfsmount* qfs_automount(struct path *path){
	printk("QFS DENTRY AUTOMOUNT\n");
	return NULL;
}

// FILE DIR OPERATIONS
static int qfs_dir_open(struct inode *inode, struct file *file){
	//creates a new file object and links it to the corresponding inode object.
	printk("QFS DIR OPEN\n");
	return 0;
}

static int qfs_dir_release(struct inode *inode, struct file *file){
	//called when last remaining reference to file is destroyed
	printk("QFS DIR RELEASE\n");
	return 0;
}

static int qfs_dir_readdir(struct file *file, void *dirent , filldir_t filldir){
	//returns next directory in a directory listing. function called by readdir() system call.
	printk("QFS DIR READDIR\n");
	return 0;
}

static int qfs_dir_lock(struct file *file, int cmd, struct file_lock *lock){
	//manipulate a file lock on the given file
	printk("QFS DIR LOCK\n");
	return 0;
}

static loff_t qfs_dir_llseek(struct file *file, loff_t offset, int origin){
	//updates file pointer to the given offset. called via llseek() system call.
	printk("QFS DIR LLSEEK\n");
	return 0;
}

//FILE FILE OPERATIONS
static int qfs_file_open(struct inode *inode, struct file *file){
	//creates a new file object and links it to the corresponding indoe object.
	printk("QFS FILE OPEN\n");
	return 0;
}

static int qfs_file_release(struct inode *inode, struct file *file){
	//called when last remaining reference to file is destroyed
	printk("QFS FILE RELEASE\n");
	return 0;
}

static loff_t qfs_file_llseek(struct file *file, loff_t offset, int origin){
	//updates file pointer to the given offset. called via llseek() system call.
	printk("QFS FILE LLSEEK\n");
	return 0;
}

static ssize_t qfs_file_read(struct file *file, char __user *buf, size_t count, loff_t *offset){
	//reads count bytes from the given file at position offset into buf.
	//file pointer is then updated
	printk("QFS FILE READ\n");
	return 0;
}

static ssize_t qfs_file_write(struct file *file, const char __user *buf, size_t count, loff_t *offset){
	//writes count bytes from buf into the given file at position offset.
	//file pointer is then updated
	printk("QFS FILE WRITE\n");
	return 0;
}

static int qfs_file_fsync(struct file *file, loff_t start, loff_t end, int datasync){
	//write all cached data for file to disk
	printk("QFS FILE FSYNC\n");
	return 0;
}

static int qfs_file_lock(struct file *file, int cmd, struct file_lock *lock){
	//manipulates a file lock on the given file
	printk("QFS FILE LOCK\n");
	return 0;
}

static int qfs_file_flock(struct file *file, int cmd, struct file_lock *lock){
	//advisory locking
	printk("QFS FILE FLOCK\n");
	return 0;
}

// INODE OPERATIONS
static int qfs_create(struct inode *dir, struct dentry *dentry, umode_t mode, bool excl) {
    struct qrpc_frame frame;
    int backing_fd = 0;
    struct inode* inode;
    struct qfs_inode* qnode;
    
    printk(KERN_INFO "qfs_create: enter\n");
    
    // Make a new inode for this file.
    inode = qfs_alloc_inode(dir->i_sb);
    qnode = qnode_of_inode(inode);
    
    // Some inode bookkeeping. From ramfs_get_inode
    // Could do with some refactoring, we might (?) need to do this elsewhere.
    inode->i_ino = get_next_ino();
    inode_init_owner(inode, dir, mode);
    
    inode->i_atime = inode->i_mtime = inode->i_ctime = CURRENT_TIME;
    switch (mode & S_IFMT) {
    // default:
        // init_special_inode(inode, mode, dev);
        // break;
    case S_IFREG:
        inode->i_op = &qfs_inode_operations;
        inode->i_fop = &qfs_file_operations;
        break;
    case S_IFDIR:
        inode->i_op = &qfs_inode_operations;
        inode->i_fop = &qfs_file_operations;

        /* directory inodes start off with i_nlink == 2 (for "." entry) */
        inc_nlink(inode);
        break;
    case S_IFLNK:
        // inode->i_op = &page_symlink_inode_operations;
        break;
    }
    
    
    // TODO: Case and assign file_operations.
    
    memcpy(frame.data, &mode, sizeof(short));
    strcpy(frame.data + sizeof(short), dentry->d_name.name);
    
    // Here we go.
    qtransfer(dir, QRPC_CMD_CREATE, &frame);
    
    memcpy(&backing_fd, frame.data, sizeof(int));
    // TODO: Do some error checking here
    
    // Add the fd to the qnode's info
    printk(KERN_INFO "qfs_create: backing fd=%d\n", backing_fd);
    qnode->backing_fd = backing_fd;
    
    // I think we have to do this. From ramfs_mknod
    d_instantiate(dentry, inode);
    dget(dentry);
    
    printk(KERN_INFO "qfs_create: exit\n");
    return 0;
}

static struct dentry* qfs_lookup(struct inode *dir, struct dentry *dentry, unsigned int flags) {
    // This function searches a directory for an inode corresponding to a filename specified in the given dentry.

    struct qfs_inode* qdir;
    printk(KERN_INFO "qfs_lookup: enter\n");
    printk("QFS INODE LOOKUP\n---dir: 0x%p\n---dentry: 0x%p\n------d_parent: 0x%p\n------d_name: %s\n---flags: %d\n",
           dir, dentry, dentry->d_parent, dentry->d_name.name, flags);

    // We're supposed to fill out the dentry. Why is it associated already?
    BUG_ON(dentry->d_inode != NULL);

    qdir = qnode_of_inode(dir);

    printk(KERN_INFO "qfs_lookup: desired path is %s\n", dentry->d_name.name);

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

static int qfs_mkdir(struct inode *dir, struct dentry *dentry, umode_t mode){
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

// static int qfs_permission(struct inode *inode, int mask){
//     printk("QFS INODE PERMISSION\n---inode: 0x%p\n---mask: %d\n", inode, mask);
//     printk("-umode_t: %i\n", inode->i_mode);
//     return 0;
// }

static int qfs_getattr(struct vfsmount *mnt, struct dentry *dentry, struct kstat *stat) {
    //invoked by VFS when it notices an inode needs to be refreshed from disk
    // struct qrpc_frame frame;
    // struct qrpc_file_info finfo;
    // int done;

    // printk("QFS INODE GETATTR\n---vfsmount: 0x%p\n---dentry: 0x%p\n---stat: 0x%p\n",
    //        mnt, dentry, stat);
    // printk("-inode name: %s\n", dentry->d_name.name);
    // printk("-inode addr: 0x%p\n", dentry->d_inode);

    //as copied from afs...
    struct inode *inode;
    inode = dentry->d_inode;
    generic_fillattr(inode, stat);

    // I now think we're not supposed to do this....
    // qtransfer(dentry->d_inode, QRPC_CMD_OPENDIR, &frame);
    // done = 0;
    // while (!done){
    //     memcpy(&finfo, &(frame.data), sizeof(struct qrpc_file_info));
        
    //     //create a dentry?
    //     printk("%s len=%i\n", finfo.name, strlen(finfo.name));
    //     struct qstr *name = kmalloc(sizeof(struct qstr), GFP_KERNEL);
    //     char *fname = kmalloc(sizeof(char) * strlen(finfo.name), GFP_KERNEL);
    //     memset(fname, 0, sizeof(char)*strlen(finfo.name));
    //     memcpy(fname, &finfo.name, sizeof(char)*strlen(finfo.name));
    //     name->name = fname;
    //     printk("fname: %s\n", name->name);
    //     struct dentry *child = d_alloc(dentry, name);
    //     if (child == NULL){
    //         printk("BAD HAPPENED\n");
    //         break;
    //     }


    //     // printk("FINFO: name: %s \t ret: %i\n", finfo.name, frame.ret);
    //     if (frame.ret == QRPC_RET_OK){
    //         done = 1;
    //     } else if (frame.ret == QRPC_RET_CONTINUE) {
    //         qtransfer(dentry->d_inode, QRPC_CMD_CONTINUE, &frame);
    //     } else {
    //         break;
    //     }
    // }
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
    .statfs        = qfs_statfs,
    .alloc_inode   = qfs_alloc_inode,
    // .drop_inode	   = NULL,
	.destroy_inode = qfs_destroy_inode,
	.evict_inode   = qfs_evict_inode,
	.show_options  = qfs_show_options, //maybe generic_show_options is good enough?
};

const struct dentry_operations qfs_dentry_operations = {
	.d_revalidate = qfs_revalidate,
	.d_delete	  = qfs_delete,
	.d_release	  = qfs_release,
	.d_automount  = qfs_automount,
};

const struct file_operations qfs_dir_operations = {
	.open    = qfs_dir_open,
	.release = qfs_dir_release,
	.readdir = qfs_dir_readdir,
	.lock    = qfs_dir_lock,
	.llseek  = qfs_dir_llseek,
};

const struct file_operations qfs_file_operations = {
	.open        = qfs_file_open,
	.release     = qfs_file_release,
	.llseek      = qfs_file_llseek,
	.read        = qfs_file_read,
	.write       = qfs_file_write,
	.aio_read    = generic_file_aio_read,
	.aio_write   = generic_file_aio_write,
	.mmap        = generic_file_readonly_mmap,
	.splice_read = generic_file_splice_read,
	.fsync       = qfs_file_fsync,
	.lock        = qfs_file_lock,
	.flock       = qfs_file_flock,
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
	// .permission = NULL, //we could use our qfs_permission... but theres a generic
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
    sb->s_bdev = bdev;

	inode = iget_locked(sb, 1);
	inode->i_mode = S_IFDIR;
	inode->i_op   = &qfs_inode_operations;
	inode->i_fop  = &qfs_file_operations;
	unlock_new_inode(inode);

	//make a root dentry out of the inode
	sb->s_root = d_make_root(inode);

	f.cmd = QRPC_CMD_MOUNT;
	f.ret = QRPC_RET_ERR;
	qrpc_transfer(bdev, &f, sizeof(f));
	if (f.ret != QRPC_RET_OK)
		panic("QRPC mount failed!");

	//fill out inodes?
	printk(KERN_INFO "ROOT INODE ADDR: 0x%p\n", inode);
	printk(KERN_INFO "ROOT DENTRY ADDR: 0x%p\n", sb->s_root);

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
    // Allocate our kmem cache.
    
    qfs_inode_cachep = kmem_cache_create("qfs_inode_cache", 
        sizeof(struct qfs_inode), 0, SLAB_HWCACHE_ALIGN, qfs_inode_init);

    register_filesystem(&qfs_type);
    return 0;
}

void __exit qfs_exit(void)
{
}

module_init(qfs_init);
module_exit(qfs_exit);

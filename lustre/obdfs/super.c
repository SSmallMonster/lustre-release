/*
 * OBDFS Super operations
 *
 * This code is issued under the GNU General Public License.
 * See the file COPYING in this distribution
 *
 * Copryright (C) 1996 Peter J. Braam <braam@stelias.com>
 * Copryright (C) 1999 Stelias Computing Inc. <braam@stelias.com>
 * Copryright (C) 1999 Seagate Technology Inc.
 * Copryright (C) 2001 Mountain View Data, Inc.
 *
 */

#define EXPORT_SYMTAB

#include <linux/config.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/string.h>
#include <linux/stat.h>
#include <linux/errno.h>
#include <linux/locks.h>
#include <linux/unistd.h>

#include <asm/system.h>
#include <asm/uaccess.h>

#include <linux/fs.h>
#include <linux/stat.h>
#include <asm/uaccess.h>
#include <linux/vmalloc.h>
#include <asm/segment.h>

#include <linux/obd_support.h>
#include <linux/obd_class.h>
#include <linux/obdfs.h>

struct list_head obdfs_super_list;
struct address_space_operations obdfs_aops;
struct super_operations obdfs_super_operations;
long obdfs_cache_count = 0;
long obdfs_mutex_start = 0;
long obd_memory = 0;

static char *obdfs_read_opt(const char *opt, char *data)
{
        char *value;
        char *retval;

        CDEBUG(D_INFO, "option: %s, data %s\n", opt, data);
        if ( strncmp(opt, data, strlen(opt)) )
                return NULL;

        if ( (value = strchr(data, '=')) == NULL )
                return NULL;

        value++;
        OBD_ALLOC(retval, char *, strlen(value) + 1);
        if ( !retval ) {
                printk(KERN_ALERT __FUNCTION__ ": out of memory!\n");
                return NULL;
        }
        
        memcpy(retval, value, strlen(value)+1);
        CDEBUG(D_PSDEV, "Assigned option: %s, value %s\n", opt, retval);
        return retval;
}

static void obdfs_options(char *options, char **dev, char **vers)
{
        char *this_char;

        if (!options)
                return;

        for (this_char = strtok (options, ",");
             this_char != NULL;
             this_char = strtok (NULL, ",")) {
                CDEBUG(D_INFO, "this_char %s\n", this_char);
                if ( (!*dev && (*dev = obdfs_read_opt("device", this_char)))||
                     (!*vers && (*vers = obdfs_read_opt("version", this_char))) )
                        continue;
                
        }
}

static int obdfs_getdev(char *devpath, int *dev)
{
        struct dentry *dentry;
        kdev_t devno;
        struct nameidata nd;
        int error = 0; 

        ENTRY;
        if (path_init(devpath, LOOKUP_POSITIVE, &nd))
                error = path_walk(devpath, &nd);
        if (error)
                return error;

        dentry = nd.dentry;
        if (!S_ISCHR(dentry->d_inode->i_mode))
                return -ENODEV;

        devno = dentry->d_inode->i_rdev;
        if ( MAJOR(devno) != OBD_PSDEV_MAJOR ) 
                return -ENODEV;
        
        if ( MINOR(devno) >= MAX_OBD_DEVICES ) 
                return -ENODEV;

        *dev = devno;
        return 0;
}


static struct super_block * obdfs_read_super(struct super_block *sb, 
                                            void *data, int silent)
{
        struct inode *root = 0; 
        struct obdfs_sb_info *sbi = (struct obdfs_sb_info *)(&sb->u.generic_sbp);
        struct obd_device *obddev;
        char *device = NULL;
        char *version = NULL;
        int devno;
        int err;
        unsigned long blocksize;
        unsigned long blocksize_bits;
        unsigned long root_ino;
        int scratch;
        

        ENTRY;
        MOD_INC_USE_COUNT; 
        
        memset(sbi, 0, sizeof(*sbi));
        
        CDEBUG(D_INFO, "\n"); 
        obdfs_options(data, &device, &version);
        if ( !device ) {
                printk(__FUNCTION__ ": no device\n");
                EXIT;
                goto ERR;
        }

        CDEBUG(D_INFO, "\n"); 
        if ( (err = obdfs_getdev(device, &devno)) ) {
                printk("Cannot get devno of %s, error %d\n", device, err);
                EXIT;
                goto ERR;;
        }

        CDEBUG(D_INFO, "\n"); 
        if ( MAJOR(devno) != OBD_PSDEV_MAJOR ) {
                printk(__FUNCTION__ ": wrong major number %d!\n", MAJOR(devno));
                EXIT;
                goto ERR;
        }
                
        CDEBUG(D_INFO, "\n"); 
        if ( MINOR(devno) >= MAX_OBD_DEVICES ) {
                printk(__FUNCTION__ ": minor of %s too high (%d)\n",
                       device, MINOR(devno));
                EXIT;
                goto ERR;
        } 

        CDEBUG(D_INFO, "\n"); 
        obddev = &obd_dev[MINOR(devno)];

        CDEBUG(D_INFO, "\n"); 
        if ( ! (obddev->obd_flags & OBD_ATTACHED) || 
             ! (obddev->obd_flags & OBD_SET_UP) ){
                printk("device %s not attached or not set up (%d)\n", 
                       device, MINOR(devno));
                EXIT;
                goto ERR;;
        } 

        CDEBUG(D_INFO, "\n"); 
        sbi->osi_obd = obddev;
        sbi->osi_ops = sbi->osi_obd->obd_type->typ_ops;
        
        sbi->osi_conn.oc_dev = obddev;
        err = sbi->osi_ops->o_connect(&sbi->osi_conn);
        if ( err ) {
                printk("OBDFS: cannot connect to %s\n", device);
                EXIT;
                goto ERR;
        }

        CDEBUG(D_INFO, "\n"); 
        /* list of dirty inodes, and a mutex to hold while modifying it */
        INIT_LIST_HEAD(&sbi->osi_inodes);
        sema_init(&sbi->osi_list_mutex, 1);

        CDEBUG(D_INFO, "\n"); 
        sbi->osi_super = sb;

        CDEBUG(D_INFO, "\n"); 
        err = sbi->osi_ops->o_get_info(&sbi->osi_conn, strlen("blocksize"),
                                       "blocksize", &scratch,
                                       (void *)&blocksize);
        if ( err ) {
                printk("getinfo call to drive failed (blocksize)\n");
                EXIT;
                goto ERR;
        }

        CDEBUG(D_INFO, "\n"); 
        err = sbi->osi_ops->o_get_info(&sbi->osi_conn, strlen("blocksize_bits"),
                                       "blocksize_bits", &scratch,
                                       (void *)&blocksize_bits);
        if ( err ) {
                printk("getinfo call to drive failed (blocksize_bits)\n");
                EXIT;
                goto ERR;
        }

        CDEBUG(D_INFO, "\n"); 
        err = sbi->osi_ops->o_get_info(&sbi->osi_conn, strlen("root_ino"), 
                                       "root_ino", &scratch, (void *)&root_ino);
        if ( err ) {
                printk("getinfo call to drive failed (root_ino)\n");
                EXIT;
                goto ERR;
        }
        
        CDEBUG(D_INFO, "\n"); 
        sb->s_blocksize = blocksize;
        sb->s_blocksize_bits = (unsigned char)blocksize_bits;
        sb->s_magic = OBDFS_SUPER_MAGIC;
        sb->s_op = &obdfs_super_operations;

        /* XXX how to get "sb->s_flags |= MS_RDONLY" here for snapshots? */

        /* make root inode */
        CDEBUG(D_INFO, "\n"); 
        root = iget(sb, root_ino);
        if (!root || is_bad_inode(root)) {
            printk("OBDFS: bad iget for root\n");
            sb->s_dev = 0;
            err = -ENOENT;
            EXIT;
            goto ERR;
        } 
        
        CDEBUG(D_INFO, "obdfs_read_super: sbdev %d, rootino: %ld, dev %s, "
               "minor: %d, blocksize: %ld, blocksize bits %ld\n", 
               sb->s_dev, root->i_ino, device, MINOR(devno), 
               blocksize, blocksize_bits);
        sb->s_root = d_alloc_root(root);
        list_add(&sbi->osi_list, &obdfs_super_list);
        OBD_FREE(device, strlen(device) + 1);
        if (version)
                OBD_FREE(version, strlen(version) + 1);
        EXIT;  
        return sb;

ERR:
        MOD_DEC_USE_COUNT;
        if (device)
                OBD_FREE(device, strlen(device) + 1);
        if (version)
                OBD_FREE(version, strlen(version) + 1);
        if (sbi) {
                sbi->osi_super = NULL;
        }
        if (root) {
                iput(root);
        }
        sb->s_dev = 0;
        return NULL;
} /* obdfs_read_super */


static void obdfs_put_super(struct super_block *sb)
{
        struct obdfs_sb_info *sbi;

        ENTRY;
        sb->s_dev = 0;
        
        sbi = (struct obdfs_sb_info *) &sb->u.generic_sbp;
        obdfs_flush_reqs(&sbi->osi_inodes, ~0UL);

        OPS(sb,disconnect)(ID(sb));
        list_del(&sbi->osi_list);
        memset(sbi, 0, sizeof(*sbi));
        
        printk(KERN_INFO "OBDFS: Bye bye.\n");

        MOD_DEC_USE_COUNT;
        EXIT;
} /* obdfs_put_super */


/* all filling in of inodes postponed until lookup */
static void obdfs_read_inode(struct inode *inode)
{
        struct obdo *oa;
        ENTRY;
        oa = obdo_fromid(IID(inode), inode->i_ino,
                         OBD_MD_FLNOTOBD | OBD_MD_FLBLOCKS);
        if ( IS_ERR(oa) ) {
                printk(__FUNCTION__ ": obdo_fromid failed\n");
                EXIT;
                return /* PTR_ERR(oa) */;
        }

        ODEBUG(oa);
        obdfs_to_inode(inode, oa);
        INIT_LIST_HEAD(obdfs_iplist(inode)); /* list of dirty pages on inode */
        INIT_LIST_HEAD(obdfs_islist(inode)); /* list of inodes in superblock */

        obdo_free(oa);
        /* OIDEBUG(inode); */

        if (S_ISREG(inode->i_mode)) {
                inode->i_op = &obdfs_file_inode_operations;
                inode->i_fop = &obdfs_file_operations;
                inode->i_mapping->a_ops = &obdfs_aops;
                EXIT;
        } else if (S_ISDIR(inode->i_mode)) {
                inode->i_op = &obdfs_dir_inode_operations;
                inode->i_fop = &obdfs_dir_operations; 
                inode->i_mapping->a_ops = &obdfs_aops;
                EXIT;
        } else if (S_ISLNK(inode->i_mode)) {
                if (inode->i_blocks) { 
                        inode->i_op = &obdfs_symlink_inode_operations;
                        inode->i_mapping->a_ops = &obdfs_aops;
                }else {
                        inode->i_op = &obdfs_fast_symlink_inode_operations;
                }
                EXIT;
        } else {
                init_special_inode(inode, inode->i_mode,
                                   ((int *)obdfs_i2info(inode)->oi_inline)[0]);
        }

        return;
}

static void obdfs_write_inode(struct inode *inode, int wait) 
{
        struct obdo *oa;
        int err;
        
        ENTRY;
        if (IOPS(inode, setattr) == NULL) {
                printk(KERN_ERR __FUNCTION__ ": no setattr method!\n");
                EXIT;
                return;
        }
        oa = obdo_alloc();
        if ( !oa ) {
                printk(__FUNCTION__ ": obdo_alloc failed\n");
                EXIT;
                return;
        }

        oa->o_valid = OBD_MD_FLNOTOBD;
        obdfs_from_inode(oa, inode);
        err = IOPS(inode, setattr)(IID(inode), oa);

        if ( err )
                printk(__FUNCTION__ ": obd_setattr fails (%d)\n", err);

        EXIT;
        obdo_free(oa);
} /* obdfs_write_inode */


/* This routine is called from iput() (for each unlink on the inode).
 * We can't put this call into delete_inode() since that is called only
 * when i_count == 0, and we need to keep a reference on the inode while
 * it is in the page cache, which means i_count > 0.  Catch 22.
 */
static void obdfs_put_inode(struct inode *inode)
{
        ENTRY;
        if (inode->i_nlink) {
                EXIT;
                return;
        }

        obdfs_dequeue_pages(inode);
        EXIT;
} /* obdfs_put_inode */


static void obdfs_delete_inode(struct inode *inode)
{
        struct obdo *oa;
        int err;

        ENTRY;
        if (IOPS(inode, destroy) == NULL) {
                printk(KERN_ERR __FUNCTION__ ": no destroy method!\n");
                EXIT;
                return;
        }

        oa = obdo_alloc();
        if ( !oa ) {
                printk(__FUNCTION__ ": obdo_alloc failed\n");
                EXIT;
                return;
        }
        oa->o_valid = OBD_MD_FLNOTOBD;
        obdfs_from_inode(oa, inode);

        ODEBUG(oa);
        err = IOPS(inode, destroy)(IID(inode), oa);
        obdo_free(oa);
        clear_inode(inode);
        if (err) {
                printk(__FUNCTION__ ": obd_destroy fails (%d)\n", err);
                EXIT;
                return;
        }

        EXIT;
} /* obdfs_delete_inode */


int obdfs_notify_change(struct dentry *de, struct iattr *attr)
{
        struct inode *inode = de->d_inode;
        struct obdo *oa;
        int err;

        ENTRY;
        if (IOPS(inode, setattr) == NULL) {
                printk(KERN_ERR __FUNCTION__ ": no setattr method!\n");
                EXIT;
                return -EIO;
        }
        oa = obdo_alloc();
        if ( !oa ) {
                printk(__FUNCTION__ ": obdo_alloc failed\n");
                return -ENOMEM;
        }

        oa->o_id = inode->i_ino;
        obdo_from_iattr(oa, attr);
        err = IOPS(inode, setattr)(IID(inode), oa);

        if ( err )
                printk(__FUNCTION__ ": obd_setattr fails (%d)\n", err);

        EXIT;
        obdo_free(oa);
        return err;
} /* obdfs_notify_change */


static int obdfs_statfs(struct super_block *sb, struct statfs *buf)
{
        struct statfs tmp;
        int bufsize = sizeof(*buf);
        int err;

        ENTRY;

        err = OPS(sb,statfs)(ID(sb), &tmp);
        if ( err ) { 
                printk(__FUNCTION__ ": obd_statfs fails (%d)\n", err);
                return err;
        }
        copy_to_user(buf, &tmp, (bufsize<sizeof(tmp)) ? bufsize : sizeof(tmp));

        EXIT;

        return err; 
}

/* exported operations */
struct super_operations obdfs_super_operations =
{
        read_inode: obdfs_read_inode,
        write_inode: obdfs_write_inode,
        put_inode: obdfs_put_inode,
        delete_inode: obdfs_delete_inode,
        put_super: obdfs_put_super,
        statfs: obdfs_statfs
};

struct file_system_type obdfs_fs_type = {
   "obdfs", 0, obdfs_read_super, NULL
};

int init_obdfs(void)
{
        int err;

        printk(KERN_INFO "OBDFS v0.1, braam@stelias.com\n");

        obdfs_sysctl_init();

        INIT_LIST_HEAD(&obdfs_super_list);
        err = obdfs_init_pgrqcache();
        if (err)
                return err;

        obdfs_flushd_init();
        return register_filesystem(&obdfs_fs_type);
}

struct address_space_operations obdfs_aops = {
        readpage: obdfs_readpage,
        writepage: obdfs_writepage,
        sync_page: block_sync_page,
        prepare_write: obdfs_prepare_write, 
        commit_write: obdfs_commit_write,
        bmap: NULL
};


#ifdef MODULE
int init_module(void)
{
        return init_obdfs();
}

void cleanup_module(void)
{
        ENTRY;

        obdfs_flushd_cleanup();
        obdfs_sysctl_clean();
        obdfs_cleanup_pgrqcache();
        unregister_filesystem(&obdfs_fs_type);
        CDEBUG(D_MALLOC, "OBDFS mem used %ld\n", obd_memory);
        EXIT;
}

#endif

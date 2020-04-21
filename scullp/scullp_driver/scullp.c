#include <linux/init.h>  
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/types.h>
#include <linux/fs.h>
#include <linux/proc_fs.h>
#include <linux/slab.h>
#include <linux/errno.h>
#include <linux/stat.h>
#include <linux/kernel.h>
#include <linux/uaccess.h>
#include <linux/device.h>
#include <linux/poll.h>

#include "scull.h"

//#define SCULLP_EXCLUSIVE	
#define SCULLP_EXCLUSIVE_USER
int g_scullp_major =   SCULL_MAJOR;
int g_scullp_dev_num = SCULL_DEVS_NUM;
int g_scullp_size = SCULLP_SIZE;
struct class *g_scullp_class;
struct scullp_dev *g_scullp_devices; 
struct proc_dir_entry *g_scullp_entry = NULL;

#ifdef SCULLP_EXCLUSIVE
static atomic_t s_scullp_count = ATOMIC_INIT(1);
#endif

#ifdef SCULLP_EXCLUSIVE_USER
static uid_t s_scullp_owner;
static int s_scullp_count = 0;
#endif

MODULE_AUTHOR("ganwei");
MODULE_LICENSE("Dual BSD/GPL");

ssize_t scullp_read(struct file *filp, char __user *data, size_t size, loff_t *f_ops);
int scullp_open(struct inode *inode, struct file *filp);
ssize_t scullp_write(struct file *filp, const char __user *data, size_t size, loff_t *f_ops);
int scullp_close(struct inode *inode, struct file *filp);
ssize_t scullp_read_proc(struct file *filp, char __user *data, size_t size, loff_t *f_ops);
long scullp_ioctl (struct file *filp, unsigned int args, unsigned long size);
unsigned int scullp_poll(struct file *filp, struct poll_table_struct *wait);
int scullp_fasync(int fd, struct file *filp, int mode);

struct file_operations g_scullp_fops = {
	.owner = 			THIS_MODULE,
	.open = 			scullp_open,
	.read = 			scullp_read,
	.write = 			scullp_write,
	.release =			scullp_close,
	.unlocked_ioctl = 	scullp_ioctl,
	.poll =				scullp_poll,
	.fasync = 			scullp_fasync,

};
const struct file_operations g_sullp_proc_fops = {
 	.read  = scullp_read_proc,
};
int scullp_fasync(int fd, struct file *filp, int on)
{
	struct scullp_dev *dev = filp->private_data;
	return fasync_helper(fd, filp, on, &(dev->scullp_async));
}
unsigned int scullp_poll(struct file *filp, struct poll_table_struct *wait)
{
	struct scullp_dev *dev = filp->private_data;
	if(down_interruptible(&(dev->sem))){
		return -EBUSY;
	}
	poll_wait(filp,&(dev->outq), wait);
	if(dev->r_pos != dev->w_pos){
		SCULL_DPRINT("%s %d: dev->r_pos = %p, dev->w_pos = %p\n",
			__func__, __LINE__, dev->r_pos, dev->w_pos);
		return POLLIN | POLLRDNORM;
	}
	up(&dev->sem);
	return 0;

}

long scullp_ioctl (struct file *filp, unsigned int cmd, unsigned long args)
{
	long retval = 0;
	if(SCULL_IOC_MAGIC != _IOC_TYPE(cmd)){
		SCULL_DPRINT("%s %d: type not OK, SCULL_IOC_MAGIC = %d, _IOC_TYPE(cmd) = %d\n", 
			__func__, __LINE__, SCULL_IOC_MAGIC, _IOC_TYPE(cmd));
		return ENOTTY;
	}
	if(_IOC_NR(cmd) >= SCULL_IOC_MAX){
		SCULL_DPRINT("%s %d: cmd number not OK, SCULL_IOC_MAX = %d, _IOC_NR(cmd) = %d\n", 
			__func__, __LINE__, SCULL_IOC_MAX, _IOC_NR(cmd));
		return ENOTTY;
	}

	return retval;
}
size_t scullp_get_free_rspace(struct scullp_dev *dev)
{
	SCULL_DPRINT("%s %d: dev->r_pos = %p, dev->w_pos = %p\n",
		__func__, __LINE__, dev->r_pos, dev->w_pos);

	if(dev->w_pos == dev->r_pos){
		return 0;
	}else{
		return (dev->buffer_size + dev->w_pos - dev->r_pos) % dev->buffer_size;
	}
	return 0;
}
size_t __scullp_read(struct scullp_dev *dev, const char __user *data, size_t size)
{
	int retval;
	int read_size_1;
	if(size + dev->r_pos > dev->end){
		read_size_1 = dev->end - dev->r_pos;
		retval = copy_to_user((void __user *)data, dev->r_pos, read_size_1);
		if(retval < 0){
			SCULL_DPRINT("%s %d: copy_to_user failed, dev->r_pos = %p, data = %p, size = %d\n",
				__func__, __LINE__, dev->r_pos, data, (unsigned int)size);
			return -EFAULT;
		}
		dev->r_pos = dev->buffer;
		retval = copy_to_user((void __user *)(data + read_size_1), dev->r_pos, size - read_size_1);
		if(retval < 0){
			SCULL_DPRINT("%s %d: copy_to_user failed, dev->r_pos = %p, data = %p, size = %d\n",
				__func__, __LINE__, dev->r_pos, data, (unsigned int)size);
			return read_size_1;
		}
		dev->r_pos = dev->r_pos + (size - read_size_1);
		
	}else{
		retval = copy_to_user((void __user *)data, dev->r_pos, size);
		if(retval < 0){
			SCULL_DPRINT("%s %d: copy_to_user failed, dev->r_pos = %p, data = %p, size = %d\n",
				__func__, __LINE__, dev->r_pos, data, (unsigned int)size);
			return -EFAULT;
		}
		dev->r_pos = dev->r_pos + size;
		if(dev->r_pos == dev->end){
			dev->r_pos = dev->buffer;
		}
	}
	return size;	


}

ssize_t scullp_read(struct file *filp, char __user *data, size_t size, loff_t *f_ops)
{
	struct scullp_dev *dev = filp->private_data;
	int read_size;
	int free_size;
	long retval;
	SCULL_DPRINT("%s %d: \n", __func__, __LINE__);

	if(down_interruptible(&(dev->sem))){
		return -EBUSY;
	}
	if(0 == scullp_get_free_rspace(dev)){
		if(filp->f_flags & O_NONBLOCK){
			up(&(dev->sem));
			return -EAGAIN;
		}
		up(&(dev->sem));		

		retval = wait_event_interruptible(dev->outq, (scullp_get_free_rspace(dev) != 0));
		if(0 != retval){
			return -ERESTARTSYS;
		}
		if(down_interruptible(&(dev->sem))){
			return -EBUSY;
		}

	}

	free_size = scullp_get_free_rspace(dev);
	read_size = min((long)size, (long)free_size);
	SCULL_DPRINT("%s %d: free_size = %d, read_size = %d\n", __func__, __LINE__, free_size, read_size);
	read_size = __scullp_read(dev, data, read_size);
	up(&(dev->sem));
	wake_up_interruptible(&(dev->inq));
	return read_size;
}
int scullp_open(struct inode *inode, struct file *filp)
{
	struct scullp_dev *dev;
	dev = container_of(inode->i_cdev, struct scullp_dev, cdevices);
	filp->private_data = dev;
#ifdef SCULLP_EXCLUSIVE
	SCULL_DPRINT("%s %d: s_scullp_count = %d\n", __func__, __LINE__, s_scullp_count.counter);
	if(atomic_dec_and_test(&s_scullp_count) == 0){
		atomic_inc(&s_scullp_count);
		return -EBUSY;
	}
#endif
#ifdef SCULLP_EXCLUSIVE_USER
	SCULL_DPRINT("%s %d: s_scullp_count = %d\n", __func__, __LINE__, s_scullp_count);
	spin_lock(&dev->scullp_lock);
	if(0 == s_scullp_count){
		s_scullp_owner = current->cred->uid.val;	
	}else if((s_scullp_owner != current->cred->uid.val) && (s_scullp_owner != current->cred->euid.val)){
		if(!capable(CAP_DAC_OVERRIDE)){
			spin_unlock(&dev->scullp_lock);
			return -EBUSY;
		}
	}
	s_scullp_count++;
	spin_unlock(&dev->scullp_lock);
#endif

	if(down_interruptible(&(dev->sem))){
		return -EBUSY;
	}

	if(NULL == dev->buffer){
		dev->buffer = kmalloc(dev->buffer_size, GFP_KERNEL);
		if(NULL == dev->buffer){
			SCULL_DPRINT("%s %d:dev->buffer malloc failed, dev->buffer_size = %d\n", __func__, __LINE__, dev->buffer_size);
			up(&(dev->sem));
			return -ENOMEM;
		}
		dev->r_pos = dev->buffer;
		dev->w_pos = dev->buffer;
		dev->end = dev->buffer + dev->buffer_size;
		dev->nreaders = 0;
		dev->nwriters = 0;
	}
	if(filp->f_mode & FMODE_READ){
		dev->nreaders++;
	}
	if(filp->f_mode & FMODE_WRITE){
		dev->nwriters++;
	}
	up(&(dev->sem));
	SCULL_DPRINT("%s %d: dev->nwriters = %d, dev->nreaders = %d\n", 
		__func__, __LINE__, dev->nwriters, dev->nreaders);
	return nonseekable_open(inode, filp);
}
size_t scullp_get_free_wspace(struct scullp_dev *dev)
{
	if(dev->w_pos == dev->r_pos){
		return dev->buffer_size - 1;
	}else{
		return (dev->buffer_size + dev->r_pos - dev->w_pos - 1) % dev->buffer_size;
	}
	return 0;
}
size_t __scullp_write(struct scullp_dev *dev, const char __user *data, size_t size)
{
	int retval;
	int write_size_1;
	if(size + dev->w_pos > dev->end){
		write_size_1 = dev->end - dev->w_pos;
		retval = copy_from_user(dev->w_pos, data, write_size_1);
		if(retval < 0){
			SCULL_DPRINT("%s %d: copy_from_user failed, dev->w_pos = %p, data = %p, size = %d\n",
				__func__, __LINE__, dev->w_pos, data, (unsigned int)size);
			return -EFAULT;
		}
		dev->w_pos = dev->buffer;
		retval = copy_from_user(dev->w_pos, data + write_size_1, size - write_size_1);
		if(retval < 0){
			SCULL_DPRINT("%s %d: copy_from_user failed, dev->w_pos = %p, data = %p, size = %d\n",
				__func__, __LINE__, dev->w_pos, data, (unsigned int)size);
			return write_size_1;
		}
		dev->w_pos = dev->w_pos + (size - write_size_1);
		
	}else{
		retval = copy_from_user(dev->w_pos, data, size);
		if(retval < 0){
			SCULL_DPRINT("%s %d: copy_from_user failed, dev->w_pos = %p, data = %p, size = %d\n",
				__func__, __LINE__, dev->w_pos, data, (unsigned int)size);
			return -EFAULT;
		}
		dev->w_pos = dev->w_pos + size;
		if(dev->w_pos == dev->end){
			dev->w_pos = dev->buffer;
		}
	}
	return size;	


}

ssize_t scullp_write(struct file *filp, const char __user *data, size_t size, loff_t *f_ops)
{
	struct scullp_dev *dev = filp->private_data;
	size_t write_size;
	size_t free_size = 0;
	if(down_interruptible(&(dev->sem))){
		return EBUSY;
	}

	if(0 == scullp_get_free_wspace(dev)){
		if(filp->f_flags & O_NONBLOCK){
			up(&(dev->sem));
			return -EAGAIN;
		}
		up(&(dev->sem));		
		if(wait_event_interruptible(dev->inq, (0 != scullp_get_free_wspace(dev)))){
			return -ERESTARTSYS;
		}
		if(down_interruptible(&(dev->sem))){
			return EBUSY;
		}
	}

	free_size = scullp_get_free_wspace(dev);
	write_size = min((long)size, (long)free_size);
	write_size = __scullp_write(dev, data, write_size);
	up(&(dev->sem));
	wake_up_interruptible(&(dev->outq));
	SCULL_DPRINT("%s %d: , dev->w_pos = %p, dev->r_pos = %p, write_size = %d\n",
		__func__, __LINE__, dev->w_pos, dev->r_pos, (int)write_size);
	if(NULL != dev->scullp_async){
		kill_fasync(&(dev->scullp_async), SIGIO, POLLIN);
	}
	return write_size;
}
int scullp_close(struct inode *inode, struct file *filp)
{
	struct scullp_dev *dev = filp->private_data;
	if(down_interruptible(&(dev->sem))){
		return EBUSY;
	}
	if(filp->f_mode & FMODE_READ){
		dev->nreaders--;
	}
	if(filp->f_mode & FMODE_WRITE){
		dev->nwriters--;
	}
	if((0 == dev->nwriters) && (0 == dev->nreaders)){
		kfree(dev->buffer);
		dev->buffer = NULL;
		dev->end = NULL;
		dev->r_pos = NULL;
		dev->w_pos = NULL;
	}
	if(&(dev->scullp_async)){
		scullp_fasync(-1, filp, 0);
	}
	up(&(dev->sem));
#ifdef SCULLP_EXCLUSIVE
	atomic_inc(&s_scullp_count);
#endif
#ifdef SCULLP_EXCLUSIVE_USER
	spin_lock(&dev->scullp_lock);
	s_scullp_count--;
	spin_unlock(&dev->scullp_lock);
#endif
	return 0;
}

void scullp_cleanup(void)
{
	int i;
	dev_t scull_dev = MKDEV(g_scullp_major, SCULL_BASE_MINOR);
	for(i = 0; i < g_scullp_dev_num; i++){
		cdev_del(&g_scullp_devices[i].cdevices);
		device_destroy(g_scullp_class,MKDEV(g_scullp_major, i));
	}
	class_destroy(g_scullp_class);

	if(NULL != g_scullp_devices){
		kfree(g_scullp_devices);
		g_scullp_devices = NULL;
	}

	
	unregister_chrdev_region(scull_dev, g_scullp_dev_num);		
}
void scullp_cdev_init(struct scullp_dev *devices, int major, int index)
{
	int ret;
	dev_t scullp_mkdev = MKDEV(major, index);
	cdev_init(&devices->cdevices, &g_scullp_fops);
	devices->cdevices.owner = THIS_MODULE;
	devices->cdevices.ops = &g_scullp_fops;
	ret = cdev_add(&devices->cdevices, scullp_mkdev, 1);
	if(0 != ret){
		SCULL_DPRINT("cdev_add failed, ret = %d, major = %d, index = %d\n", ret, major, index);
	}
}
#ifdef SCULL_USE_PROC
ssize_t scullp_read_proc(struct file *filp, char __user *data, size_t size, loff_t *f_ops)
{
	SCULL_DPRINT("g_scullp_size = %d\n", g_scullp_size);
	return 0;
}

#endif
static int scullp_module_init(void)
{
	int ret, i;
	dev_t scullp_dev = MKDEV(g_scullp_major, SCULL_BASE_MINOR);
	SCULL_DPRINT("scullp_module_init\n");
	if(0 == g_scullp_major){
		ret = alloc_chrdev_region(&scullp_dev, SCULL_BASE_MINOR, g_scullp_dev_num, "scullp");
		g_scullp_major = MAJOR(scullp_dev);
	}else{
		ret = register_chrdev_region(scullp_dev, g_scullp_dev_num, "scullp");
	}
	if(0 != ret){
		SCULL_DPRINT("%s %d: init failed\n", __func__, __LINE__);
		return ret;	
	}
	g_scullp_devices = kmalloc(sizeof(struct scullp_dev) * g_scullp_dev_num, GFP_KERNEL);
	if(NULL == g_scullp_devices){
		SCULL_DPRINT("%s %d: kmalloc g_scullp_devices failed.\n", __func__, __LINE__);
		ret = -ENOMEM;
		goto malloc_failed;
	}
	g_scullp_class = class_create(THIS_MODULE, "scullp");
	if(IS_ERR(g_scullp_class)){
		kfree(g_scullp_devices);
		g_scullp_devices = NULL;
		goto malloc_failed;
	}
	for(i = 0; i < g_scullp_dev_num; i++){
		g_scullp_devices[i].buffer_size = SCULLP_SIZE;
		g_scullp_devices[i].buffer = NULL;
		g_scullp_devices[i].end = NULL;
		g_scullp_devices[i].nreaders = 0;
		g_scullp_devices[i].nwriters = 0;
		g_scullp_devices[i].scullp_async = NULL;
		sema_init(&g_scullp_devices[i].sem, 1);
		init_waitqueue_head(&g_scullp_devices[i].inq);
		init_waitqueue_head(&g_scullp_devices[i].outq);
		spin_lock_init(&g_scullp_devices[i].scullp_lock);
		scullp_cdev_init(g_scullp_devices + i, g_scullp_major, i);
		device_create(g_scullp_class, NULL, MKDEV(g_scullp_major, i), NULL, "scullp%d", i);
	}

#ifdef SCULL_USE_PROC
	g_scullp_entry = proc_create( "scullp", 0x0644, NULL, &g_sullp_proc_fops);

#endif
	SCULL_DPRINT("%s %d: init success\n", __func__, __LINE__);
	return 0;
	
malloc_failed:
	unregister_chrdev_region(scullp_dev, g_scullp_dev_num);		
	return -1;
}
static void scullp_module_exit(void)
{
	SCULL_DPRINT("scullp_module_exit\n");
#ifdef SCULL_USE_PROC
	if(NULL != g_scullp_entry){
		remove_proc_entry("scullp", NULL);
	}
#endif
	scullp_cleanup();
	if(NULL != g_scullp_devices){
		kfree(g_scullp_devices);
		g_scullp_devices = NULL;
	}
}

module_init(scullp_module_init);
module_exit(scullp_module_exit);

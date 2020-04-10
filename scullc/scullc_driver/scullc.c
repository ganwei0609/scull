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

#include "scull.h"

int g_scullc_major =   SCULLC_MAJOR;
int g_scullc_quantum = SCULLC_QUANTUM;
int g_scullc_qset = SCULLC_QSET;
int g_scullc_dev_num = SCULLC_DEVS_NUM;
struct class *g_scullc_class;
struct kmem_cache *g_scullc_cache;
struct scullc_dev *g_scullc_devices; 
struct proc_dir_entry *g_scullc_entry = NULL;

module_param(g_scullc_quantum, int, S_IRUSR);
module_param(g_scullc_qset, int,  S_IRUSR);
module_param(g_scullc_dev_num, int,  S_IRUSR);

MODULE_AUTHOR("ganwei");
MODULE_LICENSE("Dual BSD/GPL");

extern ssize_t sculllc_read(struct file *filp, char __user *data, size_t size, loff_t *f_ops);
extern int sclullc_open(struct inode *inode, struct file *filp);
extern ssize_t scullc_write(struct file *filp, const char __user *data, size_t size, loff_t *f_ops);
extern int scullc_close(struct inode *inode, struct file *filp);
extern ssize_t scullc_read_proc(struct file *filp, char __user *data, size_t size, loff_t *f_ops);
extern long scullc_ioctl (struct file *, unsigned int, unsigned long);
extern loff_t scullc_llseek(struct file *filp, loff_t offset, int whence);
extern void scullc_trim(struct scullc_dev *devices);
extern struct scullc_dev *scullc_follow(struct scullc_dev *dev, int devnum);

struct file_operations g_scullc_fops = {
	.owner = 			THIS_MODULE,
	.open = 			sclullc_open,
	.read = 			sculllc_read,
	.write = 			scullc_write,
	.release =			scullc_close,
	.unlocked_ioctl = 	scullc_ioctl,
	.llseek = 			scullc_llseek,
};
const struct file_operations g_sullc_proc_fops = {
 	.read  = scullc_read_proc,
};

loff_t scullc_llseek(struct file *filp, loff_t offset, int whence)
{
	struct scullc_dev *device = filp->private_data;
	SCULLC_DPRINT("%s %d: whence = %d\n", __func__, __LINE__, whence);
	switch (whence) {
		case SEEK_SET:
			filp->f_pos = 0 + offset;
			break;
		case SEEK_CUR:
			filp->f_pos = filp->f_pos + offset;
			break;
		case SEEK_END:
			filp->f_pos = device->size + offset;
			break;
		default:
			return -EINVAL;
			break;
	}
	SCULLC_DPRINT("%s %d: filp->f_pos= %ld\n", __func__, __LINE__, (long)filp->f_pos);
	if(filp->f_pos < 0){
		return -EINVAL;
	}
	return filp->f_pos;
}
long scullc_ioctl (struct file *filp, unsigned int cmd, unsigned long args)
{
	long retval = 0;
	int temp;
	if(SCULLC_IOC_MAGIC != _IOC_TYPE(cmd)){
		SCULLC_DPRINT("%s %d: type not OK, SCULLC_IOC_MAGIC = %d, _IOC_TYPE(cmd) = %d\n", 
			__func__, __LINE__, SCULLC_IOC_MAGIC, _IOC_TYPE(cmd));
		return ENOTTY;
	}
	if(_IOC_NR(cmd) >= SCULLC_IOC_MAX){
		SCULLC_DPRINT("%s %d: cmd number not OK, SCULLC_IOC_MAX = %d, _IOC_NR(cmd) = %d\n", 
			__func__, __LINE__, SCULLC_IOC_MAX, _IOC_NR(cmd));
		return ENOTTY;
	}
	switch (cmd) {
		case SCULLC_IOC_GET_QSET:
			retval = g_scullc_qset;
			break;
		case SCULLC_IOC_SET_QSET:
			g_scullc_qset = args;
			retval = 0;
			break;
		case SCULLC_IOC_GET_QSET_PTR:
			retval = put_user(g_scullc_qset,(int __user *)args);
			break;
		case SCULLC_IOC_SET_QSET_PTR:
			retval = get_user(g_scullc_qset,(int __user *)args);
			break;
		case SCULLC_IOC_EX_QSET:
			retval = g_scullc_qset;
			g_scullc_qset = args;
			break;
		case SCULLC_IOC_EX_QSET_PTR:
			retval = get_user(temp,(int __user *)args);
			retval |= put_user(g_scullc_qset, (int __user *)args);
			g_scullc_qset = temp;
			break;
		case SCULLC_IOC_GET_QUANTUM:
			retval = g_scullc_quantum;
			break;
		case SCULLC_IOC_SET_QUANTUM:
			g_scullc_quantum = args;
			break;
		case SCULLC_IOC_GET_QUANTUM_PTR:
			retval = put_user(g_scullc_quantum,(int __user *)args);
			break;
		case SCULLC_IOC_SET_QUANTUM_PTR:
			retval = get_user(g_scullc_quantum,(int __user *)args);
			break;
		case SCULLC_IOC_EX_QUANTUM:
			retval = g_scullc_quantum;
			g_scullc_quantum = args;
			break;
		case SCULLC_IOC_EX_QUANTUM_PTR:
			retval = get_user(temp,(int __user *)args);
			retval |= put_user(g_scullc_quantum, (int __user *)args);
			g_scullc_quantum = temp;
			break;
		default:
			retval = -ENOTTY;
	}
	return retval;
}

ssize_t sculllc_read(struct file *filp, char __user *data, size_t size, loff_t *f_ops)
{
	struct scullc_dev *dev = filp->private_data;
	int quantum = dev->quantum;
	int qset = dev->qset;
	int devsize = quantum * qset;
	int devnum;
	int remain;
	int quantum_pos;
	int quantum_remain;
	int read_size;
	long ret;
	int i;
	struct scullc_dev *ptr;
	devnum = (long)(*f_ops) / devsize;
	remain = (long)(*f_ops) % devsize;
	quantum_pos = remain / quantum;
	quantum_remain = remain % quantum;
	SCULLC_DPRINT("%s %d: \n", __func__, __LINE__);
	if(down_interruptible(&(dev->sem))){
		return EBUSY;
	}
	SCULLC_DPRINT("%s %d: *f_ops = %ld, dev->size = %d\n", __func__, __LINE__, (long)*f_ops, (int)dev->size);
	
	ptr = scullc_follow(dev, devnum);
	if((NULL == ptr) || (NULL == ptr->data)){
		SCULLC_DPRINT("%s %d: scullc_follow failed\n", __func__, __LINE__);
		up(&(dev->sem));	
		return ENOMEM;
	}
	if(remain >= dev->size){
		SCULLC_DPRINT("%s %d:*f_ops = %ld, dev->size = %d\n", __func__, __LINE__, (long)*f_ops, (int)dev->size);
		up(&(dev->sem));	
		return 0;
	}

	for(i = 0; i < qset; i++){
		if(NULL != ptr->data[i]){
			SCULLC_DPRINT("%s %d: ptr->data[%d] = %p\n", __func__, __LINE__, i, ptr->data[i]);
		}
	}		

	read_size = min((long)size, (long)(quantum - quantum_remain));
	if(read_size > (dev->size - remain)){
		read_size = dev->size - remain;
	}
	SCULLC_DPRINT("%s %d: read_size = %d, size = %d, quantum = %d, quantum_remain = %d, dev->size = %d\n", 
		__func__, __LINE__, read_size, (int)size, quantum, quantum_remain, (int)dev->size);
	
	if(NULL == ptr->data[quantum_pos]){
		SCULLC_DPRINT("%s %d: NULL, f_ops = %ld, devnum = %d, remain = %d, quantum_pos = %d, quantum_remain = %d\n",\
			__func__, __LINE__, (long)(*f_ops), devnum, remain, quantum_pos, quantum_remain);
		up(&(dev->sem));	
		return ENOMEM;
	}
	
	ret = copy_to_user((void __user *)data, ptr->data[quantum_pos] + quantum_remain, read_size);
	if(0 != ret){
		SCULLC_DPRINT("%s %d: __copy_to_user failed, ret = %ld, quantum_pos = %d, quantum_remain = %d, data = %p, write_size = %d\n",
			__func__, __LINE__ , ret, quantum_pos, quantum_remain, data, read_size);
		up(&(dev->sem));	
		return EFAULT;
	}
	*f_ops = *f_ops + read_size;

	up(&(dev->sem));	
	SCULLC_DPRINT("%s %d: read_size = %d\n", __func__, __LINE__, read_size);
	return read_size;
}
int sclullc_open(struct inode *inode, struct file *filp)
{
	struct scullc_dev *dev;
	SCULLC_DPRINT("%s %d: \n", __func__, __LINE__);
	dev = container_of(inode->i_cdev, struct scullc_dev, cdevices);
	if(!capable(CAP_SYS_ADMIN)){
		SCULLC_DPRINT("%s %d: no CAP_SYS_ADMIN\n", __func__, __LINE__);
		return -EPERM;
	}
	if(!capable(CAP_SYS_TTY_CONFIG)){
		SCULLC_DPRINT("%s %d: no CAP_SYS_ADMIN\n", __func__, __LINE__);
		return -EPERM;
	}
	if(!capable(CAP_SYS_MODULE)){
		SCULLC_DPRINT("%s %d: no CAP_SYS_ADMIN\n", __func__, __LINE__);
		return -EPERM;
	}
	if(!capable(CAP_SYS_RAWIO)){
		SCULLC_DPRINT("%s %d: no CAP_SYS_ADMIN\n", __func__, __LINE__);
		return -EPERM;
	}
	if(!capable(CAP_DAC_OVERRIDE)){
		SCULLC_DPRINT("%s %d: no CAP_SYS_ADMIN\n", __func__, __LINE__);
		return -EPERM;
	}

	if(O_WRONLY == (filp->f_flags & O_ACCMODE)){
		if(0 != down_interruptible(&dev->sem)){
			return EBUSY;
		}
		scullc_trim(dev);
		up(&dev->sem);
	}
	filp->private_data = dev;
	return 0;
}
ssize_t scullc_write(struct file *filp, const char __user *data, size_t size, loff_t *f_ops)
{
	struct scullc_dev *dev = filp->private_data;
	int quantum = dev->quantum;
	int qset = dev->qset;
	int devsize = quantum * qset;
	int devnum;
	int remain;
	int quantum_pos;
	int quantum_remain;
	int write_size;
	int ret, i;
	struct scullc_dev *ptr;
	devnum = (long)(*f_ops) / devsize;
	remain = (long)(*f_ops) % devsize;
	quantum_pos = remain / quantum;
	quantum_remain = remain % quantum;

	if(down_interruptible(&(dev->sem))){
		return EBUSY;
	}

	ptr = scullc_follow(dev, devnum);
	if(NULL == ptr){
		SCULLC_DPRINT("%s %d:\n", __func__, __LINE__);
		up(&(dev->sem));	
		return ENOMEM;

	}

	write_size = min((long)size, (long)(quantum - quantum_remain));
	SCULLC_DPRINT("%s %d: write_size = %d\n", __func__, __LINE__, write_size);
	if(NULL == ptr->data){
		SCULLC_DPRINT("%s %d:\n", __func__, __LINE__);
		ptr->data = kmalloc(GFP_KERNEL, sizeof(void *) * qset);
		SCULLC_DPRINT("%s %d: ptr->data = %p\n", __func__, __LINE__, ptr->data);

		if(NULL == ptr->data){
			SCULLC_DPRINT("%s %d: kmalloc data failed\n", __func__, __LINE__);
			up(&(dev->sem));	
			return ENOMEM;
		}
		for(i = 0; i < qset; i++){
			 ptr->data[i] = NULL;
		}
		for(i = 0; i < qset; i++){
			if(NULL != ptr->data[i]){
				SCULLC_DPRINT("%s %d: ptr->data[%d] = %p\n", __func__, __LINE__, i, ptr->data[i]);
			}
		}		
	}
	SCULLC_DPRINT("%s %d:\n", __func__, __LINE__);
	if(NULL == ptr->data[quantum_pos]){
		SCULLC_DPRINT("%s %d: quantum_pos = %d\n", __func__, __LINE__, quantum_pos);
		ptr->data[quantum_pos] = kmem_cache_alloc(g_scullc_cache, GFP_KERNEL);	
		if(NULL == ptr->data[quantum_pos]){
			SCULLC_DPRINT("%s %d: kmem_cache_alloc failed, (long)(*f_ops) = %ld, devnum = %d, remain = %d, quantum_pos = %d, quantum_remain = %d\n",
				__func__, __LINE__, (long)(*f_ops), devnum, remain, quantum_pos, quantum_remain);
			up(&(dev->sem));	
			return ENOMEM;

		}
		memset(ptr->data[quantum_pos], 0, quantum);
	}

	ret = copy_from_user(ptr->data[quantum_pos] + quantum_remain, data, write_size);

	SCULLC_DPRINT("%s %d:ret = %d, write_size = %d\n", __func__, __LINE__, ret, write_size);
	if(ret < 0){
		SCULLC_DPRINT("%s %d: strncpy_from_user failed, ret = %d, quantum_pos = %d, quantum_remain = %d, data = %p, write_size = %d\n",
			__func__, __LINE__ , ret, quantum_pos, quantum_remain, data, write_size);
		up(&(dev->sem));	
		return EFAULT;
	}

	*f_ops = *f_ops + write_size;
	if(dev->size < (remain + write_size)){
		dev->size = (remain + write_size);
	}
	up(&(dev->sem));	
	SCULLC_DPRINT("%s %d:write_size = %d\n", __func__, __LINE__, write_size);
	return write_size;
}
int scullc_close(struct inode *inode, struct file *filp)
{
	return 0;
}
struct scullc_dev *scullc_follow(struct scullc_dev *dev, int devnum)
{
	struct scullc_dev *ptr = dev;

	while(devnum--){
		if(NULL == (ptr->next)){
			ptr->next = kmalloc(sizeof(struct scullc_dev), GFP_KERNEL);
			if(NULL == (ptr->next)){
				SCULLC_DPRINT("%s %d: malloc failed\n", __func__, __LINE__);
				return NULL;
			}
			memset(ptr->next, 0, sizeof(struct scullc_dev));
			ptr = ptr->next;
			ptr->quantum = g_scullc_quantum;
			ptr->qset = g_scullc_qset;
			ptr->data = NULL;
			ptr->next = NULL;
			ptr->size = 0;
		}else{
			ptr = ptr->next;
		}		
	}
	return ptr;
}

void scullc_trim(struct scullc_dev *devices)
{
	struct scullc_dev *next;
	struct scullc_dev *ptr;
	int i;
	ptr = devices;
	while(NULL != ptr){
		if(NULL != ptr->data){
			SCULLC_DPRINT("%s %d: ptr->data = %p\n", __func__, __LINE__, ptr->data);
			for(i = 0; i < ptr->qset; i++){
				if(NULL != ptr->data[i]){
					SCULLC_DPRINT("%s %d: ptr->data[%d] = %p\n", __func__, __LINE__, i, ptr->data[i]);
				}
			}		
			for(i = 0; i < ptr->qset; i++){
				if(NULL != ptr->data[i]){
					SCULLC_DPRINT("%s %d: i= %d\n", __func__, __LINE__, i);
					kmem_cache_free(g_scullc_cache, ptr->data[i]);
					ptr->data[i] = NULL;
				}
			}
			kfree(ptr->data);
			ptr->data = NULL;
		}
		if(NULL == ptr->next){
			break;
		}

		next = ptr->next;
		if(ptr != devices){
			kfree(ptr);
			ptr = NULL;	
		}
		ptr = next;
	}
	devices->size = 0;
	devices->next = NULL;
	devices->qset = g_scullc_qset;
	devices->quantum = g_scullc_quantum;
}
void scullc_cleanup(void)
{
	int i;
	dev_t scullc_dev = MKDEV(g_scullc_major, SCULLC_BASE_MINOR);
	for(i = 0; i < g_scullc_dev_num; i++){
		cdev_del(&g_scullc_devices[i].cdevices);
		scullc_trim(g_scullc_devices + i);
		device_destroy(g_scullc_class,MKDEV(g_scullc_major, i));
	}
	class_destroy(g_scullc_class);

	if(NULL != g_scullc_devices){
		kfree(g_scullc_devices);
		g_scullc_devices = NULL;
	}
	if(NULL != g_scullc_cache){
		kmem_cache_destroy(g_scullc_cache);
		g_scullc_cache = NULL;
	}
	
	unregister_chrdev_region(scullc_dev, g_scullc_dev_num);		
}
void scullc_cdev_init(struct scullc_dev *devices, int major, int index)
{
	int ret;
	dev_t scullc_mkdev = MKDEV(major, index);
	cdev_init(&devices->cdevices, &g_scullc_fops);
	devices->cdevices.owner = THIS_MODULE;
	devices->cdevices.ops = &g_scullc_fops;
	ret = cdev_add(&devices->cdevices, scullc_mkdev, 1);
	if(0 != ret){
		SCULLC_DPRINT("cdev_add failed, ret = %d, major = %d, index = %d\n", ret, major, index);
	}
}
#ifdef SCULLC_USE_PROC
ssize_t scullc_read_proc(struct file *filp, char __user *data, size_t size, loff_t *f_ops)
{
	SCULLC_DPRINT("g_scullc_quantum = %d, g_scullc_qset = %d\n", g_scullc_quantum, g_scullc_qset);
	return 0;
}

#endif
static int scullc_module_init(void)
{
	int ret, i;
	dev_t scullc_dev = MKDEV(g_scullc_major, SCULLC_BASE_MINOR);
	SCULLC_DPRINT("scullc_module_init\n");
	if(0 == g_scullc_major){
		ret = alloc_chrdev_region(&scullc_dev, SCULLC_BASE_MINOR, g_scullc_dev_num, "scullc");
		g_scullc_major = MAJOR(scullc_dev);
	}else{
		ret = register_chrdev_region(scullc_dev, g_scullc_dev_num, "scullc");
	}
	if(0 != ret){
		SCULLC_DPRINT("%s %d: init failed\n", __func__, __LINE__);
		return ret;	
	}
	g_scullc_devices = kmalloc(sizeof(struct scullc_dev) * g_scullc_dev_num, GFP_KERNEL);
	if(NULL == g_scullc_devices){
		SCULLC_DPRINT("%s %d: kmalloc g_scullc_devices failed.\n", __func__, __LINE__);
		ret = -ENOMEM;
		goto malloc_failed;
	}
	g_scullc_class = class_create(THIS_MODULE, "scullc");
	if(IS_ERR(g_scullc_class)){
		kfree(g_scullc_devices);
		g_scullc_devices = NULL;
		goto malloc_failed;
	}
	for(i = 0; i < g_scullc_dev_num; i++){
		g_scullc_devices[i].quantum = g_scullc_quantum;
		g_scullc_devices[i].qset = g_scullc_qset;
		g_scullc_devices[i].data = NULL;
		g_scullc_devices[i].next = NULL;
		g_scullc_devices[i].size = 0;
		sema_init(&g_scullc_devices[i].sem, 1);
		scullc_cdev_init(g_scullc_devices + i, g_scullc_major, i);
		device_create(g_scullc_class, NULL, MKDEV(g_scullc_major, i), NULL, "scullc%d", i);
	}

#ifdef SCULLC_USE_PROC
	g_scullc_entry = proc_create( "scullc", 0x0644, NULL, &g_sullc_proc_fops);

#endif
	
	g_scullc_cache = kmem_cache_create("scullc", g_scullc_quantum, SLAB_HWCACHE_ALIGN, 0, NULL);	
	if(NULL == g_scullc_cache){
		SCULLC_DPRINT("%s %d: kmem_cache_create g_scullc_cache.\n", __func__, __LINE__);
		scullc_cleanup();
		return -ENOMEM;
	}
	
	SCULLC_DPRINT("%s %d: init success\n", __func__, __LINE__);
	return 0;
	
malloc_failed:
	unregister_chrdev_region(scullc_dev, g_scullc_dev_num);		
	return -1;
}
static void scullc_module_exit(void)
{
	SCULLC_DPRINT("scullc_module_exit\n");
#ifdef SCULLC_USE_PROC
	if(NULL != g_scullc_entry){
		remove_proc_entry("scullc", NULL);
	}
#endif
	scullc_cleanup();
	if(NULL != g_scullc_devices){
		kfree(g_scullc_devices);
		g_scullc_devices = NULL;
	}
	
}

module_init(scullc_module_init);
module_exit(scullc_module_exit);

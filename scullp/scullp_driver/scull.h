#ifndef __SCULLC_H__
#define __SCULLC_H__
#include <linux/semaphore.h>
#include <linux/cdev.h>
#include <linux/kernel.h>
#include <linux/wait.h>
#include <linux/sched.h>
#include <linux/spinlock.h>
#include <linux/spinlock_types.h>


#ifdef SCULL_DEBUG
#define SCULL_USE_PROC
	#ifdef __KERNEL__
		#define SCULL_DPRINT(fmt, args ...) 	printk(KERN_DEBUG "scullc:" fmt, ##args)
	#else
		#define SCULL_DPRINT(fmt, args ...) 	printf(stderr, "scullc:" fmt, ##args)
	#endif
#else
	#define SCULL_DPRINT(fmt, args ...)

#endif


#define  	SCULL_MAJOR				0
#define 	SCULL_BASE_MINOR			0
#define		SCULL_DEVS_NUM				4
#define 	SCULLP_SIZE					4096

#define SCULL_IOC_MAGIC							's'
#define SCULL_IOC_GET_QSET							_IO(SCULLC_IOC_MAGIC, 0)
#define SCULL_IOC_SET_QSET							_IO(SCULLC_IOC_MAGIC, 1)
#define SCULL_IOC_GET_QSET_PTR						_IOR(SCULLC_IOC_MAGIC, 2, int)
#define SCULL_IOC_SET_QSET_PTR						_IOW(SCULLC_IOC_MAGIC, 3, int)
#define SCULL_IOC_EX_QSET							_IO(SCULLC_IOC_MAGIC, 4)
#define SCULL_IOC_EX_QSET_PTR						_IOWR(SCULLC_IOC_MAGIC, 5, int)

#define SCULL_IOC_GET_QUANTUM						_IO(SCULLC_IOC_MAGIC, 6)
#define SCULL_IOC_SET_QUANTUM						_IO(SCULLC_IOC_MAGIC, 7)
#define SCULL_IOC_GET_QUANTUM_PTR					_IOR(SCULLC_IOC_MAGIC, 8, int)
#define SCULL_IOC_SET_QUANTUM_PTR					_IOW(SCULLC_IOC_MAGIC, 9, int)
#define SCULL_IOC_EX_QUANTUM						_IOW(SCULLC_IOC_MAGIC, 10, int)
#define SCULL_IOC_EX_QUANTUM_PTR					_IOW(SCULLC_IOC_MAGIC, 11, int)
#define SCULL_IOC_MAX								12


struct scullp_dev{
	char *buffer;
	char *end;
	char *r_pos;
	char *w_pos;
	wait_queue_head_t inq;
	wait_queue_head_t outq;
	int buffer_size;
	int nreaders;
	int nwriters;
	struct fasync_struct *scullp_async;
	struct semaphore sem;
	spinlock_t scullp_lock;
	struct cdev cdevices;
};

#endif

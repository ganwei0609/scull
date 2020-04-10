#ifndef __SCULLC_H__
#define __SCULLC_H__
#include <linux/semaphore.h>
#include <linux/cdev.h>
#include <linux/kernel.h>


#ifdef SCULLC_DEBUG
#define SCULLC_USE_PROC
	#ifdef __KERNEL__
		#define SCULLC_DPRINT(fmt, args ...) 	printk(KERN_DEBUG "scullc:" fmt, ##args)
	#else
		#define SCULLC_DPRINT(fmt, args ...) 	printf(stderr, "scullc:" fmt, ##args)
	#endif
#else
	#define SCULLC_DPRINT(fmt, args ...)

#endif


#define  	SCULLC_MAJOR				0
#define 	SCULLC_BASE_MINOR			0
#define		SCULLC_DEVS_NUM				4
#define 	SCULLC_QUANTUM				4096
#define 	SCULLC_QSET					2


#define SCULLC_IOC_MAGIC							's'
#define SCULLC_IOC_GET_QSET							_IO(SCULLC_IOC_MAGIC, 0)
#define SCULLC_IOC_SET_QSET							_IO(SCULLC_IOC_MAGIC, 1)
#define SCULLC_IOC_GET_QSET_PTR						_IOR(SCULLC_IOC_MAGIC, 2, int)
#define SCULLC_IOC_SET_QSET_PTR						_IOW(SCULLC_IOC_MAGIC, 3, int)
#define SCULLC_IOC_EX_QSET							_IO(SCULLC_IOC_MAGIC, 4)
#define SCULLC_IOC_EX_QSET_PTR						_IOWR(SCULLC_IOC_MAGIC, 5, int)

#define SCULLC_IOC_GET_QUANTUM						_IO(SCULLC_IOC_MAGIC, 6)
#define SCULLC_IOC_SET_QUANTUM						_IO(SCULLC_IOC_MAGIC, 7)
#define SCULLC_IOC_GET_QUANTUM_PTR					_IOR(SCULLC_IOC_MAGIC, 8, int)
#define SCULLC_IOC_SET_QUANTUM_PTR					_IOW(SCULLC_IOC_MAGIC, 9, int)
#define SCULLC_IOC_EX_QUANTUM						_IOW(SCULLC_IOC_MAGIC, 10, int)
#define SCULLC_IOC_EX_QUANTUM_PTR					_IOW(SCULLC_IOC_MAGIC, 11, int)
#define SCULLC_IOC_MAX								12


struct scullc_dev{
	void **data;
	struct scullc_dev *next;
	int quantum;
	int qset;
	size_t size;
	struct semaphore sem;
	struct cdev cdevices;
};

#endif

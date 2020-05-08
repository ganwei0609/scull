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
	#ifdef __KERNEL__
		#define SCULL_DPRINT(fmt, args ...) 	printk(KERN_DEBUG "scullc:" fmt, ##args)
	#else
		#define SCULL_DPRINT(fmt, args ...) 	printf(stderr, "scullc:" fmt, ##args)
	#endif
#else
	#define SCULL_DPRINT(fmt, args ...)

#endif

#define MAX_SEQ_NUM 	2

#endif

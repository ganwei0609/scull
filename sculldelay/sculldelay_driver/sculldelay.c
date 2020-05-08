#include <linux/init.h>  
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/types.h>
#include <linux/fs.h>
#include <linux/proc_fs.h>
#include <linux/slab.h>
#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/uaccess.h>
#include <linux/device.h>
#include <linux/time.h>
#include <linux/timer.h>
#include <linux/seq_file.h>
#include <linux/interrupt.h>
#include <linux/workqueue.h>

#include "scull.h"

struct proc_dir_entry *s_scull_jitbusy_entry = NULL;
struct proc_dir_entry *s_scull_jitsched_entry = NULL;
struct proc_dir_entry *s_scull_jitwaitevent_entry = NULL;
struct proc_dir_entry *s_scull_jittimer_entry = NULL;
struct proc_dir_entry *s_scull_jittasklet_entry = NULL;
struct proc_dir_entry *s_scull_jitworkqueue_entry = NULL;

int g_jit_delay = HZ;
int g_seq_num = 0;
module_param(g_jit_delay,int,0644);
MODULE_AUTHOR("ganwei");
MODULE_LICENSE("Dual BSD/GPL");


void *scull_seq_start(struct seq_file *m, loff_t *pos)
{
	loff_t *spos;
	SCULL_DPRINT("%s %d: \n", __func__, __LINE__);
	spos = kmalloc(GFP_KERNEL, sizeof(loff_t));
	*spos = *pos;
	return spos;
}
void scull_seq_stop(struct seq_file *m, void *v)
{
	SCULL_DPRINT("%s %d: v = %p\n", __func__, __LINE__, v);
	if(v != NULL){
		kfree(v);
	}
}
void *scull_seq_next(struct seq_file *m, void *v, loff_t *pos)
{
	loff_t *spos;
	SCULL_DPRINT("%s %d: \n", __func__, __LINE__);
	if(g_seq_num > MAX_SEQ_NUM){
		return NULL;
	}
	spos = (loff_t *)v;
	*spos = ++(*pos);
	return spos;
}
int scull_jitbusy_seq_show(struct seq_file *m, void *v)
{
	loff_t *spos;
	unsigned long jiffies_delay;
	unsigned long jiffies_current;
	SCULL_DPRINT("%s %d: \n", __func__, __LINE__);
	spos = (loff_t *)v;
	jiffies_current = jiffies;
	jiffies_delay = jiffies + g_jit_delay;
	while(time_before(jiffies, jiffies_delay)){
		cpu_relax();
	}
	seq_printf(m, "%s\n", "cpu_relax");
	seq_printf(m, "number = %ld\n", (unsigned long)*spos);
	seq_printf(m, "before delay : %ld, after delay : %ld\n", jiffies_current, jiffies);
	return 0;
}
int scull_jitwaitevent_seq_show(struct seq_file *m, void *v)
{
	loff_t *spos;
	int retval;
	unsigned long jiffies_delay;
	unsigned long jiffies_current;
	wait_queue_head_t seq_event_queue;
	SCULL_DPRINT("%s %d: \n", __func__, __LINE__);
	
	spos = (loff_t *)v;
	jiffies_current = jiffies;
	jiffies_delay = jiffies + g_jit_delay;
	init_waitqueue_head(&seq_event_queue);
	retval = wait_event_interruptible_timeout(seq_event_queue, 0, g_jit_delay);

	seq_printf(m, "retval = %d\n", retval);
	seq_printf(m, "number = %ld\n", (unsigned long)*spos);
	seq_printf(m, "before delay : %ld, after delay : %ld\n", jiffies_current, jiffies);
	return 0;
}
#define JITTIME_LOOP 		5
struct jittimer_struct{
	struct seq_file *file;
	struct timer_list seq_timer;
	wait_queue_head_t seq_wait;
	int loop;
	unsigned long prejiffies;
};
void jittimer_fn(unsigned long args)
{
	struct jittimer_struct *jittimer = (struct jittimer_struct *)args;
	SCULL_DPRINT("%s %d: jittimer = %p\n", __func__, __LINE__, jittimer);
	seq_printf(jittimer->file, "prejiffies = %ld, jiffies = %ld\n", jittimer->prejiffies, jiffies);

	if(jittimer->loop != 0){
		jittimer->loop--;
		jittimer->prejiffies = jiffies;
		jittimer->seq_timer.expires = jittimer->prejiffies + HZ;
		add_timer(&(jittimer->seq_timer));
	}else{
		wake_up_interruptible(&(jittimer->seq_wait));
	}	
}

int scull_jittimer_seq_show(struct seq_file *m, void *v)
{
	loff_t *spos;
	struct jittimer_struct *jittimer;
	int retval = 0;
	spos = (loff_t *)v;
	jittimer = kmalloc(GFP_KERNEL, sizeof(struct jittimer_struct));
	if(jittimer == NULL){
		SCULL_DPRINT("%s %d: malloc failed\n", __func__, __LINE__);
		return -ENOMEM;
	}
	jittimer->prejiffies = jiffies;
	jittimer->loop = 1;
	jittimer->file = m;
	init_waitqueue_head(&(jittimer->seq_wait));
	init_timer(&(jittimer->seq_timer));
	jittimer->seq_timer.function = jittimer_fn;
	jittimer->seq_timer.data = (unsigned long)jittimer;
	jittimer->seq_timer.expires = jittimer->prejiffies + HZ;
	SCULL_DPRINT("%s %d: jittimer = %p\n", __func__, __LINE__, jittimer);
	add_timer(&(jittimer->seq_timer));

	retval = wait_event_interruptible(jittimer->seq_wait, jittimer->loop == 0);
	if(signal_pending(current)){
		return -ERESTARTSYS;	
	}
	del_timer(&(jittimer->seq_timer));
	seq_printf(jittimer->file, "%s %d: *spos = %ld\n", __func__, __LINE__, (unsigned long)*spos);
	kfree(jittimer);
	return 0;
}
#define JITTASKLET_LOOP 		5
struct jittasklet_struct{
	struct seq_file *file;
	struct tasklet_struct seq_tasklet;
	wait_queue_head_t seq_wait;
	int loop;
	unsigned long prejiffies;
};

void jittasklet_fn(unsigned long args)
{
	struct jittasklet_struct *jittasklet = (struct jittasklet_struct *)args;
	SCULL_DPRINT("%s %d: jittasklet = %p\n", __func__, __LINE__, jittasklet);
	seq_printf(jittasklet->file, "prejiffies = %ld, jiffies = %ld\n", jittasklet->prejiffies, jiffies);
	jittasklet->prejiffies = jiffies;
	if(jittasklet->loop != 0){
		jittasklet->loop--;
		tasklet_schedule(&(jittasklet->seq_tasklet));
	}else{
		wake_up_interruptible(&(jittasklet->seq_wait));
	}	
}

int scull_jittasklet_seq_show(struct seq_file *m, void *v)
{
	struct jittasklet_struct *jittasklet;
	loff_t *spos;
	int retval = 0;
	spos = (loff_t *)v;
	jittasklet = kmalloc(GFP_KERNEL, sizeof(struct jittasklet_struct));
	if(jittasklet == NULL){
		SCULL_DPRINT("%s %d: malloc failed\n", __func__, __LINE__);
		return -ENOMEM;
	}
	jittasklet->loop = 1;	
	jittasklet->prejiffies = jiffies;
	jittasklet->file = m;
	init_waitqueue_head(&(jittasklet->seq_wait));
	tasklet_init(&(jittasklet->seq_tasklet), jittasklet_fn, (unsigned long)jittasklet);
	tasklet_schedule(&(jittasklet->seq_tasklet));
	retval = wait_event_interruptible(jittasklet->seq_wait, jittasklet->loop == 0);
	if(signal_pending(current)){
		return -ERESTARTSYS;	
	}
	tasklet_kill(&(jittasklet->seq_tasklet));
	seq_printf(jittasklet->file, "%s %d: *spos = %ld\n", __func__, __LINE__, (unsigned long)*spos);
	kfree(jittasklet);	
	return 0;
}
#define JITWORKQUEUE_LOOP 		5
struct jitworkqueue_struct{
	struct seq_file *file;
	struct work_struct seq_work;
	struct delayed_work seq_delayed_work;
	struct workqueue_struct *seq_workqueue;
	wait_queue_head_t seq_wait;
	int loop;
	unsigned long prejiffies;
};

void jitworkqueue_fn(struct delayed_work *delayed_work)
{
	struct jitworkqueue_struct *jitworkqueue = container_of(delayed_work, struct jitworkqueue_struct, seq_delayed_work);
	SCULL_DPRINT("%s %d: jitworkqueue = %p\n", __func__, __LINE__, jitworkqueue);
	seq_printf(jitworkqueue->file, "prejiffies = %ld, jiffies = %ld\n", jitworkqueue->prejiffies, jiffies);
	jitworkqueue->prejiffies = jiffies;
	if(jitworkqueue->loop != 0){
		jitworkqueue->loop--;
		queue_delayed_work(jitworkqueue->seq_workqueue, &(jitworkqueue->seq_delayed_work), 2 * HZ);
	}else{
		wake_up_interruptible(&(jitworkqueue->seq_wait));
	}	
}

int scull_jitworkqueue_seq_show(struct seq_file *m, void *v)
{
	struct jitworkqueue_struct *jitworkqueue;
	loff_t *spos;
	int retval = 0;
	spos = (loff_t *)v;
	jitworkqueue = kmalloc(GFP_KERNEL, sizeof(struct jitworkqueue_struct));
	if(jitworkqueue == NULL){
		SCULL_DPRINT("%s %d: malloc failed\n", __func__, __LINE__);
		return -ENOMEM;
	}
	SCULL_DPRINT("%s %d: jitworkqueue = %p\n", __func__, __LINE__, jitworkqueue);	
	jitworkqueue->loop = 1;	
	jitworkqueue->prejiffies = jiffies;
	jitworkqueue->file = m;
	init_waitqueue_head(&(jitworkqueue->seq_wait));
	jitworkqueue->seq_workqueue = create_workqueue("jitworkqueue");
	INIT_DELAYED_WORK(&(jitworkqueue->seq_delayed_work), jitworkqueue_fn);
	queue_delayed_work(jitworkqueue->seq_workqueue, &(jitworkqueue->seq_delayed_work), 2 * HZ);
	retval = wait_event_interruptible(jitworkqueue->seq_wait, jitworkqueue->loop == 0);
	if(signal_pending(current)){
		return -ERESTARTSYS;	
	}
	destroy_workqueue(jitworkqueue->seq_workqueue);
	seq_printf(jitworkqueue->file, "%s %d: *spos = %ld\n", __func__, __LINE__, (unsigned long)*spos);
	kfree(jitworkqueue);	
	return 0;
}

int scull_jitsched_seq_show(struct seq_file *m, void *v)
{
	loff_t *spos;
	unsigned long jiffies_current;
	SCULL_DPRINT("%s %d: \n", __func__, __LINE__);

	spos = (loff_t *)v;
	jiffies_current = jiffies;
	schedule_timeout(g_jit_delay);
	seq_printf(m, "%s\n", "schedule");
	seq_printf(m, "number = %ld\n", (unsigned long)*spos);
	seq_printf(m, "before delay : %ld, after delay : %ld\n", jiffies_current, jiffies);
	return 0;
}


#define BUILD_JIT_SEQ_OPS(type)  \
	const struct seq_operations g_sull_##type##_seq_fops = {\
		.start = scull_seq_start,\
		.stop = scull_seq_stop,\
		.next = scull_seq_next,\
		.show = scull_##type##_seq_show,\
	};

BUILD_JIT_SEQ_OPS(jitbusy)
	BUILD_JIT_SEQ_OPS(jitsched)
	BUILD_JIT_SEQ_OPS(jitwaitevent)
	BUILD_JIT_SEQ_OPS(jittimer)
	BUILD_JIT_SEQ_OPS(jittasklet)
	BUILD_JIT_SEQ_OPS(jitworkqueue)


#define BUILD_JIT_PROC_OPEN(type) \
	int scull_##type##_proc_open(struct inode *inode, struct file *filp)\
	{\
		return seq_open(filp, &g_sull_##type##_seq_fops);\
	}

BUILD_JIT_PROC_OPEN(jitbusy)
	BUILD_JIT_PROC_OPEN(jitsched)
	BUILD_JIT_PROC_OPEN(jitwaitevent)
	BUILD_JIT_PROC_OPEN(jittimer)
	BUILD_JIT_PROC_OPEN(jittasklet)
	BUILD_JIT_PROC_OPEN(jitworkqueue)
	
static int scull_proc_release(struct inode *inode, struct file *file)
{
	return seq_release(inode, file);
}

#define BUILD_JIT_PROC_OPS(type)  \
	const struct file_operations g_sull_##type##_proc_fops = { \
	 	.owner  = THIS_MODULE,\
		.open = scull_##type##_proc_open,\
		.read = seq_read,\
		.llseek = seq_lseek,\
		.release = scull_proc_release,\
	};\


BUILD_JIT_PROC_OPS(jitbusy)
	BUILD_JIT_PROC_OPS(jitsched)
	BUILD_JIT_PROC_OPS(jitwaitevent)
	BUILD_JIT_PROC_OPS(jittimer)
	BUILD_JIT_PROC_OPS(jittasklet)
	BUILD_JIT_PROC_OPS(jitworkqueue)

static int scull_jit_module_init(void)
{

	s_scull_jitbusy_entry = proc_create("sculljitbusy", 0x0644, NULL, &g_sull_jitbusy_proc_fops);
	s_scull_jitsched_entry = proc_create("sculljitsched", 0x0644, NULL, &g_sull_jitsched_proc_fops);
	s_scull_jitwaitevent_entry = proc_create("sculljitwaitevent", 0x0644, NULL, &g_sull_jitwaitevent_proc_fops);
	s_scull_jittimer_entry = proc_create("sculljittimer", 0x0644, NULL, &g_sull_jittimer_proc_fops);
	s_scull_jittasklet_entry = proc_create("sculljittasklet", 0x0644, NULL, &g_sull_jittasklet_proc_fops);
	s_scull_jitworkqueue_entry = proc_create("sculljitworkqueue", 0x0644, NULL, &g_sull_jitworkqueue_proc_fops);
	SCULL_DPRINT("%s %d: module init success\n", __func__, __LINE__);
	return 0;
}
static void scull_jit_module_exit(void)
{
	SCULL_DPRINT("%s %d: module exit\n", __func__, __LINE__);
	if(NULL != s_scull_jitbusy_entry){
		remove_proc_entry("sculljitbusy", NULL);
	}
	if(NULL != s_scull_jitsched_entry){
		remove_proc_entry("sculljitsched", NULL);
	}
	if(NULL != s_scull_jitwaitevent_entry){
		remove_proc_entry("sculljitwaitevent", NULL);
	}
	if(NULL != s_scull_jittimer_entry){
		remove_proc_entry("sculljittimer", NULL);
	}
	if(NULL != s_scull_jittasklet_entry){
		remove_proc_entry("sculljittasklet", NULL);
	}
	if(NULL != s_scull_jitworkqueue_entry){
		remove_proc_entry("sculljitworkqueue", NULL);
	}
}

module_init(scull_jit_module_init);
module_exit(scull_jit_module_exit);

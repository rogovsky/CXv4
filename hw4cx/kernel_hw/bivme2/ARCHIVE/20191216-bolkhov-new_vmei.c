#include <linux/module.h>
#include <linux/config.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/ioport.h>
#include <linux/timer.h>
#include <linux/poll.h>
#include <linux/errno.h>
#include <linux/mm.h>
#include <linux/slab.h>

#include <asm/system.h>
#include <asm/uaccess.h>
#include <asm/io.h>
#include <asm/irq.h>

#include <asm/8xx_immap.h>
#include <asm/mpc8xx.h>

#define VME_MAJOR 21

#define VME_CHIP_ADDR 0xfff40000
#define VME_CHIP_SPACE 0x100

#define DRV_NAME "VMEI"

#define DEBUG

#ifdef DEBUG
#define DP(x) x
#else
#define DP(x)
#endif

#define N_IRQ	8		/* 7 irqs + AC fail */

enum
{
    Q_SIZE = 256, // In fact 255=256-1, "-1" because of head/tail logic
};

typedef struct {
    char         q_isin[256];
    unsigned int q_buff[Q_SIZE];
    int          q_head;
    int          q_tail;

    wait_queue_head_t wait;
    int opened;
} vme_data_t;

vme_data_t vd[N_IRQ];

static volatile unsigned 
    *reg0, *reg1, *reg2, *vme_chip;

static spinlock_t vmelock = SPIN_LOCK_UNLOCKED;

static void 
vme_interrupt(int irq, void *dev_id, struct pt_regs *regs)
{
    unsigned long flags;
    unsigned int cur_mask, active_mask, *inth;
    unsigned int irql;
    unsigned int vect;

    DP(printk("vme_interrupt\n"));
    spin_lock_irqsave(&vmelock, flags);

    irql = *reg2 & 0x07;
    cur_mask = 0x01 << irql;

    active_mask = *reg1 & cur_mask & 0xff;

    if (active_mask) {
	*reg1 &= ~active_mask;

	if (active_mask != 0x01) {
	    inth = (unsigned int *)((char *)vme_chip + 0x100 + 4 * irql);
	    vect = (*inth) & 0xFF;
	    //vd[irql].vector = *inth;
	    if (*reg2 & 0x20) {
		/* bus error */
	        *reg2 = 0x20;
		////vd[irql].vector = -1;
	    }
	    else if (vd[irql].q_isin[vect] == 0) {
	        if (((vd[irql].q_head + 1) & 0xFF) == vd[irql].q_tail) {
	            // Ouch... What can we do?
	        }
	        else {
	            // Store vector in a queue cell
	            vd[irql].q_buff[vd[irql].q_head] = vect;
	            // Promote queue head index
	            vd[irql].q_head = (vd[irql].q_head + 1) & 0xFF;
	            // Mark this vector as "is in the queue"
	            vd[irql].q_isin[vect] = 1;
	        }
	    }
	}
	else {
	    ////vd[irql].vector = 0xac;	/* ac fail */
	}
	wake_up_interruptible(&vd[irql].wait);
    }

    spin_unlock_irqrestore(&vmelock, flags);
}

static ssize_t vme_read(struct file *file, char *buf,
                           size_t count, loff_t *ppos)
{
    unsigned long flags;
    ssize_t       ret;
    unsigned int  vect;

    int minor = MINOR(file->f_dentry->d_inode->i_rdev);

    DP(printk("vmei read, minor %d\n", minor));

    if (buf == NULL) return -EINVAL;
    if (count < sizeof(int)) return -EINVAL;
    if (verify_area(VERIFY_WRITE, buf, sizeof(int)))
	return -EFAULT;

//    memcpy(buf, &vd[minor].vector, sizeof(int));

    spin_lock_irqsave     (&vmelock, flags);
    if (vd[minor].q_tail == vd[minor].q_head)
        ret = -EWOULDBLOCK;
    else {
        // Read vector from queue
        vect = vd[minor].q_buff[vd[minor].q_tail];
        // Promote queue tail index
        vd[minor].q_tail = (vd[minor].q_tail + 1) & 0xFF;
        // Drop this vector's "is in the queue" flag
        vd[minor].q_isin[vect] = 0;
        // Store vector in callers' buffer
        memcpy(buf, &vect, sizeof(int));
        // 
        ret = sizeof(int);
    }
    spin_unlock_irqrestore(&vmelock, flags);

    return ret;
}

static ssize_t vme_write(struct file * file, const char * buf, 
			 size_t count, loff_t *ppos)
{
    return 0;
}

static int vme_open (struct inode *inode, struct file *file)
{
    int minor = MINOR(file->f_dentry->d_inode->i_rdev);

    DP(printk("vmei open, minor %d\n", minor));

    if ((minor < 0) || (minor >= 8)) 
	return -EBADF;

    if (vd[minor].opened) return -EBUSY;
    vd[minor].opened++;
    init_waitqueue_head(&vd[minor].wait);
    MOD_INC_USE_COUNT;
    
    return 0;
}

static int vme_release(struct inode *inode, struct file *file)
{
    int minor = MINOR(file->f_dentry->d_inode->i_rdev);

    DP(printk("vmei release, %d\n", minor));

    if ((minor < 0) || (minor >= 8)) 
	return -EBADF;

    MOD_DEC_USE_COUNT;
    vd[minor].opened--;

    return 0;
}
static unsigned int 
vme_poll(struct file *file, poll_table *wait)
{
    int minor = MINOR(file->f_dentry->d_inode->i_rdev);
    int mask;
    unsigned long flags;
    unsigned int ret;
    
    mask = 1 << minor;

    DP(printk("vmei poll, minor %d, mask %x\n", minor, mask));

    spin_lock_irqsave(&vmelock, flags);
    *reg1 |= mask;
    poll_wait(file, &vd[minor].wait, wait);
    ret = (vd[minor].q_tail == vd[minor].q_head)? 0 : POLLPRI;
    spin_unlock_irqrestore(&vmelock, flags);

    DP(printk("poll wait completed\n"));
    
    return ret;
}

static struct file_operations char_fops = {
	.read		= vme_read,
	.write		= vme_write,
	.open		= vme_open,
	.poll		= vme_poll,
	.release	= vme_release,
};

static int init_cs(void)
{
    volatile immap_t *im = (immap_t *)IMAP_ADDR;

    im->im_memctl.memc_or7 = 0xffff0108; /* external TA, 64k space */
    im->im_memctl.memc_br7 = VME_CHIP_ADDR | 0x4001;

    if (!request_mem_region(VME_CHIP_ADDR, VME_CHIP_SPACE, "CAMAC driver")) {
    	printk(KERN_ERR "Can't request memory region\n");
	return -EBUSY;
    }

    vme_chip = (unsigned *)ioremap_nocache(VME_CHIP_ADDR, VME_CHIP_SPACE);
    if (!vme_chip) {
       printk(KERN_ERR "unable to ioremap() memory \n");
       release_mem_region(VME_CHIP_ADDR, VME_CHIP_SPACE);
       return -EINVAL;
    }

    reg0 = &vme_chip[0];
    reg1 = &vme_chip[1];
    reg2 = &vme_chip[2];
/*
printk("reg0 = %x, reg1 = %x, reg2 = %x\n", *reg0, *reg1, *reg2);
*/
    *reg1 |= 0x7e;

    return 0;
}
int init_module(void)
{
    init_cs();

    memset(vd, 0, N_IRQ * sizeof(vme_data_t));

    if (register_chrdev (VME_MAJOR, "VME bus", &char_fops)) {
	printk ("vmei: Cannot get major %d.\n", VME_MAJOR);
	return -1;
    }

    if (request_irq(SIU_IRQ1, vme_interrupt, SA_SHIRQ, "VME", NULL) != 0) {
	unregister_chrdev (VME_MAJOR, "VME bus");
	iounmap((void *)vme_chip);
	release_mem_region(VME_CHIP_ADDR, VME_CHIP_SPACE);
	printk("Can't not allocate IRQ!\n");
	return -EINVAL;
    }

    printk("BIVME2 driver, V.R.Mamkin, 2005, release 1.1\n");
    return 0;
}
void cleanup_module (void)
{ 
    ((immap_t *)IMAP_ADDR)->im_siu_conf.sc_siel &=
	~(0x80000000 >> SIU_IRQ1);

    free_irq(SIU_IRQ1, 0);

    unregister_chrdev (VME_MAJOR, "VME bus");

    iounmap((void *)vme_chip);
    release_mem_region(VME_CHIP_ADDR, VME_CHIP_SPACE);

    printk("BIVME2 driver unloaded\n");
}
MODULE_AUTHOR("Vitaly Mamkin");
MODULE_DESCRIPTION("BIVME2 interrupts driver");
MODULE_LICENSE("GPL");

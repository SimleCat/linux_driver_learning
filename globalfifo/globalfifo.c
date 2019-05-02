#include <linux/init.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/types.h>
#include <linux/mutex.h>
#include <linux/poll.h>
#include <linux/sched.h>

#define GLOBALFIFO_SIZE		0x1000
#define MEM_CLEAR		0x01
#define GLOBALFIFO_MAJOR	231

static int gfifo_major = GLOBALFIFO_MAJOR;

module_param(gfifo_major, int, S_IRUGO);

struct gfifo_dev {
	struct cdev cdev;
	unsigned int cur_len;
	u8 mem[GLOBALFIFO_SIZE];
	struct mutex mutex;
	wait_queue_head_t r_wait;
	wait_queue_head_t w_wait;
	struct fasync_struct *async_queue;
};


struct gfifo_dev *gfifo_devp;

static int gfifo_open(struct inode *inode, struct file *filp)
{
	filp->private_data = gfifo_devp;
	return 0;
}

static ssize_t gfifo_read(struct file *filp, char __user *buf, size_t size, loff_t *ppos)
{
	size_t count = size;
	int ret = 0;
	struct gfifo_dev *dev = filp->private_data;

	DECLARE_WAITQUEUE(wait, current);

	mutex_lock(&dev->mutex);
	add_wait_queue(&dev->r_wait, &wait);

	while (dev->cur_len == 0) {
		if (filp->f_flags & O_NONBLOCK) {
			ret = -EFAULT;
			goto out;
		}

		__set_current_state(TASK_INTERRUPTIBLE);
		mutex_unlock(&dev->mutex);

		schedule();
		if (signal_pending(current)) {
			ret = -ERESTARTSYS;
			goto out2;
		}

		mutex_lock(&dev->mutex);
	}

	if (count > dev->cur_len)
		count = dev->cur_len;


	if (copy_to_user(buf, dev->mem, count)) {
		ret = -EFAULT;
	} else {
		memcpy(dev->mem, dev->mem+count, dev->cur_len - count);
		dev->cur_len -= count;
		ret = count;

		printk(KERN_INFO "read %u bytes, current len: %u\n", count, dev->cur_len);
		wake_up_interruptible(&dev->w_wait);

		if (dev->async_queue) {
			kill_fasync(&dev->async_queue, SIGIO, POLL_OUT);
			printk(KERN_DEBUG "%s kill SIGIO\n", __func__);
		}
	}

out:
	mutex_unlock(&dev->mutex);
out2:
	remove_wait_queue(&dev->r_wait, &wait);
	set_current_state(TASK_RUNNING);

	return ret;
}


static ssize_t gfifo_write(struct file *filp, const char __user *buf, size_t size, loff_t *ppos)
{
	unsigned int count = size;
	int ret = 0;
	struct gfifo_dev *dev = filp->private_data;
	DECLARE_WAITQUEUE(wait, current);

	mutex_lock(&dev->mutex);
	add_wait_queue(&dev->w_wait, &wait);

	while (dev->cur_len >= GLOBALFIFO_SIZE) {
		if (filp->f_flags & O_NONBLOCK) {
			ret = -EAGAIN;
			goto out;
		}
		__set_current_state(TASK_INTERRUPTIBLE);

		mutex_unlock(&dev->mutex);

		schedule();
		if (signal_pending(current)) {
			ret = -ERESTARTSYS;
			goto out2;
		}
		mutex_lock(&dev->mutex);
	}

	if (count > GLOBALFIFO_SIZE - dev->cur_len)
		count = GLOBALFIFO_SIZE - dev->cur_len;


	if (copy_from_user(dev->mem + dev->cur_len, buf, count)) {
		ret = -EFAULT;
	} else {
		dev->cur_len += count;
		ret = count;

		printk(KERN_INFO "written %u bytes, current len: %u\n", count, dev->cur_len);
		wake_up_interruptible(&dev->r_wait);

		if (dev->async_queue) {
			kill_fasync(&dev->async_queue, SIGIO, POLL_IN);
			printk(KERN_DEBUG "%s kill SIGIO\n", __func__);
		}
	}
out:
	mutex_unlock(&dev->mutex);
out2:
	remove_wait_queue(&dev->w_wait, &wait);
	set_current_state(TASK_RUNNING);

	return ret;
}


static unsigned int gfifo_poll(struct file *filp, poll_table *wait)
{
	unsigned int mask = 0;
	struct gfifo_dev *dev = filp->private_data;

	mutex_lock(&dev->mutex);

	poll_wait(filp, &dev->r_wait, wait);
	poll_wait(filp, &dev->w_wait, wait);

	if (dev->cur_len != 0) {
		mask |= POLLIN | POLLRDNORM;
	}

	if (dev->cur_len != GLOBALFIFO_SIZE) {
		mask |= POLLOUT | POLLWRNORM;
	}


	mutex_unlock(&dev->mutex);

	return mask;
}

static int gfifo_fasync(int fd, struct file *filp, int mode)
{
	struct gfifo_dev *dev = filp->private_data;
	return fasync_helper(fd, filp, mode, &dev->async_queue);
}

static int gfifo_release(struct inode *inode, struct file *filp)
{
	gfifo_fasync(-1, filp, 0);
	return 0;
}


static const struct file_operations gfifo_fops = {
	.owner = THIS_MODULE,
	.open = gfifo_open,
	.read = gfifo_read,
	.write = gfifo_write,
	.release = gfifo_release,
	.poll = gfifo_poll,
	.fasync = gfifo_fasync,
};


static void gfifo_setup_cdev(struct gfifo_dev *dev, int index)
{
	int err, devno = MKDEV(gfifo_major, index);

	cdev_init(&dev->cdev, &gfifo_fops);
	dev->cdev.owner = THIS_MODULE;
	err = cdev_add(&dev->cdev, devno, 1);
	if (err)
		printk(KERN_NOTICE "Error %d adding globalmem%d", err, index);
}


static int __init gfifo_init(void)
{
	int ret;
	dev_t devno = MKDEV(gfifo_major, 0);

	if (gfifo_major)
		ret = register_chrdev_region(devno, 1, "globalmem");
	else {
		ret = alloc_chrdev_region(&devno, 0, 1, "globalmem");
		gfifo_major = MAJOR(devno);
	}

	if (ret < 0)
		return ret;
	
	gfifo_devp = kzalloc(sizeof(struct gfifo_dev), GFP_KERNEL);
	if (!gfifo_devp) {
		ret = -ENOMEM;
		goto fail_malloc;
	}

	mutex_init(&gfifo_devp->mutex);
	init_waitqueue_head(&gfifo_devp->r_wait);
	init_waitqueue_head(&gfifo_devp->w_wait);

	gfifo_setup_cdev(gfifo_devp, 0);
	return 0;

fail_malloc:	
	unregister_chrdev_region(devno, 1);
	return ret;
}


static void __exit gfifo_exit(void)
{
	cdev_del(&gfifo_devp->cdev);
	kfree(gfifo_devp);
	unregister_chrdev_region(MKDEV(gfifo_major, 0), 1);
}

module_init(gfifo_init);
module_exit(gfifo_exit);

MODULE_AUTHOR("Simel Li <mengwuhupan@163.com>");
MODULE_LICENSE("GPL v2");





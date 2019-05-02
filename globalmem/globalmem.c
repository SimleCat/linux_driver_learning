#include <linux/init.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/types.h>


#define GLOBALMEM_SIZE		0x1000
#define MEM_CLEAR		0x01
#define GLOBALMEM_MAJOR		230

static int gm_major = GLOBALMEM_MAJOR;

module_param(gm_major, int, S_IRUGO);

struct gm_dev {
	struct cdev cdev;
	u8 mem[GLOBALMEM_SIZE];
};


struct gm_dev *gm_devp;

static int gm_open(struct inode *inode, struct file *filp)
{
	filp->private_data = gm_devp;
	return 0;
}

static ssize_t gm_read(struct file *filp, char __user *buf, size_t size, loff_t *ppos)
{
	unsigned long pos = *ppos;
	unsigned int count = size;
	int ret = 0;
	struct gm_dev *dev = filp->private_data;

	if (pos > GLOBALMEM_SIZE)
		return 0;
	if (count > GLOBALMEM_SIZE - pos)
		count = GLOBALMEM_SIZE - pos;

	if (copy_to_user(buf, dev->mem+pos, count)) {
		ret = -EFAULT;
	} else {
		*ppos += count;
		ret = count;

		printk(KERN_INFO "read %u bytes from %lu\n", count, pos);
	}

	return ret;
}


static ssize_t gm_write(struct file *filp, const char __user *buf, size_t size, loff_t *ppos)
{
	unsigned long pos = *ppos;
	unsigned int count = size;
	int ret = 0;
	struct gm_dev *dev = filp->private_data;

	if (pos > GLOBALMEM_SIZE)
		return 0;
	if (count > GLOBALMEM_SIZE - pos)
		count = GLOBALMEM_SIZE - pos;

	if (copy_from_user(dev->mem + pos, buf, count)) {
		ret = -EFAULT;
	} else {
		*ppos += count;
		ret = count;

		printk(KERN_INFO "written %u bytes to %lu\n", count, pos);
	}

	return ret;
}

static loff_t gm_llseek(struct file *filp, loff_t offset, int orig)
{
	loff_t ret = 0;
	switch(orig) {
	case 0:
		if (offset < 0) {
			ret = -EINVAL;
		} else if ((unsigned int)offset > GLOBALMEM_SIZE) {
			ret = -EINVAL;
		} else {
			filp->f_pos = (unsigned int)offset;
			ret = filp->f_pos;
		}
		break;
	case 1:
		if (filp->f_pos + offset > GLOBALMEM_SIZE) {
			ret = -EINVAL;
		} else if (filp->f_pos + offset < 0) {
			ret = -EINVAL;
		} else {
			filp->f_pos += offset;
			ret = filp->f_pos;
		}
		break;
	default:
		ret = -EINVAL;
		break;
	}

	return ret;
}

static long gm_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	struct gm_dev *dev = filp->private_data;

	switch(cmd) {
	case MEM_CLEAR:
		memset(dev->mem, 0, GLOBALMEM_SIZE);
		printk(KERN_INFO "globalmem wes set to zero.\n");
		break;
	default:
		return -EINVAL;	
	}
	return 0;
}

static int gm_release(struct inode *inode, struct file *filp)
{
	return 0;
}


static const struct file_operations gm_fops = {
	.owner = THIS_MODULE,
	.open = gm_open,
	.read = gm_read,
	.write = gm_write,
	.llseek = gm_llseek,
	.unlocked_ioctl = gm_ioctl,
	.release = gm_release,
};


static void gm_setup_cdev(struct gm_dev *dev, int index)
{
	int err, devno = MKDEV(gm_major, index);

	cdev_init(&dev->cdev, &gm_fops);
	dev->cdev.owner = THIS_MODULE;
	err = cdev_add(&dev->cdev, devno, 1);
	if (err)
		printk(KERN_NOTICE "Error %d adding globalmem%d", err, index);
}


static int __init gm_init(void)
{
	int ret;
	dev_t devno = MKDEV(gm_major, 0);

	if (gm_major)
		ret = register_chrdev_region(devno, 1, "globalmem");
	else {
		ret = alloc_chrdev_region(&devno, 0, 1, "globalmem");
		gm_major = MAJOR(devno);
	}

	if (ret < 0)
		return ret;
	
	gm_devp = kzalloc(sizeof(struct gm_dev), GFP_KERNEL);
	if (!gm_devp) {
		ret = -ENOMEM;
		goto fail_malloc;
	}

	gm_setup_cdev(gm_devp, 0);
	return 0;

fail_malloc:	
	unregister_chrdev_region(devno, 1);
	return ret;
}


static void __exit gm_exit(void)
{
	cdev_del(&gm_devp->cdev);
	kfree(gm_devp);
	unregister_chrdev_region(MKDEV(gm_major, 0), 1);
}

module_init(gm_init);
module_exit(gm_exit);

MODULE_AUTHOR("Simel Li <mengwuhupan@163.com>");
MODULE_LICENSE("GPL v2");





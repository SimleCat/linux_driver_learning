/*
 * a simple kernel module: hello
 *
 * Copyright (c) 2019 Simle Li <mengwuhupan@163.com>
 *
 * Licensed under GPLv2 or later.
 */

#include <linux/init.h>
#include <linux/module.h>

static int __init hello_init(void)
{
	printk(KERN_INFO "Hello world!\n");
	return 0;
}

static void __exit hello_exit(void)
{
	printk(KERN_INFO "Goodbye!\n");
}


module_init(hello_init);
module_exit(hello_exit);

MODULE_AUTHOR("Simle Li <mengwuhupan@163.com>");
MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("A simple Hello Module");
MODULE_ALIAS("a hello module");


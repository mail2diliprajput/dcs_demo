#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>

int __init kmod_init(void){
    printk(KERN_INFO "hello world\n");
    return 0;
}

void __exit kmod_exit(void){
}

module_init(kmod_init);
module_exit(kmod_exit);

MODULE_LICENSE("MIT");
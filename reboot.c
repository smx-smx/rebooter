/**
  * Copyright (C) 2019 Stefano Moioli <smxdev4@gmail.com>
  **/
#include <linux/init.h>			// Macros used to mark up functions e.g. __init __exit
#include <linux/module.h>		// Core header for loading LKMs into the kernel
#include <linux/kobject.h>
#include <linux/kernel.h>		// Contains types, macros, functions for the kernel
#include <linux/fs.h>			// Header for the Linux file system support
#include <asm/uaccess.h>		// Required for the copy to user function
#include <linux/slab.h>			// kmalloc, kfree
#include <linux/log2.h>
#include <linux/cpu.h>
#include <asm/io.h>
#include <asm/page.h>
#include <linux/smp.h>
#include <linux/kallsyms.h>
#include <linux/buffer_head.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Stefano Moioli");
MODULE_DESCRIPTION("Kernel Rebooter");

#define BOOT_FILE "/tmp/boot.bin"

struct file *file_open(const char *path, int flags, int rights) {
    struct file *filp = NULL;
    mm_segment_t oldfs;
    int err = 0;

    oldfs = get_fs();
    set_fs(get_ds());
    filp = filp_open(path, flags, rights);
    set_fs(oldfs);
    if (IS_ERR(filp)) {
        err = PTR_ERR(filp);
        return NULL;
    }
    return filp;
}

void file_close(struct file *file) {
    filp_close(file, NULL);
}

int file_read(struct file *file, unsigned long long offset, unsigned char *data, unsigned int size) {
    mm_segment_t oldfs;
    int ret;

    oldfs = get_fs();
    set_fs(get_ds());

    ret = vfs_read(file, data, size, &offset);

    set_fs(oldfs);
    return ret;
}

static void (*soft_restart)(unsigned long addr) = NULL;

static int resolve_syms(void){
    soft_restart = (void *)kallsyms_lookup_name("soft_restart");
    if(soft_restart == NULL)
        return -1;
    return 0;
}

static void *pCodePhys;

static int load_code(void){
    struct kstat stat;
    struct file *f = file_open(BOOT_FILE, O_RDONLY, 0);
    mm_segment_t oldfs;
    int err = 0;
    int i, numPages;
    void *codeVirt;
    struct page *codePages;

    if(f == NULL){
        return -1;
    }

    oldfs = get_fs();
    set_fs(get_ds());
    err = vfs_stat(BOOT_FILE, &stat);
    set_fs(oldfs);

    printk(KERN_INFO "Reading %lld bytes\n", stat.size);

    numPages = ilog2(stat.size / PAGE_SIZE);
    if(stat.size % PAGE_SIZE){
        numPages++;
    }
    printk(KERN_INFO "NumPages: %d\n", numPages);

    codeVirt = kzalloc(stat.size, GFP_KERNEL);
    if(codeVirt == NULL){
        printk(KERN_INFO "kzalloc failed");
        return 1;
    }

    codePages = virt_to_page(codeVirt);
    set_page_private(codePages, numPages);
    for(i = 0; i < numPages; i ++) {
        SetPageReserved(&codePages[i]);
    }

    err = file_read(f, 0, codeVirt, stat.size);
    printk(KERN_INFO "%d\n", err);
    if(IS_ERR(f)){
        printk(KERN_INFO "Reading failed\n");
    }
    file_close(f);

    pCodePhys = (void *)virt_to_phys(codeVirt);
    printk(KERN_INFO "flush\n");
    flush_icache_range(
        (unsigned long) pCodePhys,
	    (unsigned long) pCodePhys + stat.size
    );

    return 0;
}

static void do_reboot(void){

    printk(KERN_INFO "stopping secondary core\n");
    cpu_down(1);

    BUG_ON(num_online_cpus() > 1);

    printk(KERN_INFO "Bye!\n");
    soft_restart((unsigned long)pCodePhys);
}


static int __init rebooter_init(void){
    if(resolve_syms() != 0){
        printk(KERN_INFO "resolve_syms() failed\n");
        return 1;
    }
    if(load_code() != 0){
        printk(KERN_INFO "load_code() failed\n");
        return 1;
    }
    do_reboot();
    printk(KERN_INFO "do_reboot() failed\n");
    return 0;
}

static void __exit rebooter_exit(void){
}


module_init(rebooter_init);
module_exit(rebooter_exit);

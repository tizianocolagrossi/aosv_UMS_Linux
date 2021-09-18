/* 
 * This file is part of the User Mode Thread Scheduling (Kernel Module).
 * Copyright (c) 2021 Tiziano Colagrossi.
 * 
 * This program is free software: you can redistribute it and/or modify  
 * it under the terms of the GNU General Public License as published by  
 * the Free Software Foundation, version 3.
 *
 * This program is distributed in the hope that it will be useful, but 
 * WITHOUT ANY WARRANTY; without even the implied warranty of 
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU 
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

/**
 * @file device.c 
 * @author Tiziano Colagrossi <tiziano.colagrossi@gmail.com>
 * 
 * @brief contain the main ioctl switch for the requested ops
 */

#include"device.h"

static long  ums_ioctl  ( struct file *file, unsigned int request, unsigned long data);

static long  ums_read   ( struct file  *, char        *, size_t, loff_t * );
static long  ums_write  ( struct file  *, const char  *, size_t, loff_t * );
static int   ums_open   ( struct inode *, struct file *);
static int   ums_release( struct inode *, struct file *);

static struct file_operations fops = {
        .read           = ums_read,
        .write          = ums_write,
        .open           = ums_open,
        .release        = ums_release,
        .unlocked_ioctl = ums_ioctl,
        .compat_ioctl   = ums_ioctl};


static struct miscdevice mdev = {
        .minor = 0,
        .name  = MODULE_NAME,
        .mode  = S_IALLUGO,
        .fops  = &fops};

// commands
char *cmds[UMS_IOC_MAXNR + 1] = {
        "RESET",                             // 0
        "UMS_IOC_THREAD_YIELD",              // 1
        "UMS_IOC_THREAD_EXECUTE",            // 2
        "UMS_IOC_ENTER_UMS_SCHEDULING_MODE", // 3
        "UMS_IOC_END_UMS_SCHEDULER",         // 4
        "UMS_IOC_INIT_WORKER_THREAD",        // 5
        "UMS_IOC_END_WORKER_THREAD",         // 6
        "UMS_IOC_INIT_COMPLETION_QUEUE",     // 7
        "UMS_IOC_APPEND_TO_COMPLETION_QUEUE",// 8
        "UMS_IOC_DEQUEUE_COMPLETION_LIST"    // 9
};

DEFINE_SPINLOCK(ioctl_spinlock);


/**
 * @brief try to register the char device
 * 
 * return  0 if registration ok
 * return -1 if registration no ok
 * 
 * @return int 
 */
int try_start_device(void){
        int err;

        err = misc_register(&mdev);
        if(err){
                printk(KERN_ALERT MODULE_DEV_LOG "Error during device registration\n");
                return EXIT_FAILURE;
        }
        printk(KERN_INFO MODULE_DEV_LOG "device registration successfull\n");

        return EXIT_SUCCESS;
}

/**
 * @brief deregister the device 
 * 
 */
void stop_device(void){
        misc_deregister(&mdev);
        printk(KERN_INFO MODULE_DEV_LOG "device unregistered\n");
}

/**
 * @brief Is the ioctl dispatcher. 
 * 
 * As illustrated here
 * https://www.oreilly.com/library/view/linux-device-drivers/0596005903/ch06.html
 * 
 * @param file 
 * @param request command number
 * @param data    optional pointer to data in user space
 * @return long 
 */
static long ums_ioctl(struct file *file, unsigned int request, unsigned long data){
        int err; 
        int retval = 0;

        /*
        * extract from the ioctl request the magic number and the request
        * wrong requests: return ENOTTY (inappropriate ioctl) before access_ok( )
        */
        if (_IOC_TYPE(request) != UMS_IOC_MAGIC) return -ENOTTY;
        if (_IOC_NR(request)    > UMS_IOC_MAXNR) return -ENOTTY;

        /*
        * check correctness of the address
        */
        err = !access_ok((void __user *)data, _IOC_SIZE(request));
#ifdef UMS_DEV_DEBUG
        printk(KERN_DEBUG MODULE_DEV_LOG "check access_ok %d", err);
#endif
        if (err) return -EFAULT;

#ifdef UMS_DEV_DEBUG
        printk(KERN_DEBUG MODULE_DEV_LOG "IOCTL %s issued by %d\n",
                cmds[_IOC_NR(request)], current->pid);
#endif

        // checks are ok
        switch (request)
        {
        case UMS_IOCRESET:
                break;
        case UMS_IOC_THREAD_YIELD:
                retval = yield_to_ums(ioctl_spinlock);
                break;
        case UMS_IOC_THREAD_EXECUTE:
                retval = execute_wkt(ioctl_spinlock, (unsigned *) data);
                break;
        case UMS_IOC_ENTER_UMS_SCHEDULING_MODE:
                retval = init_ums_scheduler(ioctl_spinlock, (ums_km_param_t *) data);
                break;
        case UMS_IOC_END_UMS_SCHEDULER:
                retval = end_ums_scheduler(ioctl_spinlock);
                break;
        case UMS_IOC_INIT_WORKER_THREAD:
                retval = init_worker_thread(ioctl_spinlock);
                break;
        case UMS_IOC_END_WORKER_THREAD:
                retval = end_worker_thread(ioctl_spinlock);
                break;
        case UMS_IOC_INIT_COMPLETION_QUEUE:
                retval = init_cq(ioctl_spinlock, (void *) data);
                break;
        case UMS_IOC_APPEND_TO_COMPLETION_QUEUE:
                retval = append_to_cq(ioctl_spinlock, (ums_cq_param_t *) data);
                break;
        case UMS_IOC_DEQUEUE_COMPLETION_LIST:
                retval = dequeue_cq(ioctl_spinlock,(ums_cq_param_t *) data);
                break;
        default:
                retval = EXIT_FAILURE;
                break;
        }
        
        return retval;
}


/**
 * @brief function called when a process afer that has open the dev try to read it
 * 
 * @param filp 
 * @param buffer 
 * @param length 
 * @param offset 
 * @return long 
 */
static long ums_read(struct file *filp, char *buffer, size_t length, loff_t *offset) {
    return 0;
}

/**
 * @brief function called when a process write to the dev file.
 * 
 * @param filp 
 * @param buffer 
 * @param length 
 * @param offset 
 * @return long 
 */
static long ums_write(struct file *filp, const char *buffer, size_t length, loff_t *offset) {
    return 0;
}

/**
 * @brief called when a process try to open the dev file
 * manipulate the module usage count, to protect against removal
 * 
 * @param inode 
 * @param filp 
 * @return int 
 */
static int ums_open(struct inode *inode, struct file *filp) {
        int err;
        // manipulate the module usage count, to protect against removal
        err = try_module_get(THIS_MODULE);
        if(err) return EXIT_SUCCESS;
        return EXIT_SUCCESS;
}

/**
 * @brief called when the dev file is closed
 * manipulate the module usage count, to protect against removal
 *
 * @param inode 
 * @param filp 
 * @return int 
 */
static int ums_release(struct inode *inode, struct file *filp) {
        // manipulate the module usage count, to protect against removal
        module_put(THIS_MODULE);
        return EXIT_SUCCESS;
}
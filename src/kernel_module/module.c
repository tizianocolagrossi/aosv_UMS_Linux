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
 * @brief This file contains the implementation of the kernel module
 * 
 * @file module.c
 * @author Tiziano Colagrossi <tiziano.colagrossi@gmail.com>
 * 
 */

#include "module.h"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Tiziano Colagrossi <tiziano.colagrossi@gmail.com>");
MODULE_DESCRIPTION("User Module Thread Scheduling kernel module");
MODULE_VERSION("1.0.0");

/**
 * @brief The init function for the Ums kernel module
 * this function handle all the part of this module
 * 
 * @return int 
 */
static int ums_module_init(void){
        int err;

        printk(KERN_INFO MODULE_LOG "module loaded\n");
        err = try_start_device();
        if(err) goto fail_start_device;
        err = try_build_ums_core();
        if(err) goto fail_build_ums_core;
        err = try_build_ums_proc();
        if(err) goto fail_build_ums_proc;

        return EXIT_SUCCESS;

fail_build_ums_proc:
        clear_ums_proc();
fail_build_ums_core:
        clear_ums_core();
fail_start_device: 
        stop_device();

        return EXIT_FAILURE;
}

/**
 * @brief The destroy function of the Ums kernel module
 * 
 */
static void ums_module_exit(void){
        printk(KERN_ERR MODULE_LOG "unload 1\n");
        clear_ums_proc();
        printk(KERN_ERR MODULE_LOG "unload 2\n");
        clear_ums_core();
        printk(KERN_ERR MODULE_LOG "unload 3\n");
        stop_device();
        printk(KERN_INFO MODULE_LOG "module unloaded\n");
}


module_init(ums_module_init);
module_exit(ums_module_exit);
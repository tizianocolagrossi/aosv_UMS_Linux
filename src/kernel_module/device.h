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
 * @file device.h 
 * @author Tiziano Colagrossi <tiziano.colagrossi@gmail.com>
 * 
 * @brief The header of the device section of the module
 * 
 */

#ifndef __DEVICE_HEADER
#define __DEVICE_HEADER

#include "shared.h"
#include "module.h"
#include "ums.h"

#include <asm/uaccess.h>
#include <asm/current.h>
#include <linux/ioctl.h>
#include <linux/miscdevice.h>
#include <linux/kernel.h>
#include <linux/cdev.h>
#include <linux/fs.h>

#define MODULE_DEV_LOG "UMS_DEV: "

// #define UMS_DEV_DEBUG

int  try_start_device(void);
void stop_device(void);


#endif
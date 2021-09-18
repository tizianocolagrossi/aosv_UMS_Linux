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
 * @file module.h 
 * @author Tiziano Colagrossi <tiziano.colagrossi@gmail.com>
 * 
 * @brief The header of the kernel module 
 * 
 */

#ifndef __MODULE_HEADER
#define __MODULE_HEADER

#include "shared.h"
#include "device.h"
#include "ums.h"

#include <linux/init.h>


#define MODULE_LOG "UMS: "

#define UMS_MOD_DEBUG


int init_core(void);
void destroy_core(void);

#endif
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
 * @brief This file contains definition shared between kernel module and user library
 * 
 * @file shared.h
 * @author Tiziano Colagrossi <tiziano.colagrossi@gmail.com>
 * 
 */

#ifndef __SHARED_HEADER
#define __SHARED_HEADER

#include <linux/ioctl.h>

#define MODULE_NAME "ums"
#define MODULE_PATH "/dev/ums"

#define EXIT_SUCCESS  0
#define EXIT_FAILURE -1
#define THREAD_RUNNG -3
#define CQ_FULL      -4

// Define ioctl command
#define UMS_IOC_MAGIC 0xF4

#define UMS_IOCRESET                       _IO  (UMS_IOC_MAGIC, 0)
#define UMS_IOC_THREAD_YIELD               _IO  (UMS_IOC_MAGIC, 1)
#define UMS_IOC_THREAD_EXECUTE             _IOW (UMS_IOC_MAGIC, 2, unsigned)
#define UMS_IOC_ENTER_UMS_SCHEDULING_MODE  _IOW (UMS_IOC_MAGIC, 3, unsigned long)
#define UMS_IOC_END_UMS_SCHEDULER          _IO  (UMS_IOC_MAGIC, 4)
#define UMS_IOC_INIT_WORKER_THREAD         _IO  (UMS_IOC_MAGIC, 5)
#define UMS_IOC_END_WORKER_THREAD          _IO  (UMS_IOC_MAGIC, 6)
#define UMS_IOC_INIT_COMPLETION_QUEUE      _IOR (UMS_IOC_MAGIC, 7, unsigned long)
#define UMS_IOC_APPEND_TO_COMPLETION_QUEUE _IOW (UMS_IOC_MAGIC, 8, int)
#define UMS_IOC_DEQUEUE_COMPLETION_LIST    _IOR (UMS_IOC_MAGIC, 9, unsigned long)


#define UMS_IOC_MAXNR 9
#define COMPLETION_QUEUE_BUFF 100

typedef void *(*worker_job_t)(void *);

typedef struct ums_km_param{
	int 	cq_id; /**< id of the completion queue that will be used by the ums*/
	int owner_pid; /**< process that has request the creation of the ums*/
}ums_km_param_t;

typedef struct ums_cq_param{
	int         completion_queue_id; /**< id of the completion queue */
	int pids[COMPLETION_QUEUE_BUFF]; /**< array with pid of the worker threads*/
}ums_cq_param_t;

#endif
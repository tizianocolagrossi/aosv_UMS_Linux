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
 * @brief This file is the header of the user library
 * 
 * @file ums.h
 * @author Tiziano Colagrossi <tiziano.colagrossi@gmail.com>
 * 
 */

#define _GNU_SOURCE

#include "../kernel_module/shared.h"
#include "list.h"
#include <stdlib.h>
#include <semaphore.h>
#include <unistd.h>  
#include <pthread.h>
#include <sys/types.h>
#include <sys/syscall.h>
#include <time.h>

typedef void *(*worker_job)(void *);
typedef void *(*ums_entry_point)(void *);

typedef int bool;
#define TRUE  1
#define FALSE 0

#define GENERAL_UMS_ERROR -1
#define EXIT_UMS_MOD -2

#define UMS_PATH "/dev/ums"
#define MODULE_UMSLIB_LOG "[UMS LIB DEBUG]: "

#define UMSLIB_DEBUG

#define __F_APPEND "__append_new_worker_to_cq: "
#define F_APPEND "AppendToCompletionQueue: "
#define F_FLUSH "FlushCompletionQueue: "

#define INIT_UMS_ENTRY_STRUCT(X,E,I,O)	\
	ums_entry_info_t X = {		\
			.entry = E, 	\
			.cq_id = I, 	\
			.ret_value = 1, \
			.owner_pid = O 	\
			}	


/**
 * @brief contain the biffer used for the 
 * operation of the completion queue
 */
typedef struct buff_cq{
        int pids[COMPLETION_QUEUE_BUFF]; /**< this array is the buffer*/
} dequeued_cq_t;

typedef struct cq_list_item
{
	int                   id; /**< id of the completion queue*/
	int 			 used_by; /**< number of the ums that use it*/
    ums_cq_param_t   cq_item; /**< param used in the ioctl call*/
    struct list_head    list; /**< list head for the list api*/
} cq_list_item_t;

typedef struct worker_thread_job_info {
	worker_job           job; /**< poiner to the job (finction) of the worker thread*/
	void *      args_routine; /**< pointer to the args of the job*/
	int  		     pid; /**< pid of the worker thread*/
} worker_thread_job_info_t;

typedef struct ums_entry_info {
	ums_entry_point    entry; /**< entry point function*/
	int 	           cq_id; /**<*id of the completion queue used*/
	int 	       owner_pid; /**< pid of the owner of the ums*/
	int 	       ret_value; /**< ret value of the initialization*/
} ums_entry_info_t;

typedef struct pthread_entry {
	pthread_t 	     tid; /**< thread id of the ums used in the ExitFromUmsSchedulingMode function */
	struct list_head    list; /**< list head for the list api*/
} pthread_entry_t;

typedef struct ums_scheduler{
	pthread_t * ums_threads_list;
	int 		n_cpu;
	int cq_id;
} ums_t;

//Functions exported to user
int CreateNewWorker(worker_job job_to_perform, void * job_args);

int UmsThreadYield(void);
int ExecuteUmsThread(unsigned worker_id);

pthread_t UMS_thread_create(ums_entry_point entry_point, int completion_queue_id, int n_cpu);

ums_t * EnterUmsSchedulingMode(ums_entry_point entry_point, int completion_queue_id);
void ExitFromUmsSchedulingMode(ums_t * ums);

int CreateCompletionQueue(void);
int AppendToCompletionQueue(int completion_queue_id, int worker_pid);
int FlushCompletionQueue(int completion_queue_id);
int DequeueUmsCompletionListItems(dequeued_cq_t * return_cq);

void resetUMSFlag(void);
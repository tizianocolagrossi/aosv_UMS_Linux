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
 * @brief This file is the header of ums.c
 * 
 * @file ums.h
 * @author Tiziano Colagrossi <tiziano.colagrossi@gmail.com>
 * 
 */
#ifndef __UMS_HEADER
#define __UMS_HEADER

#include "shared.h"
#include "utility.h"
#include "proc.h"

#include <asm/fpu/internal.h>
#include <asm/fpu/types.h>
#include <asm/ptrace.h>
#include <asm/uaccess.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/list.h>
#include <linux/hashtable.h>
#include <linux/sched.h>
#include <linux/ktime.h>
#include <linux/timekeeping.h>
#include <linux/bitmap.h>


#define MODULE_UMS_LOG "UMS_MASTER_FUNCS: "

#define F_DEQUEUE      "DEQUEUE: "
#define F_APPEND       "APPEND: "
#define F_DESTROY_CQ   "DESTROY_CQ: "
#define F_INIT_CQ      "INIT_CQ: "
#define F_INIT_WORKER  "INIT_WORKER: "
#define F_END_WORKER   "END_WORKER: "
#define F_INIT_UMS     "INIT_UMS: "
#define F_SCHED_WRK    "SCHED_WKT: "
#define F_SCHED_UMS    "SCHED_UMS: "

#define HASH_KEY_SIZE  10
#define BITMAP_CQ_SIZE 128

#define STATE_RUNNING  0x0000
#define STATE_IDLE     0x0001
#define STATE_READY    0x0002
#define STATE_WAITING  0x0004

#define ums_is_idle(ums)     ((ums->state & STATE_IDLE)    != 0)
#define ums_is_running(ums)  ((ums->state & STATE_RUNNING) != 0)
#define wkt_is_running(wkt)  ((wkt->state & STATE_RUNNING) != 0)
#define wkt_is_ready(wkt)    ((wkt->state & STATE_READY)   != 0)
#define wkt_is_waiting(wkt)  ((wkt->state & STATE_WAITING) != 0)

#define fxsave(fpu)          copy_fxregs_to_kernel(fpu)
#define fxrestore(fpu)       copy_kernel_to_fxregs(&((fpu)->state.fxsave))


#define UMS_CORE_DEBUG

#define SWITCH_PT_REGS

typedef struct ums_scheduler ums_scheduler_t;
typedef struct worker_thread worker_thread_t;


/**
 * @brief This enum represent the possible direction for update the stats
 * 
 */
typedef enum direction{
        YIELD,    
        EXECUTE,
        WAIT
} direction_t;



/**
 * @brief Is the representation of the completion queue
 * contain the id of the completion queue and the 
 * completion queue itself.
 * 
 */
typedef struct completion_queue_descriptor{
        struct list_head   completion_queue;    /**< list of all the worker thread in the cq*/
        int                id;                  /**< id of the completion queue*/
        unsigned int       used_by_couter;      /**< conter of the ums that use this cq*/
        struct hlist_node  hlist;               /**< hlist node for the htable*/
}completion_queue_descriptor_t;



/**
 * @brief This struct contain all the data related to a Ums scheduler
 * 
 */
typedef struct ums_scheduler {
        int                  pid;               /**< pid of the ums thread (is also its id)*/
        struct task_struct * task_struct;       /**< task_struct of the ums thread */
        int                  owner_pid;         /**< pid of the parent process that has generated the ums*/
        int                  ums_cq_id;         /**< id of the completion queue used by this ums*/
        struct list_head *   cq_list;           /**< completion_queue of this scheduler*/
        int                  pid_wkt_sched;     /**< pid of the worker scheduled or -1 */
        worker_thread_t *    wkt_sched_struct;  /**< datastructire of the wkt scheduled*/
#ifdef SWITCH_PT_REGS 
        struct pt_regs       saved_pt_regs;     /**< current context of the cpu registers*/
        struct fpu           saved_fpu_regs;    /**< saved fpu register*/
#endif
        unsigned long        total_switch;      /**< total number of switch done by this scheduler*/
        unsigned long        last_wkt_runtime;  /**< time needed for the last worker thread switch*/
        long                 state;             /**< current state of the scheduler IDLE | RUNNING*/
        struct list_head     list;              /**< all the scheduler are into a global list*/
        struct hlist_node    hlist;             /**< for implementing the master_ums_hashlist */
} ums_scheduler_t;

/**
 * @brief Struct with the representation of the worker thread
 * 
 */
typedef struct worker_thread {
        int                  pid;               /**< pid of the worker thread */  
        struct task_struct * task_struct;       /**< task_struct of the worker thread */
#ifdef SWITCH_PT_REGS 
        struct pt_regs       saved_pt_regs;     /**< current context of the cpu registers*/
        struct fpu           saved_fpu_regs;    /**< saved fpu register*/
#endif
        int                  scheduled_by;      /**< pid of the scheduler that has scheduled this thread*/
        unsigned long        time_at_switch;    /**< # of switch that this work thread caused */
        unsigned long        total_switch;      /**< # of switch that this work thread caused */
        unsigned long        total_runtime;     /**< total running time of the thread */
        long                 state;             /**< current state of the worker*/
        struct list_head     list;              /**< for implementing the ready queue */
        struct hlist_node    hlist;             /**< for implementing the master_wt_hashlist */
} worker_thread_t;

/*
 * IOCTL exposed handler
 */
int yield_to_ums      (spinlock_t ioctl_lock);
int execute_wkt       (spinlock_t ioctl_lock , unsigned *    u_wkt_pid);
int init_cq           (spinlock_t ioctl_lock , void *      cq_id_u_ptr);
int append_to_cq      (spinlock_t ioctl_lock , ums_cq_param_t *   args);
int dequeue_cq        (spinlock_t ioctl_lock , ums_cq_param_t * ret_cq);
int init_ums_scheduler(spinlock_t ioctl_lock , ums_km_param_t *   args); 
int end_ums_scheduler (spinlock_t ioctl_lock);
int init_worker_thread(spinlock_t ioctl_lock);
int end_worker_thread (spinlock_t ioctl_lock);

/*
 * Function exposed for proc.c
 */
worker_thread_t *          Get_WKT(int wkt_pid);
ums_scheduler_t *          Get_UMS(int ums_pid);
ums_scheduler_t * Get_UMS_from_WKT(int wkt_pid);

char * Get_UMS_Info(int ums_pid);
char * Get_WKT_Info(int wkt_pid);

/*
 * Function exposed for wait_trace.c
 */
void ums_do_wait  ( worker_thread_t * from_wkt, ums_scheduler_t * to_ums);
void ums_do_unwait( worker_thread_t * from_wkt, struct task_struct *   p);

/*
 * Constructor destructor of this part
 */
int  try_build_ums_core(void);
void clear_ums_core(void);

#endif
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
 * @brief This file contains main definiton and function for the ums
 * it contains all the function for changing the context and for 
 * handle the completion queue and the worker and ums threads
 * 
 * @file ums.c
 * @author Tiziano Colagrossi <tiziano.colagrossi@gmail.com>
 * 
 */

#include "ums.h"

/* is used for translate the state from long to char[]*/
char *state[5] = { "RUNNING"
                 , "IDLE"
                 , "READY", ""
                 , "WAITING" 
                 };

/* 
 * here are declared the global data structures for mantainig
 * info about the current running ums worker and cq
 */
DECLARE_BITMAP(cq_id_bitmap, BITMAP_CQ_SIZE);           /**< bitmap to keep track of the completion queue id */
DEFINE_HASHTABLE(master_wkt_hashlist , HASH_KEY_SIZE);  /**< master hash table key:pid data:worker_thread_t */
DEFINE_HASHTABLE(master_cq_hashlist , HASH_KEY_SIZE);   /**< master hash table key:id data:ums_completion_queue_list_t */
DEFINE_HASHTABLE(master_ums_hashlist, HASH_KEY_SIZE);   /**< hashtable of all created ums_scheduler */

/* 
 * The following are the private function for this module
 */

/**
 * @brief retrive a worker by searching into the master_wkt_hashlist
 * given the wkt_pid 
 * 
 * @param wkt_pid       pid of the worker thread
 * @return worker_thread_t* 
 */
static inline worker_thread_t * __retrive_wkt(int wkt_pid){
        worker_thread_t * wkt_cursor;
        wkt_cursor = NULL;

        retrive_from_hlist(wkt_cursor, master_wkt_hashlist, hlist, pid, wkt_pid);
        
        return wkt_cursor;
}

/**
 * @brief retrive the worker struct by searching into the 
 * completion queue of the ums given the wkt pid.
 * 
 * @param ums           pointer to the ums_scheduler_t struct where search
 * @param wkt_pid       pid of the worker that need to be retrived
 * @return worker_thread_t* 
 */
static inline worker_thread_t * __retrive_wkt_from_cq(ums_scheduler_t * ums, int wkt_pid){
        worker_thread_t * wkt_cursor;
        wkt_cursor = NULL;
        
        retrive_from_list(wkt_cursor, ums->cq_list, list, pid, wkt_pid)

        return wkt_cursor;
}

/**
 * @brief retrive an ums by searching into the master_ums_hashlist
 * given the ums_pid
 * 
 * @param ums_pid       pid of the ums that need to be retrived
 * @return ums_scheduler_t* 
 */
static inline ums_scheduler_t * __retrive_ums(int ums_pid){
        ums_scheduler_t * ums_cursor;
        ums_cursor = NULL;

        retrive_from_hlist(ums_cursor, master_ums_hashlist, hlist, pid, ums_pid);

        return ums_cursor;
}

/**
 * @brief retrive the ums that has scheduled the worker thread
 * with pid wkt_pid
 * 
 * @param wkt_pid       pid of the worker thread scheduled
 * @return ums_scheduler_t* 
 */
static inline ums_scheduler_t * __retrive_ums_from_scheduled_worker_pid(int wkt_pid){
        ums_scheduler_t * ums_cursor;
        int bkt;
        ums_cursor = NULL;

        hash_for_each(master_ums_hashlist, bkt, ums_cursor, hlist){
                printk(KERN_ALERT "ums pid %d\n",ums_cursor->pid);
                if(ums_cursor->wkt_sched_struct == NULL) continue;
                if(ums_cursor->wkt_sched_struct->pid == wkt_pid) break;
        }

        return ums_cursor;
}



/**
 * @brief retrive an ums by searching into the master_cq_hashlist
 * given the cq_id 
 * 
 * @param cq_id         id of the completion queue used by the ums
 * @return completion_queue_descriptor_t* 
 */
static inline completion_queue_descriptor_t * __retrive_cq_descriptor(int cq_id){
        completion_queue_descriptor_t * cq_descriptor;
        cq_descriptor = NULL;

        get_hlist_item_by_id(cq_descriptor, master_cq_hashlist , hlist, id, cq_id);

        return cq_descriptor;
}

/**
 * @brief init the ums pointed by new_ums (alredy allocated)
 * 
 * @param new_ums       pointer to the struct ums_scheduler_t 
 *                      that will be initialized
 */
static inline void __init_new_ums(ums_scheduler_t * new_ums, int owner_pid){
        new_ums->task_struct      =  current;
        new_ums->pid              =  current->pid;
        new_ums->state            =  STATE_IDLE;
        new_ums->total_switch     =  0;
        new_ums->last_wkt_runtime =  0;
        new_ums->pid_wkt_sched    = -1;
        new_ums->owner_pid        = owner_pid;

        return;
}

/**
 * @brief init the worker thread pointed by new_wkt (alredy allocated)
 * 
 * @param new_wkt       pointer to the struct worker_thread_t 
 *                      that will be initialized
 */
static inline void __init_new_wkt(worker_thread_t * new_wkt){
        new_wkt->task_struct    = current;
        new_wkt->pid            = current->pid;
        new_wkt->state          = STATE_READY;
        new_wkt->time_at_switch = 0;
        new_wkt->total_switch   = 0;
        new_wkt->total_runtime  = 0;

#ifdef SWITCH_PT_REGS
        //initialize context only if we use pt_regs for switch
        memcpy(&(new_wkt->saved_pt_regs) , task_pt_regs(current), sizeof(struct pt_regs));
        //initialze the saved fpu struct 
        memset(&(new_wkt->saved_fpu_regs), 0, sizeof(struct fpu));
        //save fpu
        fxsave(&(new_wkt->saved_fpu_regs));
#endif

        return;
}

/**
 * @brief try to link a completion queue to the ums 
 * 
 * @param new_ums       pointer to the ums_scheduler_t struct 
 * @param id            id of the completion queue to link
 * @return int 
 */
static inline int __try_to_link_cq(ums_scheduler_t * new_ums, int id){
        completion_queue_descriptor_t * cq_descriptor;
        cq_descriptor = __retrive_cq_descriptor(id);
        if(cq_descriptor == NULL) return EXIT_FAILURE;

        cq_descriptor->used_by_couter += 1;

        new_ums->ums_cq_id = id;
        new_ums->cq_list   = &(cq_descriptor->completion_queue);
        
        return EXIT_SUCCESS;
}

/**
 * @brief update the stats in the struct of the ums and the wkt 
 * for the case of yield
 * 
 * 
 * @param ums           pointer to the ums_scheduler_t struct
 * @param wkt           pointer to the worker_thread_t struct
 */
static inline void __update_stats_yield  (ums_scheduler_t * ums, worker_thread_t * wkt){
        unsigned long actual_current_time;
        unsigned long delta; 

        actual_current_time =  ktime_get_ns();
        delta = actual_current_time - wkt->time_at_switch;

        wkt->total_runtime   +=  delta;
        wkt->state            =  STATE_READY;
        wkt->total_switch    +=  1;
        wkt->scheduled_by     = -1;
        
        ums->last_wkt_runtime =  delta;
        ums->state            =  STATE_IDLE;
        ums->total_switch    +=  1;
        ums->pid_wkt_sched    = -1;
        ums->wkt_sched_struct =  NULL;
}

/**
 * @brief update the stats in the struct of the ums and the wkt 
 * for the case of execute
 * 
 * 
 * @param ums           pointer to the ums_scheduler_t struct
 * @param wkt           pointer to the worker_thread_t struct
 */
static inline void __update_stats_execute(ums_scheduler_t * ums, worker_thread_t * wkt){
        unsigned long actual_current_time;

        actual_current_time =  ktime_get_ns();
        
        wkt->time_at_switch   = actual_current_time;
        wkt->state            = STATE_RUNNING;
        wkt->total_switch    += 1;
        wkt->scheduled_by     = ums->pid;
        
        ums->state            = STATE_RUNNING;
        ums->total_switch    += 1;
        ums->pid_wkt_sched    = wkt->pid;
        ums->wkt_sched_struct = wkt;
}

/**
 * @brief update the stats in the struct of the ums and the wkt 
 * for the case of wait
 * 
 * 
 * @param ums           pointer to the ums_scheduler_t struct
 * @param wkt           pointer to the worker_thread_t struct
 */
static inline void __update_stats_wait   (ums_scheduler_t * ums, worker_thread_t * wkt){
        unsigned long actual_current_time;
        unsigned long delta; 

        actual_current_time =  ktime_get_ns();
        delta = actual_current_time - wkt->time_at_switch;
        
        wkt->total_runtime   +=  delta;
        wkt->state            =  STATE_WAITING;
        wkt->total_switch    +=  1;
        wkt->scheduled_by     = -1;
        
        ums->last_wkt_runtime =  delta;
        ums->state            =  STATE_IDLE;
        ums->total_switch    +=  1;
        ums->pid_wkt_sched    = -1;
        ums->wkt_sched_struct =  NULL;
}


/**
 * @brief Destroy the completion queue with the given id.
 * Destroy and free also the worker thread if contained.
 * Is detroyed only if no more UMS use this completion queue. 
 * 
 * @param id    id of the vompletion queue to destroy
 * @return int 0 if all went ok, else -1
 */
static inline int __destroy_completion_queue(int id)
{

        completion_queue_descriptor_t * selected_comp_queue_desc;
        worker_thread_t *worker_cursor, *worker_cursor_safe;
        struct list_head *completion_queue_list;

        selected_comp_queue_desc = __retrive_cq_descriptor(id);
        if (selected_comp_queue_desc == NULL) return EXIT_FAILURE;

#ifdef UMS_CORE_DEBUG
        printk(KERN_DEBUG MODULE_UMS_LOG F_DESTROY_CQ "--------- DESTROY COMPLETION QUEUE ---------\n");
        printk(KERN_DEBUG MODULE_UMS_LOG F_DESTROY_CQ "cq id: %d\n", id );
        printk(KERN_DEBUG MODULE_UMS_LOG F_DESTROY_CQ "used by: %d\n", selected_comp_queue_desc->used_by_couter );
#endif



        // the completion queue is not yet binded to an ums
        if(selected_comp_queue_desc->used_by_couter == 0) goto delete;

        // destroy only if is not used by no one ums anymore
        selected_comp_queue_desc->used_by_couter -= 1;
        if (selected_comp_queue_desc->used_by_couter!=0) return EXIT_SUCCESS; 

delete:
        completion_queue_list = &selected_comp_queue_desc->completion_queue; 
        ums_delete_list(worker_cursor, worker_cursor_safe, completion_queue_list, list);
        delete_completion_queue_descriptor(selected_comp_queue_desc, hlist);
        // clear bit in the bitmap
        clear_bit(id, cq_id_bitmap);
        // reflet the destroy to the procfs
        Proc_Update_Cq_Deleted(id);

#ifdef UMS_CORE_DEBUG
        printk(KERN_DEBUG MODULE_UMS_LOG F_DESTROY_CQ "completion queue %d destroyed\n", id );
#endif

        return EXIT_SUCCESS;
}

/*
 * From here the function are accesible from other files
 */

/**
 * @brief perform a context switch from wkt to ums that host it
 * 
 * # Implementation
 * 
 * Initially the current check are done: 
 * - Try to retrive the UMS that currenty host the execution
 *   of the worker thread that has required the yield.
 * - Try to retrive the worker thread struct scheduled saved
 *   inside the UMS struct. 
 * 
 * After this checks:
 * - Update the stats for the UMS and the worker structs.
 * - Finally perform the actual context switch:
 *   if SWITCH_PT_REGS is defined, by saving the current state 
 *   (pt_regs and fxregs) into the worker_thread_t struct and restoring the
 *   previously saved state of the ums from the ums_scheduler_t struct.
 *   if SWITCH_PT_REGS is not defined, by stop the worker thread and waking up
 *   the UMS scheduler that previously has scheduled the worker. 
 *
 * @param ioctl_lock
 * @return int 0 if all went ok, else -1 
 */
int yield_to_ums(spinlock_t ioctl_lock)
{
        ums_scheduler_t * ums_to_switch;
        worker_thread_t * wkt_caller;

        spin_lock_irq(&ioctl_lock);

#ifdef SWITCH_PT_REGS
        ums_to_switch = __retrive_ums(current->pid);
#endif
#ifndef SWITCH_PT_REGS
        ums_to_switch = __retrive_ums_from_scheduled_worker_pid(current->pid);
#endif
        if (ums_to_switch == NULL) goto err;
        wkt_caller = ums_to_switch->wkt_sched_struct;
        if (wkt_caller == NULL) goto err;

        __update_stats_yield(ums_to_switch, wkt_caller);
        list_add_tail(&wkt_caller->list, ums_to_switch->cq_list);

#ifdef SWITCH_PT_REGS
        // switch pt_resgs
        memcpy(&wkt_caller->saved_pt_regs, task_pt_regs(current), sizeof(struct pt_regs));
        memcpy(task_pt_regs(current), &ums_to_switch->saved_pt_regs, sizeof(struct pt_regs));
        // switch fpu
        fxsave(&wkt_caller->saved_fpu_regs);
        fxrestore(&ums_to_switch->saved_fpu_regs);
#endif

        spin_unlock_irq(&ioctl_lock);

#ifdef UMS_CORE_DEBUG
        printk(KERN_DEBUG MODULE_UMS_LOG F_SCHED_UMS "from worker: %d to scheduler %d\n", wkt_caller->pid, ums_to_switch->pid);
        printk(KERN_DEBUG MODULE_UMS_LOG F_SCHED_UMS "total time used from the worker: %ld\n", wkt_caller->total_runtime);
        printk(KERN_DEBUG MODULE_UMS_LOG F_SCHED_UMS "number of switch caused by the worker: %ld\n", wkt_caller->total_switch);
#endif

#ifndef SWITCH_PT_REGS
        __set_current_state(TASK_INTERRUPTIBLE);
        wake_up_process(ums_to_switch->task_struct);
        schedule();
#endif

        return EXIT_SUCCESS;
err:
        spin_unlock_irq(&ioctl_lock);
        return EXIT_FAILURE;
}

/**
 * @brief perform a context switch from ums to wkt selected
 * 
 * # Implementation
 * 
 * Initially the current check are done: 
 * - Try to copy from user the value passed to the module by ioctl
 * - The current need to be an ums pid because only an UMS can schedule
 *   a worker thread.
 * - I try to retrive the worker_thread_t from the cq_list
 *   using the wkt_pid_to_switch passed by the user arg
 * - If the state of the worker selected is W_RUNNING the function end
 *   because the thread is scheduled by another UMS
 * 
 * After this checks:
 * - Update the stats for the UMS and the worker structs.
 * - Finally perform the actual context switch:
 *   if SWITCH_PT_REGS is defined, by saving the current state 
 *   (pt_regs and fxregs) into the ums_scheduler_t stuct and restoring the
 *   previously saved state of the worker thread from the worker_thread_t struct.
 *   if SWITCH_PT_REGS is not defined, by stop the UMS and waking up
 *   the worker thread.
 * 
 * 
 * @param ioctl_lock
 * @param u_wkt_pid     pointer to the user space mem of the id of 
 *                      the worker thread choose to be executed                                 
 * @return int 0 if all went ok, else -1 
 */
int execute_wkt(spinlock_t ioctl_lock, unsigned * u_wkt_pid)
{
        worker_thread_t * wkt_to_switch;
        ums_scheduler_t * ums_caller;
        unsigned wkt_pid_to_switch;
        int ret;

        spin_lock_irq(&ioctl_lock);

        ret = copy_from_user(&wkt_pid_to_switch, u_wkt_pid, sizeof(unsigned));
        if (ret != 0) 
        {
                printk(KERN_ERR MODULE_UMS_LOG F_SCHED_WRK "Error during the copy of param from user");
                goto efault;
        }

        ums_caller = __retrive_ums(current->pid);
        if (ums_caller == NULL)
        {
                printk(KERN_ERR MODULE_UMS_LOG F_SCHED_WRK "Execute required by a non UMS thread!");
                goto err;
        }
        wkt_to_switch = __retrive_wkt_from_cq(ums_caller, wkt_pid_to_switch);
        if (wkt_to_switch == NULL)
        {
                printk(KERN_ERR MODULE_UMS_LOG F_SCHED_WRK "The worker %d is not in the completion queue!", wkt_pid_to_switch);
                goto err;
        }
        if (wkt_is_running(wkt_to_switch))
        {
                printk(KERN_ERR MODULE_UMS_LOG F_SCHED_WRK "Worker to schedule alredy running");
                goto thread_running;
        }

        __update_stats_execute(ums_caller, wkt_to_switch);

        list_del(&wkt_to_switch->list);

#ifdef SWITCH_PT_REGS
        memcpy(&ums_caller->saved_pt_regs, task_pt_regs(current), sizeof(struct pt_regs));
        memcpy(task_pt_regs(current), &wkt_to_switch->saved_pt_regs, sizeof(struct pt_regs));
        //save fpu
        fxsave(&ums_caller->saved_fpu_regs);
        fxrestore(&wkt_to_switch->saved_fpu_regs);
#endif 
        
#ifdef UMS_CORE_DEBUG
        printk(KERN_DEBUG MODULE_UMS_LOG F_SCHED_WRK "from scheduler: %d to worker %d\n", ums_caller->pid, wkt_to_switch->pid);
        printk(KERN_DEBUG MODULE_UMS_LOG F_SCHED_WRK "sched task_struck: %p wkt task_struct %p\n", ums_caller->task_struct, wkt_to_switch->task_struct);
        printk(KERN_DEBUG MODULE_UMS_LOG F_SCHED_WRK "pid_of_worker_scheduled %d\n", ums_caller->pid_wkt_sched);
        printk(KERN_DEBUG MODULE_UMS_LOG F_SCHED_WRK "number of switch caused by the scheduler: %ld\n", ums_caller->total_switch);
#endif

        spin_unlock_irq(&ioctl_lock);

#ifndef SWITCH_PT_REGS
        __set_current_state(TASK_INTERRUPTIBLE);
        wake_up_process(wkt_to_switch->task_struct);
        schedule();
#endif 

        return EXIT_SUCCESS;
thread_running:
        spin_unlock_irq(&ioctl_lock);
        return THREAD_RUNNG;
err:
        spin_unlock_irq(&ioctl_lock);
        return EXIT_FAILURE;
efault:
        spin_unlock_irq(&ioctl_lock);
        return -EFAULT;

}

/**
 * @brief Initialize the struct for a new completion queue
 * 
 * # Implementation
 * 
 * - Find id for a new completion queue from the bitmap
 *   and set the bit.
 * - Create a new completion_queue_descriptor_t and initialize it
 * - Try to return the completion queue id
 * 
 * @param ioctl_lock
 * @param cq_id_u_ptr   pointer to the cq_id in user space
 * @return int 0 if all went ok, else -1  
 */
int init_cq(spinlock_t ioctl_lock, void * cq_id_u_ptr)
{

        completion_queue_descriptor_t * new_comp_queue_descr;
        int k_comp_queue_id;
        int ret;

        spin_lock_irq(&ioctl_lock);

        k_comp_queue_id = -1;
        k_comp_queue_id = find_first_zero_bit(cq_id_bitmap, BITMAP_CQ_SIZE);
        // check if cq bitmap is full (cq limited to 128 at the same time)
        if (k_comp_queue_id == -1) {
                spin_unlock_irq(&ioctl_lock);
                return CQ_FULL;
        }
        set_bit(k_comp_queue_id, cq_id_bitmap);

#ifdef UMS_CORE_DEBUG
        printk(KERN_DEBUG MODULE_UMS_LOG F_INIT_CQ "HERE OK id %d\n", k_comp_queue_id);
#endif

        add_new_item_to_hlist(new_comp_queue_descr, master_cq_hashlist , hlist, completion_queue_descriptor_t, k_comp_queue_id);
        new_comp_queue_descr->id = k_comp_queue_id;
        new_comp_queue_descr->used_by_couter = 0;
        
        INIT_LIST_HEAD(&new_comp_queue_descr->completion_queue);
        
        ret = copy_to_user((void *)cq_id_u_ptr, (void *)&k_comp_queue_id, sizeof(int));
        if (ret != 0) {
                printk(KERN_ERR MODULE_UMS_LOG F_INIT_CQ "Error with copy_to_user");
                // if error free the struct 
                hlist_del(&new_comp_queue_descr->hlist);											\
		kfree(new_comp_queue_descr);
                spin_unlock_irq(&ioctl_lock);
                return -EFAULT;
        }

        Proc_Update_Cq_Created(k_comp_queue_id);

        spin_unlock_irq(&ioctl_lock);

#ifdef UMS_CORE_DEBUG
        printk(KERN_DEBUG MODULE_UMS_LOG "cq new id: %d\n", k_comp_queue_id );
#endif

        return EXIT_SUCCESS;
}

/**
 * @brief Append worker threads to a completion queue
 * 
 * It cycle over the cpmpletion queue buffer. The buffer 
 * is initialized to -1 so if reach -1 before that cycle 
 * over all the buffer means that it has appended all the worker
 * 
 * @param ioctl_lock
 * @param args          pointer to the ums_cq_param_t struct 
 *                      placed in the user space
 * @return int 
 */
int append_to_cq(spinlock_t ioctl_lock, ums_cq_param_t * args)
{ 
        completion_queue_descriptor_t * selected_comp_queue_desc;
        worker_thread_t * selected_worker;
        ums_cq_param_t k_args;
        int pid_i;
        int ret;

        spin_lock_irq(&ioctl_lock);

        ret = copy_from_user(&k_args, args, sizeof(ums_cq_param_t));
        if (ret != 0) {
                printk(KERN_ERR MODULE_UMS_LOG F_APPEND "Error during the copy of param from user");
                spin_unlock_irq(&ioctl_lock);
                return -EFAULT;
        }

#ifdef UMS_CORE_DEBUG
        printk(KERN_DEBUG MODULE_UMS_LOG F_APPEND "cq_id: %d\n",k_args.completion_queue_id);
        printk(KERN_DEBUG MODULE_UMS_LOG F_APPEND "[%d,%d,%d,%d,%d]\n",
                k_args.pids[0],k_args.pids[1],k_args.pids[2],k_args.pids[3],k_args.pids[4]);  
#endif

        selected_comp_queue_desc = __retrive_cq_descriptor(k_args.completion_queue_id);
        if(selected_comp_queue_desc == NULL) {
                spin_unlock_irq(&ioctl_lock);
                return EXIT_FAILURE;
        }

        // cycle over the completion queue buffer
        for(pid_i = 0; pid_i < COMPLETION_QUEUE_BUFF; pid_i++){
                if(k_args.pids[pid_i] == -1) break;
                selected_worker = __retrive_wkt(k_args.pids[pid_i]);
                get_hlist_item_by_id(selected_worker, master_wkt_hashlist , hlist, pid, k_args.pids[pid_i])
                if(selected_worker == NULL) continue;

#ifdef UMS_CORE_DEBUG
                printk(KERN_DEBUG MODULE_UMS_LOG F_APPEND "append worker pid %d\n",k_args.pids[pid_i]);
#endif
                list_add_tail(&selected_worker->list, &selected_comp_queue_desc->completion_queue);
                Proc_Update_Worker_Appended(selected_worker->pid, k_args.completion_queue_id);
        }

        spin_unlock_irq(&ioctl_lock);

        return EXIT_SUCCESS;
}


/**
 * @brief return the first COMPLETION_QUEUE_BUFF size of worker thread id in the completion queue
 * 
 * @param ioctl_lock
 * @param ret_cq        pointer to the ums_cq_param_t in the user space
 *                      used to return the worker thread pid in the 
 *                      cq that are ready
 * @return int 
 */
int dequeue_cq(spinlock_t ioctl_lock, ums_cq_param_t * ret_cq)
{
	struct list_head * completion_queue;
	int k_pids[COMPLETION_QUEUE_BUFF];
	worker_thread_t * worker_cursor;
        ums_scheduler_t * current_ums;
        unsigned current_ums_pid;
	int kp_i;
	int ret;

        spin_lock_irq(&ioctl_lock);

        //check if i'am a ums
        current_ums_pid = current->pid;
        current_ums = __retrive_ums(current_ums_pid);
        retrive_from_hlist(current_ums, master_ums_hashlist, hlist, pid, current_ums_pid);
#ifdef UMS_CORE_DEBUG
        if (current_ums == NULL)
                printk(KERN_ERR MODULE_UMS_LOG F_DEQUEUE "Dequeue requested not by an ums\n");
#endif  
        if (current_ums == NULL) {
                spin_unlock_irq(&ioctl_lock);
                return EXIT_FAILURE;
        }

	completion_queue = current_ums->cq_list;
        if (completion_queue == NULL)
        {
                printk(KERN_ERR MODULE_UMS_LOG F_DEQUEUE "UMS has not a cq linked\n");
                spin_unlock_irq(&ioctl_lock);
                return EXIT_FAILURE;
        }
        
	// initialize the array to -1
	for (kp_i = 0; kp_i<COMPLETION_QUEUE_BUFF; kp_i++)
		k_pids[kp_i] = -1;
	
	kp_i = 0;
	list_for_each_entry(worker_cursor, completion_queue, list) {
                if(wkt_is_running(worker_cursor) || wkt_is_waiting(worker_cursor)) continue;

		k_pids[kp_i] = worker_cursor->pid;
		kp_i += 1;

		if (kp_i == COMPLETION_QUEUE_BUFF) goto buffer_full;
	}

buffer_full:
	ret = copy_to_user((void *)ret_cq->pids, (void *)&k_pids, ( sizeof(int) * COMPLETION_QUEUE_BUFF) );
        if (ret != 0) {
                printk(KERN_ALERT MODULE_UMS_LOG F_DEQUEUE "Error with copy_to_user");
                spin_unlock_irq(&ioctl_lock);
                return -EFAULT;
        }

#ifdef UMS_CORE_DEBUG
        printk(KERN_DEBUG MODULE_UMS_LOG F_DEQUEUE "ret arr (first 5) [%d, %d, %d, %d, %d]\n",
                k_pids[0], k_pids[1], k_pids[2], k_pids[3], k_pids[4]);
#endif      

        spin_unlock_irq(&ioctl_lock);

	return EXIT_SUCCESS;

}

/**
 * @brief convert a standard pthread into an UmsSchedulerThread
 * 
 * converts a standard pthread in a UMS Scheduler thread, the function takes 
 * as input a completion list of worker threads and a entry point function
 * 
 * # Implementation
 * 
 * Initially the current check are done: 
 * - Try to copy from user the value passed to the module by ioctl
 * - Check if the completion queue id is a valid id
 * 
 * After this checks the function populate the structs in the kernel:
 * - Create a new ums_scheduler_t struct for the new UMS created
 * - Initialize the struct with default value
 * - Link the completion queue to this UMS and update the 
 *   used_by_couter entry in the completion_queue_descriptor_t descriptor
 *   cause the completion queue can be shared among multiple UMS
 * 
 * @param ioctl_lock
 * @param args          pointer to the  ums_km_param_t struct
 *                      in user space
 * @return int 0 if all went ok, else -1
 */
int init_ums_scheduler(spinlock_t ioctl_lock, ums_km_param_t * args)
{
        ums_scheduler_t * new_ums_scheduler;
        ums_km_param_t k_ums_km_param;
        int k_owner_pid;
        int k_cq_id;
        int ret;

#ifdef UMS_CORE_DEBUG
        ums_scheduler_t * debug_cursor_sched;
        int bkt;
#endif

        spin_lock_irq(&ioctl_lock);

        ret = copy_from_user(&k_ums_km_param, args, sizeof(ums_km_param_t));
        if (ret != 0)
        {
                printk(KERN_ERR MODULE_UMS_LOG F_INIT_UMS "Error during the copy of param from user");
                spin_unlock_irq(&ioctl_lock);
                return -EFAULT;
        }
        k_owner_pid = k_ums_km_param.owner_pid;
        k_cq_id = k_ums_km_param.cq_id;

        if(!test_bit(k_cq_id, cq_id_bitmap))
        {
                printk(KERN_ERR MODULE_UMS_LOG F_INIT_UMS "completion queue not valid");
                spin_unlock_irq(&ioctl_lock);
                return EXIT_FAILURE;
        } 

        add_new_item_to_hlist(new_ums_scheduler, master_ums_hashlist , hlist, ums_scheduler_t, current->pid);
        
        __init_new_ums(new_ums_scheduler, k_owner_pid);
        ret = __try_to_link_cq(new_ums_scheduler, k_cq_id);
        if(ret) goto err_link_cq;

        Proc_Update_Ums_Created(new_ums_scheduler->pid, new_ums_scheduler->owner_pid, k_cq_id);

        spin_unlock_irq(&ioctl_lock);


#ifdef UMS_CORE_DEBUG
        hash_for_each(master_ums_hashlist, bkt, debug_cursor_sched, hlist){
                printk(KERN_DEBUG MODULE_UMS_LOG F_SCHED_UMS "ums pid %d\n",debug_cursor_sched->pid);
        }
        printk(KERN_DEBUG MODULE_UMS_LOG F_INIT_UMS "owner_pid of ums %d id pid %d", current->pid, k_ums_km_param.owner_pid);
        printk(KERN_DEBUG MODULE_UMS_LOG F_SCHED_UMS "Pid %d and pointer to task_struct %p\n", new_ums_scheduler->pid, new_ums_scheduler->task_struct);
        printk(KERN_DEBUG MODULE_UMS_LOG F_SCHED_UMS "current pointer: %p, task_struct pointer stored: %p\n", current, new_ums_scheduler->task_struct);
#endif

        return EXIT_SUCCESS;

err_link_cq:    
        Proc_Update_Ums_Ended(new_ums_scheduler->pid);
        hash_del(&new_ums_scheduler->hlist);
        kfree(new_ums_scheduler);
        spin_unlock_irq(&ioctl_lock);
        return EXIT_FAILURE;
}

/**
 * @brief clear the data structure used by the ums
 * 
 * @param ioctl_lock 
 * @return int 
 */
int end_ums_scheduler(spinlock_t ioctl_lock)
{
        ums_scheduler_t * current_ums;

        spin_lock_irq(&ioctl_lock);

        current_ums = __retrive_ums(current->pid);
        if (current_ums == NULL) {
                spin_unlock_irq(&ioctl_lock);
                return EXIT_FAILURE;
        }

        __destroy_completion_queue(current_ums->ums_cq_id);

        Proc_Update_Ums_Ended(current_ums->pid);

        hash_del(&current_ums->hlist);
        kfree(current_ums);

        spin_unlock_irq(&ioctl_lock);

        return EXIT_SUCCESS;
}

/**
 * @brief block at startup the worker_thread to avoid that the 
 * linux scheduler shedule this thread, and initialize its
 * kernel struct used for the UMS
 * 
 * # Implementation
 * 
 * - Create a new worker_thread_t struct and initialize it
 *   by save the current pid of the real worker thread into 
 *   its own struct, and save the pointer to the worker 
 *   task_struct into task_struct. And set the 
 *   worker state to W_READY.
 * - If SWITCH_PT_REGS is defined, save the worker current 
 *   state into the worker_thread_t struct
 * - Put on wait worker thread
 * 
 * @param ioctl_lock
 * @return int 0 if all went ok, else -1 
 */
int init_worker_thread(spinlock_t ioctl_lock)
{
        worker_thread_t * new_worker;
        pid_t wkt_pid;

        spin_lock_irq(&ioctl_lock);

        wkt_pid = current->pid;
        // adding the worker_thread_t struct into the master hash table
        add_new_item_to_hlist(new_worker, master_wkt_hashlist, hlist, worker_thread_t, wkt_pid);
        
        __init_new_wkt(new_worker);

#ifdef UMS_CORE_DEBUG
        printk(KERN_DEBUG MODULE_UMS_LOG F_INIT_WORKER "pid: %d, task_struct: %p\n", new_worker->pid, new_worker->task_struct);
        printk(KERN_DEBUG MODULE_UMS_LOG F_INIT_WORKER "current pointer: %p, task_struct pointer stored: %p\n", current, new_worker->task_struct);
#endif

        Proc_Update_Worker_Created(wkt_pid);

        spin_unlock_irq(&ioctl_lock);

        __set_current_state(TASK_INTERRUPTIBLE);
        schedule();

        return EXIT_SUCCESS;
}

/**
 * @brief if SWITCH_PT_REGS is defined restore the original worker thread 
 * not hosted from the ums in order to permit the end of the worker thread 
 * and its task_struct. Also remove the worker_thread_struct from the 
 * completion queue and the hashtable of the worker thread.
 * if SWITCH_PT_REGS is not defined wake up the UMS 
 *
 * # Implementation
 *
 * Check done by the function:
 * - The current pid needs to be a UMS pid if SWITCH_PT_REGS is defined
 *   else the current pid represent the worker scheduled
 * - if SWITCH_PT_REGS is defined, try to retrive the worker_thread_t struct 
 *   else try to retrive the UMS using the worker pid
 *
 * After this check perform:
 * - Restore context of the worker into his own task_struct if SWITCH_PT_REGS is defined
 * - Restore ums context if SWITCH_PT_REGS is defined
 * - Remove the worker thread from the completion queue and from 
 *   the hashtable and free the memory of the worker_thread_t struct
 * - Finally resume the worker thread 
 * - If SWITCH_PT_REGS is not defined wake up also the UMS
 * 
 * @param ioctl_lock
 * @return int 0 if all went ok, else -1  
 */
int end_worker_thread(spinlock_t ioctl_lock)
{

        worker_thread_t * wkt_caller;
        ums_scheduler_t * current_ums;

        spin_lock_irq(&ioctl_lock);

#ifdef SWITCH_PT_REGS
        current_ums = __retrive_ums(current->pid);
#endif
#ifndef SWITCH_PT_REGS
        current_ums = __retrive_ums_from_scheduled_worker_pid(current->pid);
#endif

        if (current_ums == NULL) {
                spin_unlock_irq(&ioctl_lock);
                return EXIT_FAILURE;
        }
        wkt_caller = current_ums->wkt_sched_struct;
        if (wkt_caller == NULL) {
                spin_unlock_irq(&ioctl_lock);
                return EXIT_FAILURE;
        }

#ifdef SWITCH_PT_REGS
        // restore ums context
        memcpy(task_pt_regs(wkt_caller->task_struct), task_pt_regs(current), sizeof(struct pt_regs) );
        memcpy(task_pt_regs(current), &current_ums->saved_pt_regs, sizeof(struct pt_regs));
        //save fpu
        fxrestore(&current_ums->saved_fpu_regs);

#endif

        Proc_Update_Worker_Ended(wkt_caller->pid);
        
        hash_del(&wkt_caller->hlist);
        
        current_ums->wkt_sched_struct = NULL;

        spin_unlock_irq(&ioctl_lock);

#ifdef SWITCH_PT_REGS
        wake_up_process(wkt_caller->task_struct);
#endif
#ifndef SWITCH_PT_REGS
        wake_up_process(current_ums->task_struct);
#endif
        kfree(wkt_caller);

        schedule();

        return EXIT_SUCCESS;
}

/*
 * Function exposed for proc.c
 */

/**
 * @brief retrive a pointer to worker_thread_t struct
 * 
 * @param wkt_pid       pid of the worker thread to find
 * @return worker_thread_t* 
 */
worker_thread_t * Get_WKT(int wkt_pid){
        return __retrive_wkt(wkt_pid);
}

/**
 * @brief retrive a pointer to ums_scheduler_t struct
 * 
 * @param ums_pid       pid of the ums thread to find
 * @return ums_scheduler_t* 
 */
ums_scheduler_t * Get_UMS(int ums_pid){
        return __retrive_ums(ums_pid);
}

/**
 * @brief retrive a pointer to ums_scheduler_t struct
 * 
 * @param wkt_pid pid of the worker actual scheduled by the ums
 * @return ums_scheduler_t* 
 */
ums_scheduler_t * Get_UMS_from_WKT(int wkt_pid){
        return __retrive_ums_from_scheduled_worker_pid(wkt_pid);
}

/**
 * @brief retrive a char * with the info of the UMS in a human readable fashion
 * 
 * @param ums_pid pid of the ums
 * @return char* 
 */
char * Get_UMS_Info(int ums_pid)
{
        ums_scheduler_t * ums;
        worker_thread_t * worker_cursor;
        char *info_ums;
        char * tmp;

        ums = __retrive_ums(ums_pid);
        if(ums==NULL) return NULL;

        info_ums = kasprintf(GFP_KERNEL,        "XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX\n"
                                                "                    UMS INFO\n"
                                                "XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX\n"
                                                "\n"
                                                "Owner:               %d (pid)\n"
                                                "UMS:                 %d (pid)\n"
                                                "Task struct ptr:     %p\n"
                                                "UMS State:           %s\n"
                                                "Completion queue:    %d (id)\n"
                                                "Worker scheduled:    %d (pid)\n"
                                                "UMS total switch:    %ld\n"
                                                "Last worker runtime: %ld\n"
                                                , ums->owner_pid, ums->pid, ums->task_struct, state[ums->state]
                                                , ums->ums_cq_id, ums->pid_wkt_sched
                                                , ums->total_switch, ums->last_wkt_runtime
                        );
  
        if (ums->cq_list == NULL) return info_ums;
        tmp = kasprintf(GFP_KERNEL,     "%s\n"
                                                "XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX\n"
                                                "                 COMPLETION LIST                 \n"
                                                "XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX\n"
                                                , info_ums);
        kfree(info_ums);
        info_ums = tmp;
        tmp = NULL;

        list_for_each_entry(worker_cursor, ums->cq_list, list) {
                tmp = kasprintf(GFP_KERNEL,     "%s"
                                                "-------------------------------------------------\n"
                                                "Worker thread:       %d (pid)\n"
                                                "State:               %s\n"
                                                "Total switch:        %ld\n"
                                                "Total runtime:       %ld\n"
                                                "-------------------------------------------------\n"
                                                , info_ums
                                                , worker_cursor->pid , state[worker_cursor->state]
                                                , worker_cursor->total_switch, worker_cursor->total_runtime);
                
                kfree(info_ums);
                info_ums = tmp;
                tmp = NULL;
	}    


        return info_ums;
}

/**
 * @brief retrive a char * with the info of the worker in a human readable fashion
 * 
 * @param wkt_pid oid of the worker thread
 * @return char* 
 */
char * Get_WKT_Info(int wkt_pid){
        worker_thread_t * wkt;
        char *info_wkt;
        wkt = __retrive_wkt(wkt_pid);
        if(wkt==NULL) return NULL;

        info_wkt = kasprintf(GFP_KERNEL,        "XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX\n"
                                                "                WORKER THREAD INFO               \n"
                                                "XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX\n"
                                                "\n"
                                                "Worker:              %d (pid)\n"
                                                "State:               %s\n"
                                                "Scheduled by:        %d (pid)\n"
                                                "Task struct ptr:     %p\n"
                                                "Total switch:        %ld\n"
                                                "Total runtime:       %ld\n"
                                                , wkt->pid, state[wkt->state], wkt->scheduled_by
                                                , wkt->task_struct, wkt->total_switch, wkt->total_runtime
                        );

        return info_wkt;
}

/*
 * Function exposed for wait_trace.c
 */

/**
 * @brief handler used in the case that a worker thread is put on wait
 * 
 * @param from_wkt      pointer to worker_thread_t struct of the worker 
 *                      thread that will put in wait
 * @param to_ums        pointer to ums_scheduler_t struct if the ums
 *                      thread that will wakeup 
 */
void ums_do_wait( worker_thread_t * from_wkt, ums_scheduler_t * to_ums)
{
#ifdef SWITCH_PT_REGS 
        struct fpu * tmp_fpu;
        tmp_fpu =  (struct fpu *) kmalloc(sizeof(struct fpu), GFP_KERNEL);
        memset(tmp_fpu, 0, sizeof(struct fpu));
#endif

        __update_stats_wait(to_ums, from_wkt);
        list_add_tail(&from_wkt->list, to_ums->cq_list);

#ifdef SWITCH_PT_REGS    
        //restore wkt context
        memcpy(task_pt_regs(from_wkt->task_struct), task_pt_regs(to_ums->task_struct), sizeof(struct pt_regs));
        // restore ums context
        memcpy(task_pt_regs(to_ums->task_struct), &to_ums->saved_pt_regs, sizeof(struct pt_regs));

        fxsave(tmp_fpu);

        // restore wkt fpu context
        fxrestore(&to_ums->task_struct->thread.fpu);
        fxsave(&from_wkt->task_struct->thread.fpu);
         // restore ums fpu context
        fxrestore(&to_ums->saved_fpu_regs);
        fxsave(&to_ums->task_struct->thread.fpu);

        fxrestore(tmp_fpu);
        kfree(tmp_fpu);
#endif

#ifndef SWITCH_PT_REGS
        wake_up_process(to_ums->task_struct);
        schedule();
#endif 

}

/**
 * @brief handler used in the case that a worker thread previously
 * put on wait is awakened
 * 
 * @param from_wkt      pointer to worker_thread_t struct of the worker
 *                      awakened
 * @param p             task_struct of the worker thread awakened 
 */
void ums_do_unwait( worker_thread_t * from_wkt, struct task_struct *p)
{
#ifdef SWITCH_PT_REGS 
        struct fpu * tmp_fpu;
        tmp_fpu =  (struct fpu *) kmalloc(sizeof(struct fpu), GFP_KERNEL);
        memset(tmp_fpu, 0, sizeof(struct fpu));

        // save the pt_regs of the worker
        memcpy(&from_wkt->saved_pt_regs, task_pt_regs(p), sizeof(struct pt_regs));

        fxsave(tmp_fpu);
        // save the worker threa fpu in its own struct
        fxrestore(&p->thread.fpu);
        fxsave(&from_wkt->saved_fpu_regs);

        fxrestore(tmp_fpu);
        kfree(tmp_fpu);
#endif
        from_wkt->state = STATE_READY;
        p->state = TASK_INTERRUPTIBLE;
        return;
}

/*
 * Constructor destructor of this part
 */

/**
 * @brief Initialize the data structure needed for the UMS
 *
 * It initialize the following hashtables:
 *      - master_wkt_hashlist  (key:pid , data:worker_thread_t)
 *      - master_cq_hashlist  (key:id  , data:completion_queue_descriptor_t)
 *      - master_ums_hashlist (key:pid , data:ums_scheduler_t)
 *      - cq_id_bitmap        (is used to check the avaiable id for the completion queues)
 * 
 * @return int 
 */
int try_build_ums_core(void)
{

        hash_init(master_wkt_hashlist);
        hash_init(master_cq_hashlist);
        hash_init(master_ums_hashlist);
        bitmap_zero(cq_id_bitmap, BITMAP_CQ_SIZE);



#ifdef UMS_CORE_DEBUG
        printk(KERN_DEBUG MODULE_UMS_LOG "--------- BUILD UMS CORE ---------\n");
#endif

        return EXIT_SUCCESS;
}

/**
 * @brief Destroy the ums data structure
 * 
 */
void clear_ums_core(void)
{
        completion_queue_descriptor_t *cq_cursor;
        ums_scheduler_t *s_cursor;
        worker_thread_t *w_cursor;
        struct hlist_node * tmp;
        int bucket_cursor;

        ums_delete_hlist(w_cursor, tmp, bucket_cursor, master_wkt_hashlist, hlist);
        ums_delete_hlist(cq_cursor, tmp, bucket_cursor, master_cq_hashlist, hlist);
        ums_delete_hlist(s_cursor, tmp, bucket_cursor, master_ums_hashlist, hlist);



#ifdef UMS_CORE_DEBUG
        printk(KERN_DEBUG MODULE_UMS_LOG "--------- CLEAR UMS CORE ---------\n");
#endif

}
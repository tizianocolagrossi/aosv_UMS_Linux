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
DEFINE_RWLOCK(cqbit_rwlock);


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
        
        read_lock_irq(&ums->cq_list_des->rwlock);
        retrive_from_list(wkt_cursor, &ums->cq_list_des->completion_queue, list, pid, wkt_pid)
        read_unlock_irq(&ums->cq_list_des->rwlock);

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

        retrive_from_hlist(cq_descriptor, master_cq_hashlist , hlist, id, cq_id);

        return cq_descriptor;
}





/*
 * From here the function are accesible from other files
 */


int yield_to_ums(spinlock_t ioctl_lock)
{
        ums_scheduler_t * ums_to_switch;
        worker_thread_t * wkt_caller;
        unsigned long actual_current_time;
        unsigned long delta;

        ums_to_switch = __retrive_ums(current->pid);
        if(!ums_to_switch) goto current_not_ums;

        read_lock_irq(&ums_to_switch->rwlock);
        wkt_caller = ums_to_switch->wkt_sched_struct;
        read_unlock_irq(&ums_to_switch->rwlock);
        if (!wkt_caller) goto none_wkt;
        
        write_lock_irq(&ums_to_switch->rwlock);
        write_lock_irq(&wkt_caller->rwlock);

        
        actual_current_time =  ktime_get_ns();
        delta = actual_current_time - wkt_caller->time_at_switch;

        wkt_caller->total_runtime   +=  delta;
        wkt_caller->state            =  STATE_READY;
        wkt_caller->total_switch    +=  1;
        wkt_caller->scheduled_by     = -1;
        
        ums_to_switch->last_wkt_runtime =  delta;
        ums_to_switch->state            =  STATE_IDLE;
        ums_to_switch->total_switch    +=  1;
        ums_to_switch->pid_wkt_sched    = -1;
        ums_to_switch->wkt_sched_struct =  NULL;

        // switch pt_resgs
        memcpy(&wkt_caller->saved_pt_regs, task_pt_regs(current), sizeof(struct pt_regs));
        memcpy(task_pt_regs(current), &ums_to_switch->saved_pt_regs, sizeof(struct pt_regs));
        // switch fpu
        fxsave(&wkt_caller->saved_fpu_regs);
        fxrestore(&ums_to_switch->saved_fpu_regs);

        write_lock_irq(&ums_to_switch->cq_list_des->rwlock);

        list_add_tail(&wkt_caller->list, &ums_to_switch->cq_list_des->completion_queue);

        write_unlock_irq(&ums_to_switch->cq_list_des->rwlock);
        write_unlock_irq(&wkt_caller->rwlock);
        write_unlock_irq(&ums_to_switch->rwlock);

        return EXIT_SUCCESS;

none_wkt:
current_not_ums:
        return EXIT_FAILURE;
        
}


int execute_wkt(spinlock_t ioctl_lock, unsigned * u_wkt_pid)
{
        unsigned long actual_current_time;
        worker_thread_t * wkt_to_switch;
        ums_scheduler_t * ums_caller;
        unsigned wkt_pid_to_switch;
        int ret;

        ret = copy_from_user(&wkt_pid_to_switch, u_wkt_pid, sizeof(unsigned));
        if (ret != 0) goto err_cp_from_user;

        ums_caller = __retrive_ums(current->pid);
        if (!ums_caller) goto current_not_ums;

        wkt_to_switch = __retrive_wkt_from_cq(ums_caller, wkt_pid_to_switch);
        if(!wkt_to_switch) goto wkt_nex;

        write_lock_irq(&ums_caller->rwlock);

        write_lock_irq(&wkt_to_switch->rwlock);
        if (wkt_is_running(wkt_to_switch)) goto wkt_run;
        
        actual_current_time =  ktime_get_ns();
        
        wkt_to_switch->time_at_switch   = actual_current_time;
        wkt_to_switch->state            = STATE_RUNNING;
        wkt_to_switch->total_switch    += 1;
        wkt_to_switch->scheduled_by     = ums_caller->pid;
        
        ums_caller->state            = STATE_RUNNING;
        ums_caller->total_switch    += 1;
        ums_caller->pid_wkt_sched    = wkt_to_switch->pid;
        ums_caller->wkt_sched_struct = wkt_to_switch;

        
        memcpy(&ums_caller->saved_pt_regs, task_pt_regs(current), sizeof(struct pt_regs));
        memcpy(task_pt_regs(current), &wkt_to_switch->saved_pt_regs, sizeof(struct pt_regs));
        //save fpu
        fxsave(&ums_caller->saved_fpu_regs);
        fxrestore(&wkt_to_switch->saved_fpu_regs);

        write_lock_irq(&ums_caller->cq_list_des->rwlock);
        list_del(&wkt_to_switch->list);
        write_unlock_irq(&ums_caller->cq_list_des->rwlock);
        write_unlock_irq(&wkt_to_switch->rwlock);
        write_unlock_irq(&ums_caller->rwlock);

        return EXIT_SUCCESS;


wkt_run:
        write_unlock_irq(&wkt_to_switch->rwlock);
        write_unlock_irq(&ums_caller->rwlock);
        printk(KERN_ERR MODULE_UMS_LOG F_SCHED_WRK "Worker to schedule alredy running"); 
        return THREAD_RUNNG;
wkt_nex:
        printk(KERN_ERR MODULE_UMS_LOG F_SCHED_WRK "The worker %d is not in the completion queue!", wkt_pid_to_switch);
        return EXIT_FAILURE;
current_not_ums:
        printk(KERN_ERR MODULE_UMS_LOG F_SCHED_WRK "Execute required by a non UMS thread!");
        return EXIT_FAILURE;
err_cp_from_user:
        printk(KERN_ERR MODULE_UMS_LOG F_SCHED_WRK "Error during the copy of param from user");
        return -EFAULT; 
}

/**
 * @brief allocate and initialize the completion queue descriptor
 * 
 * @param ioctl_lock 
 * @param cq_id_u_ptr 
 * @return int the id of the completion queue allocated error if < 0
 */
int init_cq(spinlock_t ioctl_lock, void * cq_id_u_ptr)
{
        int cq_id;
        int bit_full;
        int ret;
        completion_queue_descriptor_t * new_cq_des;

        write_lock_irq(&cqbit_rwlock);
        bit_full = bitmap_full(cq_id_bitmap, BITMAP_CQ_SIZE);

        if(bit_full) goto bitmap_full;
        cq_id = find_first_zero_bit(cq_id_bitmap, BITMAP_CQ_SIZE);
        set_bit(cq_id, cq_id_bitmap);
        write_unlock_irq(&cqbit_rwlock);

        new_cq_des = (completion_queue_descriptor_t *) kmalloc(sizeof(completion_queue_descriptor_t), GFP_KERNEL);
        if(!new_cq_des) goto kmalloc_error;

        //initializing the completion qeueue descriptor
        new_cq_des->id = cq_id;
        new_cq_des->used_by_couter = 0;
        rwlock_init(&new_cq_des->rwlock); 
        INIT_LIST_HEAD(&new_cq_des->completion_queue);
        wmb();
        
        hash_add_rcu(master_cq_hashlist, &(new_cq_des->hlist), cq_id);

        ret = copy_to_user((void *)cq_id_u_ptr, (void *)&cq_id, sizeof(int));
        if (ret != 0) goto err_cp_to_user;

#ifdef UMS_CORE_DEBUG
        printk(KERN_DEBUG MODULE_UMS_LOG F_INIT_CQ "new cq with id %d created\n", cq_id);
#endif      
        
        Proc_Update_Cq_Created(cq_id);
        return EXIT_SUCCESS;

bitmap_full:
        write_unlock_irq(&cqbit_rwlock);
        return EXIT_FAILURE;
kmalloc_error:
        write_lock_irq(&cqbit_rwlock);
        clear_bit(cq_id, cq_id_bitmap);
        write_unlock_irq(&cqbit_rwlock);
        return EXIT_FAILURE;
err_cp_to_user:
        printk(KERN_ERR MODULE_UMS_LOG F_INIT_CQ "Error with copy_to_user");
        hlist_del_rcu(&new_cq_des->hlist);											\
        kfree(new_cq_des);
        return -EFAULT;
}

/**
 * @brief insert into the list insede the completion queue descriptors the pid
 *        inside the buffer passed
 * 
 * @param ioctl_lock 
 * @param args 
 * @return int 
 */
int append_to_cq(spinlock_t ioctl_lock, ums_cq_param_t * args)
{ 
        completion_queue_descriptor_t * cq_des;
        worker_thread_t * wkt_to_append;
        ums_cq_param_t k_cq_args;
        int pid_i;
        int ret;

        ret = copy_from_user(&k_cq_args, args, sizeof(ums_cq_param_t));
        if (ret != 0) goto err_cp_from_user;

        cq_des = __retrive_cq_descriptor(k_cq_args.completion_queue_id);
        if(!cq_des) goto no_cq;

        // cycle over the completion queue buffer
        for(pid_i = 0; pid_i < COMPLETION_QUEUE_BUFF; pid_i++){
                if(k_cq_args.pids[pid_i] < -1 ) goto err_pid_impossible;
                if(k_cq_args.pids[pid_i] == -1) break;
                wkt_to_append = __retrive_wkt(k_cq_args.pids[pid_i]);
                retrive_from_hlist(wkt_to_append, master_wkt_hashlist , hlist, pid, k_cq_args.pids[pid_i]);
                if(!wkt_to_append) goto err_pid_not_in_wkt_hlist;
                
                write_lock_irq(&cq_des->rwlock);
                list_add_tail(&wkt_to_append->list, &cq_des->completion_queue);
                write_unlock_irq(&cq_des->rwlock);
#ifdef UMS_CORE_DEBUG
                printk(KERN_DEBUG MODULE_UMS_LOG F_APPEND "append worker pid %d\n",k_cq_args.pids[pid_i]);
#endif
                Proc_Update_Worker_Appended(wkt_to_append->pid, k_cq_args.completion_queue_id);
        }

        return EXIT_SUCCESS;


err_pid_not_in_wkt_hlist:
        printk(KERN_ERR MODULE_UMS_LOG F_APPEND "Error pid not a worker");
        return EXIT_FAILURE;
err_pid_impossible:
        printk(KERN_ALERT MODULE_UMS_LOG F_APPEND "Error pid impossible inside the cq");
        return EXIT_FAILURE;
no_cq:
        return EXIT_FAILURE;
err_cp_from_user:
        printk(KERN_ERR MODULE_UMS_LOG F_APPEND "Error during the copy of param from user");
        return -EFAULT;
}


int dequeue_cq(spinlock_t ioctl_lock, ums_cq_param_t * ret_cq)
{
        int k_pids[COMPLETION_QUEUE_BUFF];
        int current_ums_pid;
        completion_queue_descriptor_t * cq_des;
        worker_thread_t * worker_cursor, * tmp;
        ums_scheduler_t * current_ums;
        int kp_i;
        int ret;

        current_ums_pid = current->pid;
        current_ums = __retrive_ums(current_ums_pid);
        if(!current_ums) goto current_not_ums;

        cq_des = current_ums->cq_list_des;
        if(!cq_des) goto ums_not_linked_to_cq;

        // initialize the array to -1
	for (kp_i = 0; kp_i<COMPLETION_QUEUE_BUFF; kp_i++)
		k_pids[kp_i] = -1;
        
        kp_i = 0;
        read_lock_irq(&cq_des->rwlock);
        list_for_each_entry_safe(worker_cursor, tmp, &cq_des->completion_queue, list) {
                read_lock_irq(&worker_cursor->rwlock);
                if(wkt_is_running(worker_cursor)) goto cnt;                
                k_pids[kp_i] = worker_cursor->pid;
		kp_i += 1;
		if (kp_i == COMPLETION_QUEUE_BUFF) goto buffer_full;
cnt:
                read_unlock_irq(&worker_cursor->rwlock);
	}
        read_unlock_irq(&cq_des->rwlock);

buffer_full:
	ret = copy_to_user((void *)ret_cq->pids, (void *)&k_pids, ( sizeof(int) * COMPLETION_QUEUE_BUFF) );
        if (ret != 0) goto err_cp_to_user;

        read_unlock_irq(&worker_cursor->rwlock);

	return EXIT_SUCCESS;

err_cp_to_user:
        read_unlock_irq(&worker_cursor->rwlock);
        printk(KERN_ALERT MODULE_UMS_LOG F_DEQUEUE "Error with copy_to_user");
        return -EFAULT;

ums_not_linked_to_cq:
        printk(KERN_ERR MODULE_UMS_LOG F_DEQUEUE "UMS has not a cq linked\n");
        return EXIT_FAILURE;
current_not_ums:
        printk(KERN_ERR MODULE_UMS_LOG F_DEQUEUE "Dequeue requested not by an ums\n");
        return EXIT_FAILURE;
}


int init_ums_scheduler(spinlock_t ioctl_lock, ums_km_param_t * args)
{
        ums_scheduler_t * new_ums;
        ums_km_param_t k_ums_km_param;
        completion_queue_descriptor_t * cq_descriptor;
        int ret;

        ret = copy_from_user(&k_ums_km_param, args, sizeof(ums_km_param_t));
        if (ret != 0) goto err_cp_from_user;

        read_lock_irq(&cqbit_rwlock);
        if(!test_bit(k_ums_km_param.cq_id, cq_id_bitmap)) goto not_cq;
        read_unlock_irq(&cqbit_rwlock);

        new_ums = (ums_scheduler_t *) kmalloc(sizeof(ums_scheduler_t), GFP_KERNEL);
        if(!new_ums) goto kmalloc_err;

        new_ums->task_struct      =  current;
        new_ums->pid              =  current->pid;
        new_ums->state            =  STATE_IDLE;
        new_ums->total_switch     =  0;
        new_ums->last_wkt_runtime =  0;
        new_ums->pid_wkt_sched    = -1;
        new_ums->owner_pid        =  k_ums_km_param.owner_pid;
        new_ums->ums_cq_id        =  k_ums_km_param.cq_id;
        rwlock_init(&new_ums->rwlock);

        //try to lonk to cq
        cq_descriptor = __retrive_cq_descriptor(k_ums_km_param.cq_id);
        if(cq_descriptor == NULL) goto cq_des_nex;

        new_ums->cq_list_des   = cq_descriptor;
        write_lock_irq(&cq_descriptor->rwlock);
        cq_descriptor->used_by_couter += 1;
        write_unlock_irq(&cq_descriptor->rwlock);

        wmb();

        hash_add_rcu(master_ums_hashlist, &(new_ums->hlist), current->pid); 
        Proc_Update_Ums_Created(new_ums->pid, new_ums->owner_pid, k_ums_km_param.cq_id);

        return EXIT_SUCCESS;

cq_des_nex:
        kfree(new_ums);
kmalloc_err:
        return EXIT_FAILURE;
not_cq:
        read_unlock_irq(&cqbit_rwlock);
        printk(KERN_ERR MODULE_UMS_LOG F_INIT_UMS "completion queue not valid");
        return EXIT_FAILURE;
err_cp_from_user:
        printk(KERN_ERR MODULE_UMS_LOG F_INIT_UMS "Error during the copy of param from user");
        return -EFAULT;
        
}


int end_ums_scheduler(spinlock_t ioctl_lock)
{
        ums_scheduler_t * current_ums;
        worker_thread_t * worker_cursor, * worker_cursor_safe;
        int id_safe;

        current_ums = __retrive_ums(current->pid);
        if(!current_ums) goto current_not_ums;

        read_lock_irq(&current_ums->rwlock);
        if (!current_ums->cq_list_des) goto no_cq_des;
        read_unlock_irq(&current_ums->rwlock);

        write_lock_irq(&current_ums->cq_list_des->rwlock);
        current_ums->cq_list_des->used_by_couter -= 1;
        write_unlock_irq(&current_ums->cq_list_des->rwlock);

        read_lock_irq(&current_ums->cq_list_des->rwlock);
        if(current_ums->cq_list_des->used_by_couter <= 0) goto delete;
        read_unlock_irq(&current_ums->cq_list_des->rwlock);

        return EXIT_SUCCESS;

delete:
        read_unlock_irq(&current_ums->cq_list_des->rwlock);
        
        clear_bit(current_ums->cq_list_des->id, cq_id_bitmap);
                
        ums_delete_list(worker_cursor, worker_cursor_safe, &current_ums->cq_list_des->completion_queue, list);
        write_unlock_irq(&current_ums->cq_list_des->rwlock);
        kfree(current_ums->cq_list_des);

        Proc_Update_Cq_Deleted(id_safe);
        return EXIT_SUCCESS;

no_cq_des:
        read_unlock_irq(&current_ums->rwlock);
        printk(KERN_ERR MODULE_UMS_LOG F_END_UMS "Error ums has not a cq");
        return EXIT_FAILURE;

current_not_ums:
        printk(KERN_ERR MODULE_UMS_LOG F_END_UMS "Error end ums not requested by an ums");
        return EXIT_FAILURE;
}


int init_worker_thread(spinlock_t ioctl_lock)
{
        worker_thread_t * new_wkt;
        pid_t wkt_pid;

        wkt_pid = current->pid;

        new_wkt = (worker_thread_t *) kmalloc(sizeof(worker_thread_t), GFP_KERNEL);
        if(!new_wkt) goto kmalloc_err;

        new_wkt->task_struct    = current;
        new_wkt->pid            = current->pid;
        new_wkt->state          = STATE_READY;
        new_wkt->time_at_switch = 0;
        new_wkt->total_switch   = 0;
        new_wkt->total_runtime  = 0;
        rwlock_init(&new_wkt->rwlock);

        //initialize context only if we use pt_regs for switch
        memcpy(&(new_wkt->saved_pt_regs) , task_pt_regs(current), sizeof(struct pt_regs));
        //initialze the saved fpu struct 
        memset(&(new_wkt->saved_fpu_regs), 0, sizeof(struct fpu));
        //save fpu
        fxsave(&(new_wkt->saved_fpu_regs));

        wmb();

        hash_add_rcu(master_wkt_hashlist, &(new_wkt->hlist), wkt_pid);

        Proc_Update_Worker_Created(wkt_pid);

        __set_current_state(TASK_INTERRUPTIBLE);
        schedule();

kmalloc_err:
        return EXIT_FAILURE;

}


int end_worker_thread(spinlock_t ioctl_lock)
{

        worker_thread_t * wkt_caller;
        ums_scheduler_t * current_ums;

        current_ums = __retrive_ums(current->pid);
        if(!current_ums) goto current_not_ums;

        write_lock_irq(&current_ums->rwlock);

        wkt_caller = current_ums->wkt_sched_struct;
        if(!wkt_caller) goto ums_no_wktsched;

        write_lock_irq(&wkt_caller->rwlock);
        // restore ums context
        memcpy(task_pt_regs(wkt_caller->task_struct), task_pt_regs(current), sizeof(struct pt_regs) );
        memcpy(task_pt_regs(current), &current_ums->saved_pt_regs, sizeof(struct pt_regs));
        //save fpu
        fxrestore(&current_ums->saved_fpu_regs);

        current_ums->wkt_sched_struct = NULL;
        hash_del_rcu(&wkt_caller->hlist);
        Proc_Update_Worker_Ended(wkt_caller->pid);
        
        wake_up_process(wkt_caller->task_struct);

        write_unlock_irq(&wkt_caller->rwlock);
        write_unlock_irq(&current_ums->rwlock);

        
        kfree(wkt_caller);
        schedule();

        return EXIT_SUCCESS;


ums_no_wktsched:
        write_unlock_irq(&current_ums->rwlock);
current_not_ums:
        return EXIT_FAILURE;

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
  
        if (&ums->cq_list_des->completion_queue == NULL) return info_ums;
        tmp = kasprintf(GFP_KERNEL,     "%s\n"
                                                "XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX\n"
                                                "                 COMPLETION LIST                 \n"
                                                "XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX\n"
                                                , info_ums);
        kfree(info_ums);
        info_ums = tmp;
        tmp = NULL;
        read_lock_irq(&ums->cq_list_des->rwlock);
        list_for_each_entry(worker_cursor, &ums->cq_list_des->completion_queue, list) {
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
        read_unlock_irq(&ums->cq_list_des->rwlock); 


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
        read_lock_irq(&wkt->rwlock);
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
        read_unlock_irq(&wkt->rwlock);
        return info_wkt;
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

#ifdef UMS_CORE_DEBUG
        printk(KERN_DEBUG MODULE_UMS_LOG "--------- CLEAR UMS CORE ---------\n");
#endif

}
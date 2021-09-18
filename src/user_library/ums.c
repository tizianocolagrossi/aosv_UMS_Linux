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
 *        user library
 * 
 * @file ums.c
 * @author Tiziano Colagrossi <tiziano.colagrossi@gmail.com>
 * 
 */

#include "ums.h"
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <assert.h>

sem_t ums_dev_sem;
int   global_ums_fd = -1;

/*
* Function not exported
*/
static pid_t __gettid(void);
static int __ums_open(void);
static void * __worker_thread_job_wrapper(void * args);
static int __init_worker_thread(void);
static cq_list_item_t * __init_cq_list_item(int cq_id);
static int __append_new_worker_to_cq(cq_list_item_t * cq_item, int worker_pid);
static int __end_ums_worker_thread(void);

volatile bool ums_mode_enabled = FALSE;

// init list head
LIST_HEAD(global_cq_list);

/**
 * @brief return the tid of the thread
 * 
 * @return pid_t 
 */
static pid_t __gettid(void){
    return syscall(SYS_gettid);
}

/**
 * @brief This function retrive the file descriptor of the devices
 * 
 * initially check if the device was alredy opened by checking the 
 * global_ums_fd. If the fd was not previously open this function
 * open it and than save it into the global_ums_fd variable.
 * 
 * @return int 
 */
static int __ums_open(){

    if (global_ums_fd >=0) return global_ums_fd;
    sem_wait(&ums_dev_sem);
    global_ums_fd = open(MODULE_PATH, O_RDWR, 0666);
    sem_post(&ums_dev_sem);
    if (global_ums_fd >=0) return global_ums_fd;
    return GENERAL_UMS_ERROR;

}

/**
 * @brief wrapper for the job of the worker thread
 *
 * it perform some initialization before launcing the 
 * job function.
 * 
 * @param args argument that the hob function take as input
 * @return void* 
 */
void *__worker_thread_job_wrapper(void *args){

    worker_thread_job_info_t * w_args = (worker_thread_job_info_t *)args;
    pid_t tid = __gettid();
    w_args->pid = tid;
    __init_worker_thread();
    //launch the actual job  
    w_args->job(w_args->args_routine);

    free(args);
    __end_ums_worker_thread();

    return NULL;
}

/**
 * @brief wrapper function of the ums it initialize 
 *        the ums before call the entry point
 * 
 * @param args pointer to ums_entry_info_t struct
 * @return void* 
 */
void *__ums_entry_point_wrapper(void *args){

    ums_entry_info_t * ums_args = (ums_entry_info_t *)args;
    ums_km_param_t k_ums_param;
    
#ifdef UMSLIB_DEBUG
    pid_t tid = __gettid();
    printf(MODULE_UMSLIB_LOG"ums pid: %d\n",tid);
#endif

    int ums_fd = __ums_open();
    if (ums_fd < 0) return (void *) GENERAL_UMS_ERROR;
    k_ums_param.owner_pid = ums_args->owner_pid;
    k_ums_param.cq_id     = ums_args->cq_id;
    ums_args->ret_value = ioctl(ums_fd, UMS_IOC_ENTER_UMS_SCHEDULING_MODE, &k_ums_param);
    if (ums_args->ret_value < 0) return (void *) GENERAL_UMS_ERROR;
    
    ums_args->entry(NULL);

    ums_fd = __ums_open();
    if (ums_fd < 0) return (void *) GENERAL_UMS_ERROR;
    int ret = ioctl(ums_fd, UMS_IOC_END_UMS_SCHEDULER);
    if (ret < 0) return (void *) GENERAL_UMS_ERROR;

    printf("Exiting from ums wrap###################################################\n");

    return NULL;
}

/**
 * @brief convert a pthread to a worker thread that an ums
 *        scheduler can schedule. It return the pid of the worker 
 *        thread that identify the worker.
 * 
 * @return int 
 */
static int __init_worker_thread(){

    int ums_fd = __ums_open();
    if (ums_fd < 0) return GENERAL_UMS_ERROR;
    int ret = ioctl(ums_fd, UMS_IOC_INIT_WORKER_THREAD);
    if (ret < 0) return GENERAL_UMS_ERROR;
    return EXIT_SUCCESS;

}

/**
 * @brief allocate a new cq item with an id associated it return the 
 *        cq_list_item_t pointer to the new item allocated
 *
 * @param cq_id id of the completion queue
 * @return cq_list_item_t* 
 */
static cq_list_item_t * __init_cq_list_item(int cq_id){
    cq_list_item_t * new_cq_item = (cq_list_item_t *) calloc(1, sizeof(cq_list_item_t));
    new_cq_item->id = cq_id;
    new_cq_item->used_by = 0;
    new_cq_item->cq_item.completion_queue_id = cq_id;
    for(int i = 0; i<COMPLETION_QUEUE_BUFF; i++){
        new_cq_item->cq_item.pids[i] = -1;
    }
    return new_cq_item;
}

/**
 * @brief insert worker pid into the ums_cq_param_t array of pids
 *        contained in cq_list_item_t. If the buffer with size COMPLETION_QUEUE_BUFF
 *        id full it also flush the buffer and empty it.
 * 
 * @param cq_l_item     pointer to cq_list_item_t struct where apped the worker
 * @param worker_pid    pid of the worker 
 * @return int 
 */
static int __append_new_worker_to_cq(cq_list_item_t * cq_l_item, int worker_pid){
    for(int i = 0; i<COMPLETION_QUEUE_BUFF; i++){
        if(cq_l_item->cq_item.pids[i] != -1) continue;
        // reached -1 item
        cq_l_item->cq_item.pids[i] = worker_pid;
#ifdef UMSLIB_DEBUG
        printf(MODULE_UMSLIB_LOG __F_APPEND "first 5 elem in cq [ %d, %d, %d, %d, %d ]\n",
            cq_l_item->cq_item.pids[0],cq_l_item->cq_item.pids[1],cq_l_item->cq_item.pids[2],
            cq_l_item->cq_item.pids[3],cq_l_item->cq_item.pids[4]); 
#endif
        if(i==COMPLETION_QUEUE_BUFF-1) FlushCompletionQueue(cq_l_item->id);
        return worker_pid;
    }
    return EXIT_FAILURE;
}

/**
 * @brief is called when the worker thread end its job
 *        is needed to remove the worker thread from the kernel module
 * 
 * @return int 
 */
static int __end_ums_worker_thread(){

    int ums_fd = __ums_open();
    if (ums_fd < 0) return GENERAL_UMS_ERROR;
    int ret = ioctl(ums_fd, UMS_IOC_END_WORKER_THREAD);
    if (ret < 0) return GENERAL_UMS_ERROR;
    return EXIT_SUCCESS;

}

/**
 * @brief the default entry poont for the ums schedulers
 * 
 * @param arguments 
 * @return void* 
 */
void *__default_entry_point(void *arguments){
    dequeued_cq_t deq_wkt;
    int ret;

    while(TRUE){
        ret = DequeueUmsCompletionListItems(&deq_wkt);
        if(ret == EXIT_UMS_MOD || ret == EXIT_FAILURE) break;
        ret = ExecuteUmsThread(deq_wkt.pids[0]);
        if(ret == THREAD_RUNNG) continue;
        if(ret == EXIT_FAILURE) break;
    }
    
    return NULL;
}

/*
* Function exported to the users
*/

/**
 * @brief Create a New Worker thread. It busy wait until the 
 *        pid entry in the new_job_struct is populated or is elapsed
 *        delta time. It return the pid of the new worker thread or -1
 *        in case of error
 * 
 * @param job_to_perform    job function of the worker thread
 * @param job_args          args used from the job function (optional)
 * @return int 
 */
int CreateNewWorker(worker_job job_to_perform, void * job_args){
    pthread_t thread_id;
    struct timespec ts;
    int sleep_milliseconds = 100;
    int max_delta_millisecond_time = 1000;
    int delta_i_max = max_delta_millisecond_time / sleep_milliseconds;
    int delta_i = 0;

    ts.tv_sec = sleep_milliseconds / 1000;
    ts.tv_nsec = (sleep_milliseconds % 1000) * 1000000;

    worker_thread_job_info_t * new_job_struct = (worker_thread_job_info_t *) calloc(1, sizeof(worker_thread_job_info_t));
    
    new_job_struct->pid = -1;
    new_job_struct->job = job_to_perform;
    new_job_struct->args_routine = job_args;
    
    pthread_create(&thread_id, NULL, __worker_thread_job_wrapper, (void *)new_job_struct);
    while(new_job_struct->pid == -1 && delta_i<delta_i_max ){
        nanosleep(&ts, NULL); 
    }

    return new_job_struct->pid;
    
}

/**
 * @brief called from a worker thread, it pauses the execution of 
 *        the current thread and the UMS scheduler entry point is executed 
 *        for determining the next thread to be scheduled
 * 
 * @return int 
 */
int UmsThreadYield(){

    int ums_fd = __ums_open();
    if (ums_fd < 0) return GENERAL_UMS_ERROR;
    int ret = ioctl(ums_fd, UMS_IOC_THREAD_YIELD);
    if (ret < 0) return GENERAL_UMS_ERROR;
    return ret;

}

/**
 * @brief called from a scheduler thread, it executes 
 *        the passed worker thread by switching the entire context
 * 
 * @param worker_id pid of the worker that will be executed
 * @return int 
 */
int ExecuteUmsThread(unsigned worker_id){

    int ums_fd = __ums_open();
    if (ums_fd < 0) return GENERAL_UMS_ERROR;
    int ret = ioctl(ums_fd, UMS_IOC_THREAD_EXECUTE, &worker_id);
    if (ret < 0) return GENERAL_UMS_ERROR;
    return ret;

}
 
/**
 * @brief converts a standard pthread in a UMS Scheduler thread, 
 *        the function takes as input a completion list of worker 
 *        threads and a entry point function
 * 
 * @param entry_point           entry point funtion of the ums
 * @param completion_queue_id   id of the completion queue used by the ums
 * @param n_cpu                 cpu where this ums will be scheduled
 * @return int 
 */
pthread_t UMS_thread_create(ums_entry_point entry_point, int completion_queue_id, int n_cpu){
    
    cpu_set_t cpus;
    cq_list_item_t * current_cq_item = NULL;
    pthread_attr_t attr;
    pthread_t thread_id;
    int number_of_cpu;
    int final_n_cpu;
    struct timespec ts;
    int sleep_milliseconds = 100;
    int max_delta_millisecond_time = 1000;
    int delta_i_max = max_delta_millisecond_time / sleep_milliseconds;
    int delta_i = 0;
    pid_t owner_tid = __gettid();
    INIT_UMS_ENTRY_STRUCT(new_ums_info, entry_point, completion_queue_id, owner_tid);

    ts.tv_sec = sleep_milliseconds / 1000;
    ts.tv_nsec = (sleep_milliseconds % 1000) * 1000000;

    number_of_cpu = sysconf(_SC_NPROCESSORS_ONLN);
    final_n_cpu = n_cpu % number_of_cpu;
    if(n_cpu >= number_of_cpu){
        printf("CPU selected not valid (MAX %d) thread scheduled into cpu %d instead %d\n", number_of_cpu, final_n_cpu, n_cpu );
    }
    pthread_attr_init(&attr);
    CPU_ZERO(&cpus);
    CPU_SET(final_n_cpu, &cpus);    
    pthread_attr_setaffinity_np(&attr, sizeof(cpu_set_t), &cpus);

    pthread_create(&thread_id, &attr, __ums_entry_point_wrapper, (void *)&new_ums_info);
    while(new_ums_info.ret_value == 1 && delta_i<delta_i_max ){
        nanosleep(&ts, NULL); 
    }

    // check if the id is alredy in the list
    struct list_head * current_item_list;
    list_for_each(current_item_list, &global_cq_list){
        current_cq_item = list_entry(current_item_list, cq_list_item_t, list);
        if(current_cq_item->id==completion_queue_id) break;
        current_cq_item = NULL;
    }
    if(current_cq_item == NULL) return -1;

    current_cq_item->used_by += 1;

    ums_mode_enabled = TRUE;

    return thread_id;

}

/**
 * @brief create N ums scheduler thread (N nuber of cores in the computer) and 
 *        will scheduke the thred from the completion queue id
 * 
 * @param entry_point           entry_point for the sceduler uf null will use the default
 * @param completion_queue_id   id of the completion queue used by the ums
 * @return ums_t* 
 */
ums_t * EnterUmsSchedulingMode(ums_entry_point entry_point, int completion_queue_id){
    ums_entry_point used_ep = (entry_point==NULL) ? __default_entry_point : entry_point;

    int n_cpu_ret = sysconf(_SC_NPROCESSORS_ONLN);

    ums_t * ums = (ums_t *) calloc(1, sizeof(ums_t));
    ums->ums_threads_list = (pthread_t *) calloc(n_cpu_ret, sizeof(pthread_t));
    ums->n_cpu = n_cpu_ret;
    ums->cq_id = completion_queue_id;

    for (int core_i = 0; core_i<n_cpu_ret; core_i++){
        ums->ums_threads_list[core_i] = UMS_thread_create(used_ep, completion_queue_id, core_i);
    }

    return ums;
}

/**
 * @brief Exit from UMS mode
 *
 * wait for all the ums to end
 * and then free the data structured used
 *
 * @param ums 
 */
void ExitFromUmsSchedulingMode(ums_t * ums){
    ums_mode_enabled = FALSE;
    printf("######################## EXIT UMS SCHED MODE\n\n");
    cq_list_item_t * cq_cursor;
    
    struct list_head * current_item_list;
    list_for_each(current_item_list, &global_cq_list){
        cq_cursor = list_entry(current_item_list, cq_list_item_t, list);
        if(cq_cursor->id==ums->cq_id) break;
        cq_cursor = NULL;
    }

    for(int cpu_i = 0; cpu_i < ums->n_cpu; cpu_i++){
        pthread_join(ums->ums_threads_list[cpu_i], NULL);
        if(cq_cursor != NULL) cq_cursor->used_by -= 1;
    }

    if(cq_cursor->used_by <= 0){
        list_del(&cq_cursor->list);
        free(cq_cursor);
    }

    
}

/**
 * @brief Create a Completion Queue object and return
 * the completion queue id. During this process it also 
 * init the data structure to buffer the worker thread. 
 * 
 * @return int 
 */
int CreateCompletionQueue(){

    int u_completion_queue_id = -1;
    int ums_fd = __ums_open();
    if (ums_fd < 0) return GENERAL_UMS_ERROR;
    int ret = ioctl(ums_fd, UMS_IOC_INIT_COMPLETION_QUEUE, &u_completion_queue_id);
    if (ret == CQ_FULL) return CQ_FULL;
    if (ret < 0) return GENERAL_UMS_ERROR;

    cq_list_item_t * new_cq_item = __init_cq_list_item(u_completion_queue_id);
    list_add(&new_cq_item->list, &global_cq_list);
    
    return u_completion_queue_id;

}

/**
 * @brief it insert a worker pid inside a completion queue
 * berfore it perform some check in order to see if the completion queue
 * exist
 * 
 * @param completion_queue_id   id of the completion queue where apped the worker
 * @param worker_pid            pid of the worker
 * @return int 
 */
int AppendToCompletionQueue(int completion_queue_id, int worker_pid){
    
    cq_list_item_t * current_cq_item = NULL;
    struct list_head * current_item_list;
    // check if the id is alredy in the list
    list_for_each(current_item_list, &global_cq_list){
        current_cq_item = list_entry(current_item_list, cq_list_item_t, list);
        if(current_cq_item->id==completion_queue_id) break;
        current_cq_item = NULL;
    }
    // if current_cq_item NULL there is no cq with this id
    // so return error 
    if(current_cq_item == NULL) return EXIT_FAILURE;
    
    // here current_cq_item contain the right id
#ifdef UMSLIB_DEBUG
    printf(MODULE_UMSLIB_LOG F_APPEND"cq found id: %d\n",current_cq_item->id);
#endif
    __append_new_worker_to_cq(current_cq_item, worker_pid);

    return EXIT_SUCCESS;
}

/**
 * @brief actually insert the worker pid into the data structure
 * in the kernel using the device ioctl
 * 
 * @param completion_queue_id id of the completion queue 
 * @return int 
 */
int FlushCompletionQueue(int completion_queue_id){

    cq_list_item_t * current_cq_item = NULL;
    struct list_head * current_item_list;
    // check if the id is alredy in the list
    list_for_each(current_item_list, &global_cq_list){
        current_cq_item = list_entry(current_item_list, cq_list_item_t, list);
        if(current_cq_item->id==completion_queue_id) break;
        current_cq_item = NULL;
    }
    // if current_cq_item NULL there is no cq with this id
    // so return error 
    if(current_cq_item == NULL) return EXIT_FAILURE;
#ifdef UMSLIB_DEBUG
    printf(MODULE_UMSLIB_LOG F_FLUSH "found cq to flush with id: %d\n",completion_queue_id);
#endif
    // here current_cq_item contain the right id 

    int ums_fd = __ums_open();
    if (ums_fd < 0) return GENERAL_UMS_ERROR;
    ums_cq_param_t t = current_cq_item->cq_item;
   
    int ret = ioctl(ums_fd, UMS_IOC_APPEND_TO_COMPLETION_QUEUE, &t);
    if (ret < 0) return GENERAL_UMS_ERROR;

    for(int i = 0; i<COMPLETION_QUEUE_BUFF; i++){
        current_cq_item->cq_item.pids[i] = -1;
    }

    return EXIT_SUCCESS;

}

/**
 * @brief dequeue the first 100 pid of the workers inside the return queue
 * 
 * @param return_cq pointer to dequeued_cq_t struct
 * @return int 
 */
int DequeueUmsCompletionListItems(dequeued_cq_t * return_cq){
    
    ums_cq_param_t return_cq_dequeued;
    void * ret_memcpy;
    int flag_first_iteration = 0;
    struct timespec ts;
    int sleep_milliseconds = 100;

    ts.tv_sec = sleep_milliseconds / 1000;
    ts.tv_nsec = (sleep_milliseconds % 1000) * 1000000;

    int ums_fd = __ums_open();
    if (ums_fd < 0) return GENERAL_UMS_ERROR;
    
    do{
        if ( flag_first_iteration) nanosleep(&ts, NULL);
        if (!flag_first_iteration) flag_first_iteration = 1;
        int ret = ioctl(ums_fd, UMS_IOC_DEQUEUE_COMPLETION_LIST, &return_cq_dequeued);
        if (ret < 0) return GENERAL_UMS_ERROR;
        
        if(return_cq_dequeued.pids[0]==-1 && !ums_mode_enabled)return EXIT_UMS_MOD;

    }while(return_cq_dequeued.pids[0]==-1);
    
    ret_memcpy = memcpy( (void *) return_cq->pids , (void *) return_cq_dequeued.pids , (sizeof(int) * COMPLETION_QUEUE_BUFF) );
    if (ret_memcpy != return_cq->pids) return GENERAL_UMS_ERROR;

    return EXIT_SUCCESS;

}

/*
* only debug purpose
*/

void resetUMSFlag(void){
    ums_mode_enabled = FALSE;
}


/*
* constructor and destructor of the library
*/

/**
 * @brief initialize the dev semaphore
 * 
 */
__attribute__((constructor)) void start(void) {
    sem_init(&ums_dev_sem, 0, 1);
}

/**
 * @brief close the device file 
 * 
 */
__attribute__((destructor)) void end(void) {
    close(global_ums_fd);
}
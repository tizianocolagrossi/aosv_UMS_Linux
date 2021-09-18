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
 * @brief This file is the header of the proc.c file
 * 
 * @file proc.h
 * @author Tiziano Colagrossi <tiziano.colagrossi@gmail.com>
 * 
 */

#ifndef __PROC_H
#define __PROC_H

#include "utility.h"
#include "ums.h"

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/hashtable.h>

#define MODULE_PROC_LOG "UMS_PROC: "

#define UMS_PROC_DEBUG

#define HASH_KEY_SIZE 10
#define NAME_BUFF     30

#define PROC_DIR      "ums"
#define SCHED_DIR     "schedulers"
#define WKT_DIR       "workers"
#define UMS_INFO      "ums_info"
#define WKT_INFO      "wkt_info"

#define UMS_ALL_DIR   ".all_ums"
#define WKT_ALL_DIR   ".all_wkt"
#define  CQ_ALL_DIR   ".all_cq"


/**
 * @brief contains the data of the ums owner directory that 
 * contain the scheduler dir with all the UMS thread created
 * by the owner
 */
typedef struct owner_proc{
    struct proc_dir_entry *   dir_entry; /**< directory of the owner*/
    struct proc_dir_entry * sched_entry; /**< directory that will contain all the ums schedulers*/
    unsigned                        pid; /**< pid of the owner*/
    char            name_dir[NAME_BUFF]; /**< name of the owner's dir (pid to string)*/
    struct hlist_node             hlist; /**< hlist_node used for htable*/
}owner_proc_t;

/**
 * @brief contains the data of the worker thread directory that 
 * contain the info file of the worker.
 */
typedef struct worker_proc{
    struct proc_dir_entry *   dir_entry; /**< proc_dir_entry of the directory of the worker thread*/
    struct proc_dir_entry *  info_entry; /**< proc_dir_entry of the file containing the info of the worker thread*/
    struct proc_dir_entry *     link_to; /**< will contain the proc_dir_entry of the link to the completion queue dir*/
    unsigned                        pid; /**< pid of the worker threead*/
    char            name_dir[NAME_BUFF]; /**< name of the worker dir (pid to string)*/
    char                path[NAME_BUFF]; /**< path to reach the worker dir inside of the WKT_ALL_DIR*/
    struct hlist_node             hlist; /**< hlist_node used for htable*/
}worker_proc_t;

/**
 * @brief contains the data of the UMS thread directory that will contain
 * the info file of the UMS and the directory of the completion queue
 * used by the UMS.
 */
typedef struct ums_proc{
    struct proc_dir_entry *   dir_entry; /**< proc_dir_entry of the directory for the UMS thread*/
    struct proc_dir_entry *  info_entry; /**< proc_dir_entry of the file containing the info of the UMS thread*/
    struct proc_dir_entry *     link_to; /**< will contain the proc_dir_entry of the link to the owner dir*/
    unsigned                  pid_owner; /**< pid of the owner of the UMS thread*/
    unsigned                        pid; /**< pid of the UMD thread*/
    int                           cq_id; /**< id of the completion queue used by the UMS thread*/
    char            name_dir[NAME_BUFF]; /**< name of the UMS thread dir (pid to string)*/
    char                path[NAME_BUFF]; /**< path to reach the UMS thread dir inside of the UMS_ALL_DIR*/
    struct hlist_node             hlist; /**< hlist_node used for htable*/
}ums_proc_t;

/**
 * @brief contains the data of the completion queue directory
 * that will contain the worker thread contained in the completion queue.
 */
typedef struct cq_proc{
    struct proc_dir_entry *   dir_entry; /**< proc_dir_entry of the directory for the completion queue*/
    struct proc_dir_entry *     link_to; /**< will contain the proc_dir_entry of the link to the UMS scheduler that use this completion queue*/
    int                           cq_id; /**< id of the completion queue*/
    char            name_dir[NAME_BUFF]; /**< name of the completion queue dir (id to string)*/
    char                path[NAME_BUFF]; /**< path to reach the completion queue dir inside of the CQ_ALL_DIR*/
    struct hlist_node             hlist; /**< hlist_node used for htable*/
}cq_proc_t;

/*
* Function exported for create file and directory inside the procfs 
*/
int Proc_Update_Worker_Created  (int pid    );
int Proc_Update_Worker_Ended    (int wkt_pid);
int Proc_Update_Worker_Appended (int wkt_pid , int id      );
int Proc_Update_Ums_Created     (int pid     , int pid_owner, int id);
int Proc_Update_Ums_Ended       (int ums_pid);
int Proc_Update_Cq_Created      (int id     );
int Proc_Update_Cq_Deleted      (int id     );

/*
* Function used to initialize this part of the UMS kernel module
*/
int try_build_ums_proc (void);
int clear_ums_proc     (void);

#endif
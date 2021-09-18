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
 * @brief This file contains the functionalities for managing
 * the exposure of the stats info using the procfs.
 * 
 * @file proc.c
 * @author Tiziano Colagrossi <tiziano.colagrossi@gmail.com>
 */

#include "proc.h"
#include "shared.h"

/*
* Functions not exported
*/

static ssize_t default_proc_write(struct file *file, const char __user *ubuf, size_t count, loff_t *ppos);
static ssize_t proc_read_ums_info(struct file *file, char __user *ubuf, size_t count, loff_t *ppos);
static ssize_t proc_read_wkt_info(struct file *file, char __user *ubuf, size_t count, loff_t *ppos);

static inline owner_proc_t * __create_owner(int owner_pid);
static inline void __delete_owner(owner_proc_t * own);

static inline ums_proc_t * __create_ums(int owner_pid, int pid, int cq_id);
static inline void __delete_ums(ums_proc_t * ums);

static inline worker_proc_t * __create_wkt(int pid);
static inline void __delete_wkt(worker_proc_t * wkt);

static inline cq_proc_t * __create_cq(int cq_id);
static inline void __delete_cq(cq_proc_t * cq);

/*
* Main folders used as root
*/

static struct proc_dir_entry * ums_dir;
static struct proc_dir_entry * ums_all_dir;
static struct proc_dir_entry * wkt_all_dir;
static struct proc_dir_entry * cq_all_dir;

/*
* Hashtables used to mantain all ums thread, worker thread, 
* owner and completion queue (only the data structired of this part)
*/

DEFINE_HASHTABLE(own_htable, HASH_KEY_SIZE);
DEFINE_HASHTABLE(ums_htable, HASH_KEY_SIZE);
DEFINE_HASHTABLE(wkt_htable, HASH_KEY_SIZE);
DEFINE_HASHTABLE( cq_htable, HASH_KEY_SIZE);

/**
 * @brief operation for the UMS info file
 */
static struct proc_ops ums_info_pops =
{
	.proc_read = proc_read_ums_info,
	.proc_write = default_proc_write,
};

/**
 * @brief operation for the worker thread info file
 */
static struct proc_ops wkt_info_pops =
{
	.proc_read = proc_read_wkt_info,
	.proc_write = default_proc_write,
};

/**
 * @brief handle write over a file (not supported)
 * so in this case return -EINVAL
 * 
 * @param file		file where a write was requested
 * @param ubuf		contain the data to write
 * @param count 	lengt of the buffer
 * @param ppos 		position of the cursor
 * @return ssize_t 
 */
static ssize_t default_proc_write(struct file *file, const char __user *ubuf, size_t count, loff_t *ppos)
{
        printk(KERN_ALERT MODULE_PROC_LOG "write not supported.\n");
        return -EINVAL;
}

/*
 * Handler read info
 */

/**
 * @brief handle read operation over ums file
 * 
 * @param file		file where a read was requested
 * @param ubuf 		will contain the data to read
 * @param count 	lengt of the buffer
 * @param ppos 		position of the cursor
 * @return ssize_t 
 */
static ssize_t proc_read_ums_info(struct file *file, char __user *ubuf, size_t count, loff_t *ppos)
{	
	char * k_buff;
	int len;
	int ret;
	long ums_pid;
	
	// trasform the name of the file (ums_pid) into type long   
	ret = kstrtol(file->f_path.dentry->d_parent->d_iname, 10, &ums_pid);
	if(ret <0) return -EFAULT;

	// get the info of the ums in string 
	k_buff =  Get_UMS_Info(ums_pid);
	if (k_buff == NULL) return 0;
	
        len = strlen(k_buff);

	if (*ppos > 0 || count < len) return 0;
	if (copy_to_user(ubuf, k_buff, len)) return -EFAULT;
        *ppos = len;

	kfree(k_buff);
		
	return len;
}

/**
 * @brief handle read operation over worker file
 * 
 * @param file		file where a read was requested
 * @param ubuf 		will contain the data to read
 * @param count 	lengt of the buffer
 * @param ppos 		position of the cursor
 * @return ssize_t 
 */
static ssize_t proc_read_wkt_info(struct file *file, char __user *ubuf, size_t count, loff_t *ppos)
{
        char * k_buff;
	int len;
	int ret;
	long wkt_pid;

	// trasform the name of the file (wlt_pid) into type long 
	ret = kstrtol(file->f_path.dentry->d_parent->d_iname, 10, &wkt_pid);
	if(ret <0) return -EFAULT;

	// get the info of the worker in string 
	k_buff =  Get_WKT_Info(wkt_pid);
	if (k_buff == NULL) return 0;	
        
	len = strlen(k_buff);
	if (*ppos > 0 || count < len) return 0;
	if (copy_to_user(ubuf, k_buff, len)) return -EFAULT;
        
	*ppos = len;
	kfree(k_buff);
	
	return len;
}


/*
 * function not exported
 */

/**
 * @brief create the owner folder of the UMS and link the owner into 
 * the master PROC_DIR.
 * 
 * @param owner_pid pid of the owner
 * @return owner_proc_t*
 */
static inline owner_proc_t * __create_owner(int owner_pid){

	owner_proc_t * new_owner_ptr;
	int ret;

	new_owner_ptr = (owner_proc_t *) kmalloc(sizeof(owner_proc_t), GFP_KERNEL);
	if(!new_owner_ptr) return NULL;

	// create the name of the directory from the pid of the owner
	ret = snprintf(new_owner_ptr->name_dir, NAME_BUFF, "%d",owner_pid);
	new_owner_ptr->name_dir[NAME_BUFF-1] = '\0';
	if (ret < 0) return NULL; 

	// create the directory of the owner
	new_owner_ptr->dir_entry   = proc_mkdir(new_owner_ptr->name_dir, ums_dir);
	if(new_owner_ptr->dir_entry == NULL) goto err_dir;
    	
	// create the scheduler directory inside the owner dir
	new_owner_ptr->sched_entry = proc_mkdir(SCHED_DIR, new_owner_ptr->dir_entry);
	if(new_owner_ptr->sched_entry == NULL) goto err_sched;
    	
	new_owner_ptr->pid = owner_pid;

	hash_add(own_htable, &(new_owner_ptr->hlist), new_owner_ptr->pid);

	return new_owner_ptr;

err_sched:
	proc_remove(new_owner_ptr->sched_entry);
err_dir:
	proc_remove(new_owner_ptr->dir_entry);
	kfree(new_owner_ptr);
	return NULL;

}

/**
 * @brief delete owner foder from the procfs deleting
 * also the UMS folder
 * 
 * @param own pointer to the owner_proc_t struct to delete
 */
static inline void __delete_owner(owner_proc_t * own){
	proc_remove(own->sched_entry);
	proc_remove(own->dir_entry);
	hash_del(&own->hlist);
	kfree(own);
	return;
}

/**
 * @brief create the ums folder with its info file inside the UMS_ALL_DIR. 
 * Link it into the owner folder and link the completion queue folder inside 
 * the ums folder
 * 
 * @param owner_pid	pid of the owner of the UMS
 * @param pid 		pid of the UMS thread
 * @param cq_id 	id of the completion queue used
 * @return ums_proc_t* 
 */
static inline ums_proc_t * __create_ums(int owner_pid, int pid, int cq_id){
	ums_proc_t * new_ums_ptr;
	int ret;

	new_ums_ptr = (ums_proc_t *) kmalloc(sizeof(ums_proc_t), GFP_KERNEL);
	if(!new_ums_ptr) return NULL;

	// create the name of the directory from the pid of the UMS
	ret = snprintf(new_ums_ptr->name_dir, NAME_BUFF, "%d",pid);
	new_ums_ptr->name_dir[NAME_BUFF-1] = '\0';
	if (ret < 0) return NULL;

	// create the directory of the UMS
	new_ums_ptr->dir_entry   = proc_mkdir(new_ums_ptr->name_dir, ums_all_dir);
	if(new_ums_ptr->dir_entry == NULL) goto err_dir;

	// create the info file of the ums
    	new_ums_ptr->info_entry = proc_create(UMS_INFO, S_IRUGO, new_ums_ptr->dir_entry, &ums_info_pops);
	if(new_ums_ptr->info_entry == NULL) goto err_info;
    	
	new_ums_ptr->pid = pid;
	new_ums_ptr->pid_owner = owner_pid;
	new_ums_ptr->cq_id = cq_id;

	// create the absolute path to reach the ums directory inside the UMS_ALL_DIR
	ret = snprintf(new_ums_ptr->path, NAME_BUFF, "/proc/"PROC_DIR"/"UMS_ALL_DIR"/%d/",pid);
	if (ret < 0 || ret >= NAME_BUFF ) goto err_info;

	hash_add(ums_htable, &(new_ums_ptr->hlist), new_ums_ptr->pid);

	return new_ums_ptr;

err_info:
	proc_remove(new_ums_ptr->info_entry);
err_dir:
	proc_remove(new_ums_ptr->dir_entry);
	kfree(new_ums_ptr);
	return NULL;
}

/**
 * @brief remove the UMS folder from the procfs
 * 
 * @param ums pointer to the ums_proc_t  struct to delete
 */
static inline void __delete_ums(ums_proc_t * ums){
	ums_proc_t * ums_cursor;
	owner_proc_t * owner;
	int bkt;

	proc_remove(ums->info_entry);
	proc_remove(ums->dir_entry);

	// here in to ums->pid_owner i can scann all the ums list
	// and if there is no other ums woth the same pid owner then 
	// i can remove the owner
	hash_for_each(ums_htable, bkt, ums_cursor, hlist){
		if(ums_cursor->pid_owner == ums->pid_owner && ums_cursor->pid != ums->pid) goto del_only_ums;
	}
	get_hlist_item_by_id(owner, own_htable , hlist, pid, ums->pid_owner);
	if(owner == NULL) goto del_only_ums;
	__delete_owner(owner);

del_only_ums:
	hash_del(&ums->hlist);
	kfree(ums);
	return;
}

/**
 * @brief create the worker folder with its info file inside the WKT_ALL_DIR.
 * 
 * @param pid pid of the worker thread
 * @return worker_proc_t* 
 */
static inline worker_proc_t * __create_wkt(int pid){
	worker_proc_t * new_wkt_ptr;
	int ret;

	new_wkt_ptr = (worker_proc_t *) kmalloc(sizeof(worker_proc_t), GFP_KERNEL);
	if(!new_wkt_ptr) return NULL;

	// create the name of the directory from the pid of the worker
	ret = snprintf(new_wkt_ptr->name_dir, NAME_BUFF, "%d",pid);
	new_wkt_ptr->name_dir[NAME_BUFF-1] = '\0';
	if (ret < 0) return NULL;

	// create the directory of the worker
	new_wkt_ptr->dir_entry   = proc_mkdir(new_wkt_ptr->name_dir, wkt_all_dir);
	if(new_wkt_ptr->dir_entry == NULL) goto err_dir;
    	
	// create the info file of the worker
	new_wkt_ptr->info_entry = proc_create(WKT_INFO, S_IRUGO, new_wkt_ptr->dir_entry, &wkt_info_pops);
	if(new_wkt_ptr->info_entry == NULL) goto err_info;
    	
	new_wkt_ptr->pid = pid;

	// create the absolute path to reach the worker directory inside the WKT_ALL_DIR
	ret = snprintf(new_wkt_ptr->path, NAME_BUFF, "/proc/"PROC_DIR"/"WKT_ALL_DIR"/%d/",pid);
	if (ret < 0 || ret >= NAME_BUFF ) goto err_info;

	hash_add(wkt_htable, &(new_wkt_ptr->hlist), new_wkt_ptr->pid);

	return new_wkt_ptr;

err_info:
	proc_remove(new_wkt_ptr->info_entry);
err_dir:
	proc_remove(new_wkt_ptr->dir_entry);
	kfree(new_wkt_ptr);
	return NULL;
}

/**
 * @brief delete the worker dir and the info file from the
 * proc fs
 * 
 * @param wkt pointer to the worker_proc_t struct to delete
 */
static inline void __delete_wkt(worker_proc_t * wkt){
	proc_remove(wkt->info_entry);
	proc_remove(wkt->dir_entry);
	hash_del(&wkt->hlist);
	kfree(wkt);
	return;
}

/**
 * @brief create the completion queue filder into the CQ_ALL_DIR
 * 
 * @param cq_id id of the completion queue
 * @return cq_proc_t* 
 */
static inline cq_proc_t * __create_cq(int cq_id){
	cq_proc_t * new_cq_ptr;
	int ret;

	new_cq_ptr = (cq_proc_t *) kmalloc(sizeof(cq_proc_t), GFP_KERNEL);
	if(!new_cq_ptr) return NULL;

	// create the name of the directory from the id of the completion queue
	ret = snprintf(new_cq_ptr->name_dir, NAME_BUFF, "%d",cq_id);
	new_cq_ptr->name_dir[NAME_BUFF-1] = '\0';
	if (ret < 0) return NULL;

	// create the directory of the completion queue
	new_cq_ptr->dir_entry   = proc_mkdir(new_cq_ptr->name_dir, cq_all_dir);
	if(new_cq_ptr->dir_entry == NULL) goto err_dir;

    	new_cq_ptr->cq_id = cq_id;

	// create the absolute path to reach the completion queue directory inside the CQ_ALL_DIR
	ret = snprintf(new_cq_ptr->path, NAME_BUFF, "/proc/"PROC_DIR"/"CQ_ALL_DIR"/%d/",cq_id);
	if (ret < 0 || ret >= NAME_BUFF ) goto err_dir;

	hash_add(cq_htable, &(new_cq_ptr->hlist), new_cq_ptr->cq_id);


	return new_cq_ptr;

err_dir:
	proc_remove(new_cq_ptr->dir_entry);
	kfree(new_cq_ptr);
	return NULL;
}

/**
 * @brief delete the completion queue folder from the proc
 * 
 * @param cq pointer to the cq_proc_t struct to delete
 */
static inline void __delete_cq(cq_proc_t * cq){
	proc_remove(cq->dir_entry);
	hash_del(&cq->hlist);
	kfree(cq);
	return;
}

/*
* Function exported for create file and directory inside the procfs 
*/

/**
 * @brief Insert the created worker thread into the procfs
 * 
 * @param wkt_pid pid of the worker
 * @return int 
 */
int Proc_Update_Worker_Created(int wkt_pid)
{
	worker_proc_t * new_wkt;
	new_wkt = __create_wkt(wkt_pid);

	if(new_wkt == NULL) return EXIT_FAILURE;
	return EXIT_SUCCESS;
}

/**
 * @brief Remove the worker that has ended from the procfs
 * 
 * @param wkt_pid pid of the worker
 * @return int 
 */
int Proc_Update_Worker_Ended(int wkt_pid)
{
	worker_proc_t * del_wkt;

	get_hlist_item_by_id(del_wkt, wkt_htable , hlist, pid, wkt_pid);
	if(del_wkt == NULL) return EXIT_FAILURE;
	__delete_wkt(del_wkt);

	return EXIT_SUCCESS;
}

/**
 * @brief link a worker to its completion queue
 * 
 * @param wkt_pid pid of the worker
 * @param id completion queue where the worker need to be appended
 * @return int 
 */
int Proc_Update_Worker_Appended(int wkt_pid, int id)
{
	worker_proc_t * wkt;
	cq_proc_t * cq;

	// get the worker
	get_hlist_item_by_id(wkt, wkt_htable , hlist, pid, wkt_pid);
	if(wkt == NULL) return EXIT_FAILURE;

	//get the completion queue
	get_hlist_item_by_id(cq, cq_htable , hlist, cq_id, id);
	if(cq == NULL) return EXIT_FAILURE;
	
	// link the worker into the completion queue directory
	wkt->link_to = proc_symlink(wkt->name_dir, cq->dir_entry ,wkt->path);
	if(wkt->link_to == NULL) return EXIT_FAILURE;

	return EXIT_FAILURE;
}

/**
 * @brief Insert the new UMS into the proc fs and link the
 * completion queue path to the UMS folder.
 * 
 * @param ums_pid 	pid of the UMS
 * @param owner_pid 	pid of the owner process of the UMS
 * @param id 		completion queue id of the UMS
 * @return int 
 */
int Proc_Update_Ums_Created(int ums_pid, int owner_pid, int id)
{
	ums_proc_t * new_ums;	
	owner_proc_t * own;
	cq_proc_t * cq;
	int is_new_own;
	is_new_own = 0;

	new_ums = __create_ums(owner_pid, ums_pid, id);
	if(new_ums == NULL) return EXIT_FAILURE;

	// get the owner struct
	get_hlist_item_by_id(own, own_htable , hlist, pid, owner_pid);
	if(own == NULL){
		is_new_own = 1;
		own = __create_owner(owner_pid);
		if(own == NULL)goto err_own;
	}

	// link the ums into the owner directory
	new_ums->link_to = proc_symlink(new_ums->name_dir, own->sched_entry ,new_ums->path);
	if(new_ums->link_to == NULL) goto err_link_own;

	get_hlist_item_by_id(cq, cq_htable , hlist, cq_id, id);
	if(cq == NULL) goto err_link_own;

	// link the completion queue to the ums directory
	cq->link_to = proc_symlink(WKT_DIR, new_ums->dir_entry ,cq->path);
	if(cq->link_to == NULL) goto err_link_cq;

	return EXIT_SUCCESS;

err_link_cq:
	proc_remove(cq->link_to);
err_link_own:
	proc_remove(new_ums->link_to);
	if(is_new_own) __delete_owner(own); 
err_own:
	__delete_ums(new_ums);
	return EXIT_FAILURE;

}

/**
 * @brief Remove the UMS directory from the proc fs
 * 
 * @param ums_pid pid of the ums
 * @return int 
 */
int Proc_Update_Ums_Ended(int ums_pid)
{
	ums_proc_t * ums;

	get_hlist_item_by_id(ums, ums_htable , hlist, pid, ums_pid);
	if(ums == NULL) return EXIT_FAILURE;

	proc_remove(ums->link_to);
	__delete_ums(ums);

	return EXIT_SUCCESS;
}

/**
 * @brief create a completion queue folder into the proc fs
 * 
 * @param id completion queue id
 * @return int 
 */
int Proc_Update_Cq_Created(int id)
{
	cq_proc_t * new_cq;

	new_cq = __create_cq(id);
	if(new_cq == NULL) return EXIT_FAILURE;

	return EXIT_SUCCESS;
}

/**
 * @brief remove the completion queue folder from the proc fs
 * 
 * @param id completion queue id
 * @return int 
 */
int Proc_Update_Cq_Deleted(int id)
{
	cq_proc_t * cq;

	get_hlist_item_by_id(cq, cq_htable , hlist, cq_id, id);
	if(cq == NULL) return EXIT_FAILURE;

	proc_remove(cq->link_to);
	__delete_cq(cq);

	return EXIT_FAILURE;
}

/*
* Function used to initialize this part of the UMS kernel module
*/

/**
 * @brief initialize the main data structures for the proc part
 * for the ums
 * 
 * @return int 
 */
int try_build_ums_proc(void)
{ 
	// create the master dir in procfs (/proc/ums/)
	ums_dir = proc_mkdir(PROC_DIR, NULL);
	if (ums_dir == NULL) return EXIT_FAILURE;
	
	// create the UMS_ALL_DIR into the master dir
	ums_all_dir = proc_mkdir(UMS_ALL_DIR, ums_dir);
	if (ums_all_dir == NULL) return EXIT_FAILURE;
	
	// create the WKT_ALL_DIR into the master dir
	wkt_all_dir = proc_mkdir(WKT_ALL_DIR, ums_dir);
	if (wkt_all_dir == NULL) return EXIT_FAILURE;
	
	// create the CQ_ALL_DIR into the master dir
	cq_all_dir = proc_mkdir(CQ_ALL_DIR, ums_dir);
	if (cq_all_dir == NULL) return EXIT_FAILURE;

    	hash_init(ums_htable);
    	hash_init(own_htable);
	hash_init(wkt_htable);
   	hash_init(cq_htable);

	return EXIT_SUCCESS;
}

/**
 * @brief clear the data structures allocated from this part 
 * of the UMS kernel module
 * 
 * @return int 
 */
int clear_ums_proc(void)
{
	proc_remove(ums_dir);
    	proc_remove(ums_all_dir);
    	proc_remove(wkt_all_dir);
    	proc_remove(cq_all_dir);

	return EXIT_SUCCESS;
}
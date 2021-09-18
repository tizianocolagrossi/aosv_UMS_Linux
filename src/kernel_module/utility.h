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
 * @brief This file contains utility macro definition 
 * 
 * @file utility.h
 * @author Tiziano Colagrossi <tiziano.colagrossi@gmail.com>
 * 
 */

#ifndef __UTILITY_HEADER
#define __UTILITY_HEADER

/**
 * @brief allocate and append to tail a new item to a list
 *
 * @param new_item_pointer	will be filled with the new item address.
 * @param list_head  		list head to add it before
 * @param member     		the name of the list_head within the struct
 * @param item_type             the item type of the new element
 */
#define add_new_item_to_list(new_item_pointer, list_head, member, item_type)							\
	new_item_pointer = (item_type *) kmalloc(sizeof(item_type), GFP_KERNEL);						\
	list_add_tail(&(new_item_pointer->member), list_head)

/**
 * @brief allocate and append to tail a new item to a hlist
 * 
 * @param new_item_pointer	will be filled with the new item address.
 * @param hashtable 		hashtable to add to
 * @param node    		the &struct hlist_node of the object to be added
 * @param item_type             the item type of the new element
 * @param identifier          	the key of the object to be added
 */
#define add_new_item_to_hlist(new_item_pointer, hashtable , node, item_type, identifier)					\
	new_item_pointer = (item_type *) kmalloc(sizeof(item_type), GFP_KERNEL);						\
	hash_add(hashtable, &(new_item_pointer->node), identifier)

/**
 * @brief retrive the item from an hashtable by the key
 * 
 * @param getted_item_pointer	will be filled with the retrived node.
 * @param hashtable		hashtable where to search
 * @param node			the &struct hlist_node of the object to be retrived
 * @param member_identifier	the name of the identifier within the struct
 * @param identifier		the key of the object to find
 */
#define get_hlist_item_by_id(getted_item_pointer, hashtable , node, member_identifier, identifier)				\
	hash_for_each_possible(hashtable, getted_item_pointer, node, identifier) {						\
			if (getted_item_pointer->member_identifier != identifier) continue;					\
			break;													\
	}

/**
 * @brief delete the completion queue descriptor from its hashtable and free it
 * 
 * @param cq_desc_to_delete	will be filled with the retrived node.
 * @param node			the &struct hlist_node of the object to be retrived
 */
#define delete_completion_queue_descriptor(cq_desc_to_delete, node)						\
	hash_del(&cq_desc_to_delete->node);									\
	kfree(cq_desc_to_delete)

/**
 * @brief retrive a node in an hlist by its identifier
 * 
 * @param hashtable 		hashtable where to search
 * @param item_select	        will be filled with the retrived node
 * @param node    		the &struct hlist_node of the object to be retrived
 * @param member_identifier	the name of the identifier within the struct
 * @param identifier		the key of the object to find
 */
#define retrive_from_hlist(item_select, hashtable, node, member_identifier, identifier)						\
	hash_for_each_possible(hashtable, item_select, node, identifier) {							\
		if (item_select->member_identifier != identifier) continue; 							\
		break;														\
	}

/**
 * @brief retrive a node in an hlist by its identifier
 *
 * @param item_select		will be filled with the retrived node
 * @param head			the head of the list
 * @param member    		the &struct list_head of the object to be retrived
 * @param member_identifier	the name of the identifier within the struct
 * @param identifier		the key of the object to find
 */
#define retrive_from_list(item_select, head, member, member_identifier, identifier)						\
	list_for_each_entry(item_select, head, member) {									\
		if (item_select->member_identifier != identifier) continue; 							\
		break;														\
	}

/**
 * @brief delete all the item from a list 
 * 
 * @param entry_cursor		the type * to use as a loop cursor
 * @param entry_cursor_safe	another type * to use as temporary storage
 * @param head			the head for your list.
 * @param member		the name of the list_head within the struct.
 * 
 */
#define ums_delete_list(entry_cursor, entry_cursor_safe, head, member)										\
	list_for_each_entry_safe(entry_cursor, entry_cursor_safe, head, member ){								\
                list_del(&entry_cursor->member);													\
                kfree(entry_cursor);														\
        }

/**
 * @brief delete all the item from an hlist
 *
 * @param cursor		the type * to use as a loop cursor.		
 * @param bucket		integer to use as bucket loop cursor
 * @param hashtable 		hashtable where to search
 * @param node    		the &struct hlist_node of the object to be retrived
 */
#define ums_delete_hlist(cursor, node_ptr, bucket, hashtable, node)									\
	hash_for_each_safe(hashtable, bucket, node_ptr, cursor, node){									\
                hlist_del(&cursor->node);												\
                kfree(cursor);														\
        }

#endif
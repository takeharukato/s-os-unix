/* -*- mode: C; coding:utf-8 -*- */
/**********************************************************************/
/*  SWORD Emulator list operation module                              */
/*                                                                    */
/*  Copyright 2023 Takeharu KATO                                      */
/*                                                                    */
/**********************************************************************/

#include "list.h"

/** Initialize a list node.
    @param[in] node A pointer to the list node to initialize.
 */
void
list_init(struct _list *node){

	node->prev = node->next = node;
}

/** Remove the node from the list which contains the node.
    @param[in] node A pointer to the list node to be removed.
 */
void
list_del(struct _list *node) {

	/*
	 * Remove the node from the list
	 */
	node->next->prev = node->prev;
	node->prev->next = node->next;

	list_init(node);  	/*  Re-initialize the node  */
}


/** Check the node is not linked.
    @param[in] node A pointer to the list node to be checked.
 */
int
list_not_linked(struct _list *node) {

	return ( ( node->prev == node ) && ( node->next == node ) );
}

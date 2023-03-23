/* -*- mode: C; coding:utf-8 -*- */
/**********************************************************************/
/*  SWORD Emulator queue operation module                             */
/*                                                                    */
/*  Copyright 2023 Takeharu KATO                                      */
/*                                                                    */
/**********************************************************************/

#include "list.h"
#include "queue.h"

/** Add the list node to the queue
   @param[in] head The queue to add the node to
   @param[in] node The list node to add
 */
void
queue_add(struct _queue *head, struct _list *node) {

	node->next = (struct _list *)head;
	node->prev = head->prev;
	head->prev->next = node;
	head->prev = node;
}

/** Add the list node at the top of the queue
   @param[in] head The queue to add the node to
   @param[in] node The list node to add
 */
void
queue_add_top(struct _queue *head, struct _list *node) {

	node->next = head->next;
	node->prev = (struct _list *)head;
	head->next->prev = node;
	head->next = node;
}

/** Add the list node before the specified node
   @param[in] target The list node which will be located after the node to add.
   @param[in] node The list node to add
 */
void
queue_add_before(struct _list *target, struct _list *node) {

	node->prev = target->prev;
	node->next = target;
	target->prev->next = node;
	target->prev = node;
}

/** Add the list node after the specified node
   @param[in] target The list node which will be located before the node to add.
   @param[in] node The list node to add
 */
void
queue_add_after(struct _list *target, struct _list *node) {

	node->next = target->next;
	node->prev = target;
	target->next->prev = node;
	target->next = node;
}

/** Remove the list node from the queue
    @param[in] head The queue to operate
    @param[in] node The list node to remove
    @retval True The queue has became to be empty.
    @retval False The queue is not empty.
 */
int
queue_del(struct _queue *head, struct _list *node) {

	list_del(node);
	return queue_is_empty(head);
}

/** Refer the top node in the queue
    @param[in] head The queue to operate
    @return The address of the top node in the queue
    @remark This function does not modify the queue.
 */
struct _list *
queue_ref_top(struct _queue *head) {

	return head->next;
}

/** Remove the top node from the queue and return the node.
    @param[in] head The queue to operate
    @return The address of the top node in the queue
 */
struct _list *
queue_get_top(struct _queue *head) {
	list *top;

	top = queue_ref_top(head);
	list_del(top);

	return top;
}

/** Refer the last node in the queue
    @param[in] head The queue to operate
    @return The address of the last node in the queue
    @remark This function does not modify the queue.
 */
struct _list *
queue_ref_last(struct _queue *head) {

	return head->prev;
}

/** Remove the last node from the queue and return the node.
    @param[in] head The queue to operate
    @return The address of the last node in the queue
 */
struct _list *
queue_get_last(struct _queue *head) {
	struct _list *last;

	last = queue_ref_last(head);
	list_del(last);

	return last;
}

/** Rotate the queue
    @param[in] head The queue to operate
 */
void
queue_rotate(struct _queue *head) {

	queue_add(head, queue_get_top(head));
}

/**  Rotate the queue by reverse order
    @param[in] head The queue to operate
 */
void
queue_reverse_rotate(struct _queue *head){

	queue_add_top(head, queue_get_last(head));
}

/** Determine whether the queue is empty.
    @param[in] head The queue to operate
    @retval    True  The queue is empty
    @retval    False The queue is NOT empty
 */
int
queue_is_empty(struct _queue *head) {

	return  (head->prev == (struct _list *)head)
		&& (head->next == (struct _list *)head);
}

/** Initialize the queue
    @param[in] head The queue to operate
 */
void
queue_init(struct _queue *head) {

	head->prev = head->next = (struct _list *)head;
}

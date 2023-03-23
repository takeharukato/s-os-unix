/* -*- mode: C; coding:utf-8 -*- */
/**********************************************************************/
/*  SWORD Emulator queue relevant definitions                         */
/*                                                                    */
/*  Copyright 2023 Takeharu KATO                                      */
/*                                                                    */
/**********************************************************************/

#if !defined(_QUEUE_H)
#define _QUEUE_H

#include "container.h"
#include "list.h"

#define QUEUE_ADD_ASCENDING  (0)  /**< link by ascending order */
#define QUEUE_ADD_DESCENDING (1)  /**< link by descending order  */

/** Queue data structure
 */
typedef struct _queue{
	struct _list *prev;       /*<  The pointer to the last member  */
	struct _list *next;       /*<  The pointer to the first member */
}queue;

/** Queue initializer
   @param[in] _que The pointer to the queue head.
 */
#define __QUEUE_INITIALIZER(_que)			\
	{						\
		.prev = (struct _list *)(_que),		\
		.next = (struct _list *)(_que),		\
	}

/** Macro for earch in the queue
    @param[in] _itr    Iterator (The pointer variable name of the list structure )
    @param[in] _que    The pointer of the queue
 */
#define queue_for_each(_itr, _que)				   \
	for((_itr) = queue_ref_top((struct _queue *)(_que));	   \
	    (_itr) != ((struct _list *)(_que));			   \
	    (_itr) = (_itr)->next)

/** Macro for search for the case of modifying the queue in a loop.
    @param[in] _itr  Iterator (The pointer variable name of the list structure ).
    @param[in] _que  The pointer of the queue.
    @param[in] _np   The pointer to points the next element.
 */
#define queue_for_each_safe(_itr, _que, _np)				\
	for((_itr) = queue_ref_top((struct _queue *)(_que)), (_np) = (_itr)->next; \
	    (_itr) != ((struct _list *)(_que));				\
	    (_itr) = (_np), (_np) = (_itr)->next )

/** Macro for search in the queue by reverse order.
    @param[in] _itr  Iterator (The pointer variable name of the list structure ).
    @param[in] _que  The pointer of the queue.
 */
#define queue_reverse_for_each(_itr, _que)		      \
	for((_itr) = queue_ref_last((struct _queue *)(_que)); \
	    (_itr) != (struct _list *)(_que);		      \
	    (_itr) = (_itr)->prev )

/**  Macro for search by reverse order for the case of modifying the queue in a loop.
    @param[in] _itr  Iterator (The pointer variable name of the list structure ).
    @param[in] _que  The pointer of the queue.
    @param[in] _np   The pointer to points the next element.
 */
#define queue_reverse_for_each_safe(_itr, _que, _np)			\
	for((_itr) = queue_ref_last((struct _queue *)(_que)), (_np) = (_itr)->prev; \
	    (_itr) != (struct _list *)(_que);				\
	    (_itr) = (_np), (_np) = (_itr)->prev )

/** Add a node to a sorted list.
    @param[in] _que  The pointer of the queue.
    @param[in] _type The type of the structure which contains the list node.
    @param[in] _member  The member name of the list node in the structure.
    @param[in] _nodep   The list node to add.
    @param[in] _cmp     The pointer to the compare function.
    @param[in] _how     Sort order
 */
#define queue_add_sort(_que, _type, _member, _nodep, _cmp, _how) do{	     \
		struct _list   *_lp;					     \
		struct _list *_next;					     \
		_type    *_elem_ref;					     \
		_type    *_node_ref;					     \
									     \
		(_node_ref) =						     \
			container_of(_nodep, _type, _member);		     \
									     \
		_lp = queue_ref_top((struct _queue *)(_que));		     \
		_next = _lp->next;					     \
		do{							     \
									     \
			(_elem_ref) =				 	     \
				container_of(_lp, _type, _member);	     \
									     \
			if ( ( (_next) == (struct _list *)(_que) )    ||     \
			    ( ( (_how) == QUEUE_ADD_ASCENDING ) &&	     \
				( _cmp((_node_ref), (_elem_ref)) < 0 ) ) ||  \
			    ( ( (_how) == QUEUE_ADD_DESCENDING ) &&	     \
				( _cmp((_node_ref), (_elem_ref)) > 0 ) ) ) { \
									     \
				queue_add_before((_lp),(_nodep));	     \
				break;					     \
			}						     \
									     \
			_lp = _next;					     \
			_next = _lp->next;				     \
		}while( _lp != (struct _list *)(_que) );		     \
	}while(0)

/** Find an element in the queue.
    @param[in] _que  The pointer of the queue.
    @param[in] _type The type of the structure which contains the list node.
    @param[in] _member  The member name of the list node in the structure.
    @param[in] _keyp    The pointer to the data structure of the key for search.
    @param[in] _cmp     The pointer to the compare function.
    @param[in] _elemp   The pointer to the found element
 */
#define queue_find_element(_que, _type, _member, _keyp, _cmp, _elemp) do{ \
		struct _list *_lp;					\
		_type  *_elem_ref;					\
									\
		*((_type **)(_elemp)) = NULL;				\
									\
		queue_for_each((_lp), (_que)) {				\
									\
			(_elem_ref) =					\
				container_of(_lp, _type, _member);	\
									\
			if ( _cmp((_keyp), (_elem_ref)) == 0 ) {	\
									\
				*((_type **)(_elemp)) =	(_elem_ref);	\
				break;					\
			}						\
		}							\
	}while(0)

/** Find an element in the queue by reverse order.
    @param[in] _que  The pointer of the queue.
    @param[in] _type The type of the structure which contains the list node.
    @param[in] _member  The member name of the list node in the structure.
    @param[in] _keyp    The pointer to the data structure of the key for search.
    @param[in] _cmp     The pointer to the compare function.
    @param[in] _elemp   The pointer to the found element.
 */
#define queue_reverse_find_element(_que, _type, _member, _keyp, _cmp, _elemp) do{ \
		struct _list *_lp;					\
		_type  *_elem_ref;					\
									\
		*((_type **)(_elemp)) = NULL;				\
									\
		queue_reverse_for_each((_lp), (_que)) {			\
									\
			(_elem_ref) =					\
				container_of(_lp, _type, _member);	\
									\
			if ( _cmp((_keyp), (_elem_ref)) == 0 ) {	\
									\
				*((_type **)(_elemp)) =	(_elem_ref);	\
				break;					\
			}						\
		}							\
	}while(0)

void queue_add(struct _queue *_head, struct _list *_node);
void queue_add_top(struct _queue *_head, struct _list *_node);
void queue_add_before(struct _list *_target, struct _list *_node);
void queue_add_after(list *_target, list *_node);
int queue_del(struct _queue *_head, struct _list *_node);
struct _list *queue_ref_top(struct _queue *_head);
struct _list *queue_get_top(struct _queue *_head);
struct _list *queue_ref_last(struct _queue *_head);
struct _list *queue_get_last(struct _queue *_head);
void queue_rotate(struct _queue *_head);
void queue_reverse_rotate(struct _queue *_head);
int queue_is_empty(struct _queue *_head);
void queue_init(struct _queue *_head);
#endif  /*  _QUEUE_H  */

/* -*- mode: C; coding:utf-8 -*- */
/**********************************************************************/
/*  SWORD Emulator list relevant definitions                          */
/*                                                                    */
/*  Copyright 2023 Takeharu KATO                                      */
/*                                                                    */
/**********************************************************************/

#if !defined(_LIST_H)
#define _LIST_H

#include "container.h"

/** List structure
 */
typedef struct _list{
	struct _list *prev;       /**<  The pointer to the previous node  */
	struct _list *next;       /**<  The pointer to the next node   */
}list;

void list_del(struct _list *_node);
void list_init(struct _list *_node);
int list_not_linked(struct _list *_node);
#endif  /*  _LIST_H  */

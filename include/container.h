/* -*- mode: C; coding:utf-8 -*- */
/**********************************************************************/
/*  SWORD Emulator container operations                               */
/*                                                                    */
/*  Copyright 2023 Takeharu KATO                                      */
/*                                                                    */
/**********************************************************************/

#if !defined(_CONTAINER_H)
#define  _CONTAINER_H

/** Calculate the offset address of the data structure embedded in the structured data
    @param[in] t The type of the structure which contains the member
    name of the structure specified by the 'm' argument.
    @param[in] m The member name of the structure.
 */
#define offset_of(t, m)			\
	( (void *)( &( ( (t *)(0) )->m ) ) )

/** Get the container data address from the address of the member in the structure.
    @param[in] p The address of the member in the structure.
    @param[in] t The type of the structure which contains the address
    which is specified by the 'p' argument.
    @param[in] m The member name in the structure which corresponds to the 'p' argument.
 */
#define container_of(p, t, m)			\
	( (t *)( ( (void *)(p) ) - offset_of(t, m) ) )

#endif  /*  _CONTAINER_H   */

/*
 * Copyright (c) 2014, Michael Müller
 * Copyright (c) 2014, Sebastian Lackner
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#ifndef _H_LIST_
#define _H_LIST_
/** \addtogroup LinkedList
 *  @{
 */

struct linkedList
{
	struct linkedList *next;
	struct linkedList *prev;
};

/**
 * @brief Initializes a linkedList structure
 * @details This function should be called before using a linkedList structure
 *			with any other command. It initializes the pointers such that the
 *			linkedList is empty.
 *
 * @param linkedList Pointer to the linkedList structure
 */
static inline void ll_init(struct linkedList *list)
{
	list->next = list;
	list->prev = list;
}

/**
 * @brief Checks if the linkedList is empty
 *
 * @param linkedList Pointer to the linkedList structure
 * @return True if the linkedList is empty, otherwise false
 */
static inline bool ll_empty(struct linkedList *list)
{
	return (list->next == list);
}

/**
 * @brief Appends a new element after a reference element
 * @details This function can be used to append a linkedList element after another
 *			one. List should point to the linkedList structure which is part of the
 *			reference object, element points to the linkedList structure of the element
 *			which should be added.
 *
 * @param linkedList Pointer to the linkedList reference element
 * @param linkedList Pointer to the new linkedList entry
 */
static inline void ll_add_after(struct linkedList *list, struct linkedList *element)
{
	element->next		= list->next;
	element->prev		= list;
	list->next->prev	= element;
	list->next			= element;
}

/**
 * @brief Inserts a new element before a reference element
 * @details See ll_add_after(), but adds the element before instead of after the
 *			reference element.
 *
 * @param linkedList Pointer to the linkedList reference element
 * @param linkedList Pointer to the new linkedList entry
 */
static inline void ll_add_before(struct linkedList *list, struct linkedList *element)
{
	element->next		= list;
	element->prev		= list->prev;
	list->prev->next	= element;
	list->prev			= element;
}

#define ll_add_head ll_add_after
#define ll_add_tail ll_add_before

/**
 * @brief Removes an element from a list
 * @details This function removes an element from a linkedList. Element should point
 *			to the linkedList structure of the element which should be deleted.
 *
 * @param linkedList Pointer to the linkedList element which should be deleted
 * @return element
 */
static inline struct linkedList *ll_remove(struct linkedList *element)
{
	element->next->prev = element->prev;
	element->prev->next = element->next;
	return element;
}

/**
 * @brief Initializes a linkedList
 * @details Static version of ll_init()
 *
 * @param list linkedList structure object
 * @return Initialization variables
 */
#define LL_INIT(list) \
	{ &(list), &(list) }

#define LL_ENTRY(element, type, field) \
	((type *)((uint8_t *)(element) - offsetof(type, field)))

/**
 * @brief Allows to iterate a linkedList similar to a for-loop.
 *
 * @param element Iterator variable (of the object type)
 * @param list Pointer to the linkedList head structure
 * @param type Type of the iterator elements
 * @param field Field of the object structure which represents the iterated linkedList
 */
#define LL_FOR_EACH(element, list, type, field) \
	for ((element) = LL_ENTRY((list)->next, type, field); \
		&(element)->field != (list); \
		(element) = LL_ENTRY((element)->field.next, type, field))

/**
 * @brief Allows to iterate a linkedList similar to a for-loop (safe when deleting elements).
 * @details Similar to LL_FOR_EACH(), but wil not crash, even if elements are deleted
 *			while iterating through the linkedList.
 */
#define LL_FOR_EACH_SAFE(element, next_element, list, type, field) \
	for ((element) = LL_ENTRY((list)->next, type, field), (next_element) = LL_ENTRY((element)->field.next, type, field); \
		&(element)->field != (list); \
		(element) = (next_element), (next_element) = LL_ENTRY((element)->field.next, type, field))

/**
 *  @}
 */
#endif /* _H_LIST_ */
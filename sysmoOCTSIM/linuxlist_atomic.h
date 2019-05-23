/*! \file linuxlist_atomic.h
 *
 * Atomic versions of doubly linked list implementation.
 */

#pragma once

/*! \defgroup linuxlist Simple doubly linked list implementation
 *  @{
 * \file linuxlist.h */

#include <osmocom/core/linuxlist.h>

/*! Initialize a llist_head to point back to itself.
 *  \param[in] ptr llist_head to be initialized.
 */
#define INIT_LLIST_HEAD_AT(ptr) do { \
	CRITICAL_SECTION_ENTER();		\
	(ptr)->next = (ptr); (ptr)->prev = (ptr); \
	CRITICAL_SECTION_LEAVE();		\
} while (0)

/*! Add a new entry into a linked list (at head).
 *  \param _new the entry to be added.
 *  \param head llist_head to prepend the element to.
 *
 * Insert a new entry after the specified head.
 * This is good for implementing stacks.
 */
static inline void llist_add_at(struct llist_head *_new, struct llist_head *head)
{
	CRITICAL_SECTION_ENTER()
	__llist_add(_new, head, head->next);
	CRITICAL_SECTION_LEAVE()
}

/*! Add a new entry into a linked list (at tail).
 *  \param _new the entry to be added.
 *  \param head llist_head to append the element to.
 *
 * Insert a new entry before the specified head.
 * This is useful for implementing queues.
 */
static inline void llist_add_tail_at(struct llist_head *_new, struct llist_head *head)
{
	CRITICAL_SECTION_ENTER()
	__llist_add(_new, head->prev, head);
	CRITICAL_SECTION_LEAVE()
}

/*! Delete a single entry from a linked list.
 *  \param entry the element to delete.
 *
 * Note: llist_empty on entry does not return true after this, the entry is
 * in an undefined state.
 */
static inline void llist_del_at(struct llist_head *entry)
{
	CRITICAL_SECTION_ENTER()
	__llist_del(entry->prev, entry->next);
	entry->next = (struct llist_head *)LLIST_POISON1;
	entry->prev = (struct llist_head *)LLIST_POISON2;
	CRITICAL_SECTION_LEAVE()
}

/*! Delete a single entry from a linked list and reinitialize it.
 *  \param entry the element to delete and reinitialize.
 */
static inline void llist_del_init_at(struct llist_head *entry)
{
	CRITICAL_SECTION_ENTER()
	__llist_del(entry->prev, entry->next);
	INIT_LLIST_HEAD(entry);
	CRITICAL_SECTION_LEAVE()
}

/*! Delete from one llist and add as another's head.
 *  \param llist the entry to move.
 *  \param head  the head that will precede our entry.
 */
static inline void llist_move_at(struct llist_head *llist, struct llist_head *head)
{
	CRITICAL_SECTION_ENTER()
	__llist_del(llist->prev, llist->next);
	llist_add(llist, head);
	CRITICAL_SECTION_LEAVE()
}

/*! Delete from one llist and add as another's tail.
 *  \param llist the entry to move.
 *  \param head  the head that will follow our entry.
 */
static inline void llist_move_tail_at(struct llist_head *llist, struct llist_head *head)
{
	CRITICAL_SECTION_ENTER()
	__llist_del(llist->prev, llist->next);
	llist_add_tail(llist, head);
	CRITICAL_SECTION_LEAVE()
}

/*! Join two linked lists.
 *  \param llist the new linked list to add.
 *  \param head  the place to add llist within the other list.
 */
static inline void llist_splice_at(struct llist_head *llist, struct llist_head *head)
{
	CRITICAL_SECTION_ENTER()
	if (!llist_empty(llist))
		__llist_splice(llist, head);
	CRITICAL_SECTION_LEAVE()
}

/*! Join two llists and reinitialise the emptied llist.
 *  \param llist the new linked list to add.
 *  \param head  the place to add it within the first llist.
 *
 * The llist is reinitialised.
 */
static inline void llist_splice_init_at(struct llist_head *llist,
				     struct llist_head *head)
{
	CRITICAL_SECTION_ENTER()
	if (!llist_empty(llist)) {
		__llist_splice(llist, head);
		INIT_LLIST_HEAD(llist);
	}
	CRITICAL_SECTION_LEAVE()
}

/*! Count number of llist items by iterating.
 *  \param head the llist head to count items of.
 *  \returns Number of items.
 *
 * This function is not efficient, mostly useful for small lists and non time
 * critical cases like unit tests.
 */
static inline unsigned int llist_count_at(const struct llist_head *head)
{
	struct llist_head *entry;
	unsigned int i = 0;
	CRITICAL_SECTION_ENTER()
	llist_for_each(entry, head)
		i++;
	CRITICAL_SECTION_LEAVE()
	return i;
}

/*!
 *  @}
 */

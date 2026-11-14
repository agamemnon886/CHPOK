/*
 * Modified by Kirill Brazhnikov, 2026-10-31: built-in integer types (int,
 * unsigned, short, long, ...) are replaced with the fixed-width types
 * from <stdint.h>.
 *
 * Modified by Kirill Brazhnikov, 2026-11-02: MISRA C:2025: the header
 * guard isn't a reserved identifier; essentially Boolean conditions;
 * deviations are documented where a guideline can't be followed;
 * list_empty() returns bool; macro parameters are parenthesized.
 *
 * Modified by Kirill Brazhnikov, 2026-11-08: Doxygen headers for the file
 * and its functions.
 *
 * Modified by Kirill Brazhnikov, 2026-11-11: the Doxygen comments that don't
 * document a function definition are plain comments, so that only the function
 * headers are in the function documentation.
 *
 * Modified by Kirill Brazhnikov, 2026-11-13: struct list_head is the typedef
 * list_head_t; Doxygen comments for its types and its macros; the unused macros
 * LIST_HEAD, LIST_HEAD_INIT, list_for_each_entry, list_next_entry,
 * list_last_entry and list_prev_entry are removed.
 *
 * Modified by Kirill Brazhnikov, 2026-11-14: the Doxygen comment of the file is
 * removed (header files are not documented); the Doxygen headers of the static
 * inline functions are plain comments (they are not documented).
 */

#ifndef POK_LIST_H
#define POK_LIST_H

/*
 * Embedded double-linked lists.
 * 
 * Mostly originated from Linux kernel.
 */
#include "common.h"
#include <stdbool.h>

/**
 * @typename `list_head_t`
 *
 * @description
 * One element in the list or head of the list.
 *
 * If plays role of list's element, then
 *    `next` field points to the next element of the list or,
 *        if given element is the last element, to the head of the list,
 *    `prev` field points to the previous element of the list or,
 *        if given element is the first element, to the head of the list.
 *
 * If plays role of list's head,
 *   `next` field points to the first element in the list or,
 *        if list is empty, to the head itself.
 *   `prev` field points to the last element in the list or,
 *        if list is empty, to the head itself.
 */
typedef struct list_head {
    /**
     * The next element, or the first one of a head (see above).
     */
    struct list_head* next;
    /**
     * The previous element, or the last one of a head (see above).
     */
    struct list_head* prev;
} list_head_t;

/*
 * Initializes a list head (or an element), empty: it points to itself.
 */
static inline void INIT_LIST_HEAD(list_head_t *list)
{
    list->next = list;
    list->prev = list;
}

/*
 * Inserts an element between two consecutive elements.
 */
static inline void __list_add(list_head_t *new,
                              list_head_t *prev,
                              list_head_t *next)
{
    next->prev = new;
    new->next = next;
    new->prev = prev;
    prev->next = new;
}

/*
 * Inserts an element right after a head (at the start of the list).
 */
static inline void list_add(list_head_t *new, list_head_t *head)
{
    __list_add(new, head, head->next);
}

/*
 * Inserts an element right before a head (at the end of the list).
 */
static inline void list_add_tail(list_head_t *new, list_head_t *head)
{
    __list_add(new, head->prev, head);
}

/*
 * Links two elements to each other, removing what was between them.
 */
static inline void __list_del(list_head_t * prev, list_head_t * next)
{
    next->prev = prev;
    prev->next = next;
}

/*
 * Removes an element from its list (the element itself isn't changed).
 */
static inline void __list_del_entry(list_head_t *entry)
{
    __list_del(entry->prev, entry->next);
}

/*
 * Removes an element from its list and makes it empty (not in a list).
 */
static inline void list_del_init(list_head_t *entry)
{
    __list_del_entry(entry);
    INIT_LIST_HEAD(entry);
}

/*
 * Tells whether a list is empty (or an element isn't in a list).
 */
static inline bool list_empty(const list_head_t *head)
{
    return head->next == head;
}

/**
 * @macroname `list_entry`
 *
 * @description
 * Get the struct for this entry: the element of type type whose list_head_t
 * member is at ptr (container_of()).
 *
 * @params
 * | Name | Description |
 * |------|-------------|
 * | `ptr` | The list_head_t pointer. |
 * | `type` | The type of the struct this is embedded in. |
 * | `member` | The name of the list_head within the struct. |
 */
/*
 * MISRA C:2025 Rule 11.3 deviation: see container_of(); list elements are
 * list_head_t members of the 'type' objects.
 */
 #define list_entry(ptr, type, member) \
         container_of(ptr, type, member)

/**
 * @macroname `list_first_entry`
 *
 * @description
 * Get the first element from a list. Note, that list is expected to be not
 * empty.
 *
 * @params
 * | Name | Description |
 * |------|-------------|
 * | `ptr` | The list head to take the element from. |
 * | `type` | The type of the struct this is embedded in. |
 * | `member` | The name of the list_head within the struct. |
 */
#define list_first_entry(ptr, type, member) \
        list_entry((ptr)->next, type, member)

/**
 * @macroname `list_for_each`
 *
 * @description
 * Iterate over a list: a for statement whose body runs for each element (pos is
 * its list_head_t).
 *
 * @params
 * | Name | Description |
 * |------|-------------|
 * | `pos` | The list_head_t pointer to use as a loop cursor. |
 * | `head` | The head for your list. |
 */
#define list_for_each(pos, head) \
        for ((pos) = (head)->next; (pos) != (head); (pos) = (pos)->next)

/*
 * There are more macros in Linux kernel sources `include/linux/list.h`.
 * If some missed macro is needed, feel free to copy it from there.
 */

#endif /* POK_LIST_H */

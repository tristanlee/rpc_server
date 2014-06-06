#ifndef __NET_LIST_H__
#define __NET_LIST_H__

struct _ListNodeT
{
    struct _ListNodeT  *next;          /* point to next node. */
    struct _ListNodeT  *prev;          /* point to prev node. */
};

typedef struct _ListNodeT ListNodeT;

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @addtogroup rscf_list
 */
/*@{*/

/**
 * @brief initialize a list
 *
 * @param l list to be initialized
 */
#define list_init(pListHead) \
    ( (pListHead)->next = (pListHead)->prev = (pListHead) )

/**
 * @brief insert a node after a list
 *
 * @param l list to insert it
 * @param n new node to be inserted
 */
#define list_insert_after(pos, pEntry) \
    do { \
        (pEntry)->prev = (pos); \
        (pEntry)->next = (pos)->next; \
        (pos)->next->prev = (pEntry); \
        (pos)->next = (pEntry); \
    } while(0)

/**
 * @brief insert a node before a list
 *
 * @param n new node to be inserted
 * @param l list to insert it
 */
#define list_insert_before(pos, pEntry) \
    do { \
        (pEntry)->next = (pos); \
        (pEntry)->prev = (pos)->prev; \
        (pos)->prev->next = (pEntry); \
        (pos)->prev = (pEntry); \
    } while(0)

/**
 * @brief remove node from list.
 * @param n the node to remove from the list.
 */
#define list_remove(pEntry) \
    do { \
        (pEntry)->next->prev = (pEntry)->prev; \
        (pEntry)->prev->next = (pEntry)->next; \
        (pEntry)->prev = pEntry; \
        (pEntry)->next = pEntry; \
    } while(0)

/**
 * @brief tests whether a list is empty
 * @param l the list to test.
 */
#define list_isempty(pListHead) \
    ( (pListHead)->next == (pListHead) )

/**
 * @brief get the struct for this entry
 * @param node the entry point
 * @param type the type of structure
 * @param member the name of list in structure
 */
#define list_entry(node, type, member) \
    ((type *)((char *)(node) - (unsigned long)(&((type *)0)->member)))

/*@}*/

#ifdef __cplusplus
}
#endif

#endif


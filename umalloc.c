#include "types.h"
#include "stat.h"
#include "user.h"
#include "param.h"

// Memory allocator by Kernighan and Ritchie,
// The C programming Language, 2nd ed.  Section 8.7.
// Refactored to make variables and algorithm more clear

struct header {
	uint size;
};

struct freelist_node {
	struct freelist_node *next;
	struct header         hdr;
};

/* Base static node of size 0 used as degenerate freelist */
static struct freelist_node base;

/* A pointer that rotates around the freelist. It is set to previous node at the end of malloc or free */
static struct freelist_node *freelist;

/*
 * Frees previously allocated memory, adding the resulting freenode to the freelist
 * This is assumed to be a valid freenode allocated by malloc with a valid header preceeding the passed pointer
 * @param ptr - the address of previously allocated memory we want to add to the freelist
 */
void
free(void *ptr)
{
	struct freelist_node *mem, *node;

	mem = (struct freelist_node *)ptr - 1;

	node = freelist;
	while (!(mem > node && mem < node->next)) {
		if (node >= node->next                   /* If we're about to wrap around */
		    && (mem > node || mem < node->next)) /* And the freed memory is not between the first and last node,
		                                            just break */
			break;
		node = node->next;
	}

	if (mem + mem->hdr.size == node->next) {
		mem->hdr.size += node->next->hdr.size;
		mem->next = node->next->next;
	} else {
		mem->next = node->next;
	}

	if (node + node->hdr.size == mem) {
		node->hdr.size += mem->hdr.size;
		node->next = mem->next;
	} else {
		node->next = mem;
	}

	freelist = node;
}

/**
 * Expands the backing memory allocated to the freelist
 * @param nu the number of "core" units to add to the freelist.
 * This is an abstract unit equal to the size of a freelist_node struct.
 * @returns address of the body of a newly allocated freelist node or 0 on error
 **/
static struct freelist_node *
morecore(uint nu)
{
	if (nu < 4096) nu = 4096;

	char *ptr = sbrk(nu * sizeof(struct freelist_node));
	if (ptr == (char *)-1) goto err;

	struct freelist_node *node = (struct freelist_node *)ptr;
	node->hdr.size             = nu;

	free((void *)(node + 1));

	return freelist;
err:
	return 0;
}

/**
 * @param nbytes the number of bytes of allocate
 * @returns address of allocated memory or 0 on error
 **/
void *
malloc(uint nbytes)
{
	struct freelist_node *node;
	struct freelist_node *prev_node;

	uint nunits = (nbytes + sizeof(struct freelist_node) - 1) / sizeof(struct freelist_node) + 1;

	if (freelist == 0) {
		base.next = freelist = prev_node = &base;
		base.hdr.size                    = 0;
	}

	node      = freelist->next;
	prev_node = freelist;

	while (1) {
		if (node->hdr.size == nunits) {
			prev_node->next = node->next;
			goto done;
		}

		if (node->hdr.size > nunits) {
			node->hdr.size -= nunits;
			node += node->hdr.size;
			node->hdr.size = nunits;
			goto done;
		}

		if (node == freelist) {
			node = morecore(nunits);
			if (node == 0) goto err_alloc;
		}

		prev_node = node;
		node      = node->next;
	}

done:
	freelist = prev_node;
	return (void *)(node + 1);
err_alloc:
	return 0;
}

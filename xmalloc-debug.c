/* $Id: xmalloc-debug.c,v 1.1 2007-07-25 23:13:18 nicm Exp $ */

/*
 * Copyright (c) 2007 Nicholas Marriott <nicm@users.sourceforge.net>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF MIND, USE, DATA OR PROFITS, WHETHER
 * IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING
 * OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#ifdef DEBUG

#include <sys/types.h>

#include <ctype.h>
#include <dlfcn.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "tmux.h"

/* Single xmalloc allocated block. */
struct xmalloc_blk {
	void	*caller;

	void	*ptr;
	size_t	 size;

	SPLAY_ENTRY(xmalloc_blk) entry;
};

/* Splay tree of allocated blocks. */
SPLAY_HEAD(xmalloc_tree, xmalloc_blk);
struct xmalloc_tree xmalloc_tree = SPLAY_INITIALIZER(&xmalloc_tree);

/* Various statistics. */
size_t	xmalloc_allocated;
size_t	xmalloc_freed;
size_t	xmalloc_peak;
u_int	xmalloc_frees;
u_int	xmalloc_mallocs;
u_int	xmalloc_reallocs;

/* Print function. */
#define XMALLOC_PRINT log_debug3

/* Bytes of unallocated blocks and number of allocated blocks to show. */
#define XMALLOC_BYTES 8
#define XMALLOC_LINES 32

/* Macro to update peek usage variable. */
#define XMALLOC_UPDATE() do {						\
	if (xmalloc_allocated - xmalloc_freed > xmalloc_peak)		\
		xmalloc_peak = xmalloc_allocated - xmalloc_freed;	\
} while (0)

/* Tree functions. */
int xmalloc_cmp(struct xmalloc_blk *, struct xmalloc_blk *);
SPLAY_PROTOTYPE(xmalloc_tree, xmalloc_blk, entry, xmalloc_cmp);
SPLAY_GENERATE(xmalloc_tree, xmalloc_blk, entry, xmalloc_cmp);

/* Compare two blocks. */
int
xmalloc_cmp(struct xmalloc_blk *blk1, struct xmalloc_blk *blk2)
{
	uintptr_t	ptr1 = (uintptr_t) blk1->ptr;
	uintptr_t	ptr2 = (uintptr_t) blk2->ptr;

	if (ptr1 < ptr2)
		return (-1);
	if (ptr1 > ptr2)
		return (1);
	return (0);
}

/* Clear statistics and block list; used to start fresh after fork(2). */
void
xmalloc_clear(void)
{
 	struct xmalloc_blk	*blk;

	xmalloc_allocated = 0;
	xmalloc_freed = 0;
	xmalloc_peak = 0;
	xmalloc_frees = 0;
	xmalloc_mallocs = 0;
	xmalloc_reallocs = 0;

	while (!SPLAY_EMPTY(&xmalloc_tree)) {
		blk = SPLAY_ROOT(&xmalloc_tree);
		SPLAY_REMOVE(xmalloc_tree, &xmalloc_tree, blk);
		free(blk);
	}
}

/* Print report of statistics and unfreed blocks. */
void
xmalloc_report(pid_t pid, const char *hdr)
{
 	struct xmalloc_blk	*blk;
	u_char			*iptr;
 	char	 		 buf[4 * XMALLOC_BYTES + 1], *optr;
 	size_t		 	 len;
  	u_int	 		 n;
	Dl_info			 info;

 	XMALLOC_PRINT("%s: %ld: allocated=%zu, freed=%zu, difference=%zd, "
	    "peak=%zu", hdr, (long) pid, xmalloc_allocated, xmalloc_freed,
	    xmalloc_allocated - xmalloc_freed, xmalloc_peak);
 	XMALLOC_PRINT("%s: %ld: mallocs=%u, reallocs=%u, frees=%u", hdr,
	    (long) pid, xmalloc_mallocs, xmalloc_reallocs, xmalloc_frees);

	n = 0;
	SPLAY_FOREACH(blk, xmalloc_tree, &xmalloc_tree) {
		n++;
		if (n >= XMALLOC_LINES)
			continue;

		len = blk->size;
		if (len > XMALLOC_BYTES)
			len = XMALLOC_BYTES;

		memset(&info, 0, sizeof info);
		if (dladdr(blk->caller, &info) == 0)
			info.dli_sname = info.dli_saddr = NULL;

		optr = buf;
		iptr = blk->ptr;
		for (; len > 0; len--) {
			if (isascii(*iptr) && !iscntrl(*iptr)) {
				*optr++ = *iptr++;
				continue;
			}
			*optr++ = '\\';
			*optr++ = '0' + ((*iptr >> 6) & 07);
			*optr++ = '0' + ((*iptr >> 3) & 07);
			*optr++ = '0' + (*iptr & 07);
			iptr++;
		}
		*optr = '\0';

		XMALLOC_PRINT("%s: %ld: %u, %s+0x%02tx: [%p %zu: %s]", hdr,
		    (long) pid, n, info.dli_sname, ((u_char *) blk->caller) -
		    ((u_char *) info.dli_saddr), blk->ptr, blk->size, buf);
	}
	XMALLOC_PRINT("%s: %ld: %u unfreed blocks", hdr, (long) pid, n);
}

/* Record a newly created block. */
void
xmalloc_new(void *caller, void *ptr, size_t size)
{
	struct xmalloc_blk	*blk;

	xmalloc_allocated += size;
	XMALLOC_UPDATE();

	if ((blk = malloc(sizeof *blk)) == NULL)
		abort();

	blk->ptr = ptr;
	blk->size = size;

	blk->caller = caller;

	SPLAY_INSERT(xmalloc_tree, &xmalloc_tree, blk);

	xmalloc_mallocs++;
	XMALLOC_UPDATE();
}

/* Record changes to a block. */
void
xmalloc_change(void *caller, void *oldptr, void *newptr, size_t newsize)
{
	struct xmalloc_blk	*blk, key;
	ssize_t			 change;

	if (oldptr == NULL) {
		xmalloc_new(caller, newptr, newsize);
		return;
	}

	key.ptr = oldptr;
	blk = SPLAY_FIND(xmalloc_tree, &xmalloc_tree, &key);
	if (blk == NULL)
		return;

	change = newsize - blk->size;
	if (change > 0)
		xmalloc_allocated += change;
	else
		xmalloc_freed -= change;
	XMALLOC_UPDATE();

	SPLAY_REMOVE(xmalloc_tree, &xmalloc_tree, blk);

 	blk->ptr = newptr;
	blk->size = newsize;

	blk->caller = caller;

	SPLAY_INSERT(xmalloc_tree, &xmalloc_tree, blk);

	xmalloc_reallocs++;
	XMALLOC_UPDATE();
}

/* Record a block free. */
void
xmalloc_free(void *ptr)
{
	struct xmalloc_blk	*blk, key;

	key.ptr = ptr;
	blk = SPLAY_FIND(xmalloc_tree, &xmalloc_tree, &key);
	if (blk == NULL)
		return;

	xmalloc_freed += blk->size;

	SPLAY_REMOVE(xmalloc_tree, &xmalloc_tree, blk);
	free(blk);

	xmalloc_frees++;
	XMALLOC_UPDATE();
}

#endif /* DEBUG */

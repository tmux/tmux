/* Stub tmux.h for the standalone cmd-parse.y test harness.
 *
 * This is NOT the real tmux.h. It provides just enough for cmd-parse.y to
 * compile and link on its own: the queue macros, the allocation/logging
 * helpers it calls, and the small part of struct cmd_parse_input it touches.
 * It is found ahead of the real tmux.h via the harness include path.
 */

#ifndef TMUX_TEST_STUB_H
#define TMUX_TEST_STUB_H

#include <sys/types.h>

#include <limits.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "compat/queue.h"

#ifndef __dead
#define __dead __attribute__((__noreturn__))
#endif
#ifndef __unused
#define __unused __attribute__((__unused__))
#endif
#ifndef printflike
#define printflike(a, b) __attribute__((format(printf, a, b)))
#endif

/* Allocation helpers (implemented in xstubs.c). */
void	*xmalloc(size_t);
void	*xcalloc(size_t, size_t);
void	*xrealloc(void *, size_t);
char	*xstrdup(const char *);
char	*xstrndup(const char *, size_t);
int	 xasprintf(char **, const char *, ...) printflike(2, 3);
int	 xvasprintf(char **, const char *, va_list);

/* Logging and fatal errors (implemented in xstubs.c). */
void		 log_debug(const char *, ...) printflike(1, 2);
__dead void	 fatal(const char *, ...) printflike(1, 2);
__dead void	 fatalx(const char *, ...) printflike(1, 2);

#endif /* TMUX_TEST_STUB_H */

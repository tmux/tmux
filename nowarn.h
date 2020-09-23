#ifndef NOWARN_H
# define NOWARN_H

#include <time.h>

#define NOWARN(x) nowarn_ ## x

size_t NOWARN(strftime)(char *, size_t, const char *, const struct tm *);

#endif /* NOWARN_H */

#include "nowarn.h"

size_t
nowarn_strftime(char *s, size_t max, const char *fmt, const struct tm *tm)
{
   return strftime(s, max, fmt, tm);
}

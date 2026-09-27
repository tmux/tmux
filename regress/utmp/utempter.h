#ifndef UTMP_FIXTURE_UTEMPTER_H
#define UTMP_FIXTURE_UTEMPTER_H

int	utempter_add_record(int, const char *);
int	utempter_remove_record(int);
int	utmp_fixture_only(void);

#endif

/* Recording substitute only: this file never opens an accounting database. */
#include <sys/types.h>

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "utempter.h"

struct record {
	int		 fd;
	char		*identity;
	int		 failed;
	struct record	*next;
};

static struct record *records;

int
utmp_fixture_only(void)
{
	return (1);
}

static void
record_event(const char *operation, int fd, const char *identity)
{
	const char	*path = getenv("UTMP_FIXTURE_LOG");
	FILE		*file;
	int		 valid;

	if (path == NULL || *path == '\0')
		_exit(90);
	file = fopen(path, "a");
	if (file == NULL)
		_exit(91);
	valid = (fcntl(fd, F_GETFD) != -1 && isatty(fd));
	if (fprintf(file, "%s %d %s %s\n", operation, fd, identity,
	    valid ? "valid" : "invalid") < 0 || fclose(file) != 0)
		_exit(92);
	if (!valid || !utmp_fixture_only())
		_exit(93);
}

static int
fail_operation(const char *name)
{
	const char	*path = getenv(name);

	return (path != NULL && access(path, F_OK) == 0);
}

static int
success_result(void)
{
	const char	*value = getenv("UTMP_FIXTURE_ZERO_SUCCESS");

	return (value == NULL || strcmp(value, "1") != 0);
}

int
utempter_add_record(int fd, const char *identity)
{
	struct record	*record;

	for (record = records; record != NULL; record = record->next) {
		if (record->fd == fd || strcmp(record->identity, identity) == 0) {
			record_event("duplicate-add", fd, identity);
			return (0);
		}
	}
	record = calloc(1, sizeof *record);
	if (record == NULL || (record->identity = strdup(identity)) == NULL)
		_exit(94);
	record->fd = fd;
	record->failed = fail_operation("UTMP_FIXTURE_FAIL_ADD");
	record->next = records;
	records = record;
	record_event(record->failed ? "add-failed" : "add", fd, identity);
	if (record->failed) {
		errno = EIO;
		return (success_result() ? 0 : -1);
	}
	return (success_result());
}

int
utempter_remove_record(int fd)
{
	struct record	**previous, *record;

	for (previous = &records; (record = *previous) != NULL;
	    previous = &record->next) {
		if (record->fd != fd)
			continue;
		record_event(record->failed ? "remove-after-failed-add" : "remove",
		    fd, record->identity);
		*previous = record->next;
		free(record->identity);
		free(record);
		return (success_result());
	}
	record_event("unmatched-remove", fd, "unknown");
	return (0);
}

#include <sys/types.h>

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

int
main(void)
{
	time_t	t;
	int	i;

	setvbuf(stdout, NULL, _IONBF, 0);

	t = time(NULL);
	srandom((u_int) t);

	for (;;) {
		putchar('\033');

		for (i = 0; i < random() % 25; i++) {
			if (i > 22)
				putchar(';');
			else
				putchar(random() % 256);
		}

		/* usleep(100); */
	}
}

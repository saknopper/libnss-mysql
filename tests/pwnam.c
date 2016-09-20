/* Copyright (C) 2016 Sander Knopper
   This file is part of the nss-mysql library.

   The nss-mysql library is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as published
   by the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   The nss-mysql library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
   You should have received a copy of the GNU General Public License
   along with the nss-mysql library; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <pthread.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>

static const int NR_OF_THREADS = 16;

static size_t bufsize;
static char *username;

static void *
test_getpwnam_r(void *arg)
{
    struct passwd pwd;
	struct passwd *result;
	char *buf;
	int s;

	buf = malloc(bufsize);
	if (buf == NULL) {
		perror("malloc");
        return NULL;
	}

	s = getpwnam_r(username, &pwd, buf, bufsize, &result);
	if (result == NULL) {
		if (s == 0)
			printf("Not found\n");
		else {
			errno = s;
			perror("getpwnam_r");
		}

		return NULL;
	}

	printf("UID: %ld, Name: %s; Shell: %s\n", (long) pwd.pw_uid, pwd.pw_gecos, pwd.pw_shell);
	free(buf);

	return NULL;
}

int
main(int argc, char *argv[])
{
	if (argc != 2) {
		fprintf(stderr, "Usage: %s username\n", argv[0]);
		exit(EXIT_FAILURE);
	}

	username = argv[1];

	bufsize = sysconf(_SC_GETPW_R_SIZE_MAX);
	if (bufsize == -1) {
		// Should be enough
		bufsize = 16384;
	}

	pthread_t threads[NR_OF_THREADS];
	for (int i = 0; i < NR_OF_THREADS; i++) {
		pthread_create (&threads[i], 0, &test_getpwnam_r, NULL);
	}

	for (int i = 0; i < NR_OF_THREADS; i++) {
		pthread_join(threads[i], NULL);
	}

	exit(EXIT_SUCCESS);
}

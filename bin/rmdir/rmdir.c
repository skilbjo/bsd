static char *sccsid = "@(#)rmdir.c	4.6 (Berkeley) 12/19/82";
/*
 * Remove directory
 */
#include <stdio.h>

main(argc,argv)
	int argc;
	char **argv;
{
	int errors = 0;

	if (argc < 2) {
		fprintf(stderr, "rmdir: arg count\n");
		exit(1);
	}
	while (--argc)
		if (rmdir(*++argv) < 0) {
			perror(*argv);;
			errors++;
		}
	exit(errors != 0);
}

/*-
 * Copyright (c) 1980 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.proprietary.c%
 */

#ifndef lint
static char sccsid[] = "@(#)move.c	5.2 (Berkeley) 04/22/91";
#endif /* not lint */

#include "dumb.h"

move(x,y)
int x,y;
{
	scale(x, y);
	currentx = x;
	currenty = y;
}

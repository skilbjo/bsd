/*
 * Copyright (c) 1983 Regents of the University of California.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that this notice is preserved and that due credit is given
 * to the University of California at Berkeley. The name of the University
 * may not be used to endorse or promote products derived from this
 * software without specific written prior permission. This software
 * is provided ``as is'' without express or implied warranty.
 */

#if defined(SYSLIBC_SCCS) && !defined(lint)
_sccsid:.asciz	"@(#)execle.s	5.2 (Berkeley) 05/20/88"
#endif /* SYSLIBC_SCCS and not lint */

#include "SYS.h"

ENTRY(execle)
	movw	-2(fp),r0	# removed word.
	subw2	$4,r0
	shar	$2,r0,r0	# num. of args.
	pushl	(fp)[r0]
	pushab	8(fp)
	pushl	4(fp)
	calls	$16,_execve
	ret		# execle(file, arg1, arg2, ..., env);

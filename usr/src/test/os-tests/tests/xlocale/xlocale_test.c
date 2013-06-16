/*
 * This file and its contents are supplied under the terms of the
 * Common Development and Distribution License ("CDDL"), version 1.0.
 * You may only use this file in accordance with the terms of version
 * 1.0 of the CDDL.
 *
 * A full copy of the text of the CDDL should have accompanied this
 * source.  A copy of the CDDL is also available via the Internet at
 * http://www.illumos.org/license/CDDL.
 */

/*
 * Copyright 2013 David Hoeppner.  All rights reserved.
 */

#include <locale.h>
#include <stdlib.h>

static void
newlocale_test(void)
{
	locale_t	new_loc = NULL;

	new_loc = newlocale(LC_CTYPE_MASK, "loc1", (locale_t)NULL);
	if (new_loc == (locale_t)NULL)
		return;
}

static void
run_tests(void)
{
	newlocale_test();
}

int
main(int argc, char * const argv[])
{

	run_tests();

	exit(0);
}

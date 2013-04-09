/*
 * Copyright (c) 2011 The Chromium OS Authors.
 * See file CREDITS for list of people who contributed to this
 * project.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */

#include <stdio.h>
#include <stdlib.h>

#include "lib.h"
#include "shared-memory.h"

unsigned arg_verbose;

void verbose(const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	if (arg_verbose)
		vfprintf(stderr, fmt, ap);
	va_end(ap);
}

void fatal(char const * const msg)
{
	fprintf(stderr, "sandbox-daemon: fatal: %s\n", msg);
	exit(-1);
}

void cleanup_and_exit(void)
{
	remove_shared_memory();
	exit(0);
}

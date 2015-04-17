/*
 * Program to unwind the stack frames of a x86_64 process using libunwind
 *
 * Author: Arun Prakash Jana <engineerarun@gmail.com>
 * Copyright (C) 2014, 2015 by Arun Prakash Jana <engineerarun@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with tictactoe.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdio.h>
#include <errno.h>
#include <libunwind.h>
#include <libunwind-ptrace.h>

int main()
{
	unw_addr_space_t addrspace;
	struct UPT_info *uptinfo;
	unw_accessors_t accessors;

	/* Create address space for little endian */
	addrspace = unw_create_addr_space(&accessors, 0);
	if (!addrspace) {
		fprintf(stderr, "unw_create_addr_space failed\n");
		return -1;
	}

	unw_destroy_addr_space(addrspace);
	return 0;
}

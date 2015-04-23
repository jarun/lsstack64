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
#include <stdlib.h>
#include <unistd.h>
#include <stdarg.h>
#include <errno.h>
#include <string.h>
#include <libunwind.h>
#include <libunwind-ptrace.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/ptrace.h>

#include "log.h"

#define WAIT_TIME 1000

int current_log_level = DEBUG;

int process_stack(pid_t PID)
{
	unw_addr_space_t addrspace;
	struct UPT_info *uptinfo = NULL;
	unw_cursor_t cursor;
	unw_word_t RIP, RBP;

	int ret = 0;
	int wait_loops = 20;
	int waitstatus;
	int stopped = 0;

	/* Create address space for little endian */
	addrspace = unw_create_addr_space(&_UPT_accessors, 0);
	if (!addrspace) {
		log(ERROR, "unw_create_addr_space failed\n");
		return -1;
	}

        ret = ptrace(PTRACE_ATTACH, PID, NULL, NULL);
        if (0 != ret && 0 != errno) {
		log(ERROR, "ptrace failed. errno: %d (%s)\n", errno, strerror(errno));
		unw_destroy_addr_space(addrspace);
		return -1;
        }

	while (wait_loops-- > 0) {
		ret = waitpid(PID, &waitstatus, WUNTRACED | WNOHANG);
		if (WIFSTOPPED(waitstatus)) {
			stopped = 1;
			break;
		}
		usleep(WAIT_TIME);
	}

	if (!stopped) {
		log(ERROR, "Process %d couldn't be stopped\n", PID);
		ret = -1;
		goto bail;
	}

	uptinfo = (struct UPT_info *)_UPT_create(PID);
	if (!uptinfo) {
		log(ERROR, "_UPT_create failed\n");
		ret = -1;
		goto bail;
	}

	ret = unw_init_remote(&cursor, addrspace, (void *)uptinfo);
	if (ret < 0) {
		log(ERROR, "unw_init_remote failed\n");
		goto bail;
	}

	if (unw_get_reg(&cursor, UNW_X86_64_RIP, &RIP) < 0 || unw_get_reg(&cursor, UNW_X86_64_RBP, &RBP) < 0) {
		log(ERROR, "unw_get_reg RIP/RBP failed\n");
		ret = -1;
		goto bail;
	}

	log(INFO, "RIP: 0x%lx\n", RIP);
	log(INFO, "RBP: 0x%lx\n", RBP);

	ret = 0;

bail:
	if (uptinfo)
		_UPT_destroy(uptinfo);
	ptrace(PTRACE_DETACH, PID, NULL, NULL);
	unw_destroy_addr_space(addrspace);

	return ret;
}

int main(int argc, char **argv)
{
	pid_t PID = 1;

	if (argc !=2) {
		fprintf(stderr, "Usage: unwind PID\n");
		return -1;
	}

	if ((PID = atoi(argv[1])) <= 0) {
		fprintf(stderr, "Valid PID please!\n");
		return -1;
	}

	if (kill(PID, 0) == 0)
		log(INFO, "Tracing PID: %d\n", PID);
	else {
		log(ERROR, "kill failed. errno: %d (%s)\n", errno, strerror(errno));
		return -1;
	}

	if (process_stack(PID) != 0)
		log(ERROR, "Process stack printing failed\n");

	return 0;
}

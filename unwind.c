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

#include <stdlib.h>
#include <unistd.h>
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
	unw_proc_info_t procinfo;
	unw_cursor_t cursor;
	unw_word_t RIP, RSP, RBP, offset;

	int ret = 0, step = 0;
	int wait_loops = 20;
	int waitstatus;
	int stopped = 0;
	char procname[512] = {0};
	size_t len;

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

	do {
		log(INFO, "STACK FRAME %d\n", step);

		if (unw_get_reg(&cursor, UNW_X86_64_RIP, &RIP) < 0 || unw_get_reg(&cursor, UNW_X86_64_RSP, &RSP) < 0
				|| unw_get_reg(&cursor, UNW_X86_64_RBP, &RBP) < 0) {
			log(ERROR, "unw_get_reg RIP/RSP/RBP failed\n");
			ret = -1;
			goto bail;
		}

		procname[0] = '\0';
		unw_get_proc_name (&cursor, procname, sizeof(procname), &offset);
		if (offset) {
			len = strlen(procname);
			if (len >= sizeof(procname) - 32)
				len = sizeof(procname) - 32;
			sprintf((char *)(procname + len), "+0x%016lx", (unsigned long)offset);
		}
		log(INFO, "RIP = 0x%016lx %-32s\n", RIP, procname);
		log(INFO, "RSP = 0x%016lx RBP = 0x%016lx\n", RSP, RBP);

		ret = unw_get_proc_info (&cursor, &procinfo);
		if (ret < 0) {
			log(ERROR, "unw_get_proc_info failed. ret: %d\n", ret);
			goto bail;
		}

		log(INFO, "frame range = (0x%016lx <-> 0x%016lx)\n", procinfo.start_ip, procinfo.end_ip);
		log(INFO, "handler = %0lx lsda = %0lx\n\n\n", procinfo.handler, procinfo.lsda);

		ret = unw_step(&cursor);
		if (ret == 0) {
			log(DEBUG, "Last frame reached\n");
			goto bail;
		} else if (ret < 0) {
			log(ERROR, "unw_step failed. ret: %d\n", ret);
			goto bail;
		} else if (++step > 32) {
			log(ERROR, "Too deeply nested. Breaking out.\n");
			ret = -1;
			goto bail;
		}
	} while (ret > 0);

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
		log(INFO, "Tracing PID: %d\n\n", PID);
	else {
		log(ERROR, "kill failed. errno: %d (%s)\n", errno, strerror(errno));
		return -1;
	}

	if (process_stack(PID) != 0)
		log(ERROR, "Process stack printing failed\n");

	return 0;
}

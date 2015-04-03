/*

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */

/*
	Copyright (c) 2002 Bozeman Pass, Inc.
	Written by: David Boreham <david@bozmanpass.com>
 */


/*
	Todo: 
	install signal handler so we detatch and free the target if someone interrupts us.
	Figure out what we should be free()'ing at the end.
	Implement file and line number identification.
	Correctly handle the case where the target is expecting a signal as we attach to it.
	Correctly handle relative paths (LD_LIBRARY_PATH) on shared objects.
 */

#include <sys/types.h>
#include <unistd.h>
#include <linux/stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>

#include <sys/ptrace.h>
#include <asm/ptrace.h>
#include <sys/wait.h>

#include <link.h>

#include <bfd.h>

#include <stddef.h>
#include <unistd.h> 
#include <fcntl.h>

#ifndef false
#define false 0
#endif

static int verbose_option = 0;
static int debug_option = 0;
static int execute_option = 0;
static int period_option = 0;

static int wait_loops = 20;
static int wait_time = 1;
static const char* append_file = NULL;

static int pointer_size = 4; /* DBDB there has to be an official place to get this from */

typedef Elf32_Addr TARGET_ADDRESS;

struct _symbol_entry {
	TARGET_ADDRESS value;
	struct _symbol_entry *next;
	char *name;
};

typedef struct _symbol_entry symbol_entry;


typedef struct _process_info {
	int pid;
	int threads_present_flag;
	TARGET_ADDRESS link_map_head;
	TARGET_ADDRESS link_map_current; /* Used to iterate through the link map */
	symbol_entry *symbols;
	int *thread_pids;
	int initial_thread_id;
	int manager_thread_id;
} process_info;


static void msleep(int msecs)
{
	usleep(msecs*1000);
}

static int attach_target(int thepid)
{
	int ret;
	int waitstatus;
	int x;

	if (debug_option) printf("Attaching to the target process...\n");
	ret = ptrace(PTRACE_ATTACH, thepid, NULL, NULL);

	if (0 != ret && 0 != errno) {
		ret = errno;	
		return ret;
	}
	/* ptrace(PTRACE_ATTACH) does the equivalent of sending a SIG_STOP to the target.
	   So we should wait for that signal to be handled before proceeding.
	 */
	if (debug_option) printf("Waiting for target process to stop...\n");
	x = 0;
	while (x < wait_loops) {
		ret = waitpid(thepid, &waitstatus, WUNTRACED | WNOHANG);
		if (debug_option) {
			printf("waitpid after attach returned: %d, status=%d\n",ret, waitstatus);
		}
		if (WIFSTOPPED(waitstatus)) {
			return 0;
		}
		msleep(wait_time); /* Sleep for a bit so we don't busy wait */
		x++;
	}
	if (debug_option) printf("Target process has stopped.\n");
	
	/* If we did attach, install a signal handler to allow us to detatch if we're interrupted */
	if (0 == errno) {
		/* DBDB implement this */
		/* Try to catch the following signals: 
			SIGINT, SIGSEV, 
		 */
	}
  
	return errno;
}

static int attach_thread(int threadpid)
{
	int ret;
	int waitstatus;

	if (debug_option) printf("Attaching to the target thread %d...\n", threadpid);
	ret = ptrace(PTRACE_ATTACH, threadpid, NULL, NULL);

	if (0 != ret && 0 != errno) {
		perror("ptrace(PTRACE_ATTACH)");
		return errno;
	}
	while (1) {
		ret = waitpid(threadpid, &waitstatus, __WCLONE);
		if (ret > 0) {
			break;
		}
	}
	
	return errno;
}

static int detatch_target(process_info *pi)
{
	int ret;
	if (pi->threads_present_flag) {
		int thread_pid = 0;
		int x = 0;
		if (debug_option) printf("Detatching from threads...\n");
		for (x = 1; (pi->thread_pids)[x];x++) {
			thread_pid = (pi->thread_pids)[x];
			if (debug_option) printf("Detatching from thread %d\n", thread_pid);
			ret = ptrace(PTRACE_CONT, thread_pid, 1, 0);
			if (debug_option) printf("ptrace(PTRACE_CONT) returned: %d\n", ret);
		}
	}
	if (debug_option) printf("Detaching from target...\n");
	ret = ptrace(PTRACE_DETACH, pi->pid, 0, 0);
	if (debug_option) printf("ptrace(PTRACE_DETACH) returned: %d\n", ret);
	return ret;
}

process_info *pi_alloc(int pid)
{
	process_info* ret = (process_info*)calloc(sizeof(process_info),1);
	if (NULL != ret) {
		ret->pid = pid;
	}
	return ret;
}

void pi_free(process_info *pi)
{
	free(pi);
}

/* Symbol table helper functions */

void add_new_symbol(process_info *pi, symbol_entry *symbol)
{
	symbol_entry *temp = pi->symbols;
	pi->symbols = symbol;
	symbol->next = temp;
}

int symbol_entry_from_asymbol(process_info *pi, symbol_entry **outsym, asymbol *insym, TARGET_ADDRESS base_address)
{
	int ret = 0;
	int size = 0;
	
	size = sizeof(symbol_entry) + strlen(bfd_asymbol_name(insym)) + 1;
	*outsym = (symbol_entry*) malloc(size);
	if (NULL == *outsym) {
		fprintf(stderr, "Failed to allocate space for symbol\n");
		return ENOMEM;
	}
	(*outsym)->name = (char*)(*outsym) + sizeof(symbol_entry);
	
	(*outsym)->value = bfd_asymbol_value(insym) + base_address;
	
	strcpy( (*outsym)->name ,bfd_asymbol_name(insym));
	
	return ret;
}

int get_symbol_address(TARGET_ADDRESS *address, process_info *pi, char *symbol)
{
	int found = 0;
	symbol_entry *sym = NULL;
	if (debug_option) printf("Fetching address for symbol: %s\n",symbol);
	for (sym = pi->symbols; sym; sym = sym->next) {
		if (0 == strcmp(sym->name,symbol)) {
			if (debug_option) printf("Found symbol, value: 0x%x\n",sym->value);
			found = 1;
			*address = sym->value;
		}
	}
	return found;
}

static int max_symbol_distance = 1024 * 256; /* Addresses more than 256K from a symbol are in space */

int get_symbol_for_address(char** symbol, process_info *pi, TARGET_ADDRESS address, int include_difference)
{
	int ret = 0;
	int distance = max_symbol_distance;
	symbol_entry *hit = NULL;
	symbol_entry *sym = NULL;
	*symbol = NULL;
	/* Dumb implementation---linear scan through the symbols */
	for (sym = pi->symbols; sym; sym = sym->next) {		
		int d = (address - sym->value);
		if ( (d > 0) && (d < distance) ) {
			distance = d;
			hit = sym;
		}
	}
	if (distance < max_symbol_distance) {
		*symbol = malloc(strlen(hit->name) + 30);
		if (NULL == *symbol) {
			fprintf(stderr,"Failed to allocate symbol string\n");
			return ENOMEM;
		}
		if (distance && include_difference) {
			sprintf(*symbol,"%s + %d",hit->name,distance);
		} else {
			sprintf(*symbol,"%s",hit->name);
		}
	} else {
		*symbol = strdup("");
		ret = -1;
	}
	return ret;
}
	
/* End of symbol table helper functions */

/* Target memory read helper functions */

int read_target_pointer(TARGET_ADDRESS *value, process_info *pi, TARGET_ADDRESS address)
{
	int ret = 0;
	ret = ptrace(PTRACE_PEEKDATA, pi->pid, address, 0);
	if (errno) {
		ret = errno;
	} else {
		*value = (TARGET_ADDRESS) ret;
		ret = 0;
	}
	return ret;
}

int read_target_word(int *value, process_info *pi, TARGET_ADDRESS address)
{
	int ret = 0;
	ret = ptrace(PTRACE_PEEKDATA, pi->pid, address, 0);
	if (errno) {
		ret = errno;
	} else {
		*value = ret;
		ret = 0;
	}
	return ret;
}

int read_target_userpointer(TARGET_ADDRESS *value, process_info *pi, int thepid, TARGET_ADDRESS address)
{
	int ret = 0;
	ret = ptrace(PTRACE_PEEKUSER, thepid, address, 0);
	if (errno) {
		ret = errno;
	} else {
		*value = (TARGET_ADDRESS) ret;
		ret = 0;
	}
	return ret;
}

int read_target_byte(char *value, process_info *pi, TARGET_ADDRESS address)
{
	int ret = 0;
	/* Read a word at the target address, word aligned */
	TARGET_ADDRESS aligned_address = address & ~(pointer_size - 1);
	int byte = address - aligned_address;
	ret = ptrace(PTRACE_PEEKDATA, pi->pid, aligned_address);
	if (errno) {
		ret = errno;
	} else {
		*value = (ret >> (byte*8) ) & 0xff; 
		ret = 0;
	}
	return ret;
}

int read_target_memory(char *value, size_t length, process_info *pi, TARGET_ADDRESS address)
{
	/* We need to read word-aligned, otherwise ptrace blows up */
	int ret = 0;
	int offset = 0;
	for (offset = 0; offset < length; offset++ ) {
		ret = read_target_byte(value+offset, pi, address + offset);
		if (errno) {
			ret = errno;
			break;
		} else {
			ret = 0;
		}
	}
	return ret; 
}

int read_target_string(char **value, process_info *pi, TARGET_ADDRESS address)
{
	int ret = 0;
	int length = 0;
	char byte = 0;
	int offset = 0;
	
	/* First find out the string length */
	do {
		ret = read_target_byte(&byte,pi,address + length);	
		if (ret) {
			fprintf(stderr,"Failed to read string length from target address 0x%x : %s\n",address+length,strerror(ret));
			return ret;
		}
		if (byte) {
			length++;
		}
	} while (byte);
	if (debug_option) printf("read_target_string from address 0x%x, length=%d\n",address,length);
	/* Now allocate memory for the string and terminator */
	*value = malloc(length + 1);
	if (NULL == *value) {
		fprintf(stderr,"Failed to allocate string buffer in read_target_string\n");
		return ENOMEM;
	}

	for (offset = 0; offset < (length + 1); offset ++) {
		ret = read_target_byte((*value + offset),pi,address+offset);
		if (ret) {
			fprintf(stderr,"Failed to read string from target: %s\n",strerror(ret));
			break;
		} 
	}
	return ret;
}

void grok_and_print_program_counter(TARGET_ADDRESS pc, process_info *pi)
{
	char *symbol = NULL;
	int ret = 0;
	/* Get the symbol for this address */
	ret = get_symbol_for_address(&symbol,pi,pc,0);
	if (ret) {
		printf("0x%08x", pc);
	} else {	
		printf("0x%08x in %s", pc, symbol);
	}
}

/* We should get argument information from the debug data, but in the meantime we
   have to make do with the frame pointers. The compiler seems to set the frame pointer
   to 32-byte boundaries, so even when there is one argument, it looks like there are 6.
   For now, let's set a maximum number of arguments we want to print.
 */

static int maximum_number_of_arguments = 4;

int grok_and_print_function_arguments(TARGET_ADDRESS previous_bp,TARGET_ADDRESS next_bp, process_info *pi)
{
	int ret = 0;
	int x = 0;
	int number_of_arguments = ((next_bp - previous_bp) / pointer_size) - 2;
	char* comma = "";
	
	if (debug_option) printf("Found %d arguments\n", number_of_arguments);
	if (number_of_arguments > maximum_number_of_arguments) {
		number_of_arguments = maximum_number_of_arguments;
	}
	printf(" (");
	for (x = 2; x < (number_of_arguments + 2); x++) {
		int parameter = 0;
		TARGET_ADDRESS argument_pointer = previous_bp + (pointer_size * x);
		if (debug_option) printf("Reading argument from address 0x%08x\n", argument_pointer);
		ret = read_target_word(&parameter, pi, argument_pointer);
		if (ret) {
			fprintf(stderr, "Failed to read parameter from target: %s\n", strerror(ret) );
			return ret;
		} else {
			printf("%s0x%08x",comma,parameter);
		}
		comma = " ,";
	}
	printf(")\n");
	return ret;
}

int grok_and_print_thread_stack(process_info *pi, int thepid)
{
	int ret = 0;
	TARGET_ADDRESS ip;
	TARGET_ADDRESS bp;
	TARGET_ADDRESS previous_bp;
	TARGET_ADDRESS previous_ip;
	/* Get the IP and the BP */
	ret = read_target_userpointer(&ip,pi,thepid,EIP * pointer_size);
	if (ret) {
		if (debug_option) printf("Failed to read IP from target: %s\n", strerror(ret) );
			return ret;
	} else {
		if (debug_option) printf("Read IP: 0x%x\n",ip);
	}
	ret = read_target_userpointer(&bp,pi,thepid,EBP * pointer_size);
	if (ret) {
		if (debug_option) printf("Failed to read BP from target: %s\n", strerror(ret) );
			return ret;
	} else {
		if (debug_option) printf("Read BP: 0x%x\n",bp);
	}
	/* walk up the stack */
	previous_bp = bp;
	previous_ip = ip;
	while (1) {
		
		TARGET_ADDRESS next_bp;
		TARGET_ADDRESS next_ip;
		
		ret = read_target_pointer(&next_bp,pi,previous_bp);
		if (ret) {
			fprintf(stderr,"Failed to read next BP from target: %s\n", strerror(ret) );
			return ret;
		} else {
			if (debug_option) printf("Read next BP: 0x%x\n", next_bp);
		}
		
		ret = read_target_pointer(&next_ip,pi,previous_bp + pointer_size);
		if (ret) {
			fprintf(stderr,"Failed to read next IP from target: %s\n", strerror(ret) );
			return ret;
		} else {
			if (debug_option) printf("Read next IP: 0x%x\n", next_ip);
		}
		
		grok_and_print_program_counter(previous_ip, pi);
		
		if (NULL == (void*)next_bp) {
			if (debug_option) printf("Reached the top of the stack\n");
			printf("\n");
			break;
		} else {
			ret = grok_and_print_function_arguments(previous_bp, next_bp, pi);
			if (ret) {
				return ret;
			}
		}
		
		previous_bp = next_bp;
		previous_ip = next_ip;
	}
	return ret;
}

int grok_and_print_stacks(process_info *pi)
{
	int ret = 0;
	if (pi->threads_present_flag) {
		int thread_pid;
		int x;
		for (x = 0; (pi->thread_pids)[x] ; x++) {
			char *thread_name = "";
			thread_pid = (pi->thread_pids)[x];
			if (thread_pid == pi->initial_thread_id) {
				thread_name = " (initial thread)";
			} else {
				if (thread_pid == pi->manager_thread_id) {
					thread_name = " (manager thread)";
				}
			}
			printf("LWP %d%s:\n",thread_pid,thread_name);
			ret = grok_and_print_thread_stack(pi,thread_pid);
			if (ret) {
				break;
			}
		}
	} else {
		ret = grok_and_print_thread_stack(pi,pi->pid);
	}
	return ret;
}

/* End of target memory read helper functions */

int grok_threads(process_info *pi)
{
	int ret = 0;
	int thread_test_positive = 0;
	int loops = 0;
	
	/* Magic interaction with the pthread library here, copied from GDB */
	
	static char* magic_names[] = {
		"__pthread_threads_debug",
		"__pthread_handles",
		"__pthread_initial_thread",
		"__pthread_manager_thread",
		"__pthread_sizeof_handle",
		"__pthread_offsetof_descr",
		"__pthread_offsetof_pid",
		"__pthread_handles_num",
		NULL
	};
	
	static TARGET_ADDRESS magic_addresses[9] = {0};
		
	thread_test_positive = 1;
	for (loops = 0; NULL != magic_names[loops]; loops++) {
		ret = get_symbol_address(&magic_addresses[loops],pi,magic_names[loops]);
		if (!ret) {
			/* Failed to find one */
			thread_test_positive = 0;
			if (debug_option) printf("Failed to find thread symbol %s\n",magic_names[loops]);
			break;
		} else {
			if (debug_option) printf("Read thread symbol %s  -> 0x%x\n",magic_names[loops],magic_addresses[loops]);
		}
	}

	
	if (thread_test_positive) {
		/* we think that we have threads */
		int number_of_threads = 0;
		int *thread_pid_array = NULL;
		
		TARGET_ADDRESS handle_array = magic_addresses[1];
		int pid_offset = 0;
		int descriptor_offset = 0;
		int handle_size = 0;
		TARGET_ADDRESS initial_thread_descriptor, manager_thread_descriptor;
		
		/* How many ? */
		ret = read_target_word(&number_of_threads,pi,magic_addresses[7]);
		if (ret) {
			fprintf(stderr, "Failed to read number of threads from target: %s\n",strerror(ret));
			return ret;
		} else {
			if (debug_option) printf("Found %d threads\n", number_of_threads);
		}
				
		thread_pid_array = (int*) calloc(number_of_threads + 1, sizeof(int));
		if (NULL == thread_pid_array) {
			fprintf(stderr,"Failed to allocate thread pid array\n");
			return ENOMEM;
		}
		
		ret = read_target_pointer(&initial_thread_descriptor,pi,magic_addresses[2]);
		if (ret) {
			fprintf(stderr, "Failed to read initial thread descriptor from target: %s\n",strerror(ret));
			return ret;
		} else {
			if (debug_option) printf("Initial thread descriptor: 0x%08x\n", initial_thread_descriptor);
		}
		ret = read_target_pointer(&manager_thread_descriptor,pi,magic_addresses[3]);
		if (ret) {
			fprintf(stderr, "Failed to read manager thread descriptor from target: %s\n",strerror(ret));
			return ret;
		} else {
			if (debug_option) printf("Manager thread descriptor: 0x%08x\n", manager_thread_descriptor);
		}
		ret = read_target_word(&handle_size,pi,magic_addresses[4]);
		if (ret) {
			fprintf(stderr, "Failed to read handle size from target: %s\n",strerror(ret));
			return ret;
		} else {
			if (debug_option) printf("Handle Size: %d\n", handle_size);
		}
		ret = read_target_word(&pid_offset,pi,magic_addresses[6]);
		if (ret) {
			fprintf(stderr, "Failed to read pid offset from target: %s\n",strerror(ret));
			return ret;
		} else {
			if (debug_option) printf("Pid offset: %d\n", pid_offset);
		}
		ret = read_target_word(&descriptor_offset,pi,magic_addresses[5]);
		if (ret) {
			fprintf(stderr, "Failed to read descriptor offset from target: %s\n",strerror(ret));
			return ret;
		} else {
			if (debug_option) printf("Descriptor offset: %d\n", descriptor_offset);
		}

		/* Fill in the pid array, attaching as we go */
		
		for (loops = 0; loops < number_of_threads; loops++) {
			TARGET_ADDRESS descriptor_address;
			int thread_pid = 0;
			ret = read_target_pointer(&descriptor_address,pi,handle_array + descriptor_offset + (handle_size * loops));
			if (ret) {
				fprintf(stderr,"Failed to read descriptor address from target: %s\n", strerror(ret));
				return ret;
			} else {
				if (debug_option) printf("Descriptor Address: 0x%x\n",descriptor_address);
			}
			ret = read_target_word(&thread_pid,pi,descriptor_address + pid_offset);
			if (ret) {
				fprintf(stderr,"Failed to read thread pid from target: %s\n", strerror(ret));
				return ret;
			} else {
				if (debug_option) printf("Thread PID: %d\n",thread_pid);
			}
			
			if (descriptor_address == initial_thread_descriptor) {
				pi->initial_thread_id = thread_pid;
			} else {
				if (descriptor_address == manager_thread_descriptor) {
					pi->manager_thread_id = thread_pid;
				}
			}
			
			if (thread_pid != pi->pid) {
				ret = attach_thread(thread_pid);
				if (ret) {
					fprintf(stderr,"Failed to attach to target thread %d : %s\n", thread_pid, strerror(ret) );
					return ret;
				}
			}
			
			thread_pid_array[loops] = thread_pid;
		}
		
		pi->thread_pids = thread_pid_array;
		pi->threads_present_flag = 1;
	}
	
	return ret;
}

int process_symbol(process_info *pi, asymbol *sym, TARGET_ADDRESS base_address)
{
	int ret = 0;
	symbol_entry *outsym = NULL;
	if (debug_option) printf("Groking symbol: %s -> 0x%llx\n",bfd_asymbol_name(sym), bfd_asymbol_value(sym) + base_address);
	ret = symbol_entry_from_asymbol(pi,&outsym,sym, base_address);
	if (!ret) {
		add_new_symbol(pi,outsym);
	}
	return ret;
}


int get_file_symbols(process_info *pi, char* filename, TARGET_ADDRESS base_address)
{
	int ret = 0;
	char *target = NULL;
	bfd *file;
	
	if (debug_option) printf("get_file_symbols for %s\n", filename);
	
	file = bfd_openr (filename, target);
	if (NULL == file) {
		const char *errmsg = bfd_errmsg (bfd_get_error ());
		fprintf(stderr,"Failed to open file: %s (%s)\n",filename, errmsg);
		return -1;
    }
	if (debug_option) printf("opened file ok\n");
	/* We have to do this otherwise bfd crashes */
	bfd_check_format(file,bfd_object);
	/* Now get the symbols */
	
	{
		long storage_needed;
		asymbol **symbol_table;
		long number_of_symbols;
		long i;
		int dynamic = 0;

		storage_needed = bfd_get_symtab_upper_bound (file);

		if (storage_needed == 0) {
			if (debug_option) printf("storage needed == 0, trying dynamic\n");
			dynamic = 1;
			storage_needed = bfd_get_dynamic_symtab_upper_bound (file);
			if (storage_needed == 0) {
				if (debug_option) printf("storage needed still == 0, give up\n");
				return -1;
			}
        }
		if (debug_option) printf("storage needed = %ld\n", storage_needed);
        symbol_table = (asymbol **) malloc (storage_needed);
		if (NULL == symbol_table) {
			fprintf(stderr,"failed to allocate symbol table buffer\n");
			return ENOMEM;
		}
       
        number_of_symbols = dynamic ? 
            bfd_canonicalize_dynamic_symtab (file, symbol_table) :
			bfd_canonicalize_symtab (file, symbol_table);

		if (debug_option) printf("found %ld symbols\n",number_of_symbols);
		
		for (i = 0; i < number_of_symbols; i++) {
			process_symbol (pi,symbol_table[i],base_address);
		}
	}
	
	if (bfd_close (file) == false) {
		fprintf(stderr,"Error closing file: %s\n", filename);
		return -1;
	}
	return ret;
}

int dynamic_libs_present(process_info *pi)
{
	int ret = 0;
	/* If the symbol "_DYNAMIC" is present in the executable, then we return true and set the link map head in the pi. */
	TARGET_ADDRESS dynamic;
	
	ret = get_symbol_address(&dynamic,pi,"_DYNAMIC");
	if (ret) {
		/* Find the entry in the DYNAMIC array which has the valid entry we need. */
		int keep_looking = 1;
		int found_it = 0;
		TARGET_ADDRESS r_debug_address;
		while (keep_looking) {
			ElfW(Dyn) thisdyn;
			ret = read_target_memory((char*)&thisdyn, sizeof( ElfW(Dyn) ), pi, dynamic);
			if (ret) {
				fprintf(stderr,"Error %d (%s) reading from DYNAMIC array\n",ret,strerror(ret));
				return 0;
			} else {
				if (debug_option) {
					printf("Read ElfW(Dyn) from address 0x%x, tag=%d, value=0x%x\n",dynamic,thisdyn.d_tag,thisdyn.d_un.d_ptr);
				}
			}
			if (DT_NULL == thisdyn.d_tag) {
				if (debug_option) printf("Found a DT_NULL entry\n");
				keep_looking = 0;
			}
			if (DT_DEBUG == thisdyn.d_tag) {
				if (debug_option) printf("Found r_debug in _DYNAMIC array.\n");
				r_debug_address = thisdyn.d_un.d_ptr;
				keep_looking = 0;
				found_it = 1;
			}
			dynamic += sizeof( ElfW(Dyn) );
		}
		if (!found_it) {
			if (debug_option) printf("Didn't find r_debug in _DYNAMIC array.\n");
			return 0;		
		}

		{
			/* Get the link map head */
			TARGET_ADDRESS link_map_address;
			int r_map_offset = offsetof(struct r_debug, r_map);
			/* Now we've found the r_debug structure, get the link map from it. */
			ret = read_target_pointer(&link_map_address, pi, r_debug_address + r_map_offset );
			if (ret) {
				if (debug_option) printf("Failed to read link map address.\n");
			} else {
				if (debug_option) printf("Read r_map: 0x%x\n",link_map_address);
				pi->link_map_head = link_map_address;
				pi->link_map_current = link_map_address;
			}
		}
		ret = 1;
	} else {
		if (debug_option) printf("No _DYNAMIC symbol found in executable\n");
		ret = 0;
	}
	return ret;
}

int get_next_so_file_name(char** file_name, process_info *pi, TARGET_ADDRESS *base_address, int *more)
{
	int ret = 0;
	struct link_map lm; 
	/* Are we at the end of the list ? */
	if (NULL == (void*)pi->link_map_current) {
		*more = 0;
		return 0;
	} else {
		*more = 1;
	}
	/* Dereference the current pointer to get the filename */
	ret = read_target_memory((void*)&lm,sizeof(lm),pi,pi->link_map_current);
	if (ret) {
		fprintf(stderr,"Failed to read link map structure from target.\n");
		return ret;
	} else {
		if (debug_option) printf("Read link_map entry from address 0x%x, base=0x%x, name=0x%x, next=0x%x\n",(TARGET_ADDRESS)lm.l_addr,pi->link_map_current,(TARGET_ADDRESS)lm.l_name,(TARGET_ADDRESS)lm.l_next);
	}
	ret = read_target_string(file_name,pi,(TARGET_ADDRESS)lm.l_name);
	if (ret) {
		fprintf(stderr,"Failed to copy filename from target.\n");
		return ret;
	} else {
		if (debug_option) printf ("Copied filename: '%s' from target\n", *file_name);
	}
	*base_address = (TARGET_ADDRESS)lm.l_addr;
	/* Advance down the linked list */
	pi->link_map_current = (TARGET_ADDRESS)lm.l_next;
	return ret;
}

int grok_symbols(process_info *pi)
{
	int ret = 0;
	/* There are symbols in the executable, and also in any dynamic libraries it has loaded 
	   So we first get the executable's symbols, then look for dynamic libraries and get those too.
	 */
	/* First fill in the process executable file */
	char *format_string = "/proc/%d/exe";
	char *exe_file_name = calloc(strlen(format_string) + 10 ,1);
	
	bfd_init();
	
	sprintf(exe_file_name,format_string,pi->pid);
	if (debug_option) printf("Fetching symbols from executable: %s\n",exe_file_name);
	ret = get_file_symbols(pi,exe_file_name,0);
	if (!ret) {
		if (dynamic_libs_present(pi)) {
			int more_libs_to_check = 1;
			char *so_file_name = NULL;
			TARGET_ADDRESS base_address;
			while(more_libs_to_check) {
				ret = get_next_so_file_name(&so_file_name,pi,&base_address,&more_libs_to_check);
				if (ret) {
					return ret;
				}
				if (strlen(so_file_name) > 0) {
					if (debug_option) printf("Fetching symbols from shared object: %s\n",so_file_name);
					get_file_symbols(pi,so_file_name, base_address);
				} else {
					if (debug_option) printf("Skipping zero length so file name\n");
				}
			}
		}
	}
	return ret;
}

static void fatal(char* s)
{
	fprintf(stderr,"lsstack: fatal error: %s\n",s);
	exit(0);
}

static void usage()
{
	printf("lsstack: [-v] [-D] [-p peridod_in_ms] [-o file_to_append] {<pid> | -e program arguments}\n");
	exit(1);
}

int main(int argc, char** argv)
{
	/* look for command line options */
	int pid = 0;
	int ret = 0;
	process_info *pi = NULL;
	int option_position = 1;
	
	while ( option_position < (argc-1) && *argv[option_position] == '-') {
		switch (*(argv[option_position]+1)) {
			case 'v':
				verbose_option = 1;
				break;
			case 'D':
				debug_option = 1;
				break;
			case 'e':
				execute_option = 1;
				break;
			case 'p':
				++option_position;
				period_option = atoi(argv[option_position]);
				break;
			case 'o':
				++option_position;
				append_file = argv[option_position];
				break;
			default:
				usage();
				break;
		}
		option_position++;
		if(execute_option) {
		    break;
		}
	}
	if (execute_option) {
	    pid = fork();
	    if (pid) {
		execvp(argv[option_position], argv+option_position);
		return 1;
	    } else {
		pid = getppid();
		msleep(1);
	    }
	} else {
	    if (option_position != (argc-1) ) {
		    usage();
	    }
	    pid = atoi(argv[option_position]);
	    if (0 == pid) {
		    usage();
	    }
	}

	if (append_file) {
	    close(1);
	    int fd = open(append_file, O_WRONLY|O_APPEND|O_CREAT, 0666);
	    dup2(fd, 1);
	}
	
	if (debug_option) {
		printf("verbose option: %s\n",verbose_option?"on":"off");
		printf("pid: %d\n",pid);
	}
	
	/* check that the pesky user hasn't tried to lsstack himself */
	if (pid == getpid() ) {
		fprintf(stderr,"Error: specified pid belongs to the lsstack process\n");
		exit(1);
	}
	
here_we_go_in_polling_mode:

	/* See if we can attach to the target */
	ret = attach_target(pid);
	
	if (ret) {
		if(!period_option) {
		    fprintf(stderr,"Failed to attach to the target process: %s\n", strerror(ret) );
		}
		exit(1);
	}
	
	if (debug_option) printf("Attached to target process\n");
	
	pi = pi_alloc(pid);
	if (NULL == pi) fatal("failed to allocate process info structure\n");
		
	ret = grok_symbols(pi);
	
	ret = grok_threads(pi);

	ret = grok_and_print_stacks(pi);
	
	detatch_target(pi);
	
	pi_free(pi);
	
	if (debug_option) printf("Detatched from target process\n");

	if(period_option) {
	    msleep(period_option);
	    goto here_we_go_in_polling_mode;
	}
	
	return 0;
}


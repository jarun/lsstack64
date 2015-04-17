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

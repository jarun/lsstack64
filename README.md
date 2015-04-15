# lsstack64
lsstack for x86_64 architecture.  
A fork of the pstack utility for Solaris which works only for x86 arch. Originally it's not even compilable on x86_64. The current project is an attempt to make it work on x86_64 Linux.  

15 Apr 2015: After some research, it turns out that x86_64 does NOT same the previous frame's frame pointer in RBP register (other than those compiled without -fomit-frame-pointer in GCC). The default optimization level in GCC is O2, which includes -fomit-frame-pointer. So the next strategy would be to use libunwind (http://www.nongnu.org/libunwind/).

License: While the original utility is GPLv2, the current project is licensed under GPLv3.


Important links:  
http://sourceforge.net/projects/lsstack/  
http://lxr.free-electrons.com/source/arch/powerpc/boot/elf.h#L12  
http://linux.die.net/man/2/ptrace  
http://linux.die.net/include/sys/user.h  
http://lxr.free-electrons.com/source/arch/x86/include/asm/ptrace.h?v=3.3  

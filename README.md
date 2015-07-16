# lsstack64
lsstack for x86_64 architecture.  
A fork of the pstack utility for Solaris which works only for x86 arch. Originally it's not even compilable on x86_64. The current project is an attempt to make it work on x86_64 Linux.  
  
# License  
  
While the original utility is GPLv2, the current project is licensed under GPLv3.  

**********
# Instructions (tested on Ubuntu 14.04 x86_64)  

Compilation:  
$ git clone https://github.com/jarun/lsstack64  
$ cd lsstack64  
$ make  
$ sudo make install  
  
Usage:  
$ unwind PID  
// as long as you are the owner of the process, use sudo otherwise  
// unwind is the test program on x86_64. The functionality will be merged to lsstack64.
  
A sample output with SRWare Iron browser: http://pastebin.com/vTFhDUD2  
  
**********
# News  

16 Jul 2015: Implemented thread tracing support.

03 Jun 2015: Merging unwind.c fnctionality to lsstack64 started. New logging mechanism incorporated into lsstack64.  

25 Apr 2015: unwind.c is the new stub progam that uses libunwind to run through all the stack frames. The goal is to make unwind provide the information that pstack did.  

23 Apr 2015: log.h and log.c demonstrate a tiny, easy to use and extensible logging frameowork with severity level and useful information for easy debugging.  

15 Apr 2015: After some research, it turns out that x86_64 does NOT save the previous frame's frame pointer in RBP register (other than those compiled without -fomit-frame-pointer in GCC). The default optimization level in GCC is O2, which includes -fomit-frame-pointer. So the next strategy would be to use libunwind (http://www.nongnu.org/libunwind/).  
**********  

# Important links  
http://sourceforge.net/projects/lsstack/  
http://lxr.free-electrons.com/source/arch/powerpc/boot/elf.h#L12  
http://linux.die.net/man/2/ptrace  
http://linux.die.net/include/sys/user.h  
http://lxr.free-electrons.com/source/arch/x86/include/asm/ptrace.h?v=3.3  
http://www.nongnu.org/libunwind/man/libunwind(3).html#section_3  
http://manpages.ubuntu.com/manpages/trusty/en/man3/libunwind-ptrace.3.html  
http://stackoverflow.com/questions/3194479/relation-between-program-instruction-pointerrip-and-base-frame-pointerrbp-on  
http://stackoverflow.com/questions/8625352/x86-64-calling-conventions-and-stack-frames  
  
// Threads  
http://stackoverflow.com/questions/6402160/getting-a-backtrace-of-other-thread  
http://comments.gmane.org/gmane.comp.lib.unwind.devel/696
http://stackoverflow.com/questions/11468333/linux-threads-suspend-resume
http://stackoverflow.com/questions/18577956/how-to-use-ptrace-to-get-a-consistent-view-of-multiple-threads
  
**********
# Contributions  
[Ellié Computing](http://www.elliecomputing.com/) contributes to this project by giving free licences of ECMerge, comparison/merge tool.

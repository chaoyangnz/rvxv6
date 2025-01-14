#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "defs.h"

volatile static int started = 0;

extern char bss_start[], bss_end[];

// start() jumps here in supervisor mode on all CPUs.
void
main()
{
  // clear BSS
  for (volatile unsigned int *p = (volatile unsigned int *)bss_start; p < (volatile unsigned int*)bss_end; p++) {
	*p = 0;
  }

  if(cpuid() == 0){
    consoleinit();
    printfinit();
    printf("\n");
    printf(" +-+-+-+-+-+\n |r|v|x|o|s|\n +-+-+-+-+-+\n");
    printf("kernel is booting\n");
    printf("\n");
    kinit();         // physical page allocator
    kvminit();       // create kernel page table
    kvminithart();   // turn on paging
    procinit();      // process table
    trapinit();      // trap vectors
    trapinithart();  // install kernel trap vector
    plicinit();      // set up interrupt controller
    plicinithart();  // ask PLIC for device interrupts
    binit();         // buffer cache
    iinit();         // inode cache
    fileinit();      // file table
    ramdiskinit(); // emulated hard disk
    userinit();      // first user process
    __sync_synchronize();
    started = 1;
  } else {
    while(started == 0)
      ;
    __sync_synchronize();
    printf("hart %d starting\n", cpuid());
    kvminithart();    // turn on paging
    trapinithart();   // install kernel trap vector
    plicinithart();   // ask PLIC for device interrupts
  }

  scheduler();        
}

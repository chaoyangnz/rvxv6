#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "defs.h"

void main();
void timerinit();
void pmpinit();

// entry.S needs one stack per CPU. global variable stack0
__attribute__ ((aligned (16))) char stack0[4096 * NCPU];

// a scratch area per CPU for machine-mode timer interrupts. global variable timer_scratch
uint64 timer_scratch[NCPU][5];

// entry.S jumps here in machine mode on stack0.
void
start()
{
  sys_clock_init();
  sys_uart0_init();

  // set M Previous Privilege mode to Supervisor, for mret.
  unsigned long x = r_mstatus();
  x &= ~MSTATUS_MPP_MASK;
  x |= MSTATUS_MPP_S;
  w_mstatus(x);

  // set M Exception Program Counter to main, for mret.
  // requires gcc -mcmodel=medany
  w_mepc((uint64)main);

  // disable paging for now.
  w_satp(0);

  // delegate all interrupts and exceptions to supervisor mode.
  w_medeleg(0xffff);
  w_mideleg(0xffff);
  w_sie(r_sie() | SIE_SEIE | SIE_STIE | SIE_SSIE);

  // switch to supervisor mode and jump to main().
  pmpinit();

  // ask for clock interrupts.
  timerinit();

  *(uint32 *)PLIC_CTRL = 1;

  // keep each CPU's hartid in its tp register, for cpuid().
  int id = r_mhartid();
  w_tp(id);

  asm volatile("mret");
}

// set up to receive timer interrupts in machine mode,
// which arrive at timervec in kernelvec.S,
// which turns them into software interrupts for
// devintr() in trap.c.
void
timerinit()
{
  // each CPU has a separate source of timer interrupts.
  int id = r_mhartid();

  // ask the CLINT for a timer interrupt.
  int interval = 1000000; // cycles; about 1/10th second in qemu.

  uint64 t = r_mtime() + 10000000;
  *(uint32*)CLINT_MTIMECMP(id) = t & 0xffffffff;
  *(uint32*)(CLINT_MTIMECMP(id)+4) = t >> 32; // ???

  // prepare information in scratch[] for timervec.
  // scratch[0..2] : space for timervec to save registers.
  // scratch[3] : address of CLINT MTIMECMP register.
  // scratch[4] : desired interval (in cycles) between timer interrupts.
  uint64 *scratch = &timer_scratch[id][0];
  scratch[3] = CLINT_MTIMECMP(id);
  scratch[4] = interval;
  w_mscratch((uint64)scratch);

  // set the machine-mode trap handler.
  w_mtvec((uint64)timervec);

  // enable machine-mode interrupts.
  w_mstatus(r_mstatus() | MSTATUS_MIE);

  // enable machine-mode timer interrupts.
  w_mie(r_mie() | MIE_MEIE | MIE_MTIE | MIE_MSIE);
}

void pmpinit()
{
  // see https://l1nxy.me/2021/05/26/fix-xv6-boot/
  w_pmpaddr0(0xffffffffULL);
  w_pmpcfg0(0x0FULL); // PMP_R | PMP_W | PMP_X | PMP_MATCH_TOR);
}


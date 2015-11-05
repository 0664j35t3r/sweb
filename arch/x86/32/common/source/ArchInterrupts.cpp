/**
 * @file ArchInterrupts.cpp
 *
 */

#include "ArchInterrupts.h"
#include "8259.h"
#include "ports.h"
#include "InterruptUtils.h"
#include "SegmentUtils.h"
#include "ArchThreads.h"
#include "kprintf.h"

void ArchInterrupts::initialise()
{
  uint16 i; // disableInterrupts();
  initialise8259s();
  SegmentUtils::initialise();
  InterruptUtils::initialise();
  for (i=0;i<16;++i)
    disableIRQ(i);
}

void ArchInterrupts::initialise_ap()
{
  InterruptUtils::initialise_ap();
}

void ArchInterrupts::enableTimer()
{
  enableIRQ(0);
}

void ArchInterrupts::disableTimer()
{
  disableIRQ(0);
}

void ArchInterrupts::enableKBD()
{
  enableIRQ(1);
  enableIRQ(9);
}

void ArchInterrupts::disableKBD()
{
  disableIRQ(1);
}

void ArchInterrupts::EndOfInterrupt(uint16 number) 
{
  sendEOI(number);
}

void ArchInterrupts::enableInterrupts()
{
     __asm__ __volatile__("sti"
   :
   :
   );
}

bool ArchInterrupts::disableInterrupts()
{
   uint32 ret_val;

 __asm__ __volatile__("pushfl\n"
                      "popl %0\n"
                      "cli"
 : "=a"(ret_val)
 :);

return (ret_val & (1 << 9));  //testing IF Flag

}

bool ArchInterrupts::testIFSet()
{
  uint32 ret_val;

  __asm__ __volatile__(
  "pushfl\n"
  "popl %0\n"
  : "=a"(ret_val)
  :);

  return (ret_val & (1 << 9));  //testing IF Flag
}

void ArchInterrupts::yieldIfIFSet()
{
  extern uint32 boot_completed;
  if (boot_completed && currentThread && testIFSet())
  {
    ArchThreads::yield();
  }
  else
  {
    __asm__ __volatile__("nop");
  }
}


struct context_switch_registers {
  uint32 es;
  uint32 ds;
  uint32 edi;
  uint32 esi;
  uint32 ebp;
  uint32 esp;
  uint32 ebx;
  uint32 edx;
  uint32 ecx;
  uint32 eax;
};

struct interrupt_registers {
  uint32 eip;
  uint32 cs;
  uint32 eflags;
  uint32 esp3;
  uint32 ss3;
  uint32 gs;
};

extern "C" void arch_saveThreadRegisters(uint32 error, uint32 load_gs)
{
  register struct context_switch_registers* registers;
  registers = (struct context_switch_registers*) (&error + 3);
  register struct interrupt_registers* iregisters;
  iregisters = (struct interrupt_registers*) (&error + 3 + sizeof(struct context_switch_registers)/sizeof(uint32) + (error));

  if (load_gs)
  {
	  asm volatile("mov %0,%%eax\n"
			       "mov %%ax,%%gs\n"
			       :: "m" (iregisters->gs) : "eax");
  }

  register ArchThreadInfo* info = currentThreadInfo;

  asm("fnsave (%0)\n"
      "frstor (%0)\n"
      :
      : "r"((void*)(&(info->fpu)))
      :);

  if (iregisters->cs & 0x3)
  {
    info->ss = iregisters->ss3;
    info->esp = iregisters->esp3;
  }
  else
  {
    info->esp = registers->esp + 0xc;
  }
  info->eip = iregisters->eip;
  info->cs = iregisters->cs;
  info->eflags = iregisters->eflags;
  info->eax = registers->eax;
  info->ecx = registers->ecx;
  info->edx = registers->edx;
  info->ebx = registers->ebx;
  info->ebp = registers->ebp;
  info->esi = registers->esi;
  info->edi = registers->edi;
  info->ds = registers->ds;
  info->es = registers->es;
}

#include "types.h"
#include "riscv.h"
#include "param.h"
#include "defs.h"
#include "memlayout.h"
#include "spinlock.h"
#include "proc.h"
#include "sysinfo.h"

uint64
sys_exit(void)
{
  int n;
  argint(0, &n);
  exit(n);
  return 0;  // not reached
}

uint64
sys_getpid(void)
{
  return myproc()->pid;
}

uint64
sys_fork(void)
{
  return fork();
}

uint64
sys_wait(void)
{
  uint64 p;
  argaddr(0, &p);
  return wait(p);
}

uint64
sys_sbrk(void)
{
  uint64 addr;
  int n;

  argint(0, &n);
  addr = myproc()->sz;
  if(growproc(n) < 0)
    return -1;
  return addr;
}

uint64
sys_sleep(void)
{
  int n;
  uint ticks0;


  argint(0, &n);
  acquire(&tickslock);
  ticks0 = ticks;
#ifdef LAB_TRAPS
  backtrace();
#endif
  while(ticks - ticks0 < n){
    if(killed(myproc())){
      release(&tickslock);
      return -1;
    }
    sleep(&ticks, &tickslock);
  }
  release(&tickslock);
  return 0;
}


#ifdef LAB_PGTBL
int
sys_pgaccess(void)
{
  // lab pgtbl: your code here.
  uint64 first_address = 0;
  int check_num = 0;
  uint64 buffer_address = 0;
  argaddr(0,&first_address);
  argint(1,&check_num);
  argaddr(2,&buffer_address);
  pagetable_t pagetable = myproc()->pagetable;
  pte_t *pte;
  uint64 mask = 0;
  for (int i = 0; i < check_num; i++) {
    if ((pte = walk(pagetable, first_address + i * PGSIZE, 0)) == 0){
      return -1;
    }
    if (*pte & PTE_A) {
      mask |= (1 << i);
      *pte ^= PTE_A;
    }
  }
  if (copyout(pagetable, buffer_address, (char*)&mask, sizeof(mask)) < 0) {
    return -1;
  }
  return 0;
}
#endif

uint64
sys_kill(void)
{
  int pid;

  argint(0, &pid);
  return kill(pid);
}

// return how many clock tick interrupts have occurred
// since start.
uint64
sys_uptime(void)
{
  uint xticks;

  acquire(&tickslock);
  xticks = ticks;
  release(&tickslock);
  return xticks;
}

uint64
sys_trace(void){
  int trace_sys_mask;
  argint(0, &trace_sys_mask);
  myproc()->tracemask |= trace_sys_mask;
  return 0;
}

uint64
sys_sysinfo(void) {
  struct proc *my_proc = myproc();
  uint64 addr;
  argaddr(0, &addr);//get the buffer address
  struct sysinfo s;//construct the sysinfo in kernel
  s.freemem = kfree_mem();//get free memory
  s.nproc = count_free_proc();//get number of not UNUSED process
  if (copyout(my_proc->pagetable, addr, (char *)&s, sizeof (s)) < 0) {//copy to user
    return -1;
  }
  return 0;
}

uint64
sys_sigalarm(void) {
  int interval;
  uint64 handler;
  argint(0, &interval);
  argaddr(1, &handler);
  myproc()->alarm_interval = interval;
  myproc()->alarm_handler = (void (*)()) handler;
  myproc()->ticks_since_last_alarm = 0;
  return 0;
}

uint64
sys_sigreturn(void) {
  struct proc* my_proc = myproc();
  my_proc->in_alarm = 0;//set in_alarm to tell not in alarm
  *my_proc->trapframe = *my_proc->alarmframe;//restore trapframe
  my_proc->ticks_since_last_alarm = 0;//reset ticks
  return (*my_proc->trapframe).a0;
}
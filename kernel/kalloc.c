// Physical memory allocator, for user processes,
// kernel stacks, page-table pages,
// and pipe buffers. Allocates whole 4096-byte pages.

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "defs.h"

void freerange(void *pa_start, void *pa_end);

extern char end[]; // first address after kernel.
                   // defined by kernel.ld.

struct run {
  struct run *next;
};

struct {
  struct spinlock lock[NCPU];
  struct run *freelist[NCPU];
} kmem;


int cowcount[PHYSTOP/PGSIZE];
struct spinlock cowlock;


void
kinit()
{
  for(int i = 0; i < NCPU; i++){
    char fmt[8] = {'k','m','e','m','_','0','\0'};
    char id = i + '0';
    fmt[5] = id;
    initlock(&kmem.lock[i], fmt);
  }
  initlock(&cowlock,"cowlock");
  freerange(end, (void*)PHYSTOP);
}

void
freerange(void *pa_start, void *pa_end)
{
  char *p;
  p = (char*)PGROUNDUP((uint64)pa_start);
  for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE){
    cow_set_one((uint64)p);
    kfree(p);
  }  
}

// Free the page of physical memory pointed at by pa,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void
kfree(void *pa)
{
  int cow_flag;
  cow_flag = cow_down((uint64)pa);
  if(!cow_flag)return;

  struct run *r;

  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);

  r = (struct run*)pa;

  push_off();

  int cpu = cpuid();
  acquire(&kmem.lock[cpu]);
  r->next = kmem.freelist[cpu];
  kmem.freelist[cpu] = r;
  release(&kmem.lock[cpu]);

  pop_off();
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void)
{
  struct run *r;

  push_off();
  int cpu = cpuid();
  int steal = cpu;

  acquire(&kmem.lock[cpu]);
  r = kmem.freelist[cpu];
  if(r)
    kmem.freelist[cpu] = r->next;
  release(&kmem.lock[cpu]);

  if (r == 0) {
    for (int i = 1; i < NCPU; i++) {
      steal = (cpu + i) % NCPU;
      acquire(&kmem.lock[steal]);
      r = kmem.freelist[steal];
      if (r) {
        kmem.freelist[steal] = r->next;
      }
      release(&kmem.lock[steal]);
      if (r) {
        break;
      }
    }
  }

  if(r){
    cow_set_one((uint64)(r));
    memset((char*)r, 5, PGSIZE); // fill with junk
  }
  pop_off();
  return (void*)r;
}

uint64
kfree_mem(void) {//return the number of free memory
  struct run *r;
  uint64 freemem = 0;
  for(int cpu = 0;cpu < NCPU;cpu++){
    acquire(&kmem.lock[cpu]);
    r = kmem.freelist[cpu];
    while(r){
        freemem += PGSIZE;
        r = r->next;
    }
    release(&kmem.lock[cpu]);
    }
  return freemem;
}

int get_cow_count(uint64 pa){
  int count; 
  acquire(&cowlock);
  count = cowcount[(uint64)pa / PGSIZE];
  release(&cowlock);
  return count;
}

void cow_up(uint64 pa){
  acquire(&cowlock);
  ++cowcount[(uint64)pa / PGSIZE];
  release(&cowlock);
  return;
}

int cow_down(uint64 pa){//flag == 1,代表free前是最后一个，flag == 0，代表free前cow至少有两个
  int flag = 0;
  acquire(&cowlock);
  cowcount[(uint64)pa / PGSIZE]--;
  if((cowcount[(uint64)pa / PGSIZE]) == 0){
    flag = 1;
  }
  release(&cowlock);
  return flag;
}

void cow_set_one(uint64 pa){
  acquire(&cowlock);
  cowcount[(uint64)pa / PGSIZE] = 1;
  release(&cowlock);
  return;
}
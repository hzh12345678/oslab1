// Buffer cache.
//
// The buffer cache is a linked list of buf structures holding
// cached copies of disk block contents.  Caching disk blocks
// in memory reduces the number of disk reads and also provides
// a synchronization point for disk blocks used by multiple processes.
//
// Interface:
// * To get a buffer for a particular disk block, call bread.
// * After changing buffer data, call bwrite to write it to disk.
// * When done with the buffer, call brelse.
// * Do not use the buffer after calling brelse.
// * Only one process at a time can use a buffer,
//     so do not keep them longer than necessary.


#include "types.h"
#include "param.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "riscv.h"
#include "defs.h"
#include "fs.h"
#include "buf.h"

#define NUM_HASH_BUCKET 13
#define BUFFER_PER_BUCKET 5

struct {
  struct spinlock lock;
  struct buf buf[BUFFER_PER_BUCKET];
} bcache[NUM_HASH_BUCKET];

void
binit(void)
{
  for(int i = 0; i < NUM_HASH_BUCKET; i++){
    char fmt[10] = {'b','c','a','c','h','e','_','0','\0'};
    char id = i + '0';
    fmt[7] = id;
    initlock(&bcache[i].lock, fmt);
    for(int j=0;j<BUFFER_PER_BUCKET;j++){
        initsleeplock(&bcache[i].buf[j].lock, "buffer");
    }
  }
}

int hash(uint blockno)
{
  return blockno % NUM_HASH_BUCKET;
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf*
bget(uint dev, uint blockno)
{
  struct buf *b;
  int bucketid = hash(blockno);
  acquire(&bcache[bucketid].lock);
  for(int i=0;i<BUFFER_PER_BUCKET;i++){
      b = &(bcache[bucketid].buf[i]);
      if(b->dev == dev && b->blockno == blockno){
          b->refcnt++;
          release(&bcache[bucketid].lock);
          acquiresleep(&b->lock);
          return b;
      }
  }
  for(int bufferid = 0;bufferid < BUFFER_PER_BUCKET;bufferid++){
    b = &(bcache[bucketid].buf[bufferid]);
    if(b->refcnt == 0){
      b->dev = dev;
      b->blockno = blockno;
      b->valid = 0;
      b->refcnt = 1;
      release(&bcache[bucketid].lock);
      acquiresleep(&b->lock);
      return b;
    }
  }
  release(&bcache[bucketid].lock);
  panic("bget: no unused buffer for recycle"); 
  acquiresleep(&b->lock);
  return b;
}

// Return a locked buf with the contents of the indicated block.
struct buf*
bread(uint dev, uint blockno)
{
  struct buf *b;

  b = bget(dev, blockno);
  if(!b->valid) {
    virtio_disk_rw(b, 0);
    b->valid = 1;
  }
  return b;
}

// Write b's contents to disk.  Must be locked.
void
bwrite(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("bwrite");
  virtio_disk_rw(b, 1);
}

// Release a locked buffer.
// Move to the head of the most-recently-used list.
void
brelse(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("brelse");

  int bucketid = hash(b->blockno);
  acquire(&bcache[bucketid].lock);
  b->refcnt--;
  releasesleep(&b->lock);
  release(&bcache[bucketid].lock);
}

void
bpin(struct buf *b) {
  int bucketid = hash(b->blockno);
  acquire(&bcache[bucketid].lock);
  b->refcnt++;
  release(&bcache[bucketid].lock);
}

void
bunpin(struct buf *b) {
  int bucketid = hash(b->blockno);
  acquire(&bcache[bucketid].lock);
  b->refcnt--;
  release(&bcache[bucketid].lock);
}



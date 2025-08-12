#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>

/*
Notes:
Why (h + 1) Works:
Pointer Arithmetic:

In C, adding 1 to a pointer of type (struct header *) advances the pointer by
sizeof(struct header) bytes. E.g., if struct header is 16 bytes, (h + 1)
moves the pointer 16 bytes forward in memory.

Memory Layout:
The struct header is stored at the memory location pointed to by h.
By advancing h by 1 (using (h + 1)), we effectively move past the header to the
start of the memory block following it.

Why Not Use sizeof(header)?:
While we could manually compute the memory location by using ((void *)h +
sizeof(struct header)), using (h + 1) is simpler, more readable, and less
error-prone. The compiler automatically calculates sizeof(struct header) when
performing pointer arithmetic, so there's no need to do it explicitly.
*/

/**
 * Plan:
 * - store a header in front of every allocated block with the following info:
 *    - number of bytes in this block
 *    - whether the block is free (1) or allocated (0)
 * - when allocating, walk linearly from heap start to heap end and check
 * if there is a free block (use block's header) to find a first fit
 * -
 */

#if defined(__x86_64__) || defined(_M_X64) || defined(__aarch64__) ||          \
    defined(__LP64__)
#define ALIGNMENT 8
#else
#define ALIGNMENT 4
#endif

// Aligns a size s upwards to the next multiple of ALIGNMENT value
#define ALIGN(s) (((uintptr_t)(s) + (ALIGNMENT - 1)) & ~(ALIGNMENT - 1))
#define HEAP_SIZE (1 << 20) // 1 Mb

// Error codes
#define ERR_NONE 0
#define ERR_MMAP_FAILED -1
#define ERR_OUT_OF_MEM -2
#define ERR_INVALID_FREE -3

// Global error state
static int last_error = ERR_NONE;

void *heapStart = NULL;
void *heapEnd = NULL;
void *heapMax = NULL;

// TODO: should be single int with low order bit for
// freeness information
// Each allocated block includes a header that stores
// metadata per block (its size and whether it’s free).
struct header {
  size_t size;
  int free;
};

// Helper: set error and return NULL
static void *alloc_error(int code, const char *msg) {
  last_error = code;
  if (msg)
    fprintf(stderr, "Allocator error: %s\n", msg);
  return NULL;
}

void *myAlloc(size_t size) {
  last_error = ERR_NONE;

  if (size == 0)
    return alloc_error(ERR_OUT_OF_MEM, "Cannot allocate 0 bytes");

  // Add memory alignment
  size_t alignedSize = ALIGN(size);
  struct header *h;

  // If the heap is not initialised
  if (heapStart == NULL) {
    // Allocate a block of memory via mmap and treat it as the heap.
    heapStart = mmap(NULL, HEAP_SIZE, PROT_READ | PROT_WRITE,
                     MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
    if (heapStart == MAP_FAILED) {
      return alloc_error(ERR_MMAP_FAILED, strerror(errno));
    }

    printf("mmap succeeded, heapStart = %p\n", heapStart);
    // mmap() returns a pointer to the mapped region --> set the heapend
    heapEnd = heapStart;
    heapMax = (char *)heapStart + HEAP_SIZE;
  }
  // Iterate from the beginning of the heap, checking each header.
  void *p = heapStart;
  while (p < heapEnd) {
    // Cast the header pointer to the current pointer
    h = (struct header *)p;
    // If a block is free and the h->size >= size, reuse that block.
    if (h->free && h->size >= alignedSize) {
      h->free = 0;
      // Return a pointer to the usable memory block,
      // which is right after the header (h + 1).
      return (void *)(h + 1);
    }
    // If no such blocks, move to the next block.
    p += sizeof(struct header) + h->size;
  }

  // Allocate at heap end
  if ((char *)heapEnd + sizeof(struct header) + alignedSize > (char *)heapMax) {
    return alloc_error(ERR_OUT_OF_MEM, "Heap out of memory");
  }

  h = (struct header *)heapEnd;
  h->size = alignedSize;
  h->free = 0;
  // Progress the heapEnd by the size of the header +
  // the size of the block we're allocating
  // - h + 1: skips past the header (struct header)
  // - size: advances by the size of the block being allocated.
  heapEnd = (void *)(h + 1) + alignedSize;
  // Return a pointer to the usable memory block,
  // which is right after the header (h + 1).
  return (void *)(h + 1);
}

void myFree(void *p) {
  last_error = ERR_NONE;

  if (p == NULL)
    return;

  // Check if p is within heap bounds
  if ((char *)p < (char *)heapStart + sizeof(struct header) ||
      (char *)p >= (char *)heapEnd) {
    alloc_error(ERR_INVALID_FREE, "invalid free pointer");
    return;
  }

  // Go backwards in memory from the payload pointer to the
  // header of that block and set it to free
  struct header *h = (struct header *)p - 1;
  h->free = 1;
}

int main() {
  int *p = (int *)myAlloc(4);
  char *q = (char *)myAlloc(1000);
  *p = 42;
  *q = 'A';
  assert(*p == 42);
  assert(*q == 'A');

  myFree(p);
  // Reuse the space
  int *r = (int *)myAlloc(4);
  *r = 19;
  assert(*r == 19);
  assert(p == r);
  printf("p: %p, q: %p, r: %p\n", p, q, r);

  return 0;
}
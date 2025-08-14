#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>

/*
Notes:
1) Why (h + 1) Works:
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

2) Why a single free bit is always guaranteed in 8-byte alignment:
8-byte alignment guarantees that the last 3 bits are always 000:
| Decimal | Binary      |
| ------- | ----------- |
| 8       | `0000 1000` |
| 16      | `0001 0000` |
| 24      | `0001 1000` |
| 32      | `0010 0000` |
| 40      | `0010 1000` |
| 48      | `0011 0000` |

Bit position:  [63 ............... 3][2][1][0]
               ^ actual size bits   | unused  | free flag

Example:
 size=24 (aligned) → binary:   000...000 11000
 free=1             → binary:  000...000 11001

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

// Masks out the lowest bit to give only the aligned size
#define GET_SIZE(h) (h->meta_data & ~(size_t)1)
#define IS_FREE(h) (h->meta_data & 1)
#define SET_SIZE(h, s) (h->meta_data = ((s) & ~(size_t)1) | (h->meta_data & 1))
#define MARK_ALLOCATED(h) (h->meta_data &= ~(size_t)1)
#define MARK_FREE(h) (h->meta_data |= (size_t)1)

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

// Each allocated block includes a header that stores
// metadata per block (its size and whether it’s free).
// - bits 1..(N-1) = aligned size of the payload (upper bits)
// - bit 0 = free flag (0 = allocated, 1 = free)
// [ ... size bits ... | free bit ]
struct header {
  // size_t size;
  // int free;
  size_t meta_data;
};

// Helper: set error and return NULL
static void *alloc_error(int code, const char *msg) {
  last_error = code;
  if (msg)
    fprintf(stderr, "Allocator error: %s\n", msg);
  return NULL;
}

static void free_error(int code, const char *msg) {
  last_error = code;
  if (msg)
    fprintf(stderr, "Allocator error: %s\n", msg);
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
    if (IS_FREE(h) && GET_SIZE(h) >= alignedSize) {
      MARK_ALLOCATED(h);
      // Return a pointer to the usable memory block,
      // which is right after the header (h + 1).
      return (void *)(h + 1);
    }
    // If no such blocks, move to the next block.
    p += sizeof(struct header) + GET_SIZE(h);
  }

  // Allocate at heap end
  if ((char *)heapEnd + sizeof(struct header) + alignedSize > (char *)heapMax) {
    return alloc_error(ERR_OUT_OF_MEM, "Heap out of memory");
  }

  h = (struct header *)heapEnd;
  SET_SIZE(h, alignedSize);
  MARK_ALLOCATED(h);
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
    free_error(ERR_INVALID_FREE, "invalid free pointer");
    return;
  }

  // Go backwards in memory from the payload pointer to the
  // header of that block and set it to free
  struct header *h = (struct header *)p - 1;
  MARK_FREE(h);
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
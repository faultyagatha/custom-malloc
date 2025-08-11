#include <stdint.h>
#include <stdlib.h>

#define BLOCK_SIZE 64 // in bytes
#define BLOCK_COUNT 1024

/**
 * Plan:
 * - grab a big chunk of memory once (to not ask the OS for
 * memory every time) that persist between allocations and frees
 * - split it into blocks
 * - when allocating, find the first free block, set it to !free
 * and return a chunk of memory
 * - if no free blocks, out of memory
 * - when freeing, find the idx of the block in the memory pool
 * - set it to free.
 */

static uint8_t memory[BLOCK_SIZE * BLOCK_COUNT];
static int free_list[BLOCK_COUNT];

void *myAlloc() {
  for (int i = 0; i < BLOCK_COUNT; i++) {
    if (!free_list[i]) {
      free_list[i] = 1;
      return memory + i + BLOCK_SIZE;
    }
  }
  // Out of memory
  return NULL;
}

void myFree(void *p) {
  // memory  ---> start address of our pool
  // p       ---> somewhere inside the pool
  // (p - memory) gives positive number of bytes between start and p
  int idx = ((uint8_t *)p - memory) / BLOCK_SIZE;
  free_list[idx] = 0;
}
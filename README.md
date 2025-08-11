# Custom Malloc

## Summary

Implementations of malloc and free as:
- implicit free list as malloc (O^n)
- segregated storage (TODO:)
  
## Description

`malloc` (memory allocation) is a standard C library function used to dynamically allocate a block of memory of a specified size during runtime.

- returns a pointer to a contiguous block of memory large enough to hold the requested size
- the allocated memory is typically uninitialised
- the programmer must call free later to release this memory back to the system

The OS provides memory pages in large chunks, malloc manages a heap within that memory chunk. It handles:
- keeping track of allocated and free blocks
- finding a suitable free block for a new allocation
- splitting large blocks if needed
- coalescing adjacent free blocks on free
- minimising fragmentation and overhead

### Common malloc implementation strategies

1. Fixed Block Allocator
- the heap is divided into equal-sized blocks (e.g., 64 bytes each)
- all allocations return one of these blocks — regardless of how much memory was actually requested

To allocate:
- keep a list (or bitmap) of free blocks
- find the first free block and mark it as used.

To free:
- mark the block as free again
  
Characteristics:
- simple to implement
- constant time allocation and free
- no headers before each block
- no fragmentation within a block - the block size is fixed
- internal fragmentation: if one requests 5 bytes and the block size is 64 bytes, 59 bytes is wasted
- cannot efficiently handle variable-sized allocations — either waste space or be forced to split blocks into different pools

2. Implicit Free List
- the heap is a linear linked list of blocks: each block has a header (metadata) containing size and free/used status
- the list is traversed sequentially from the start for every allocation or free.

To allocate:
- walk through the list
- find the first free block that fits
- mark it allocated, or
- if none fits, extend the heap by requesting more memory.

To free:
- mark the block free
- optionally coalesce with adjacent free blocks

Characteristics:
- simple to implement
- requires scanning the entire list, O(n) time per allocation
- no explicit pointers between free blocks (implicit free list)
- susceptible to fragmentation and slow allocation in large heaps.

- includes a header struct with size and free flag as metadata right before the pointer to the data
- memory alignment: ensures that allocated memory is aligned to appropriate byte boundaries (usually 4 or 8 bytes depending on the architecture)

3. Segregated Storage (Segregated Free Lists)
- instead of a single list, maintain multiple free lists
- each storing free blocks of a certain size range (e.g., blocks of size 16-31 bytes in list 1, 32-63 in list 2, etc.).

Allocation:
- calculate the appropriate size class
- pick a free block from that list
- if none available, check larger classes or split bigger blocks.

Free:
- insert the block back into the appropriate size class list
- coalesce adjacent free blocks if needed.

Characteristics:
- much faster allocation than implicit lists: direct access to free blocks by size class
- reduces search time to O(1) or close, since we avoid scanning the whole heap
- better handling of fragmentation since blocks are grouped by size
- more complex data structures (multiple lists, pointer management inside blocks).
  
## TODO

- [ ] pack the free flag of the header into the lowest bit of size
- [X] add memory alignment
- [ ] grow mapping or add more heaps as needed
- [ ] split available blocks
- [ ] coallesce adjucent free blocks
# Custom Malloc

## Summary

Implementations of malloc and free as:
- implicit free list as malloc (O^n)
- segregated storage (TODO:)
  
## Description

- includes a header struct with size and free flag as metadata right before the pointer to the data
- memory alignment: ensures that allocated memory is aligned to appropriate byte boundaries (usually 4 or 8 bytes depending on the architecture)
  
## TODO

- [ ] add memory alignment
- [ ] grow mapping or add more heaps as needed
- [ ] split available blocks
- [ ] coallesce adjucent free blocks
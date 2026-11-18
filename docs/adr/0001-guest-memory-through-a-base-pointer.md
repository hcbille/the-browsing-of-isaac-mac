# Guest memory is reached through a base pointer, not mapped at its own addresses

The Browser Build puts Guest address N at WebAssembly address N, so a Guest pointer is a Host pointer. The Mac Build cannot copy that: on Apple Silicon the first 4 GB of every process is reserved, a plain `mmap` at 0x10000 fails with ENOMEM, and a binary linked with a smaller `-pagezero_size` is killed by the kernel at launch (measured 2026-11-18, macOS 27, arm64). So the Mac Build reserves one 4 GB region anywhere and every Guest memory access, and every Host access to Guest memory, adds that region's base. This is a change to the lifter's output and to Host code that dereferences Guest addresses, and it is why the Mac work lives in a copy of the upstream repository (kept merging from upstream) rather than in a separate repository that only adds files.

## Considered Options

- **Identity mapping at low addresses** (as in the Browser Build): impossible on arm64 macOS, as measured above.
- **Running the Guest under Rosetta as an x86-64 process**: Rosetta also reserves low memory and the goal is a native app.

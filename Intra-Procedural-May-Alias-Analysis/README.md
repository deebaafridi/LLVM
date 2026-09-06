# Project Title
Intra-Procedural May-Alias Analysis for LLVM IR

## Technology Stack
**Compiler Infrastructure:** LLVM

**Language:** C++

**Build System:** CMake

## Project Description
This project is a custom LLVM pass that performs intra-procedural, flow-sensitive May-Alias analysis. The core question it answers is: within a single function, can two pointer variables ever end up referring to the same memory location? Knowing this matters a lot in practice — a compiler that can't rule out aliasing has to assume the worst whenever it sees a pointer, which blocks optimizations like keeping values in registers, reordering instructions, or eliminating redundant loads/stores. Getting a precise-enough "may alias" answer is what makes those optimizations safe to apply in the first place.

The pass is built as an LLVM `FunctionPass` that walks the control-flow graph (CFG) of a function and propagates points-to information forward, instruction by instruction, using a worklist-based fixed-point data-flow algorithm (the same general idea as Kildall's algorithm). Rather than tracking aliasing directly, it tracks, for every pointer variable, the set of variables it might currently point to — and only at the very end works out aliasing by checking which pointers share something in their points-to sets.

The analysis handles the pointer operations that actually show up in compiled C code:
- **Address-of** (`p = &x`) — creates a new points-to entry.
- **Loads and stores through pointers** (`*p = q`, `p = *q`) — including pointer-to-pointer indirection, where a store has to update whatever the pointer itself points to, not the pointer variable directly.
- **Pointer arithmetic / field access** (`getelementptr`) and **casts** (`bitcast`) — propagate or derive points-to information from the base pointer.
- **Merging at branches** — when two paths join after an `if`/`else` or a loop, the points-to sets from every incoming path are unioned together, so a pointer's possible targets never get lost just because of which branch happened to run.

It operates directly on LLVM IR, so it works on whatever the input C was compiled down to, rather than needing to understand C syntax itself. The final output is a per-function report listing every pointer variable and the full set of other pointers it may alias with by the end of that function.

## Key Features
- **Flow-sensitive** — the analysis respects the actual order of instructions, rather than just looking at what's declared, which gives noticeably more precise results than a flow-insensitive pass.
- **Intra-procedural** — scoped to one function at a time; it doesn't try to reason across function calls, so any pointer value coming from outside the function is handled conservatively.
- **May-alias, not must-alias** — it reports "these two pointers might refer to the same memory," which is the conservative, safe answer optimizations need, rather than a stronger guarantee it can't always make.
- **Handles real control flow** — conditional branches and loops are supported by merging (unioning) the incoming points-to sets at every join point in the CFG.
- **Verified against hand-written test cases** — `input.c` has eight functions covering direct aliasing, branch-dependent aliasing, pointer-to-pointer indirection, aliasing through parameters, and aliasing through loops, each with the expected alias sets written out as comments so the pass's output can be checked directly against them.

## Build & Run
This is an out-of-tree LLVM pass using the legacy pass manager, meant to be built inside an LLVM source/build tree — the `BUILDTREE_ONLY` / `PLUGIN_TOOL opt` options in `CMakeLists.txt` are LLVM's own macros for exactly that setup.

1. Drop this directory into your LLVM checkout (e.g. under `llvm/lib/Transforms/`) so it builds alongside LLVM's own passes.
2. Build LLVM/`opt` as usual (CMake + ninja/make). This produces a shared library, e.g. `libaliasCustom.so`, in your build tree's `lib/` directory.
3. Compile the test source to LLVM IR and run the pass on it:

```bash
clang -S -emit-llvm input.c -o input.ll
opt -load /path/to/build/lib/libaliasCustom.so -alias_lib_given -disable-output input.ll
```

On LLVM 14+, the new pass manager is the default, so the legacy one may need to be forced explicitly:

```bash
opt -enable-new-pm=0 -load /path/to/build/lib/libaliasCustom.so -alias_lib_given -disable-output input.ll
```

The pass is analysis-only and doesn't modify the IR, it just writes its findings to `output.txt` next to the source file.

## Output
For every function, the pass appends a block to `output.txt` listing each pointer variable and everything it may alias with at the function's last program point:

```
Function: test2
p -> {y}
y -> {p}

Function: test8
r -> {p1, p2}
p1 -> {r}
p2 -> {r}
```

An empty `{}` means that pointer doesn't alias with any other tracked pointer at that point.

## Project Outcomes
- Built a complete LLVM pass from scratch, from CFG traversal down to per-instruction transfer functions.
- Got hands-on with worklist-based data-flow analysis and fixed-point computation over a CFG with branches and loops.
- Worked directly with the LLVM IR API — instructions, basic blocks, operands — instead of just the theory.
- Practiced translating a program-analysis concept (points-to sets, alias mapping) into working transfer functions for real instructions.
- Verified the pass against eight hand-crafted test functions, correctly identifying aliasing that only shows up depending on which branch of an `if` executes, and aliasing introduced through pointer-to-pointer indirection.

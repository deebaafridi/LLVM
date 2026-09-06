# Project Title
Intra-Procedural Constant Propagation Analysis for LLVM IR

## Technology Stack
**Compiler Infrastructure:** LLVM

**Language:** C++

**Build System:** CMake

## Project Description
This project is a custom LLVM pass that performs intra-procedural constant propagation. The core question it answers is: at any given point inside a function, which variables are guaranteed to hold one fixed, known value, no matter which path the program took to get there? Compilers rely on this kind of answer constantly — once a variable is known to be a constant, later passes can fold arithmetic at compile time, delete branches that can never be taken, and simplify code that would otherwise just be recomputing something already known. Without a sound constant propagation pass, a compiler has to treat almost every variable as unknown, which shuts off a lot of otherwise-safe optimizations.

The pass is built as an LLVM `FunctionPass` that walks the control-flow graph (CFG) of a function and propagates constant information forward, instruction by instruction, using a worklist-based fixed-point data-flow algorithm (the same general idea as Kildall's algorithm). Every variable is tracked using a small three-value lattice:

- TOP — nothing is known about the variable yet.
- CONSTANT(v) — the variable is guaranteed to hold the exact value v at this point.
- BOTTOM — the variable's value is not a compile-time constant, either because it depends on runtime input or because different paths disagree on its value.

The analysis handles the instructions that actually decide constant-ness in compiled C code:

- Alloca and load/store — locals in LLVM IR live in stack slots accessed through load/store rather than pure registers, so constant-ness has to be tracked through memory, not just SSA values.
- Binary operators (add, sub, mul, sdiv, srem, and, or, xor, shifts) — folded into a constant result when both operands are known constants, and marked TOP/BOTTOM otherwise.
- Merging at branches — when two paths join after an if/else, a variable is only kept as a constant if every incoming path agrees on the exact same value; any disagreement forces it to BOTTOM.
- Function calls — values written through a call (such as a scanf into a variable) are conservatively marked BOTTOM, since they come from outside the compiler's knowledge.

It operates directly on LLVM IR, so it works on whatever the input C was compiled down to, rather than needing to understand C syntax itself. The final output is the function's IR annotated, instruction by instruction, with what the analysis knows about every relevant variable at that point.

## Key Features

- Flow-sensitive — the analysis respects the actual order of instructions, rather than just looking at what's declared, which gives noticeably more precise results than a flow-insensitive pass.
- Intra-procedural — scoped to one function at a time; it doesn't try to reason across function calls, so any value coming from outside the function is handled conservatively.
- Sound merging at joins — a variable is only reported as constant if every path reaching that point agrees on the same value, so conditional branches can never produce a false constant.
- Handles real control flow — branches and loops are supported through a proper worklist fixed-point algorithm, re-visiting basic blocks until nothing changes.
- Verified against hand-written test cases — Input/file1.ll through file5.ll cover straight-line folding, values coming from scanf (forcing BOTTOM), and branches that merge either matching or conflicting constants, with the expected output captured for each.

## Build & Run
This is an out-of-tree LLVM pass using the legacy pass manager, meant to be built inside an LLVM source/build tree — the `BUILDTREE_ONLY` / `PLUGIN_TOOL opt` options in `CMakeLists.txt` are LLVM's own macros for exactly that setup.

1. Drop this directory into your LLVM checkout (e.g. under `llvm/lib/Transforms/`) so it builds alongside LLVM's own passes.
2. Build LLVM/`opt` as usual (CMake + ninja/make). This produces a shared library, e.g. `libcpCustom.so`, in your build tree's `lib/` directory.
3. Run the pass against an IR file with `opt`, loading the plugin and invoking it by its registered name (`libCP_given`).

## Output
For each input `.ll` file, the pass writes `output/<file_name>.txt` containing the function's instructions, each annotated with the constant-propagation state of the relevant variables right after that instruction runs:

```
define dso_local i32 @main() #0 {
entry:
    %x = alloca i32, align 4 --> %x=TOP
    store i32 5, i32* %x, align 4 --> %x=5
    %call = call i32 (...) @__isoc99_scanf(...) --> %call=BOTTOM, %y=BOTTOM
    ...
}
```

TOP means no information yet, a number means that variable is known to hold that exact constant at this point, and BOTTOM means it could be anything.

## Project Outcomes

- Built a complete LLVM pass from scratch, from CFG traversal down to per-instruction transfer functions.
- Got hands-on with worklist-based data-flow analysis and fixed-point computation over a CFG with branches and loops.
- Worked directly with the LLVM IR API — instructions, basic blocks, operands — instead of just the theory.
- Practiced translating a program-analysis concept (a constant-propagation lattice, meet operator) into working transfer functions for real instructions.
- Verified the pass against five hand-crafted test files, correctly identifying constants that only hold on one branch, values forced to BOTTOM by external input, and straight-line arithmetic folded all the way down.

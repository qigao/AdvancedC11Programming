# English Edition

This directory contains the English edition of *C with Modern Grammar*.

Chapter filenames such as `ch-13.md` are stable source IDs, not visible chapter numbers. Publication order is defined by [BOOK_MANIFEST.txt](./BOOK_MANIFEST.txt), while the displayed chapter number comes from each file's H1.

## Table of Contents

<!-- book-toc:start -->
**Part I — Native Semantic IR**
- [Chapter 1: From Linux-Style C to CMeta — Macros, static inline, and Zero-Runtime Abstraction](./ch-01.md)
- [Chapter 2: A Finite Metaprogramming Kernel — Generic, Struct, Traits, and Type Relations](./ch-02.md)
- [Chapter 3: Function Semantics and Executable Projections — FunctionDesc, Receiver, Bind, and Invokable](./ch-03.md)

**Part II — Execution IR**
- [Chapter 4: From Callable to Graph — Make Computation Itself Data](./ch-04.md)
- [Chapter 5: From Graph to Stream — Build the Same Typed Graph Through a Higher-Level Surface](./ch-05.md)
- [Chapter 6: Trusted Graph Semantics — From Observable Semantics to Verified Rewrite](./ch-06.md)
- [Chapter 7: From Graph to Plan and Direct — Move Complexity Earlier and Return the Hot Path to Ordinary C](./ch-07.md)

**Part III — Contract IR and the Compiler**
- [Chapter 8: From Repeated API Contracts to a Contract Compiler — DataBind IDL, FunctionDesc, and Multi-Backend Generation](./ch-13.md)

**Part IV — Live Runtime and Engineering Boundaries**
- [Chapter 9: From Stream to Reactive — WAIT, Wake, Demand, and Backpressure](./ch-08.md)
- [Chapter 10: Executor — Separate Execution Policy from Computation Semantics](./ch-09.md)
- [Chapter 11: From Event to State Machine — Turn State Change into a Verifiable Execution Model](./ch-10.md)
- [Chapter 12: From State Machine to Actor — Compose Concurrent Objects with Mailbox, Serialized Execution, and Lifecycle](./ch-11.md)
- [Chapter 13: Engineering Boundaries — ABI, Multi-TU, Semantic Identity, and Installable C Libraries](./ch-12.md)

**Closing — Restraint and Synthesis**
- [Chapter 14: Finite, Explicit, On Demand — When to Stop Adding Meta](./ch-14.md)
- [Chapter 15: From Repeated Knowledge to Three IRs — Contract, Native, and Execution Semantics for Modern C](./ch-15.md)
<!-- book-toc:end -->

Implementation/API/theorem references remain pinned through [SOURCE_SNAPSHOTS.md](../SOURCE_SNAPSHOTS.md).

Back to the [project README](../README.md).

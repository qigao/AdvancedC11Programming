# Advanced C11 Programming

**From Macros to Modern Programming Models**

*Metaprogramming, Reflection, Streaming, and Reactive Programming*

[中文](./README_CN.md) | **English**

**Manuscripts:** [Chinese chapters](./cn/) · [English chapters](./en/)

This book is for C programmers who already know the language.

It does not try to turn C into another language, and it does not treat macros, IDLs, graphs, or Lean as ends in themselves.

Its central question is:

> **When the same stable knowledge is duplicated across C code, metadata, HTTP/RPC bindings, plugins, mocks, and optimizers, how can we lift that knowledge into finite, explicit, checkable IRs, consume as much of it as possible before execution, and still end in ordinary C?**

## Three IRs

This book calls its metaprogramming, RAII, and Reflection facilities **CMeta**, and the `cmeta_plugin_*` API **CMeta Plugin**. Types, functions, and declaration macros use `cmeta_*`; constants and low-level macros use `CMETA_*`. **Salts** names the source repository and SDK distribution, so installation examples retain the actual build identifiers `find_package(Salts)`, `Salts::CMeta`, and `Salts::Plugin`. Components such as CFlow and DataBind keep their own names.

The book now revolves around three distinct semantic layers:

| IR | Question answered |
|---|---|
| DataBind Contract IR | what the program promises |
| CMeta Native Semantic IR | what the C implementation is |
| CFlow Execution IR | how admitted computation is composed, checked, and optimized |

They are not three runtimes.

A typical path is:

~~~text
DataBind Service
        +
CMeta FunctionDesc
        ↓
BindingPlan / exact adapter
        ↓
HTTP / RPC / Plugin / WASM
        ↓
optional CFlow execution
        ↓
ordinary C
~~~

A recurring example in the later chapters is:

~~~text
service UserService {
    GetUser: GetUserRequest -> GetUserResponse;
}
~~~

bound to ordinary C:

~~~c
int get_user(
    UserRepository *repo,
    uint64_t id,
    User *out_user);
~~~

The book shows how that one logical contract can be checked and projected to HTTP, RPC, Plugin, WASM, OpenAPI, and Mock without maintaining six independent API descriptions.

## Four causal stages

### Part I — Native Semantic IR

Start with the division of labor inside ordinary C rather than treating macros as a temporary stage to escape:

~~~text
normal function / static inline
        -> typed behavior

finite macro / PP
        -> tokens / declarations / schema replay

compiler capability
        -> typeof / same-type / cleanup / section

canonical Reflection
        -> stable semantic truth
~~~

Then move through Generic, Struct/Enum, Traits, semantic type identity, FunctionDesc, and Callable. CMeta remains directly consumable from ordinary C; optional source lowering is not a prerequisite.

A key distinction is:

| Object | Role |
|---|---|
| `cmeta_function_desc` | what the function is |
| `cmeta_callable` | one admitted executable representation |

Lean is not required to justify this layer.

### Part II — Execution IR

Start with a normal loop and turn the computation into an inspectable graph:

~~~text
Source<int>
    ↓
Filter(is_even)
    ↓
Map(square)
    ↓
Reduce(sum)
~~~

This is where observable semantics, Lean-backed rewrite laws, normalization, optimization, compiled Plans, and Direct/AOT lowering become central.

Every abstraction must eventually show how the hot path returns to ordinary C.

### Part III — Contract IR and the compiler

Start with one C operation that is redundantly described again as an HTTP route, RPC method, plugin export, OpenAPI schema, mock signature, and WASM boundary.

DataBind makes the logical contract explicit with message / enum / union, service, channel, and component.

Then the compiler joins Contract IR with CMeta native semantics:

~~~text
Contract IR
    +
FunctionDesc
    ↓
BindingPlan
    ↓
projection backends
~~~

HTTP / RPC / PLUGIN / WASM / OPENAPI / MOCK are projections, not separate IDL languages.

### Part IV — Live runtime and engineering qualification

This covers Reactive/async I/O, Executor/Scheduler, State Machine, Actor, plugin loading and quiescent unload, CHttp::Server, ABI/Multi-TU, installed consumers, sanitizers, and measurements.

Each runtime chapter must identify which IR it consumes and which metadata has already disappeared before the hot path.

## Table of Contents

<!-- book-toc:start -->
**Part I — Native Semantic IR**
- [Chapter 1: From Linux-Style C to CMeta — Macros, static inline, and Zero-Runtime Abstraction](./en/ch-01.md)
- [Chapter 2: A Finite Metaprogramming Kernel — Generic, Struct, Traits, and Type Relations](./en/ch-02.md)
- [Chapter 3: Function Semantics and Executable Projections — FunctionDesc, Receiver, Bind, and Invokable](./en/ch-03.md)

**Part II — Execution IR**
- [Chapter 4: From Callable to Graph — Make Computation Itself Data](./en/ch-04.md)
- [Chapter 5: From Graph to Stream — Build the Same Typed Graph Through a Higher-Level Surface](./en/ch-05.md)
- [Chapter 6: Trusted Graph Semantics — From Observable Semantics to Verified Rewrite](./en/ch-06.md)
- [Chapter 7: From Graph to Plan and Direct — Move Complexity Earlier and Return the Hot Path to Ordinary C](./en/ch-07.md)

**Part III — Contract IR and the Compiler**
- [Chapter 8: From Repeated API Contracts to a Contract Compiler — DataBind IDL, FunctionDesc, and Multi-Backend Generation](./en/ch-13.md)

**Part IV — Live Runtime and Engineering Boundaries**
- [Chapter 9: From Stream to Reactive — WAIT, Wake, Demand, and Backpressure](./en/ch-08.md)
- [Chapter 10: Executor — Separate Execution Policy from Computation Semantics](./en/ch-09.md)
- [Chapter 11: From Event to State Machine — Turn State Change into a Verifiable Execution Model](./en/ch-10.md)
- [Chapter 12: From State Machine to Actor — Compose Concurrent Objects with Mailbox, Serialized Execution, and Lifecycle](./en/ch-11.md)
- [Chapter 13: Engineering Boundaries — ABI, Multi-TU, Semantic Identity, and Installable C Libraries](./en/ch-12.md)

**Closing — Restraint and Synthesis**
- [Chapter 14: Finite, Explicit, On Demand — When to Stop Adding Meta](./en/ch-14.md)
- [Chapter 15: From Repeated Knowledge to Three IRs — Contract, Native, and Execution Semantics for Modern C](./en/ch-15.md)
<!-- book-toc:end -->

## Editorial method

The rewrite now follows this default sequence:

~~~text
Plain C baseline
    ↓
concrete duplication or ambiguity
    ↓
smallest semantic object
    ↓
C representation
    ↓
compiler/runtime pseudocode
    ↓
flow
    ↓
logic / invariant
    ↓
Lean only if necessary
    ↓
lowered ordinary C
    ↓
failure case
    ↓
evidence
~~~

If an abstraction can only be described as “cleaner”, “more flexible”, or “more modern”, without code, IR, a judgment, a failure rule, or evidence, it is not yet explained.

## Formal methods

Lean is reserved for real semantic obligations such as rewrite preservation, normalization, state determinism, lifecycle invariants, and refinement.

Many compiler rules should first be stated directly:

~~~text
Γ ⊢ field : T
Γ ⊢ param : U
convertible(T, U)
────────────────────
Γ ⊢ bind(field, param) : valid
~~~

ABI, DSO behavior, sanitizers, and performance use compile/link tests, integration tests, sanitizers, and benchmarks respectively.

## Writing constraints

See:

- [BOOK_ARCHITECTURE.md](./BOOK_ARCHITECTURE.md)
- [CHAPTER_TEMPLATE.md](./CHAPTER_TEMPLATE.md)
- [SOURCE_SNAPSHOTS.md](./SOURCE_SNAPSHOTS.md)

Chapter filenames are stable source IDs; publication order and visible chapter numbering are defined by each edition's `BOOK_MANIFEST.txt` plus the chapter H1.

## Publication build

The Markdown chapters remain the editable source of truth. Publication order is defined by [BOOK_MANIFEST.txt](./BOOK_MANIFEST.txt).

~~~bash
python3 scripts/validate_book.py
python3 scripts/build_book.py
~~~

The release workflow renders HTML, EPUB3, and PDF from the same canonical Markdown manuscript.

## License

Licensed under the [Apache License 2.0](./LICENSE).

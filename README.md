# Advanced C11 Programming

**From Macros to Modern Programming Models**

*Metaprogramming, Reflection, Streaming, and Reactive Programming*

**English** | [中文](./README_CN.md)

This book develops **advanced CMeta techniques, their correctness arguments, and their applications**. It uses C11, finite macro expansion, inline functions, and static metadata to build typed interfaces. Concrete C examples explain reflection, call adaptation, resource lifetime, and code generation before applying them to graphs, Streams, reactive execution, and service contracts.

## Audience and Prerequisites

Readers should understand pointers, structs, function pointers, object lifetime, and basic compilation and linking. No previous knowledge of CMeta, compiler construction, or formal methods is required. Specialized terms such as thunk, continuation, and lowering are explained alongside the code that uses them. C11 is the baseline; compiler extensions and platform ABI requirements are identified where needed.

## What You Will Learn

- Generate declarations with finite macro mapping and tuple projection, and reason about evaluation count, type constraints, and expansion limits.
- Describe types, fields, and functions with CMeta while distinguishing metadata, execution capabilities, and resource ownership.
- Expand a call adapter into ordinary C and understand binding, type erasure, and their storage and lifetime assumptions.
- Compose typed graphs and Streams, using pseudocode, invariants, and proofs to check transformations.
- Apply these techniques in DataBind, CFlow, and CMeta Plugin to handle contract binding, backpressure, cancellation, cleanup, and ABI boundaries.
- Use **CMeta-based ACE patterns** for typed Strategy/Interceptor, Platform-backed Scoped Locking, Monitor Object, real Thread-Specific Storage and application-owned CPU-event Leader/Followers, plus optional CFlow Active Object, while keeping CNet/NativeIO in charge of Reactor/Proactor and SG networking. Two distinct installed C11 examples compare Reactor-style Acceptor-Connector dispatch with the Proactor SG completion/ownership path. A further C11 Service Configurator composes independent server Owner Placement and client Destination Strategy using actual installed CNet APIs. See [Chapter 3](./en/ch-03.md), [Chapter 10](./en/ch-09.md), [Chapter 12](./en/ch-11.md), and the [Chapter 15 decision map](./en/ch-15.md).

## An Example: Start with an Ordinary C Call

This complete C11 program introduces the native function used later in the reflection chapter:

~~~c
#include <stdio.h>

static int increment(int value)
{
    return value + 1;
}

int main(void)
{
    printf("%d\n", increment(4));
    return 0;
}
~~~

Save it as `increment.c`, compile with `cc -std=c11 -Wall -Wextra increment.c -o increment`, and run `./increment` to print `5`. This input cannot overflow; a general call to the function requires `value < INT_MAX`.

Chapter 3 shows how CMeta generates function descriptors and a call adapter, or thunk, from parameter declarations. The adapter reads arguments from storage of known types, calls `increment`, and stores the result. Expanded C code, validation pseudocode, and reasoning about success and failure show why a uniform `void *` entry point still requires correctly typed, aligned, live storage.

## Reading Path

### Part I — CMeta: Macros, Types, and Functions

The first three chapters move from macro and function evaluation rules to finite metaprogramming, static reflection, and call adaptation. Examples show the generated code and derive rules for type matching, parameter projection, and borrowing.

### Part II — CMeta Applications: Graphs, Streams, and Proofs

A sum of even squares becomes a CFlow graph built from CMeta types and callables, then a Stream expression. Pseudocode explains graph construction, normalization, and compilation; proofs establish which rewrites preserve specified observations. A baseline loop provides a comparison for execution form and cost.

### Part III — CMeta Applications: Contract Binding and Code Generation

The `get_user` example joins a DataBind service contract to CMeta function descriptions to generate input conversion, exact invocation, and output conversion. HTTP, RPC, Plugin, and other applications expose their mapping rules, rollback behavior, and resource lifetimes.

### Part IV — CMeta Applications: Reactive Execution and Engineering Boundaries

A Source that temporarily lacks data introduces waiting, waking, demand, and backpressure. Executors, state machines, Actors, and shared libraries extend the example. Transition algorithms and invariants explain concurrency; independent compilation and installed consumers check the engineering boundaries.

## Table of Contents

<!-- book-toc:start -->
**Part I — CMeta: Macros, Types, and Functions**
- [Chapter 1: CMeta Foundations: Macros, Inline Functions, and Compile-Time Constraints](./en/ch-01.md)
- [Chapter 2: Finite Metaprogramming: Generics, Structs, Traits, and Type Relations](./en/ch-02.md)
- [Chapter 3: Function Reflection: Descriptors, Call Adapters, and Parameter Binding](./en/ch-03.md)

**Part II — CMeta Applications: Graphs, Streams, and Proofs**
- [Chapter 4: From Callables to Graphs: Representation and Type Checking](./en/ch-04.md)
- [Chapter 5: Stream Interfaces: Building Typed Graphs](./en/ch-05.md)
- [Chapter 6: Graph Semantics and Rewrite Proofs](./en/ch-06.md)
- [Chapter 7: From Graphs to Execution Plans: Optimization and Direct Execution](./en/ch-07.md)

**Part III — CMeta Applications: Contract Binding and Code Generation**
- [Chapter 8: Contract Binding and Code Generation with CMeta and DataBind](./en/ch-13.md)

**Part IV — CMeta Applications: Reactive Execution and Engineering Boundaries**
- [Chapter 9: Reactive Execution: Waiting, Waking, Demand, and Backpressure](./en/ch-08.md)
- [Chapter 10: Executors and Schedulers: Tasks, Capacity, and Completion Obligations](./en/ch-09.md)
- [Chapter 11: Events and State Machines: Types, Transitions, and Commit](./en/ch-10.md)
- [Chapter 12: Actors: Mailboxes, Serialized Mutation, and Object Lifetime](./en/ch-11.md)
- [Chapter 13: CMeta Engineering Boundaries: Type Identity, ABI, and Shared Libraries](./en/ch-12.md)

**Closing — Trade-offs and Integration**
- [Chapter 14: Choosing Techniques: When Ordinary C Is Sufficient](./en/ch-14.md)
- [Chapter 15: Integrating CMeta: From Declared Facts to Reliable Execution](./en/ch-15.md)
<!-- book-toc:end -->

## Code and Evidence

The text distinguishes complete programs, illustrative fragments, pseudocode, and implementations from pinned source versions. Hand derivations and machine-checked Lean theorems identify their assumptions and scope. Compilation, ABI behavior, concurrent implementations, and performance require their corresponding engineering evidence. See [SOURCE_SNAPSHOTS.md](./SOURCE_SNAPSHOTS.md) for versions and references.

The metaprogramming, RAII, and reflection component is called **CMeta**; `cmeta_plugin_*` belongs to **CMeta Plugin**. **Salts** names the source repository and SDK distribution, so build examples retain identifiers such as `find_package(Salts)` and `Salts::CMeta`. CFlow and DataBind retain their component names.

## Building and Contributing

The [Chinese edition](./cn/README.md) and [English edition](./en/README.md) each follow their `BOOK_MANIFEST.txt`. Chapter filenames are stable source identifiers; displayed chapter numbers and the contents give the reading order.

~~~bash
python3 scripts/validate_book.py --edition cn
python3 scripts/build_book.py --edition cn
python3 scripts/validate_book.py --edition en
python3 scripts/build_book.py --edition en
~~~

The release pipeline generates Markdown, HTML, EPUB, and PDF from the same chapter sources. Editorial guidance is maintained in [Book Architecture](./BOOK_ARCHITECTURE.md) and the [Chapter Template](./CHAPTER_TEMPLATE.md).

## License

This project uses the [Apache License 2.0](./LICENSE).

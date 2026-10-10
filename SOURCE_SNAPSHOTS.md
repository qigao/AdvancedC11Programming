# Source Snapshots

This book is intentionally grounded in fixed implementation snapshots instead of tracking moving `master` branches.

The snapshots below are the reproducible reference points used when the manuscript describes concrete C structs, APIs, tests, Lean theorem names, runtime contracts, and application-layer behavior.

## Linux kernel reference

- Repository: `torvalds/linux`
- Reference snapshot: `602042bf29f6efde39cfb5fdd9289bf4854bc0c5`
- Used only by Chapter 1 to ground the phrase “Linux-style C” in concrete production patterns.
- Reference paths: `include/linux/syscalls.h` (finite `__MAP` pair replay), `include/linux/minmax.h` (single-evaluation/type-checked min/max), `include/linux/container_of.h` (member-type/layout checked container recovery), and `include/linux/cleanup.h` (scope cleanup/guard patterns).
- The manuscript extracts engineering rules and uses its own simplified diagrams/API examples; kernel-specific GNU/C extension syntax is not presented as a portable CMeta contract.

This snapshot is a comparative reference, not an implementation dependency of Salts or the book.

## Salts repository — CMeta, CFlow, and runtime components

- Repository: `qigao/salts`
- Edition implementation snapshot: `a650262031e9b3e5f11a227a7911a75db9ae1fb8`
- Used primarily by Chapters 1–12 and by the CMeta/CFlow engineering boundaries in Chapter 13.
- Covers the Salts 2.1.0 preparation baseline, including the post-#976 finite PP/compiler kernel, canonical `cmeta_type` routing, native-data reflection, CMeta/CFlow implementation, and `formal/cmeta_cflow_calculus` proof package referenced by this edition.
- The same snapshot also anchors Part III's concrete NativeIO/CNet, CSTL, TinyTest/TinyMock, and CFlow I/O examples; those examples are not inferred from moving `master`.

The Salts repository may move beyond this commit. Statements such as “the implementation in this edition” refer to the snapshot above, not necessarily the latest `master`.

Naming in prose follows the component: CMeta for metaprogramming, RAII, and Reflection; CMeta Plugin for the `cmeta_plugin_*` API; CFlow for execution semantics. Salts identifies the repository, release, and CMake package. The pinned build exports `Salts::CMeta`, `Salts::CFlow`, and `Salts::Plugin`; these exact build identifiers do not rename the component APIs.

## SaltsUtils

- Repository: `qigao/salts-utils`
- Edition implementation snapshot: `23c01b1379d3e578a4633acdf7c44166a1eb0714`
- Used by Chapter 8/13 for the concrete DataBind/IDL compiler and runtime binding model.
- Anchors the post-`databindc` architecture in which `salts-idlc` + `salts_idl_target` own the typed frontend/build projections, generated `DataBindServiceNativeBinding` is joined with current CMeta descriptors by `data_bind_binding_plan_compile_service()`, and Plugin execution admission remains lease-bounded through bind/invoke/egress/cleanup.
- Also anchors the installed-consumer qualification showing generated Plugin/client targets no longer depend on the IDL/Schema frontend at runtime.

The SaltsUtils repository may move beyond this commit. Chapter 8/13 API names and lifecycle claims marked as current in this edition refer to this snapshot.

## Post-edition published-SDK compatibility evidence (not edition provenance)

The **fixed edition snapshots above remain unchanged**. An additional independent
installed-consumer gate checks APIs that were added to later SDK releases, without
rewriting those earlier implementation claims or using source-tree dependencies.

- CNet SG policy reference implementation: `qigao/salts`
  [`v2.3.0-rc.4`](https://github.com/qigao/salts/releases/tag/v2.3.0-rc.4),
  source SHA `233a0d1c2086808d85160ad70010e13b4c63d5c8`.
- Reference public headers: `cnet/owner_placement.h`, `cnet/destination_policy.h`,
  `cnet/manager.h`, `cnet/handoff.h`, `cnet/client_pool.h` and
  `cnet/recovery_policy.h`; the ACE-style typed handler uses
  `cmeta/interface.h`.
- The Chapter 12 contrast and `qualification/ch12_cnet_strategy.c` explain
  post-edition API admission/ownership semantics. The gate resolves the
  **newest published Salts.Native SDK**, including prereleases; the source tag is
  provenance for the explanation, **not a consumer version pin**.
- The separate `qualification/ch12_cnet_sg_handoff.c` gate exercises one
  **real two-SG-shard** loopback TCP accept -> detached bounded handoff -> final
  Owner Manager adoption -> two-way transfer -> terminal/recycle -> credit return,
  using only the latest installed public SDK. It does not change the source
  snapshot recorded above or pin the newest package to its example reference.
- The published 2.3 ACE pattern API reference adds `cmeta/interface.h`,
  `cmeta/ace_interceptor.h` and `cmeta/ace_synchronization.h` for typed
  Strategy, Interceptor and borrowed Lockable/Scoped Locking. `NativeIO` owns
  the `salts/native_io_ace_token.h` one-shot completion identity, Platform
  owns real mutexes/threads, and `CFlow` owns application Active Object
  lifecycle. Upstream CMeta synchronization, Leader/Followers, and CFlow
  Half-Sync/Pipes conformance tests are source evidence, not a claim that
  the book has reimplemented every ACE pattern.
- `qualification/ch03_cmeta_ace_patterns.c` and
  `qualification/ch12_cmeta_ace_active_object.c` are independently built
  **latest-published-SDK C11 consumers** exercising those selected public
  APIs, their rejection/settlement outcomes and owner boundaries. This
  expands compatibility evidence only; it does not move or overwrite the
  edition implementation snapshots.

- The pure-policy and live-TCP gates jointly establish selected API admission
  and one real lifecycle path. They do **not** establish all SG execution races,
  cross-owner handoff under load, sanitizer results, device runtime behavior,
  Actor/SG equivalence, or performance.

## CHTTP

- Repository: `qigao/chttp`
- Edition implementation snapshot: `2e950615a7f47190258c4cb1d2cdab32e80a05b8`
- Used by Chapter 8/13 for the concrete HTTP/RPC runtime projection examples.
- Anchors installed `CHttp::Service` and `CHttp::RpcService`, mount-time FunctionDesc/FunctionAbi/native-layout admission, direct/CFlow/Plugin deferred execution modes, hot-path no-Reflection/no-dynamic-ABI qualification, deferred domain-lifetime gating, single-materialization HTTP egress, and generated-plan typed RPC client/server behavior.
- Also anchors the DataBind HTTP OpenAPI provider used to demonstrate that runtime route semantics and documentation are derived from the same HTTP projection facts.

The CHttp repository may move beyond this commit. Chapter 8/13 CHttp API names and lifecycle claims marked as current in this edition refer to this snapshot.

The pinned [CHttp Service header](https://github.com/qigao/chttp/blob/2e950615a7f47190258c4cb1d2cdab32e80a05b8/service/include/chttp_service/service.h) still uses pre-migration Plugin type names. Chapter 8 labels its mount struct as an illustration normalized to the edition's `cmeta_plugin_registry` / `cmeta_plugin_ref` names, not a verbatim excerpt. These independently pinned repositories provide implementation evidence; they do not establish a qualified combined SDK. CHttp's Plugin API migration and a combined installed-consumer build remain necessary before claiming compatibility with this edition's Salts / SaltsUtils snapshots.

## Original manuscript migration

The initial 15-chapter manuscript was extracted from:

- Repository: `qigao/salts`
- Branch: `master`
- Migration source snapshot: `a90053416f1af748f8a356baf2e3f957be6105a4`
- Initial book migration commit: `5133429d2c7cc24f5b9b633b1fd1d1a059b40987`

This migration snapshot is historical provenance and is distinct from the later implementation snapshots used for technical verification.

## Editorial rule

When this book includes an exact source/API excerpt, it should either:

1. match the edition snapshot exactly; or
2. be explicitly labeled as a simplified shape, pseudocode, or conceptual model.

Performance claims require a separately identified benchmark run and are not implied merely by these source snapshots.

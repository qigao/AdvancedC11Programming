# Source Snapshots

This book is intentionally grounded in fixed implementation snapshots instead of tracking moving `master` branches.

The snapshots below are the reproducible reference points used when the manuscript describes concrete C structs, APIs, tests, Lean theorem names, runtime contracts, and application-layer behavior.

## Salts

- Repository: `qigao/salts`
- Edition implementation snapshot: `a650262031e9b3e5f11a227a7911a75db9ae1fb8`
- Used primarily by Chapters 1–12 and by the Salts side of Chapter 13.
- Covers the Salts 2.1.0 preparation baseline, including the post-#976 finite PP/compiler kernel, canonical `cmeta_type` routing, native-data reflection, CMeta/CFlow implementation, and `formal/cmeta_cflow_calculus` proof package referenced by this edition.
- The same snapshot also anchors Part III's concrete NativeIO/CNet, CSTL, TinyTest/TinyMock, and CFlow I/O examples; those examples are not inferred from moving `master`.

The Salts repository may move beyond this commit. Statements such as “the implementation in this edition” refer to the snapshot above, not necessarily the latest `master`.

## SaltsUtils

- Repository: `qigao/salts-utils`
- Edition implementation snapshot: `23c01b1379d3e578a4633acdf7c44166a1eb0714`
- Used by Chapter 8/13 for the concrete DataBind/IDL compiler and runtime binding model.
- Anchors the post-`databindc` architecture in which `salts-idlc` + `salts_idl_target` own the typed frontend/build projections, generated `DataBindServiceNativeBinding` is joined with current CMeta descriptors by `data_bind_binding_plan_compile_service()`, and Plugin execution admission remains lease-bounded through bind/invoke/egress/cleanup.
- Also anchors the installed-consumer qualification showing generated Plugin/client targets no longer depend on the IDL/Schema frontend at runtime.

The SaltsUtils repository may move beyond this commit. Chapter 8/13 API names and lifecycle claims marked as current in this edition refer to this snapshot.

## CHTTP

- Repository: `qigao/chttp`
- Edition implementation snapshot: `5e9f388c2009836d024e0fcd5a3ff8f7c2d49e39`
- Used by the RPC application case in Chapter 13.

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

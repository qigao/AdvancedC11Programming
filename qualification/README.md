# Executable book qualification

These sources are executable counterparts of selected manuscript examples.

They serve a different purpose from `SOURCE_SNAPSHOTS.md`:

- the edition snapshots are the fixed provenance for implementation claims in the book;
- this directory is a compatibility/drift gate against the **latest released** `Salts.Native` SDK package.

CI intentionally does not pin a package version. At the start of each run it resolves the latest GitHub
release asset, uses the matching `sdk/linux-x64` install tree through
`CMAKE_PREFIX_PATH`, then builds and runs these programs as independent
consumers. If the latest release cannot be resolved, downloaded, configured, or consumed, the gate fails; it does not fall back to an older package.

A failure here means that a public SDK change has made one of the book's
executable contracts stale. It does not silently rewrite the edition snapshot.

Current gates:

- `ch01_compiler_skills.c`: expression-level constant requirements, unevaluated native type checks, capability-gated single-evaluation locals, and type-checked `container_of` recovery while preserving ordinary C execution;
- `ch01_cmeta_pp.c`: finite pair mapping, separator-aware mapping, explicit zero-arity behavior, tuple projection, and expansion-safe stringify through the installed CMeta PP surface;
- `ch02_declaration_metadata.c`: one declaration supplies native struct layout plus field metadata, existing native records are reflected without rewriting their declaration, and one tagged traits declaration supplies both capability flags and callback slots;
- `ch02_type_identity_*.c`: independent translation units own distinct descriptor/identity objects for the same stable semantic type; semantic equality succeeds across those TU boundaries, while an otherwise identical layout with a different stable ID remains unequal;
- `ch03_receiver_resolution.c`: receiver metadata remains an ordinary first C parameter while the public resolver accepts the correct receiver/method/argument relation and rejects wrong receiver types, unknown operations, and wrong argument types;
- `ch03_lifecycle_binding.c`: validate raw Data/lifecycle metadata once into an admitted binding, then reuse that capability for init/move/restore; mismatched native layout fails closed and clears the rejected binding;
- `ch03_exact_invoke.c`: exact thunk success, aliasing, and fail-before-call behavior;
- `ch05_stream_graph.c`: Stream façade builds the expected typed Graph IR;
- `ch13_plugin_exact_abi.c`: current Plugin manifest validates only at the exact public ABI epoch and exact manifest layout; a different host ABI/query, ABI epoch, or manifest size is rejected with no negotiation or fallback;
- `ch13_parse_u64.c`: bounded decimal conversion and failure-atomic output.

The Chapter 1 compiler-skills gate checks a different layer: compiler facts may constrain or generate ordinary C, but type queries must not evaluate user expressions and member-pointer recovery must evaluate its pointer expression exactly once. Configure-time negative checks also require false expression-level constraints and wrong member-pointer types to fail translation. Unsupported inferred capabilities remain conditional rather than silently weakened.

The TypeFunction compile matrix runs during CMake configuration. Declared token keys and identical duplicate typedef results must compile; missing keys, alias-token keys, and conflicting results must fail translation. These expected failures are qualification evidence, not runtime tests.

The Chapter 1 PP gate checks the resulting ordinary C declarations and values. It
does not treat preprocessor cleverness as a goal and does not use source-tree or
private implementation headers.

The declaration-metadata gate qualifies the earlier Chapter 2 boundary: struct fields, reflection layout, and trait capability slots are generated from authoritative C facts rather than maintained as an independent handwritten schema. Wrong reflected field types, duplicate trait rows, and wrong trait callback signatures must fail translation.

The Chapter 2 identity gate deliberately separates **address identity** from **semantic
identity**. Matching descriptor or identity pointer values are neither expected
nor required across translation units.

The receiver gate keeps execution semantics ordinary C: the receiver is still a normal pointer parameter; metadata only supplies a checked compile/generation-time method relation.

The lifecycle gate models another modern-C boundary: raw metadata is validated
at admission, while repeated typed operations reuse the admitted capability. It
does not create a universal destroy abstraction or infer ownership from C
spelling.

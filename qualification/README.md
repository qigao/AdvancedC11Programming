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

- `ch01_cmeta_pp.c`: finite pair mapping, separator-aware mapping, explicit zero-arity behavior, tuple projection, and expansion-safe stringify through the installed CMeta PP surface;
- `ch03_exact_invoke.c`: exact thunk success, aliasing, and fail-before-call behavior;
- `ch05_stream_graph.c`: Stream façade builds the expected typed Graph IR;
- `ch13_parse_u64.c`: bounded decimal conversion and failure-atomic output.

The Chapter 1 gate checks the resulting ordinary C declarations and values. It
does not treat preprocessor cleverness as a goal and does not use source-tree or
private implementation headers.

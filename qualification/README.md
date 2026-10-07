# Executable book qualification

These sources are executable counterparts of selected manuscript examples.

They serve a different purpose from `SOURCE_SNAPSHOTS.md`:

- the edition snapshots are the fixed provenance for implementation claims in the book;
- this directory is a compatibility/drift gate against the **latest released** installed SDK packages used by the book: `Salts.Native` for CMeta/CFlow/Plugin and `SaltsUtils.Native` for DataBind.

CI intentionally does not pin package versions. At the start of each run it resolves the latest GitHub
release assets for both repositories, uses their matching `sdk/linux-x64`
install trees through `CMAKE_PREFIX_PATH`, then builds and runs these programs
as independent consumers. If either required latest release cannot be resolved,
downloaded, configured, or consumed, the gate fails; it does not fall back to
an older package or a source-tree dependency.

A failure here means that a public SDK change has made one of the book's
executable contracts stale. It does not silently rewrite the edition snapshot.

Current gates:

- `ch01_compiler_skills.c`: expression-level constant requirements, unevaluated native type checks, capability-gated single-evaluation locals, and type-checked `container_of` recovery while preserving ordinary C execution;
- `ch01_cmeta_pp.c`: finite pair mapping, separator-aware mapping, explicit zero-arity behavior, tuple projection, and expansion-safe stringify through the installed CMeta PP surface;
- `ch02_declaration_metadata.c`: one declaration supplies native struct layout plus field metadata, existing native records are reflected without rewriting their declaration, and one tagged traits declaration supplies both capability flags and callback slots;
- `ch02_type_identity_*.c`: independent translation units own distinct descriptor/identity objects for the same stable semantic type; semantic equality succeeds across those TU boundaries, while an otherwise identical layout with a different stable ID remains unequal;
- `ch03_bind_capture.c`: binding captures a trivial scalar as a fixed inline snapshot, preserves that value after the original capture struct changes, and produces checked/admitted invokable calls with identical results; pointer-as-value capture remains a compile-time error;
- `ch03_scope_cleanup.c`: structured lexical scope initializes managed automatic values in declaration order, preserves body/failure status, and restores the live prefix in exact LIFO order on both normal body exit and partial initialization failure;
- `ch03_receiver_resolution.c`: receiver metadata remains an ordinary first C parameter while the public resolver accepts the correct receiver/method/argument relation and rejects wrong receiver types, unknown operations, and wrong argument types;
- `ch03_lifecycle_binding.c`: validate raw Data/lifecycle metadata once into an admitted binding, then reuse that capability for init/move/restore; mismatched native layout fails closed and clears the rejected binding;
- `ch03_exact_invoke.c`: exact thunk success, aliasing, and fail-before-call behavior;
- `ch04_graph_admission.c`: low-level typed Graph builders reject an incompatible edge without advancing the version token, while successful mutation changes version and clone produces an independent structurally equal snapshot that remains unchanged after source-only mutation;
- `ch05_stream_graph.c`: Stream façade builds the expected typed Graph IR;
- `ch06_normalize_snapshot.c`: a real ZIP surface Graph lowers to primitive RELATION IR in a new snapshot; source/version remain unchanged, and normalizing the normalized Graph again yields an independently versioned but structurally equal Graph;
- `ch06_optimizer_trace.c`: an idempotent-map rewrite emits one stable proof-trace rule bound to the exact source/output Graph versions; optimizing the result again is structurally idempotent, and later source mutation invalidates the old trace binding;
- `ch07_plan_certificate.c`: a normalized Graph compiles into a reusable Plan and sequential certificate; mutating the source Graph invalidates the old certificate while the already-compiled Plan continues executing its original pre-decoded program;
- `ch07_direct_no_fallback.c`: an eligible generated Filter/Map Direct pipeline executes through StaticTarget stages, while an effectful stateful Direct schema returns `INELIGIBLE`; explicitly selecting Plan for the same callable succeeds without any hidden Direct fallback;
- `ch06_normalize_idempotence.c`: a real ZIP surface Graph lowers to an independent primitive snapshot, source structure/version remain unchanged, and normalizing the normalized Graph again yields a structurally equal snapshot with its own version token;
- `ch06_authorized_rewrite.c`: an IDEMPOTENT endomap pair admits exactly one idempotent-map elimination with a bound proof-trace event, while behaviorally similar code without the property contract retains both callable applications and emits no semantic rewrite event;
- `ch08_databind_binding_plan.c`: installed `salts-idlc` generates one native Service contract; runtime compiles an immutable BindingPlan, survives codec release, applies an absent-field default during `bind_call`, invokes the generated ordinary C service, and consumes exactly one call-lifetime teardown obligation;
- `ch08_databind_public_sdk.c`: independent SaltsUtils installed consumer links only the canonical `Salts::DataBind` target and verifies the public DataBind header/runtime version contract;
- `ch09_reactive_demand.c`: downstream demand limits emitted values exactly, remaining publisher values survive between requests, terminal completion occurs once, and post-terminal request returns CLOSED without producing more callbacks;
- `ch09_wait_wake.c`: readiness Publisher WAIT preserves outstanding demand, wake permits retry without creating demand, later request resumes remaining values, and stale wake after terminal produces no callbacks;
- `ch10_executor_settlement.c`: capacity-one manual Executor accepts one descriptor, rejects the next with FULL without invoking callbacks, settles accepted work exactly once through run+finalize, and rejects post-shutdown admission with CLOSED while preserving the settlement ledger;
- `ch11_machine_staged_commit.c`: a failed Machine action returns ERROR while preserving the original state id/value, while a successful staged action commits once to a terminal state and later event admission is rejected without re-running the action;
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

The lexical-scope gate treats scope cleanup as structured C control flow over canonical lifecycle callbacks. It does not introduce a heap registry, infer ownership, or claim that cross-scope jumps can be made safe automatically.

The bind/capture gate treats a closure as a finite generated call projection, not a hidden ownership system. Value capture copies admitted scalar bytes into the callable; borrowed pointers, provider lifetime and Plugin leases remain the caller's explicit responsibility.

The Graph-admission gate treats Graph as control-plane IR: builder calls are the mutation authority, version binds downstream knowledge to one process-local snapshot, and runtime values/scheduler state remain outside the Graph.

The normalization gate treats lowering as artifact transformation rather than runtime dataflow. It proves source immutability and structural idempotence while keeping process-local Graph versions distinct; optimizer theorem authority remains a separate concern.

The optimizer-trace gate checks the C bridge, not the theorem itself: metadata admits a candidate, the optimizer records a stable rule id and exact artifact coordinates, and version changes invalidate stale trace bindings. Semantic theorem authority remains outside this runtime test.

The Plan/Certificate gate separates execution artifact from witness: Plan evaluation does not query later Graph topology, while Certificate checking fails closed once the source Graph version/fingerprint no longer matches. Certificate rows are execution-only, not a persistent wire identity.

The Direct gate makes backend policy observable: Direct eligibility is a strict permission to remove generic runtime layers, capacity/ineligibility failures return explicit statuses, and selecting Plan for an ineligible callable is a separate caller action rather than an internal fallback.

The normalization gate treats lowering as a static artifact transformation. Structural equality compares executable IR while version tokens continue to identify distinct process-local snapshots; no theorem prover or scheduler participates in ordinary normalization.

The authorized-rewrite gate keeps metadata claims, optimizer actions, and rewrite witnesses distinct: the property marks an admissible candidate, the optimizer applies one named rule, and the trace binds that concrete event to exact Graph snapshots.

The reactive-demand gate treats demand as a downstream-value ledger owned by the live Subscription. Graph remains reusable program structure, while Publisher and Scheduler retain their separate runtime responsibilities.

The WAIT/wake gate keeps readiness and demand separate: WOULD_BLOCK/WAIT leaves downstream demand intact, a waker only schedules a retry opportunity, and terminal state remains absorbing even if an older waker is invoked.

The Executor-settlement gate treats FULL and CLOSED as ownership-preserving protocol results. Accepted work must remain conserved until completion/cancellation; rejected descriptors stay entirely with the caller and execute no callbacks.

The Machine staged-commit gate keeps admission, action evaluation and commit separate: callbacks may fail before commit without publishing partial Machine-owned state, while a successful transition commits exactly once and terminal state closes further admission.

The DataBind SDK smoke remains the package/public-target admission gate. The Service BindingPlan gate separately exercises installed IDL generation, generated native binding, runtime plan compilation, defaulted ingress binding, ordinary C invocation, and single-owner frame teardown without source-tree includes or compatibility targets.

# Book Architecture — 从普通 C 到三种 IR，再 Lower 回普通 C

本文件定义下一阶段的全书结构与写作约束。

这本书不是 CMeta API reference，不是 Lean 教材，不是 RPC/Plugin/WASM 框架说明，也不是“如何用宏模拟 C++”。

它回答一个更具体的问题：

> **当一个 C 系统开始重复维护类型事实、函数契约、外部 API 契约和执行关系时，怎样把这些稳定知识提升成有限、显式、可检查的中间表示，并在构建阶段尽量消费掉，最终让运行时重新回到简单 C。**

全书最终围绕三种 IR 展开：

| IR | 回答的问题 |
|---|---|
| DataBind Contract IR | what the program promises |
| CMeta Native Semantic IR | what the C implementation is |
| CFlow Execution IR | how admitted computation is composed |

三者不是三套 runtime。

它们分别解决三种不同的重复知识。

---

# 1. 全书唯一主线

全书默认沿着下面的因果链推进：

~~~text
ordinary C
    ↓
find repeated stable knowledge
    ↓
make the knowledge explicit
    ↓
DataBind / CMeta / CFlow IR
    ↓
validate / prove / compile
    ↓
generate or lower
    ↓
ordinary C execution
    ↓
tests / ABI / sanitizer / benchmark
~~~

更具体地：

~~~text
logical contract              native implementation
      │                              │
      ▼                              ▼
 DataBind IR                    CMeta IR
      │                              │
      └──────── binding/compiler ────┘
                     │
                     ▼
             immutable plans/adapters
                     │
          ┌──────────┼──────────┐
          ▼          ▼          ▼
        HTTP        Plugin      WASM
          │
          └──── optional ───────┐
                                ▼
                              CFlow
                         Execution IR
                                │
                       verify / optimize
                                │
                              lower
                                ▼
                           ordinary C
~~~

这张图是全书新的总图。

---

# 2. 写作纪律：减少抽象谈话，增加可检查对象

每一个主要 claim 至少要落到下面五种 artifact 之一：

1. **C code**：真实 baseline、public API、generated shape 或 lowered hot path；
2. **pseudocode**：算法、状态迁移、compiler pass；
3. **flow diagram**：owner、数据流、control plane 与 hot path；
4. **logic / proof obligation**：类型判断、状态不变量、rewrite 前提；
5. **engineering evidence**：compile-fail、Multi-TU、installed consumer、sanitizer、benchmark。

默认不要连续写超过两段纯概念 prose 而没有新的 artifact。

禁止用“架构更优雅”“能力更强”“更现代”“更加灵活”“可以方便扩展”代替技术内容。

这类句子后面必须紧接至少一个可检查对象：before code、after code、exact invariant、generated representation 或 measured evidence。

---

# Part I — Native Semantic IR

Part I 不需要 Lean 才成立，也不要求一套新的 C source language。

它回答：

> **普通 C、static inline、有限 macro、compiler capability 与 Reflection 应怎样分工，才能把重复而稳定的 native knowledge 提前到构建期，同时让 hot path 继续执行普通 C？**

Part I 的新入口是 Linux-style Modern C：

~~~text
ordinary C
    |
    +-- normal function / static inline
    |       typed behavior
    |
    +-- finite macro / PP
    |       tokens / declarations / schema replay
    |
    +-- compiler capability
    |       typeof / same-type / cleanup / section
    |
    '-- canonical Reflection
            stable semantic truth
                |
        +-------+-------+
        |               |
   direct typed C   dynamic boundary
                        |
                 validate/admit once
~~~

这里必须反复强调：

> **Reflection does not imply dynamic dispatch.**

CMeta Native Semantic IR 可以直接通过普通 C declaration、macro-generated metadata 与 static-inline accessor 暴露。可选 source lowering 只能是 syntax frontend，不能成为普通 C consumer 使用 CMeta 的前置条件。

## Chapter 1 — 从 Linux-style C 到 CMeta

先从普通函数开始，而不是先从 DSL 开始。

第一条判断：

~~~text
Can a normal C function express it?
        yes -> normal function / static inline

Need tokens, declarations, identifiers or schema replay?
        yes -> finite macro

Need native compiler knowledge?
        yes -> capability-gated compiler abstraction

Need stable meaning shared by several consumers?
        yes -> canonical Reflection
~~~

必须展示：

~~~c
static inline int int_max(int a, int b)
{
    return a > b ? a : b;
}
~~~

并与可能重复求值的 expression macro 对比。

然后展示 production PP/compiler primitives，例如：

~~~c
CMETA_PP_PAIR_MAP_N(...)
CMETA_PP_MAP_COMMA_N(...)
CMETA_PP_UNIQUE(...)

CMETA_NATIVE_TYPEOF(expr)
CMETA_SAME_TYPE(a, b)
CMETA_AUTO(name, expr)
CMETA_CONST_REQUIRE(condition)
~~~

以及 type/layout 例子：

~~~c
cmeta_container_of(ptr, Owner, member)
~~~

本章结论不再是“宏不够，所以离开宏”，而是：

~~~text
facts once
   -> macros generate structure
   -> static inline preserves typed C semantics
   -> Reflection preserves meaning
   -> compiler removes abstraction
   -> ordinary C executes
~~~

## Chapter 2 — Generic / Struct / Traits：让类型事实只写一次

核心 before/after：

~~~c
DECLARE_LIST(IntList, int);
DECLARE_VEC(IntVec, int);
DECLARE_OPTION(MaybeInt, int);
~~~

变成：

~~~c
typed(List, IntList, int);
typed(Vec, IntVec, int);
typed(Option, MaybeInt, int);
~~~

必须落到 type identity、field facts、traits、finite relations、generated C type 和 ordinary C algorithm。

本章不把 Lean 当卖点。

## Chapter 3 — FunctionDesc 与 Callable：描述函数，不等于执行函数

这是 Part I 需要重点升级的一章。

首先给普通 C：

~~~c
int get_user(
    UserRepository *repo,
    uint64_t id,
    User *out_user);
~~~

然后明确拆成两个概念：

| 对象 | 角色 |
|---|---|
| `cmeta_function_desc` | descriptive native semantic truth |
| `cmeta_callable` | one admitted executable representation |

FunctionDesc 至少回答 name、return type、parameter types、IN/OUT/INOUT、ownership/nullability，以及 effects/properties。

Callable 回答 how this supported shape is invoked/composed、capture、dispatch、finite signature 和 execution representation。

因此：

~~~text
FunctionDesc
    ├── DataBind binding
    ├── TinyMock
    ├── Plugin publication
    ├── HTTP/RPC tooling
    └── diagnostics

FunctionDesc
    ↓ admission / exact adapter
Callable
    ↓
CFlow
~~~

这里不要用 Lean 去证明“signature list 没重复”。

这类问题优先用 generator validation / compile-time checks。

Lean 留给真正的 semantic law。

---

# Part II — Execution IR

Part II 顺序保持连续：

| Chapter | 主线 |
|---|---|
| 4 | Callable → Typed Graph |
| 5 | Graph → LINQ-like Stream surface |
| 6 | Observable Semantics → Lean → Verified Rewrite |
| 7 | Normalize / Optimize → Plan / Direct / AOT |

这仍然是全书的执行语义核心。

## Canonical example

持续使用：

~~~c
long sum_even_squares(const int *xs, size_t n)
{
    long total = 0;

    for (size_t i = 0; i < n; ++i) {
        int x = xs[i];

        if ((x & 1) != 0)
            continue;

        total += (long)x * x;
    }

    return total;
}
~~~

对应：

~~~text
Source<int>
    ↓
Filter(is_even)
    ↓
Map(square)
    ↓
Reduce(sum)
~~~

每一章都必须展示 surface C、internal IR、semantic rule 和 lowered C。

## Graph 的证明义务

例如 Map fusion 不能只写：

~~~text
Map(f); Map(g) -> Map(g ∘ f)
~~~

必须说明 observable semantics：

~~~text
eval(Map(g, Map(f, xs)))
=
eval(Map(g ∘ f, xs))
~~~

若 rewrite 依赖 property，需要明确：

| 对象 | 作用 |
|---|---|
| property bit | admission claim |
| semantic theorem | rewrite authority |

## Lowering

Part II 的终点仍是：

~~~text
Surface Graph
    ↓
Normalize
    ↓
Optimize
    ↓
Compiled Plan
    ↓
Direct / AOT
    ↓
ordinary C hot path
~~~

例如：

~~~c
for (size_t i = 0; i < n; ++i) {
    int x = xs[i];

    if ((x & 1) != 0)
        continue;

    total += (long)x * x;
}
~~~

如果最终不能解释高级 IR 怎样重新变成这种代码，就还没有完成 lowering 叙事。

---

# Part III — Contract IR 与 Compiler

IDL 不是一个附加 serialization feature。

它回答另一类重复知识：C prototype、HTTP route、RPC method、Plugin export、OpenAPI schema、Mock signature 和 WASM boundary。

为什么同一个 application contract 要被描述很多次？

Plain C baseline：

~~~c
int get_user(
    UserRepository *repo,
    uint64_t id,
    User *out_user);
~~~

HTTP 再写一次：

~~~c
chttp_server_get(server, "/users/:id", get_user_http, ctx);
~~~

RPC 再写一次：

~~~c
crpc_server_register(server, "UserService.GetUser", get_user_rpc, ctx);
~~~

OpenAPI 再写一次参数和响应。

Plugin 再写一次 export metadata。

问题不是函数不能工作。

问题是：

> **同一份逻辑 contract 已经被多个 backend 独立维护。**

## 5.1 Canonical DataBind vocabulary

IDL 只保留语义 primitive：

~~~text
Data
    message
    enum
    union

Interaction
    service
    channel

Composition
    component
~~~

例如：

~~~text
message GetUserRequest {
    uint64 id;
}

message GetUserResponse {
    User user;
}

service UserService {
    GetUser: GetUserRequest -> GetUserResponse;
}

component UserModule {
    service UserService;
}
~~~

注意：HTTP、RPC、PLUGIN、WASM、OPENAPI、MOCK 都不是 IDL semantic keyword。

它们是 compiler projection。

## 5.2 Contract IR 与 Native IR 的 join

同一个 logical operation：

~~~text
UserService.GetUser:
    GetUserRequest -> GetUserResponse
~~~

可以绑定普通 C：

~~~c
int get_user(
    UserRepository *repo,
    uint64_t id,
    User *out_user);
~~~

CMeta 描述 native function。

DataBind 描述 logical contract。

Compiler 做 join。

可以用一个简化 judgment 表达：

~~~text
Γ ⊢ op : Req -> Res
Γ ⊢ f  : (Repo*, uint64, User*) -> int
Γ ⊢ bind(op, f) : valid
~~~

其中 valid 至少要求：

- request fields can construct all required IN params；
- OUT/result can construct the declared response；
- ownership/lifetime is admitted；
- conversion is explicit；
- failure mapping is explicit。

错误在 build/control plane 失败，而不是 request hot path 猜。

## 5.3 BindingPlan

编译后得到 immutable plan：

~~~text
Service Operation
        +
FunctionDesc
        ↓
validate / resolve
        ↓
BindingPlan
        +
exact adapter
~~~

热路径不重新解释 IDL：

~~~text
request
    ↓
precompiled ingress
    ↓
exact adapter
    ↓
precompiled egress
    ↓
response
~~~

---

# 6. Projection：同一个 Contract IR 生成不同 artifact

统一 compiler entry：

~~~cmake
databind_target(
    TARGET users
    IDL users.dbidl
    SOURCES users.c

    PROJECTIONS
        NATIVE
        HTTP
        RPC
        PLUGIN
        WASM
        OPENAPI
        MOCK
)
~~~

不要设计：

~~~cmake
databind_http(...)
databind_rpc(...)
databind_plugin(...)
databind_wasm(...)
~~~

## HTTP

~~~text
Service IR
    +
HTTP projection config/convention
    ↓
HTTP MethodPlan
    ↓
CHttp::Service
    ↓
CHttp::Server
~~~

CHttp::Server 不解析 IDL，也不做 FunctionDesc lookup。

## RPC

~~~text
Service IR
    +
RPC projection
    ↓
RPC MethodPlan
    ↓
CRPC runtime
~~~

## Plugin

业务 Service：

~~~text
Service
    ↓
FunctionDesc + exact adapter
    ↓
generated Plugin publication
~~~

长生命周期 provider：

~~~text
CMeta Interface
    ↓
{ self, vtable }
    ↓
Plugin Interface publication
~~~

两者共享 loader / registry / lease / quiescent unload。

不要 everything = vtable，也不要 everything = FunctionDesc。

## WASM

~~~text
component
    + WASM projection
    ↓
host/guest ABI lowering
    ↓
.wasm
~~~

native pointer / vtable address 不能成为 portable contract value。

需要 adapter，或者 generation failure。

## OpenAPI

OpenAPI 的 source 是：

~~~text
Service Contract
    +
HTTP Projection
~~~

不是 Service alone，也不是另一套手写 schema。

---

# Part IV — Live Runtime 与工程边界

Reactive、Executor、Machine、Actor、ABI、Plugin runtime、CHttp runtime 等内容仍然重要。

但它们在新的全书框架中要明确自己消费哪一种 IR。

## Reactive / async I/O

主要消费 CFlow Execution IR。

证明重点是 WAIT、wake、demand、cancel 和 terminal。

## Executor / Scheduler

只回答：

| 对象 | 问题 |
|---|---|
| Executor | how |
| Scheduler | when |
| Graph / Machine / Plan | what |

## Machine / Actor

继续使用 state transition judgment：

~~~text
(state, event)
    -> action
    -> state'
~~~

Lean 证明 determinism、terminal absorption 和 admission/lifecycle invariant。

## ABI / Multi-TU

这部分现在不只是 CMeta qualification。

还必须连接 DataBind contract identity、CMeta semantic identity、generated ABI、Plugin cross-DSO identity 和 installed consumer。

始终坚持 **semantic identity != descriptor pointer**。

---

# 8. Formal methods 的位置

Lean 只在真正存在 semantic claim 时进入。

## 适合 Lean

适合 Lean 的内容包括 graph rewrite preservation、normalization、state transition invariant、protocol refinement、certificate relation，以及 backend 声称 preservation 时的 projection semantics。

## 不适合 Lean 代替的东西

generated file exists、C ABI links、dlopen works、malloc succeeds、sanitizer clean、benchmark faster、network eventually delivers 等 claim 都应使用对应工程证据。

这些分别使用 generator test、compile/link test、DSO integration、fault injection、sanitizer、benchmark、runtime integration。

## Contract compiler proof obligations

DataBind 章节可以先使用轻量 logic，不必把所有内容都 Lean 化。

字段绑定：

~~~text
Γ ⊢ field : T
Γ ⊢ param : U
convertible(T, U)
────────────────────
Γ ⊢ bind(field, param) : valid
~~~

transactional commit：

~~~text
bind(request) = error
──────────────────────
observable(native_output) = unchanged
~~~

projection consistency：

~~~text
HTTPPlan(op) and OpenAPI(op)
must be generated from the same HTTP projection IR
~~~

先把 claim 说清楚，再决定是否需要 machine-check。

---

# 9. Evidence matrix

| Claim | Required evidence |
|---|---|
| generated API matches IDL | generator golden/compile test |
| FunctionDesc matches implementation | compile/admission test |
| binding is failure-atomic | unit/integration test + invariant |
| Graph rewrite preserves result | semantic theorem + differential test |
| descriptor identity works across TU | Multi-TU test |
| Plugin survives DSO boundary | installed DSO consumer |
| unload is safe | lease/quiescence tests + sanitizer/stress |
| HTTP docs match runtime | same projection IR + contract test |
| WASM ABI is portable | host/guest integration test |
| optimization is faster | benchmark with compiler/flags/platform |

不要让一个 evidence 替另一个 evidence。

---

# 10. Canonical examples

全书减少独立 demo，尽量复用三个持续演化的 case。

## Case A — typed data

~~~c
typedef struct User {
    uint64_t id;
    const char *name;
} User;
~~~

用于 Generic / Struct / Traits / DataBind / ABI。

## Case B — typed computation

~~~c
long sum_even_squares(const int *xs, size_t n);
~~~

用于 Callable / Graph / Stream / Lean / Optimize / Direct。

## Case C — user service

~~~text
service UserService {
    GetUser: GetUserRequest -> GetUserResponse;
}
~~~

native implementation：

~~~c
int get_user(
    UserRepository *repo,
    uint64_t id,
    User *out_user);
~~~

用于 FunctionDesc / DataBind binding / HTTP / RPC / Plugin / WASM / OpenAPI / Mock。

这样读者可以看到同一份 contract 怎样穿过整个系统。

---

# 11. 稳定 Source ID 与出版顺序

章节文件名现在明确是稳定 source ID，不再等同于可见章号。

出版顺序由 `cn/BOOK_MANIFEST.txt` 与 `en/BOOK_MANIFEST.txt` 统一定义。

当前正式顺序：

~~~text
Part I — Native Semantic IR
    Chapter 1  <- ch-01.md
    Chapter 2  <- ch-02.md
    Chapter 3  <- ch-03.md

Part II — Execution IR
    Chapter 4  <- ch-04.md
    Chapter 5  <- ch-05.md
    Chapter 6  <- ch-06.md
    Chapter 7  <- ch-07.md

Part III — Contract IR 与 Compiler
    Chapter 8  <- ch-13.md

Part IV — Live Runtime 与工程边界
    Chapter 9  <- ch-08.md
    Chapter 10 <- ch-09.md
    Chapter 11 <- ch-10.md
    Chapter 12 <- ch-11.md
    Chapter 13 <- ch-12.md

Closing
    Chapter 14 <- ch-14.md
    Chapter 15 <- ch-15.md
~~~

这样可以在不反复 rename source file 的情况下调整出版结构。

QA 必须同时检查 manifest order、chapter H1 visible number、generated README TOC 和 canonical manuscript TOC。

任何一个漂移都让 Publication gate 失败。

---

# 12. Chapter 8 的任务：Contract Compiler

Chapter 8 不再是“高级应用大清单”。

它必须成为一章真正的 compiler chapter：

~~~text
repeated API descriptions
    ↓
DataBind IDL
    ↓
Canonical Contract IR
    +
CMeta FunctionDesc
    ↓
BindingPlan
    ↓
HTTP / RPC / Plugin / WASM / OpenAPI / Mock
~~~

必须包含：

- 一个 plain C service；
- 一个 IDL；
- 一个 native implementation；
- 一个 binding judgment；
- 一个 generated BindingPlan；
- 至少 HTTP + Plugin + WASM 三种 projection；
- 一个 hot-path diagram；
- 一个明确的 failure case；
- 一个 qualification matrix。

---

# 13. Chapter 15 的新总结公式

最终不能只总结 CMeta + CFlow + Lean。

需要升级为：

~~~text
DataBind
    = Contract IR

CMeta
    = Native Semantic IR

CFlow
    = Execution IR

Lean
    = proof tool for selected semantic laws

ordinary C
    = implementation and final execution language
~~~

全书最终公式：

~~~text
Ordinary C
+
Explicit Contract IR
+
Explicit Native Semantics
+
Explicit Execution IR
+
Verified High-Risk Transformations
+
Aggressive Lowering
=
Modern C
~~~

---

# 14. Completion definition

重构完成后，读者应该能够用代码回答这些问题：

1. 同一个 type fact 为什么会在宏、metadata、container 中重复？
2. cmeta_function_desc 与 cmeta_callable 为什么不是同一个概念？
3. Graph 比 for-loop 多保存了什么信息？
4. 哪一个 theorem 真正授权了哪一个 rewrite？
5. Plan/Direct 怎样让 Graph 退出 hot path？
6. 为什么 Service contract 不能等于 HTTP route？
7. DataBind Contract IR 与 CMeta Native IR 在哪里 join？
8. 为什么 Plugin export 应该生成，而不是用户维护？
9. 为什么 Service/function 与 Interface/vtable 都可以进入同一个 Plugin runtime？
10. 同一个 component 怎样生成 Plugin 与 WASM？
11. 为什么 CHttp::Server 不应该理解 IDL？
12. OpenAPI 为什么应该来自 Service + HTTP projection？
13. 哪些错误必须在 build/control plane fail-fast？
14. 哪些 claim 需要 Lean，哪些只需要 compile/link/sanitizer/benchmark？
15. 最后 hot path 是否仍然能够被解释成普通 C？

如果这些问题只能用架构口号回答，而不能用代码、IR、判断规则或测试回答，书还没有完成。

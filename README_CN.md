# 用现代语法写 C

**更多知识在构建期，更少工作在运行时**

**中文** | [English](./README.md)

**正文：** [中文版章节](./cn/) · [English chapters](./en/)

这本书面向已经有一定工程经验的 C 程序员。

它不试图把 C 变成另一门语言，也不把宏、IDL、Graph 或 Lean 当成目的。

全书真正关心的是：

> **当同一个稳定知识开始被多个 C 模块、metadata、HTTP/RPC、Plugin、Mock 或 optimizer 重复维护时，怎样把它提升成有限、显式、可检查的 IR，并在构建阶段尽量消费掉，最终仍然执行普通 C。**

## 三种 IR

本书使用 **CMeta** 称呼元编程、RAII 与 Reflection 能力，并将 `cmeta_plugin_*` 接口称为 **CMeta Plugin**。类型、函数与声明宏使用 `cmeta_*`，常量与底层宏使用 `CMETA_*`。**Salts** 是源码仓库与 SDK 发行包的名称，安装示例中的 `find_package(Salts)`、`Salts::CMeta` 和 `Salts::Plugin` 沿用真实构建标识。CFlow、DataBind 等组件各用自己的名称。

全书现在围绕三个不同问题展开：

| IR | 回答的问题 |
|---|---|
| DataBind Contract IR | 程序对外承诺什么 |
| CMeta Native Semantic IR | C 实现实际上是什么 |
| CFlow Execution IR | 被接受的计算怎样组合、证明和优化 |

它们不是三套 runtime。

典型路径是：

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

一个核心例子会贯穿后半本：

~~~text
service UserService {
    GetUser: GetUserRequest -> GetUserResponse;
}
~~~

对应普通 C 实现：

~~~c
int get_user(
    UserRepository *repo,
    uint64_t id,
    User *out_user);
~~~

书中会具体展示它怎样被检查、绑定并生成 HTTP、RPC、Plugin、WASM、OpenAPI 和 Mock，而不是分别维护六份 API 描述。

## 四段因果链

### Part I — Native Semantic IR

从 ordinary C 的工具分工开始，而不是把 macro 当成必须逃离的过渡阶段：

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

然后进入 Generic、Struct/Enum、Traits、semantic type identity、FunctionDesc 和 Callable。CMeta 可以被普通 C 直接消费；可选 source lowering 不是前置条件。

重点是区分：

| 对象 | 角色 |
|---|---|
| `cmeta_function_desc` | 描述函数是什么 |
| `cmeta_callable` | 一种 admitted executable representation |

Part I 不靠 Lean 才成立。

### Part II — Execution IR

从普通循环：

~~~c
for (...) {
    if (!is_even(x))
        continue;
    total += square(x);
}
~~~

进入：

~~~text
Source<int>
    ↓
Filter(is_even)
    ↓
Map(square)
    ↓
Reduce(sum)
~~~

重点不是链式语法，而是让整个 computation 成为可检查的 program object。

这里才真正需要 observable semantics、Lean law、verified rewrite、normalize/optimize、Plan 和 Direct/AOT。

最终必须再次展示 lowering 后的普通 C hot path。

### Part III — Contract IR 与 Compiler

这一部分从“同一个 API 为什么被描述很多次”开始。

Plain C：

~~~c
int get_user(UserRepository *, uint64_t, User *);
~~~

然后工程里又出现 HTTP route、RPC method、Plugin export、OpenAPI schema、Mock signature 和 WASM ABI。

DataBind IDL 把 logical contract 收成一次：message / enum / union、service、channel、component。

再与 CMeta native semantics 编译连接：

~~~text
Contract IR
    +
FunctionDesc
    ↓
BindingPlan
    ↓
projection backends
~~~

HTTP / RPC / PLUGIN / WASM / OPENAPI / MOCK 都是 projection，而不是新的 IDL 语言。

### Part IV — Live Runtime 与工程边界

这里讨论真正活着的 runtime：Reactive/async I/O、Executor/Scheduler、State Machine、Actor、Plugin loader/lease/quiescent unload、CHttp::Server，以及 ABI/Multi-TU/installed consumer。

每个 runtime 都必须说明它消费的是哪一种 IR，以及哪些 metadata 已经在进入 hot path 前被消掉。

## 目录

<!-- book-toc:start -->
**Part I — Native Semantic IR**
- [第一章：从 Linux-style C 到 CMeta——Macro、static inline 与零运行时抽象](./cn/ch-01.md)
- [第二章：有限元编程内核——Generic、Struct、Traits 与类型关系](./cn/ch-02.md)
- [第三章：函数语义与执行投影——FunctionDesc、Receiver、Bind 与 Invokable](./cn/ch-03.md)

**Part II — Execution IR**
- [第四章：从 Callable 到 Graph——把计算本身变成数据](./cn/ch-04.md)
- [第五章：从 Graph 到 Stream——用高级接口构造同一张 Typed Graph](./cn/ch-05.md)
- [第六章：Graph 的可信语义——从 Observable Semantics 到 Verified Rewrite](./cn/ch-06.md)
- [第七章：从 Graph 到 Plan 与 Direct——把复杂性提前，把 Hot Path 变回普通 C](./cn/ch-07.md)

**Part III — Contract IR 与 Compiler**
- [第八章：从重复 API 契约到 Contract Compiler——DataBind IDL、FunctionDesc 与多后端生成](./cn/ch-13.md)

**Part IV — Live Runtime 与工程边界**
- [第九章：从 Stream 到 Reactive——WAIT、Wake、Demand 与 Backpressure](./cn/ch-08.md)
- [第十章：Executor——把执行策略从计算语义中拆出来](./cn/ch-09.md)
- [第十一章：从 Event 到 State Machine——把状态变化变成可验证的执行模型](./cn/ch-10.md)
- [第十二章：从 State Machine 到 Actor——用 Mailbox、串行执行与生命周期组合并发对象](./cn/ch-11.md)
- [第十三章：工程化边界——ABI、Multi-TU、Semantic Identity 与可安装的 C Library](./cn/ch-12.md)

**收束 — Restraint 与 Synthesis**
- [第十四章：有限、显式、按需——什么时候应该停止 Meta 化](./cn/ch-14.md)
- [第十五章：从重复知识到三种 IR——Modern C 的 Contract、Native 与 Execution 语义](./cn/ch-15.md)
<!-- book-toc:end -->

## 写法：少讲形容词，多给可检查对象

本书后续重写默认遵守：

~~~text
Plain C baseline
    ↓
具体重复或歧义
    ↓
最小 semantic object
    ↓
C representation
    ↓
compiler/runtime pseudocode
    ↓
flow
    ↓
logic / invariant
    ↓
Lean only if needed
    ↓
lowered ordinary C
    ↓
failure case
    ↓
evidence
~~~

如果一个 abstraction 只能用“更优雅、更灵活、更现代”解释，而不能给出代码、IR、判断规则、失败条件或证据，它还没有写清楚。

## 形式证明的位置

Lean 只用于真正的 semantic obligation，例如 graph rewrite preservation、normalization、state determinism、lifecycle invariant 和 protocol refinement。

轻量 contract/binding 规则先直接写成逻辑判断：

~~~text
Γ ⊢ field : T
Γ ⊢ param : U
convertible(T, U)
────────────────────
Γ ⊢ bind(field, param) : valid
~~~

ABI、DSO、sanitizer 和性能分别用 compile/link、integration、sanitizer、benchmark 验证，不让一种证据代替另一种。

## 章节写作约束

详细结构见：

- [BOOK_ARCHITECTURE.md](./BOOK_ARCHITECTURE.md)
- [CHAPTER_TEMPLATE.md](./CHAPTER_TEMPLATE.md)
- [SOURCE_SNAPSHOTS.md](./SOURCE_SNAPSHOTS.md)

章节文件名是稳定 source ID；真正的出版顺序与章号由各 edition 的 `BOOK_MANIFEST.txt` 和章节 H1 共同决定。

## 出版构建

各章 Markdown 是唯一可编辑正文；出版顺序由 [BOOK_MANIFEST.txt](./BOOK_MANIFEST.txt) 定义。

~~~bash
python3 scripts/validate_book.py
python3 scripts/build_book.py
~~~

Release workflow 从同一份 canonical Markdown 生成 HTML、EPUB3 与 PDF。

## License

本项目采用 [Apache License 2.0](./LICENSE)。

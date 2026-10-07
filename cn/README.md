# 中文版

本目录是《用现代语法写 C》的中文正文。

章节文件名（例如 `ch-13.md`）是稳定 source ID，不再等同于出版章号。真正的阅读顺序由 [BOOK_MANIFEST.txt](./BOOK_MANIFEST.txt) 定义，章节显示编号来自各文件 H1。

## 目录

<!-- book-toc:start -->
**Part I — Native Semantic IR**
- [第一章：从 Linux-style C 到 CMeta——Macro、static inline 与零运行时抽象](./ch-01.md)
- [第二章：有限元编程内核——Generic、Struct、Traits 与类型关系](./ch-02.md)
- [第三章：函数语义与执行投影——FunctionDesc、Receiver、Bind 与 Invokable](./ch-03.md)

**Part II — Execution IR**
- [第四章：从 Callable 到 Graph——把计算本身变成数据](./ch-04.md)
- [第五章：从 Graph 到 Stream——用高级接口构造同一张 Typed Graph](./ch-05.md)
- [第六章：Graph 的可信语义——从 Observable Semantics 到 Verified Rewrite](./ch-06.md)
- [第七章：从 Graph 到 Plan 与 Direct——把复杂性提前，把 Hot Path 变回普通 C](./ch-07.md)

**Part III — Contract IR 与 Compiler**
- [第八章：从重复 API 契约到 Contract Compiler——DataBind IDL、FunctionDesc 与多后端生成](./ch-13.md)

**Part IV — Live Runtime 与工程边界**
- [第九章：从 Stream 到 Reactive——WAIT、Wake、Demand 与 Backpressure](./ch-08.md)
- [第十章：Executor——把执行策略从计算语义中拆出来](./ch-09.md)
- [第十一章：从 Event 到 State Machine——把状态变化变成可验证的执行模型](./ch-10.md)
- [第十二章：从 State Machine 到 Actor——用 Mailbox、串行执行与生命周期组合并发对象](./ch-11.md)
- [第十三章：工程化边界——ABI、Multi-TU、Semantic Identity 与可安装的 C Library](./ch-12.md)

**收束 — Restraint 与 Synthesis**
- [第十四章：有限、显式、按需——什么时候应该停止 Meta 化](./ch-14.md)
- [第十五章：从重复知识到三种 IR——Modern C 的 Contract、Native 与 Execution 语义](./ch-15.md)
<!-- book-toc:end -->

实现、API、Lean theorem 与 source snapshot 的引用仍由仓库根目录的 [SOURCE_SNAPSHOTS.md](../SOURCE_SNAPSHOTS.md) 统一约束。

返回：[项目主页](../README_CN.md)

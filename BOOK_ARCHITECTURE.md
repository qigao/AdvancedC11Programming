# 全书结构：CMeta 的技巧、证明与应用

书名为 **Advanced C11 Programming: From Macros to Modern Programming Models**，中文版为**《C11 高级编程：从宏到现代编程模型》**。内容说明为“元编程、反射、流式与响应式编程”。全书面向掌握指针、结构体、函数指针、对象生存期和基本构建工具的 C 程序员，以 CMeta 为实现案例，解释可迁移到其他 C 项目的高级技巧。

CMeta 主线包括有限宏展开、编译期类型约束、泛型与类型关系、静态反射、调用适配及资源生存期。CFlow、DataBind 和 CMeta Plugin 提供应用场景，使读者看到这些机制如何进入计算图、服务契约、响应式执行和动态模块。每个应用都要回答：它从 CMeta 取得哪些事实，怎样校验或生成代码，以及普通 C 最终执行什么。

语言基线是 C11；使用 GNU/Clang 扩展或平台 ABI 的位置需显式说明能力与限制。书中 API、定理和工程证据以 `SOURCE_SNAPSHOTS.md` 为准；示意代码与当前实现分开标注。CMeta 是正文组件名，Salts 用于仓库、发行包和真实 CMake 标识。

## 写作与论证方法

一节从具体问题开始，以代码建立对象，以伪代码展开过程，以条件和推导解释正确性，再通过反例与实际调用说明适用范围。每一部分都必须具备相互对应的代码、算法和证明，不能用流程图或测试名清单代替论证。形式证明只覆盖模型中明确陈述的性质；ABI、工具链、并发实现和性能仍需相应工程证据。

段落应连续解释一个论点，避免单句逐行、空泛过渡和重复宣言。生僻术语先定义再使用，中文版用中文完成解释；即使面向高级 C 用户，也应说明 thunk、continuation、lowering 等词在当前代码中的具体含义。完整规则见 [CHAPTER_TEMPLATE.md](./CHAPTER_TEMPLATE.md)。

# Part I — CMeta：宏、类型与函数

前三章建立可复用的 CMeta 技巧。从表达式重复求值、声明生成和独立翻译单元的类型身份出发，读者依次看到宏展开、类型描述符、函数描述符和调用适配代码。证明以有限展开、类型关系和控制流推导为主，使读者先能在普通 C 中检查结论，再进入后续形式模型。

| 章 | 代码与算法 | 推导重点 | 应用 |
|---|---|---|---|
| 1 | 函数与宏、`container_of`、编译器能力、作用域清理 | 单次求值、成员类型约束、清理与资源生存期的关系 | 选择函数、宏或编译器扩展 |
| 2 | 有限映射、元组投影、Generic、Struct、Traits、TypeFunction | 有界展开、唯一匹配、结构化类型身份 | 从同一字段或参数声明生成多个使用方 |
| 3 | FunctionDesc、FunctionAbi、thunk、receiver、bind、capture | 参数投影、适配前置条件、失败前不调用、借用生存期 | 直接调用、动态适配与绑定函数 |

贯穿例子是 `User` 的结构与类型，以及 `increment(int)` 的调用。函数反射章节应把 `void *` 入口展开为普通 C，说明类型擦除后的真实对象类型仍由调用者和生成代码共同约束。

# Part II — CMeta 应用：计算图、Stream 与证明

这部分使用 CMeta 的类型身份与可调用对象描述偶数平方和计算。先从手写循环得到有类型的图，再用 Stream 接口构图，定义输出、顺序和错误等观察结果，最后推导允许的图改写并检查生成的执行形式。

| 章 | 代码与算法 | 推导重点 | 应用 |
|---|---|---|---|
| 4 | 节点、边、构图校验与不可变快照 | 相邻节点类型相容及整图有效性 | 把调用关系保存为可分析的数据 |
| 5 | Stream 构造、范围借用与执行入口 | 接口构图保持 Graph 语义 | 有类型的数据转换接口 |
| 6 | 观察语义、规则匹配、改写记录 | 幂等、融合、精化及其假设 | 授权具体优化步骤 |
| 7 | 规范化、计划编译、Direct/AOT 资格检查 | 变换前后结果一致与不支持条件 | 减少逐值解释和调度开销 |

公式中的相等必须说明比较哪些观察结果。函数的性质标志只是声明；允许移除或合并调用之前，还需证明对应语义规律，处理副作用、整数范围、错误顺序和资源行为等限制。固定快照仅支持的 Direct 范围必须如实描述。

# Part III — CMeta 应用：契约绑定与代码生成

第八章把 `get_user` 的服务契约与 CMeta 函数描述连接起来，展示 DataBind 如何编译输入转换、精确调用和输出转换。HTTP、RPC、Plugin、WASM、OpenAPI 和 Mock 是同一契约的不同应用；介绍每个后端时应给出新增的映射规则与失败条件。

核心算法包括请求字段与参数匹配、ABI 检查、计划生成和失败回滚。证明要说明何时可以调用本地函数、哪些对象已经初始化、失败时如何清理，以及 Plugin 租约为什么必须覆盖回调和销毁过程。描述符相等不能代替实际调用能力，契约类型也不能直接当作跨平台二进制布局。

# Part IV — CMeta 应用：响应式执行与工程边界

响应式执行、执行器、状态机和 Actor 使用已有的类型与调用契约，并各自维护明确的运行时状态。读者应能从代码看出状态属于哪个实例，从伪代码看出何时检查与提交，从不变量看出怎样避免重复执行、资源遗失和过期回调。

| 章 | 代码与算法 | 推导重点 | 应用 |
|---|---|---|---|
| 9 | poll、订阅状态、等待登记与唤醒 | 需求额度守恒、丢失唤醒、终止状态 | 暂时无数据的流式输入 |
| 10 | 有界队列、投递、运行、取消与清理 | 已接受任务的计数守恒 | 分离计算语义与执行策略 |
| 11 | 有类型事件、转移表、暂存与提交 | 转移选择、原子提交与串行修改 | 连接状态机 |
| 12 | Actor 身份、邮箱、发送与串行取出 | 多生产者准入、单修改者和关闭顺序 | 并发对象组合 |
| 13 | 独立翻译单元、DSO、ABI 与安装包 | 类型身份、生存期和二进制契约 | 可发布、可安装的 C 库 |

第十二章保留 Actor 作为**独立应用邮箱与单一状态修改者**的语义案例，但新增 Salts 2.3 CNet SG Owner + 策略作为**无需 Actor**的对照。Policy 选择不等于容量预留，远端 Endpoint 不等于本地 Owner，Reconnect 不等于逻辑请求 Retry；这些最新安装版 SDK 事实必须与固定版 Actor/Lean 证明严格分开，并通过独立 C11 consumer gate 验证。

第十二章另外将 ACE **Reactor 风格事件派发**与 **Proactor NativeIO 完成路径**进行一对一比较：前者从真实 TCP listener readiness 出发、由调用方 poll 并通过 CMeta typed Handler 处理事件；后者由最终 SG Owner 的单一 host lease 观察 completion，再执行 socket handoff、终止回收和 credit 结算。两者不额外创建 I/O 引擎；`cnet_client_poll` 内部也消费 NativeIO completion，因此不宣称存在独立纯内核 Reactor 实现。新 gate 为 `ch12_ace_reactor_dispatch.c`，现有 Proactor gate 为 `ch12_cnet_sg_handoff.c`。

第十二章还通过安装版 `Salts::NativeIO` + `Salts::CNet` 验证两个真实 SG Owner 的 TCP handoff：一次单独的 socket 转移、双向实际负载、owner-affine 回调与 terminal/Manager 回收。这是对纯 Strategy API gate 的运行时补充，不能被解释为压力测试或 Actor Lean 证明。

## 贯穿章节：CMeta 版 ACE 模式的设计与应用

本书不另设一套继承式 ACE Runtime，而把设计模式的**合同与执行者**按层解释和验证：

- 第三章：CMeta `CMETA_INTERFACE`、`FunctionDesc/FunctionAbi` 和 `CMETA_INTERCEPTOR_TYPE` 描述 Strategy、Adapter、Interceptor；生成 exact C 调用、检查 hook 顺序、拒绝路径与 provider 借用。
- 第十章：`cmeta_ace_lockable` 与 `CMETA_ACE_SYNCHRONIZED` 只生成有类型的同步入口；Platform 才拥有 mutex、condition、TLS 与 Leader/Followers 的线程交接。Monitor Object 不等于 CMeta 自带一把锁。
- 第十二章：应用真正需要独立邮箱时才组合 CMeta typed Port/Strategy 与 CFlow Active Object；CNet Acceptor-Connector、SG Owner 和 NativeIO Reactor/Proactor 不因此复制执行引擎。Half-Sync/Half-Async 和 Pipes/Filters 的有界容量、需求额度仍归 CFlow。
- 第十五章：给出按需求选择 ACE 模式的决策表，再通过严格配置文件、CMeta typed Strategy、owner-affine **一次性调用 lease** 与静止点提交演示**应用级 Service Configurator**；CMeta Interface `self` 指向不可复用的借用槽位，过期 Interface 即使碰上另一活跃租约也不能复活；增补独立配置 CNet 服务端 Owner Placement 与客户端远端 Destination 的真实 API 演示，精确区分 `choose` 与资源准入。不冒充已发布通用机制，也不声称可以自动热替换 Plugin 或 CNet 连接。

对后续 Salts 2.3 的接口只作**增补验证**，不反推它们已存在于固定版源码快照。安装版 C11 `ch03_cmeta_ace_patterns.c` 和 `ch12_cmeta_ace_active_object.c` 通过真实执行检查精确 Strategy/Interceptor 顺序、Scoped Locking、多线程递增、NativeIO 一次性完成、Actor STOPPED/STALE；这仍不是全部 ACE 模式、性能或多平台发布保证。任何高级模式都必须最终呈现为可检查的普通 C 数据结构、调用和所有权责任。

第十三章将模型假设放回真实工具链中检查。保留 exact ABI epoch、无静默回退、显式所有权与有界资源等约束；同时区分已经完成的快照证据和仍需组合构建验证的边界。

## 收束：技巧取舍与综合应用

第十四章用普通循环、回调、内部函数和静态链接作对照，分析新增描述符或运行时是否解决实际维护问题。第十五章把类型事实、函数契约、图变换和运行时状态连成完整应用，归纳可复用的判断方法与其限制。

三个中间表示保留为解释应用的工具：CMeta 描述本地类型与函数，DataBind 描述外部契约，CFlow 描述计算关系。它们之间的对应关系要落实为字段、校验和生成代码；章节组织始终服务于读者掌握 CMeta 高级技巧的目标。

## 出版组织

保持十五章及 `BOOK_MANIFEST.txt` 的阅读顺序；`ch-NN.md` 是稳定源文件名，不等同于出版章号。目录由 `scripts/update_toc.py` 生成。中文和英文采用相同的编号结构、技术结论与示例条件，各自使用自然的连贯语言。

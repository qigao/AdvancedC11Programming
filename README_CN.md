# C11 高级编程

**从宏到现代编程模型**

*元编程、反射、流式与响应式编程*

**中文** | [English](./README.md)

本书以 **CMeta 的高级技巧、正确性推导和应用**为主线，讲解怎样用 C11、有限宏展开、内联函数和静态元数据构造有类型的接口。读者将从具体 C 代码理解反射、调用适配、资源生存期与代码生成，并把这些技巧应用到计算图、Stream、响应式执行和服务契约。

## 读者与预备知识

适合已经掌握指针、结构体、函数指针、对象生存期和基本编译链接的 C 程序员。阅读前无需了解 CMeta，也不要求具备编译器或形式化方法背景；thunk、continuation、lowering 等术语会结合首次使用的代码解释。示例以 C11 为基线，使用编译器扩展或平台 ABI 时会说明条件。

## 将学到什么

- 用有限宏映射与元组投影生成声明，并分析单次求值、类型约束和展开边界。
- 用 CMeta 描述类型、字段和函数，区分元数据、调用能力及资源所有权。
- 展开调用适配函数，理解参数绑定、类型擦除及其存储与生存期前提。
- 组合有类型的计算图与 Stream，用伪代码、不变量和证明检查变换的正确性。
- 在 DataBind、CFlow 与 CMeta Plugin 应用中处理契约绑定、背压、取消、清理和 ABI。
- 用 **CMeta 实现 ACE 设计模式**：有类型的 Strategy/Interceptor、Platform 支持的 Scoped Locking、按需组合的 CFlow Active Object；CNet/NativeIO 仍负责 Reactor/Proactor 与 SG 网络。参见[第三章](./cn/ch-03.md)、[第十章](./cn/ch-09.md)、[第十二章](./cn/ch-11.md)及[第十五章模式选择表](./cn/ch-15.md)。

## 示例：从一个普通 C 调用开始

下面是一个完整的 C11 程序，展示后续函数反射章节使用的本地函数：

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

保存为 `increment.c`，执行 `cc -std=c11 -Wall -Wextra increment.c -o increment`，运行 `./increment` 后输出 `5`。这个输入不会发生有符号整数溢出；函数的一般调用需要满足 `value < INT_MAX`。

第三章在此基础上展示 CMeta 怎样从参数声明生成函数描述符和调用适配函数（thunk）。适配函数从已知类型的存储读取参数，调用 `increment`，再写回结果。书中会给出展开的 C 代码、检查次序的伪代码和成功/失败路径推导，说明统一的 `void *` 入口仍然需要正确类型、对齐和生存期的存储。

## 阅读路线

### Part I — CMeta：宏、类型与函数

前三章从宏与函数的求值规则进入有限元编程、静态反射和调用适配。示例展示生成前后的代码，并推导类型匹配、参数投影和借用条件。

### Part II — CMeta 应用：计算图、Stream 与证明

以偶数平方和为例，用 CMeta 类型与可调用对象构造 CFlow 计算图，再通过 Stream 表达数据转换。伪代码说明构图、规范化和编译过程，证明说明哪些改写保持约定的结果，基线循环用于比较执行形式和成本。

### Part III — CMeta 应用：契约绑定与代码生成

以 `get_user` 为例，把 DataBind 服务契约与 CMeta 函数描述连接起来，生成输入转换、精确调用和输出转换。HTTP、RPC、Plugin 等应用展示不同的映射规则、失败回滚和资源生存期。

### Part IV — CMeta 应用：响应式执行与工程边界

从暂时无数据的 Source 进入等待、唤醒、需求额度与背压，再讨论执行器、状态机、Actor 和动态库。状态转移伪代码与不变量解释并发行为，独立编译和安装后使用方验证真实工程边界。

## 目录

<!-- book-toc:start -->
**Part I — CMeta：宏、类型与函数**
- [第一章：CMeta 基础：宏、内联函数与编译期约束](./cn/ch-01.md)
- [第二章：有限元编程：泛型、结构体、特征与类型关系](./cn/ch-02.md)
- [第三章：函数反射：描述符、调用适配与参数绑定](./cn/ch-03.md)

**Part II — CMeta 应用：计算图、Stream 与证明**
- [第四章：从可调用对象到计算图：表示与类型检查](./cn/ch-04.md)
- [第五章：Stream 接口：构造有类型的计算图](./cn/ch-05.md)
- [第六章：计算图的语义与改写证明](./cn/ch-06.md)
- [第七章：从计算图到执行计划：优化与直接执行](./cn/ch-07.md)

**Part III — CMeta 应用：契约绑定与代码生成**
- [第八章：契约绑定与代码生成：CMeta 与 DataBind 的应用](./cn/ch-13.md)

**Part IV — CMeta 应用：响应式执行与工程边界**
- [第九章：响应式执行：等待、唤醒、需求额度与背压](./cn/ch-08.md)
- [第十章：执行器与调度器：任务、容量和完成责任](./cn/ch-09.md)
- [第十一章：事件与状态机：类型检查、转移和提交](./cn/ch-10.md)
- [第十二章：Actor：邮箱、串行修改与对象生存期](./cn/ch-11.md)
- [第十三章：CMeta 工程边界：类型身份、ABI 与动态库](./cn/ch-12.md)

**收束 — 技巧取舍与综合应用**
- [第十四章：高级技巧的取舍：何时保留普通 C](./cn/ch-14.md)
- [第十五章：CMeta 综合应用：从声明事实到可靠执行](./cn/ch-15.md)
<!-- book-toc:end -->

## 代码与证据

正文标明完整程序、示意片段、伪代码及固定版本实现的区别。手工推导和机器检查的 Lean 定理分别说明假设与覆盖范围；编译、ABI、并发实现和性能采用相应工程验证。源码与证据版本见 [SOURCE_SNAPSHOTS.md](./SOURCE_SNAPSHOTS.md)。

元编程、RAII 与反射组件使用 **CMeta** 名称，`cmeta_plugin_*` 使用 **CMeta Plugin** 名称。**Salts** 是源码仓库和 SDK 发行包名称，构建示例保留 `find_package(Salts)`、`Salts::CMeta` 等真实标识；CFlow、DataBind 使用各自组件名。

## 构建与编写

[中文版正文](./cn/README.md)与[英文版正文](./en/README.md)按各自的 `BOOK_MANIFEST.txt` 构建。章节文件名是稳定源文件号，显示章号和目录给出实际阅读顺序。

~~~bash
python3 scripts/validate_book.py --edition cn
python3 scripts/build_book.py --edition cn
python3 scripts/validate_book.py --edition en
python3 scripts/build_book.py --edition en
~~~

发布流程从同一份章节源文件生成 Markdown、HTML、EPUB 与 PDF。编写与审阅依据 [全书结构](./BOOK_ARCHITECTURE.md)和[章节写作规范](./CHAPTER_TEMPLATE.md)。

## License

本项目采用 [Apache License 2.0](./LICENSE)。

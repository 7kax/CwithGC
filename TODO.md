# CwithGC TODO

## 优先级说明

- **P0**：会导致未定义行为、数据损坏或 GC 核心逻辑错误
- **P1**：接口安全性、可移植性或资源管理问题
- **P2**：测试、构建和可维护性改进

## 核心正确性

| 状态 | 优先级 | 模块 | TODO | 完成标准 |
| --- | --- | --- | --- | --- |
| [x] | P0 | copying | 复制对象后，更新 to-space 对象内部的所有指针，而不是修改 from-space 源对象 | 嵌套对象、链表、树在 GC 后内部指针全部指向新空间；连续执行多次 GC 仍通过 |
| [ ] | P0 | ref_count | 修复释放元数据后仍访问 `meta_ptr->size` 的 use-after-free | ASan 下 `ref_count_basic` 和 `ref_count_recursion` 无 UAF |
| [ ] | P0 | copying / mark_sweep | 对每个分配块做最大对齐，并统一元数据、payload 和 free block 的地址计算 | UBSan alignment 检查无错误；不同大小的对象均可安全分配 |
| [ ] | P0 | mark_sweep | 正确处理 `free_list == nullptr`，避免满堆或无空闲块时在 sweep 阶段断言失败 | 满堆分配后调用 `gc_collect()` 不崩溃；无法分配时统一进入 allocation failure |
| [ ] | P0 | ref_count | 处理 `gc_ptr_copy(dst, src)` 的自赋值和释放顺序，避免先释放后增加引用 | `gc_ptr_copy(&p, p)`、别名赋值和字段替换在 ASan 下安全 |
| [ ] | P0 | all | 检查 `size + sizeof(meta_data)` 及指针表大小计算的整数溢出 | 超大 size 被拒绝，不会绕回成小块并写越界 |

## 生命周期与接口

| 状态 | 优先级 | 模块 | TODO | 完成标准 |
| --- | --- | --- | --- | --- |
| [ ] | P1 | all | 为 `gc_init()`、`gc_collect()`、`gc_malloc()`、`gc_cleanup()` 增加初始化状态检查，并处理重复初始化 | 未初始化、重复初始化、清理后调用都得到明确行为 |
| [ ] | P1 | all | 让空 root 集合调用 `gc_pop()` 安全返回 | 不再访问空 vector 的 `back()` |
| [ ] | P1 | all | 重新设计 root 生命周期，减少对 `__builtin_frame_address(1)` 的依赖 | 在优化构建、不同编译器和递归调用下行为稳定；最好改为显式 scope/token API |
| [ ] | P1 | all | 明确 `gc_local_var()`、`gc_ptr_copy()` 的所有权和调用约束 | 文档说明哪些指针必须注册、哪些字段必须通过 `gc_ptr_copy()` 更新 |
| [ ] | P1 | ref_count | 让 `gc_cleanup()` 释放仍存活的对象，或明确要求调用者先释放全部引用 | LeakSanitizer 无遗留 GC 对象；cleanup 后状态一致 |
| [ ] | P1 | ref_count | 明确或实现循环引用回收 | 文档明确“循环引用不会回收”，或增加 cycle collector 测试与实现 |
| [ ] | P1 | all | 为 `gc_register()` 校验 pointer table 的范围、offset、数组长度和对象 payload 大小 | 非法指针表被拒绝，不会在 mark/copy/decrement 阶段越界 |
| [ ] | P1 | all | 明确 pointer table 的所有权和生命周期，避免 metadata 保存悬空 table 指针 | pointer table 在对象生命周期内有效，且不产生未释放的辅助内存 |

## 内存布局与可移植性

| 状态 | 优先级 | 模块 | TODO | 完成标准 |
| --- | --- | --- | --- | --- |
| [ ] | P1 | gc.h / all | 用标准类型替换 `u_int8_t`、`u_int64_t`，避免直接用整数承载指针运算 | 使用 `uint8_t`、`uintptr_t` 或标准字节指针运算 |
| [ ] | P1 | gc.h | 重新设计 `positions[0]`，避免依赖 C/C++ 的零长度数组扩展 | C11 和 C++20 的目标编译器都不依赖非标准扩展 |
| [ ] | P1 | all | 避免通过整数比较/加法操作对象指针，统一使用安全的字节指针和边界检查 | UBSan、严格编译器和 32/64 位环境下行为明确 |
| [ ] | P1 | debug API | 统一 `gc_mem_layout()` 的分配与释放方式；当前实现使用 `new[]`，测试使用 `free()` | ASan 不再报告 alloc/dealloc mismatch；最好提供 `gc_mem_layout_free()` |
| [ ] | P1 | ref_count | 决定 `gc_mem_layout()` 是否支持引用计数实现，避免公开接口返回永远为 `nullptr` | 文档和实现一致，通用调试代码不会误用该接口 |

## 测试与验证

| 状态 | 优先级 | 模块 | TODO | 完成标准 |
| --- | --- | --- | --- | --- |
| [ ] | P1 | tests | 增加嵌套对象、循环引用、连续多次 GC、满堆分配和自赋值回归测试 | 每个已修复核心 bug 都有最小可复现测试 |
| [ ] | P1 | tests | 增加 ASan、UBSan、LeakSanitizer 构建/测试配置 | CI 或本地命令可一键运行 sanitizer 测试 |
| [ ] | P2 | verify | 当前 `_main()` 中的危险示例没有被调用，补充真正验证 dangling pointer、double free、use-after-free 的测试方式 | 验证用例能明确检测预期信号或 sanitizer 报告，而不是仅检查正常退出 |
| [ ] | P2 | tests | 避免测试只依赖 `assert`，保证 Release 构建仍然检查结果 | `NDEBUG` 下测试仍能正确失败 |
| [ ] | P2 | tests | 释放测试中动态分配的 pointer table，或改为静态常量表 | LeakSanitizer 不报告测试辅助内存泄漏 |

## 构建与代码质量

| 状态 | 优先级 | 模块 | TODO | 完成标准 |
| --- | --- | --- | --- | --- |
| [ ] | P2 | CMake | 用 `target_include_directories()`、`target_link_libraries(... PRIVATE ...)` 替代全局 `include_directories()` | 目标依赖边界清晰，目录间不相互污染 |
| [ ] | P2 | CMake | 增加 `BUILD_TESTING` 选项，并区分普通单元测试和故意触发错误的 verify 用例 | 默认构建可控，负向测试不会伪装成普通通过测试 |
| [ ] | P2 | gc.h / CMake | 不要在公共头文件中无条件定义 `GC_DEBUG` | Debug API 是否启用由构建配置决定 |
| [ ] | P2 | gc.h | 将无参数 C 函数声明改为 `(void)`，并补充失败行为、线程安全和生命周期文档 | C 编译器可严格检查调用；API 契约完整 |
| [ ] | P2 | all | 将重复的 root 管理、大小检查、pointer table 校验抽成共享工具 | 三种 GC 的公共行为一致，减少重复修复 |
| [ ] | P2 | all | 统一 clang-format 风格和中英文注释规范 | 格式检查可自动执行 |

## 当前验证基线

- [x] 普通 Clang 构建通过
- [x] 当前 CTest：36/36 通过
- [ ] ASan/UBSan 全量测试通过
- [x] 连续 GC 的嵌套对象测试通过
- [ ] Release（`NDEBUG`）测试通过

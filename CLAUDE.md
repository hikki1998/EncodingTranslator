# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## 项目概述

Source Encoding Translator — 基于 Qt5 的 Windows 桌面应用，用于批量扫描源代码文件、检测文本编码（UTF-8/GBK/ASCII），并在编码间转换文件。典型场景：将 GBK 编码的遗留 C++ 项目批量转换为 UTF-8。

## 构建与运行

依赖：Qt 5.15+（Core、Widgets、Concurrent 模块）、CMake 3.16+、MSVC with C++17。

```bash
# 配置
cmake -B build -DCMAKE_BUILD_TYPE=Release

# 构建主程序
cmake --build build --config Release

# 运行
./build/Release/EncodingTranslator.exe

# 构建并运行命令行测试
cmake --build build --config Release --target test_search
./build/Release/test_search.exe
```

## 架构

```
main.cpp
  └─ MainWindow（UI + 线程调度）
       ├─ SearchWorker（QThread 中的后台扫描线程）
       ├─ ConvertWorker（QThread 中的后台转换线程）
       └─ EncodingUtils（纯函数工具，无 Qt GUI 依赖）
```

### 关键设计决策

- **多线程搜索**：`SearchWorker::process()` 先递归收集所有子目录（排除被忽略的目录），然后使用 `QtConcurrent::map` 将各子目录分配给 `QThreadPool` 中的线程并行处理。每个线程独立遍历其目录内的文件、检测编码。进度通过 `QTimer` 每 150ms 轮询 `QAtomicInt` 计数器报告给 UI。`process()` 立即返回，不阻塞工作线程的事件循环。
- **Worker 线程模型**：`SearchWorker` 和 `ConvertWorker` 是 `QObject` 子类，通过 `moveToThread()` 移入 `QThread` 运行。主线程通过 `QMetaObject::invokeMethod` + `Qt::QueuedConnection` 触发其 `process()` 槽函数。
- **编码检测**（`EncodingUtils::detectEncoding`）：纯 ASCII → 检查 BOM → UTF-8 字节序列验证 → Windows `MultiByteToWideChar`（936 代码页）验证 GBK。大文件只读取前 256KB 采样。
- **转换管道**：所有转换经过 Unicode 中间层（`QString`），即 源编码 → `QString` → 目标编码。仅支持 GBK ↔ UTF-8 互转。
- **目录排除**：`defaultExcludePatterns()` 提供 18 项默认排除模式（build、.git、.vs、node_modules 等），`isDirExcluded()` 对路径的每个段做 wildcard 匹配。UI 中可自定义。

### 文件说明

| 文件 | 职责 |
|------|------|
| `src/main.cpp` | 入口，启动 QApplication 和 MainWindow |
| `src/MainWindow.h/.cpp` | GUI 布局、信号连接、线程生命周期管理 |
| `src/EncodingUtils.h/.cpp` | 编码检测、模式匹配、排除逻辑、文件转换（纯逻辑） |
| `src/SearchWorker.h/.cpp` | 多线程目录遍历 + 编码检测 |
| `src/ConvertWorker.h/.cpp` | 对选中文件执行编码转换 |

### 扩展名过滤

默认扩展名硬编码在 `MainWindow::defaultExtensions()`，覆盖 C/C++ 系。用户输入通过 `parseExtensionPatterns()` 解析，分隔符为逗号、分号或空白。模式自动补充通配符（`cpp` → `*.cpp`）。

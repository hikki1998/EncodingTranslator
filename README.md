# Source Encoding Translator

Source Encoding Translator 是一个基于 Qt5 的 Windows 桌面工具，用于批量扫描源代码文件的文本编码，并在 GBK 和 UTF-8 之间转换文件。它适合把遗留 GBK 编码的 C/C++ 工程迁移到 UTF-8，也可以反向转换为 GBK。

## 功能

- 扫描指定目录下的源代码文件
- 检测 ASCII、UTF-8、GBK 和未知编码
- 按扩展名过滤文件
- 排除 `build`、`.git`、`.vs`、`node_modules` 等目录
- 使用多线程提升大目录扫描速度
- 勾选需要处理的文件后批量转换编码

## 环境要求

- Windows
- CMake 3.16 或更高版本
- MSVC，支持 C++17
- Qt 5.15 或更高版本，包含 Core、Widgets、Concurrent 模块

## 构建

```powershell
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

构建成功后，主程序位于：

```text
build/Release/EncodingTranslator.exe
```

## 使用方法

1. 启动 `EncodingTranslator.exe`。
2. 点击 `Browse` 选择要扫描的项目目录。
3. 在 `Extensions` 中配置要扫描的文件扩展名，例如 `.cpp,.h,.hpp,.c`。
4. 在 `Exclude Dirs` 中配置要排除的目录，默认会排除构建目录、版本控制目录和常见依赖目录。
5. 在 `Target Encoding` 中选择目标编码：`GBK` 或 `UTF-8`。
6. 点击 `Search` 扫描不符合目标编码的文件。
7. 确认列表中勾选的文件后，点击 `Convert Selected` 执行转换。

建议在转换前先提交或备份源码，编码转换会直接覆盖原文件。

## 测试

```powershell
cmake --build build --config Release --target test_search
./build/Release/test_search.exe
```

## 项目结构

```text
src/
  main.cpp              应用入口
  MainWindow.*          界面、交互和线程调度
  SearchWorker.*        后台目录扫描和编码检测
  ConvertWorker.*       后台文件编码转换
  EncodingUtils.*       编码检测、过滤规则和转换逻辑
tests/
  test_search.cpp       搜索和排除规则测试
```

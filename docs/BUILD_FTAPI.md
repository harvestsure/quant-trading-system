# FTAPI 编译指南

## 问题背景

富途官方提供的预编译 FTAPI 库不完整，缺少某些 Proto 消息类的符号定义（如 `Trd_FlowSummary::Response` 和 `Trd_GetOrderFee::Response`），导致链接时出现"Undefined symbols"错误。

## 解决方案：从源码编译 FTAPI

### 前置要求

- Xcode 工具链已安装
- CMake 3.15 或更高版本
- 对于 x86_64 架构：需要在 Apple Silicon Mac 上使用 Rosetta 2

### 编译步骤

#### 1. 进入 FTAPI 源码目录

```bash
cd /path/FTAPI4CPP_<version>/Src
```

#### 2. 创建构建目录

```bash
mkdir -p build
cd build
```

#### 3. 配置并编译（x86_64 Release）

```bash
cmake .. -DCMAKE_OSX_ARCHITECTURES=x86_64 -DCMAKE_BUILD_TYPE=Release -DTARGET_OS=Mac
make -j$(nproc)
```

**关键参数说明：**
- `-DCMAKE_OSX_ARCHITECTURES=x86_64` - 指定编译为 x86_64 架构（适用于 Apple Silicon Mac 使用 Rosetta 2）
- `-DCMAKE_BUILD_TYPE=Release` - 编译 Release 版本
- `-DTARGET_OS=Mac` - 指定目标平台为 macOS

#### 4. 安装库文件

```bash
make install
```

这会将编译好的库文件安装到 `/path/FTAPI4CPP_<version>/Bin/Mac/Release/` 目录。

### 验证编译结果

编译完成后，检查库文件是否存在：

```bash
ls -lh /path/FTAPI4CPP_<version>/Bin/Mac/Release/
```

应该看到以下文件：
- `libFTAPI.a` - FTAPI 静态库
- `libprotobuf.a` - Protocol Buffers 库
- `libFTAPIChannel.dylib` - FTAPI 通道动态库

### 项目编译

编译完整个项目：

```bash
cd /path/quant-trading-system
./build.sh --ftapi-home /path/FTAPI4CPP_<version> --debug
```

或者

```bash
./build.sh --ftapi-home /path/FTAPI4CPP_<version> --release
```

### 其他架构编译

#### Debug 版本

```bash
cmake .. -DCMAKE_OSX_ARCHITECTURES=x86_64 -DCMAKE_BUILD_TYPE=Debug -DTARGET_OS=Mac
make -j$(nproc)
make install
```

编译好的库会安装到 `/path/FTAPI4CPP_<version>/Bin/Mac/Debug/`

#### ARM64 版本（如需要）

```bash
cmake .. -DCMAKE_OSX_ARCHITECTURES=arm64 -DCMAKE_BUILD_TYPE=Release -DTARGET_OS=Mac/ARM64
make -j$(nproc)
make install
```

**注意：** ARM64 编译需要 `FTAPIChannel.dylib` 的 ARM64 版本，目前官方可能未提供。

### 清理构建

```bash
cd /path/FTAPI4CPP_<version>/Src/build
rm -rf *
```

然后重新执行上述编译步骤。

## 常见问题

### Q: 为什么官方提供的库不完整？

A: 官方可能提供的是精简版本，只包含核心功能。通过从源码编译，可以获得完整的库文件。

### Q: 编译失败提示找不到 protobuf？

A: FTAPI 会自动编译 protobuf，确保 `protobuf-all-3.5.1.tar.gz` 在 `Src/` 目录中。

### Q: 如何针对不同的 Debug/Release 版本？

A: 在 `quant-trading-system` 的 `build.sh` 中使用 `--debug` 或 `--release` 参数。CMakeLists.txt 会自动选择对应的库文件。

## 相关文件

- FTAPI CMakeLists.txt: `/path/FTAPI4CPP_<version>/Src/CMakeLists.txt`
- 项目构建脚本: `/path/quant-trading-system/build.sh`
- 项目 CMakeLists.txt: `/path/quant-trading-system/CMakeLists.txt`

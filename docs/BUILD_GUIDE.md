# 构建指南 (Build Guide)

本项目支持在 macOS 上进行 x86_64 和 ARM64 两种架构的编译。

## 架构说明

- **x86_64**: 使用 Futu SDK 提供的预编译库，需要通过 Rosetta 2 运行（Apple Silicon Mac 上）
- **ARM64**: 从源码编译 FTAPI 和 protobuf，实现原生 ARM64 支持（推荐 Apple Silicon Mac 使用）

## 编译选项

### 1. x86_64 编译（使用预编译库）

默认方式，适用于所有 Mac（Intel 和 Apple Silicon）：

```bash
cd build
cmake -DFTAPI_HOME=/path/FTAPI4CPP_<version> -DENABLE_FUTU=ON -DFTAPI_BUILD_FROM_SOURCE=OFF ..
make
```

特点：
- ✅ 简单快速，使用 Futu 官方提供的库
- ✅ 稳定可靠
- ⚠️ Apple Silicon Mac 需要通过 Rosetta 2 运行
- ⚠️ 性能略低于原生 ARM64

### 2. ARM64 编译（从源码编译，推荐 Apple Silicon Mac）

#### 方式 A: 直接从源码编译（推荐开发调试）

```bash
cd build
cmake -DFTAPI_HOME=/path/FTAPI4CPP_<version> \
      -DENABLE_FUTU=ON \
      -DFTAPI_BUILD_FROM_SOURCE=ON \
      -DCMAKE_BUILD_TYPE=Debug \
      ..
make
```

特点：
- ✅ 每次编译时从源码构建，适合开发调试
- ✅ 可以随时切换 Debug/Release 模式
- ⚠️ 编译时间较长（首次约 2-3 分钟）

#### 方式 B: 使用预编译的 ARM64 库（推荐生产部署）

第一步：预编译 FTAPI ARM64 库

```bash
# 编译 Debug 和 Release 两个版本
./build_ftapi_arm64.sh both

# 或者只编译其中一个版本
./build_ftapi_arm64.sh debug
./build_ftapi_arm64.sh release
```

这会在 `/path/ftapi_arm64/` 目录下创建两个子目录：
- `Debug/`: Debug 版本的库（带调试符号）
- `Release/`: Release 版本的库（优化过的）

第二步：编译项目

```bash
cd build
# Debug 构建
cmake -DFTAPI_HOME=/path/FTAPI4CPP_<version> \
      -DENABLE_FUTU=ON \
      -DFTAPI_BUILD_FROM_SOURCE=OFF \
      -DFTAPI_USE_ARM64=ON \
      -DCMAKE_BUILD_TYPE=Debug \
      ..
make

# 或者 Release 构建
cd build
cmake -DFTAPI_HOME=/path/FTAPI4CPP_<version> \
      -DENABLE_FUTU=ON \
      -DFTAPI_BUILD_FROM_SOURCE=OFF \
      -DFTAPI_USE_ARM64=ON \
      -DCMAKE_BUILD_TYPE=Release \
      ..
make
```

特点：
- ✅ 编译速度快（只需编译业务代码）
- ✅ 原生 ARM64 性能
- ✅ 支持 Debug 和 Release 两种配置
- ⚠️ 需要先运行 `build_ftapi_arm64.sh` 预编译库

## CMake 选项说明

| 选项 | 默认值 | 说明 |
|------|--------|------|
| `ENABLE_FUTU` | OFF | 启用 Futu 交易所支持 |
| `FTAPI_HOME` | 无 | FTAPI SDK 路径（必需） |
| `FTAPI_BUILD_FROM_SOURCE` | ON | 是否从源码编译 FTAPI |
| `FTAPI_USE_ARM64` | OFF | 使用预编译的 ARM64 库 |
| `CMAKE_BUILD_TYPE` | Release | 构建类型（Debug/Release） |

## 构建类型对比

### Debug vs Release

**Debug 模式**:
- 编译选项: `-g -O0`
- 包含调试符号
- 未优化，便于调试
- 可执行文件体积较大
- 适用场景: 开发、调试、测试

**Release 模式**:
- 编译选项: `-O3`
- 不含调试符号
- 完全优化
- 可执行文件体积较小
- 适用场景: 生产部署、性能测试

## 推荐的构建工作流

### 开发阶段
```bash
# 使用从源码编译的 Debug 模式
cmake -DFTAPI_HOME=/path/to/FTAPI \
      -DENABLE_FUTU=ON \
      -DFTAPI_BUILD_FROM_SOURCE=ON \
      -DCMAKE_BUILD_TYPE=Debug \
      ..
make
```

### 生产部署
```bash
# 1. 预编译 Release 版本的 ARM64 库（只需一次）
./build_ftapi_arm64.sh release

# 2. 使用预编译库构建项目
cmake -DFTAPI_HOME=/path/to/FTAPI \
      -DENABLE_FUTU=ON \
      -DFTAPI_BUILD_FROM_SOURCE=OFF \
      -DFTAPI_USE_ARM64=ON \
      -DCMAKE_BUILD_TYPE=Release \
      ..
make
```

## 故障排查

### 问题: "undefined symbols for architecture arm64"

**原因**: 使用 x86_64 库但尝试编译 ARM64 程序

**解决方案**:
1. 要么使用 x86_64 编译: `FTAPI_BUILD_FROM_SOURCE=OFF` 且 `FTAPI_USE_ARM64=OFF`
2. 要么先预编译 ARM64 库: `./build_ftapi_arm64.sh both`

### 问题: protobuf 版本不匹配

**原因**: 系统 protobuf 版本与 FTAPI 要求的版本不一致

**解决方案**: 使用从源码编译模式或预编译 ARM64 库，这些方式会使用特定版本的 protobuf 3.5.1

### 问题: FTAPIChannel.dylib 找不到

**原因**: ARM64 版本目前没有 FTAPIChannel 的动态库

**解决方案**: 暂时使用 x86_64 编译模式，或联系 Futu 获取 ARM64 版本的 FTAPIChannel

## 清理构建

```bash
# 清理项目构建
rm -rf build/*

# 清理预编译的 ARM64 库
rm -rf /path/ftapi_arm64
rm -rf /path/protobuf-3.5.1
```

## 性能对比

在 Apple Silicon Mac 上的初步测试结果（仅供参考）：

| 模式 | 编译时间 | 运行性能 | 可执行文件大小 |
|------|---------|---------|---------------|
| x86_64 (预编译) | ~30秒 | 基准 | ~10MB |
| ARM64 (从源码) | ~2-3分钟 | +20-30% | ~12MB (Debug) |
| ARM64 (预编译) | ~30秒 | +20-30% | ~8MB (Release) |

注: ARM64 性能提升主要体现在 CPU 密集型操作上。

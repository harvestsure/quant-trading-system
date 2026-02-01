# 编译系统简化说明

## 主要变化

已简化编译系统，**移除了从源码编译 FTAPI 的选项**，现在只使用预编译库。

## 新的编译方式

### 默认方式（x86_64 库）
```bash
./build.sh
```
- 使用 Futu 官方提供的 x86_64 预编译库
- 编译速度快（约 30 秒）
- 兼容所有 Mac（Apple Silicon 通过 Rosetta 2 运行）
- 适合快速测试和开发

### ARM64 原生方式（推荐 Apple Silicon）
```bash
# 第一步：构建 ARM64 库（只需一次）
cd scripts
./build_ftapi_arm64.sh both
cd ..

# 第二步：使用 ARM64 库编译
./build.sh --use-arm64
```
- 原生 ARM64 性能
- 编译速度快（约 30 秒）
- 最佳性能
- 库位置：`${FTAPI_HOME}/Bin/Mac/ARM64/Debug` 和 `${FTAPI_HOME}/Bin/Mac/ARM64/Release`

## 选项变化

### 移除的选项
- ❌ `--from-source` - 不再支持从源码编译
- ❌ `--use-prebuilt-x86` - 现在是默认行为，不需要指定

### 保留的选项
- ✅ `--use-arm64` - 使用 ARM64 预编译库
- ✅ `--debug` / `--release` - 构建类型
- ✅ `--ftapi-home <path>` - 指定 FTAPI 路径
- ✅ `--enable-ibkr` / `--enable-binance` - 启用其他交易所

## 优势

1. **更快的编译** - 不再需要编译大量 FTAPI 源码和 Proto 文件
2. **更简单的逻辑** - 只有两种模式：x86_64 或 ARM64
3. **更清晰的控制** - 用户明确控制何时构建依赖库

## 常见用法

```bash
# 快速编译（默认 x86_64）
./build.sh

# Debug 模式
./build.sh --debug

# ARM64 原生（最佳性能）
cd scripts && ./build_ftapi_arm64.sh both && cd ..
./build.sh --use-arm64

# 指定 FTAPI 路径
./build.sh --ftapi-home /path/to/FTAPI4CPP_<version> --use-arm64
```

## 迁移指南

如果你之前使用：
- `./build.sh` - 无需改变，现在更快了
- `./build.sh --from-source` - 改为 `./build.sh`（默认 x86_64）或先运行 `scripts/build_ftapi_arm64.sh` 再使用 `--use-arm64`
- `./build.sh --use-prebuilt-x86` - 改为 `./build.sh`（现在是默认）
- `./build.sh --use-prebuilt-arm64` - 改为 `./build.sh --use-arm64`

# Futu 交易所编译选项快速参考

## 三种编译模式

### 模式 1: 从源码实时编译（开发推荐）
```bash
cd build
cmake -DFTAPI_HOME=/path/FTAPI4CPP_<version> \
      -DENABLE_FUTU=ON \
      -DFTAPI_BUILD_FROM_SOURCE=ON \
      -DCMAKE_BUILD_TYPE=Debug \
      ..
make -j4
```
**特点:**
- ✅ 原生 ARM64 或 x86_64（自动检测）
- ✅ 灵活切换 Debug/Release
- ⚠️ 首次编译慢（约 2-3 分钟）

### 模式 2: 使用 ARM64 预编译库（生产推荐）

第一步：预编译库（只需一次）
```bash
./build_ftapi_arm64.sh both
```

第二步：编译项目
```bash
cd build
cmake -DFTAPI_HOME=/path/FTAPI4CPP_<version> \
      -DENABLE_FUTU=ON \
      -DFTAPI_BUILD_FROM_SOURCE=OFF \
      -DFTAPI_USE_ARM64=ON \
      -DCMAKE_BUILD_TYPE=Release \
      ..
make -j4
```

**特点:**
- ✅ 原生 ARM64 性能
- ✅ 快速编译（约 30秒）
- ✅ 支持 Debug/Release 独立库
- ℹ️ 需要先执行 `build_ftapi_arm64.sh`

### 模式 3: 使用 x86_64 预编译库（兼容模式）
```bash
cd build
cmake -DFTAPI_HOME=/path/FTAPI4CPP_<version> \
      -DENABLE_FUTU=ON \
      -DFTAPI_BUILD_FROM_SOURCE=OFF \
      -DFTAPI_USE_ARM64=OFF \
      ..
make -j4
```

**特点:**
- ✅ 简单快速，无需额外编译
- ✅ Intel Mac 原生运行
- ⚠️ Apple Silicon Mac 需要 Rosetta 2
- ⚠️ 性能略低于原生 ARM64

## CMake 选项说明

| 选项 | 默认值 | 说明 |
|------|--------|------|
| `FTAPI_BUILD_FROM_SOURCE` | ON | 是否从源码编译 FTAPI |
| `FTAPI_USE_ARM64` | OFF | 使用预编译的 ARM64 库 |
| `CMAKE_BUILD_TYPE` | Release | Debug 或 Release |

## 推荐工作流

### 开发阶段
使用**模式 1**（从源码编译），可以随时切换 Debug/Release：
```bash
# Debug 版本
cmake .. -DCMAKE_BUILD_TYPE=Debug
make -j4

# Release 版本
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j4
```

### 生产部署
1. 预编译 ARM64 库（只需一次）：
```bash
./build_ftapi_arm64.sh both
```

2. 使用**模式 2**（ARM64 预编译库）编译项目：
```bash
cmake -DFTAPI_HOME=/path/to/FTAPI \
      -DENABLE_FUTU=ON \
      -DFTAPI_BUILD_FROM_SOURCE=OFF \
      -DFTAPI_USE_ARM64=ON \
      -DCMAKE_BUILD_TYPE=Release \
      ..
make -j4
```

### 快速测试
使用**模式 3**（x86_64 预编译库），最快速：
```bash
cmake -DFTAPI_HOME=/path/to/FTAPI -DENABLE_FUTU=ON -DFTAPI_BUILD_FROM_SOURCE=OFF ..
make -j4
```

## 故障排查

### 问题：undefined symbols for architecture arm64
**解决方案：** 确保使用 ARM64 预编译库或从源码编译
```bash
# 选项 1: 从源码编译
cmake .. -DFTAPI_BUILD_FROM_SOURCE=ON

# 选项 2: 使用 ARM64 预编译库
./build_ftapi_arm64.sh both
cmake .. -DFTAPI_BUILD_FROM_SOURCE=OFF -DFTAPI_USE_ARM64=ON
```

### 问题：protobuf 版本不匹配
**解决方案：** 使用项目内置的 protobuf（自动处理）
```bash
cmake .. -DFTAPI_BUILD_FROM_SOURCE=ON  # 使用内置 protobuf 3.5.1
```

### 问题：找不到 ARM64 预编译库
**解决方案：** 先运行编译脚本
```bash
./build_ftapi_arm64.sh both
# 然后确认库文件存在
ls -la /path/ftapi_arm64/Release/lib/
```

## 性能对比

| 模式 | 架构 | 编译时间 | 运行性能 | 可执行文件 |
|------|------|----------|----------|------------|
| 模式 1 (源码) | ARM64 | 2-3分钟 | 100% | 12MB (Debug) |
| 模式 2 (ARM64库) | ARM64 | ~30秒 | 100% | 8MB (Release) |
| 模式 3 (x86_64库) | x86_64 | ~30秒 | ~75% (Rosetta 2) | 10MB |

注：运行性能仅供参考，实际性能取决于具体应用场景。

#!/bin/bash

echo "======================================"
echo "  Building Quant Trading System"
echo "======================================"
echo ""

# 解析命令行参数
ENABLE_FUTU=ON
ENABLE_IBKR=OFF
ENABLE_BINANCE=OFF
BUILD_TYPE=Release
FTAPI_HOME_ARG=""

while [[ $# -gt 0 ]]; do
    case $1 in
        --enable-futu)
            ENABLE_FUTU=ON
            shift
            ;;
        --disable-futu)
            ENABLE_FUTU=OFF
            shift
            ;;
        --enable-ibkr)
            ENABLE_IBKR=ON
            shift
            ;;
        --enable-binance)
            ENABLE_BINANCE=ON
            shift
            ;;
        --ftapi-home)
            FTAPI_HOME_ARG="$2"
            shift 2
            ;;
        --debug)
            BUILD_TYPE=Debug
            shift
            ;;
        --release)
            BUILD_TYPE=Release
            shift
            ;;
        --help)
            echo "Usage: $0 [options]"
            echo ""
            echo "Options:"
            echo "  --enable-futu              Enable Futu Exchange (default: ON)"
            echo "  --disable-futu             Disable Futu Exchange"
            echo "  --enable-ibkr              Enable IBKR Exchange (default: OFF)"
            echo "  --enable-binance           Enable Binance Exchange (default: OFF)"
            echo "  --ftapi-home <path>        Set FTAPI4CPP home directory"
            echo "  --debug                    Build in Debug mode"
            echo "  --release                  Build in Release mode (default)"
            echo "  --help                     Show this help message"
            echo ""
            echo "Examples:"
            echo "  $0                                         # Build (Release)"
            echo "  $0 --debug                                 # Build (Debug)"
            echo "  $0 --ftapi-home /path/to/FTAPI --debug     # Build with specific FTAPI path (Debug)"
            echo "  $0 --enable-ibkr --debug                   # Build with Futu and IBKR (Debug)"
            echo ""
            echo "Environment Variables:"
            echo "  FTAPI_HOME                 Alternative way to set FTAPI4CPP home directory"
            exit 0
            ;;
        *)
            echo "Unknown option: $1"
            echo "Use --help for usage information"
            exit 1
            ;;
    esac
done

echo "Exchange Configuration:"
echo "  Futu:    $ENABLE_FUTU"
echo "  IBKR:    $ENABLE_IBKR"
echo "  Binance: $ENABLE_BINANCE"
echo ""
echo "Build Configuration:"
echo "  Build Type: $BUILD_TYPE"

if [ "$ENABLE_FUTU" == "ON" ]; then
    echo ""
    echo "FTAPI Configuration:"
    echo "  Mode: x86_64 prebuilt libraries"
    echo "  Library: FTAPI4CPP/Bin (Rosetta 2 on Apple Silicon)"
fi
echo ""

# 创建构建目录
if [ ! -d "build" ]; then
    mkdir build
fi

cd build

# 构建 CMake 命令
CMAKE_CMD="cmake .. \
    -DCMAKE_BUILD_TYPE=$BUILD_TYPE \
    -DCMAKE_OSX_ARCHITECTURES=x86_64 \
    -DENABLE_FUTU=$ENABLE_FUTU \
    -DENABLE_IBKR=$ENABLE_IBKR \
    -DENABLE_BINANCE=$ENABLE_BINANCE"

# 添加 FTAPI_HOME 到 CMake 命令
if [ -n "$FTAPI_HOME_ARG" ]; then
    CMAKE_CMD="$CMAKE_CMD -DFTAPI_HOME=$FTAPI_HOME_ARG"
fi

# 运行CMake with exchange options
echo "Running CMake..."
eval $CMAKE_CMD

if [ $? -ne 0 ]; then
    echo "CMake configuration failed!"
    exit 1
fi

# 编译
echo "Compiling..."
make -j$(nproc)

if [ $? -ne 0 ]; then
    echo "Compilation failed!"
    exit 1
fi

echo ""
echo "======================================"
echo "  Build completed successfully!"
echo "======================================"
echo ""
echo "Run the system with:"
echo "  ./build/quant-trading-system config.json"
echo ""

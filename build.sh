#!/bin/bash

echo "======================================"
echo "  Building Futu Quant Trading System"
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
            echo "  $0                                              # Build with Futu in Release mode"
            echo "  $0 --debug                                      # Build with Futu in Debug mode"
            echo "  $0 --ftapi-home /path/to/FTAPI4CPP --debug      # Build with specific FTAPI path"
            echo "  $0 --enable-ibkr --debug                        # Build with Futu and IBKR in Debug mode"
            echo "  $0 --disable-futu --enable-ibkr                 # Build with IBKR in Release mode"
            echo ""
            echo "Environment Variables:"
            echo "  FTAPI_HOME                 Alternative way to set FTAPI4CPP home directory"
            echo ""
            echo "Examples with environment variables:"
            echo "  export FTAPI_HOME=/path/to/FTAPI4CPP && $0"
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

# 创建构建目录
if [ ! -d "build" ]; then
    mkdir build
fi

cd build

# 构建 CMake 命令
CMAKE_CMD="cmake .. \
    -DCMAKE_BUILD_TYPE=$BUILD_TYPE \
    -DENABLE_FUTU=$ENABLE_FUTU \
    -DENABLE_IBKR=$ENABLE_IBKR \
    -DENABLE_BINANCE=$ENABLE_BINANCE"

# 如果指定了 FTAPI_HOME，添加到 CMake 命令
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

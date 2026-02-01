cmake_minimum_required(VERSION 3.15)
project(QuantTradingSystem)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ========== 交易所编译开关 ==========
# 用户可以通过 cmake -DENABLE_FUTU=ON 来控制是否启用某个交易所
option(ENABLE_FUTU "Enable Futu Exchange Support" ON)
option(ENABLE_IBKR "Enable Interactive Brokers Exchange Support" OFF)
option(ENABLE_BINANCE "Enable Binance Exchange Support" OFF)

# Find required packages
find_package(Threads REQUIRED)

# nlohmann/json - prefer a checked-in git submodule under libraries/json
# If the submodule exists use it, otherwise fall back to FetchContent
if(EXISTS ${CMAKE_SOURCE_DIR}/libraries/json/CMakeLists.txt)
    message(STATUS "Using nlohmann/json from libraries/json submodule")
    add_subdirectory(${CMAKE_SOURCE_DIR}/libraries/json ${CMAKE_BINARY_DIR}/libraries_json_build EXCLUDE_FROM_ALL)
else()
    include(FetchContent)
    FetchContent_Declare(
        json
        URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz
    )
    FetchContent_MakeAvailable(json)
endif()

# Include directories
include_directories(${CMAKE_SOURCE_DIR}/include)

# 根据启用的交易所添加对应的include目录和link目录
if(ENABLE_FUTU)
    message(STATUS "Futu Exchange: ENABLED")
    add_definitions(-DENABLE_FUTU)
    
    # FTAPI4CPP 路径配置 - 支持 CMake 选项和环境变量
    if(NOT FTAPI_HOME)
        if(DEFINED ENV{FTAPI_HOME})
            set(FTAPI_HOME "$ENV{FTAPI_HOME}")
            message(STATUS "Using FTAPI_HOME from environment: ${FTAPI_HOME}")
        else()
            message(FATAL_ERROR "FTAPI_HOME not specified. Please use one of the following methods:\n"
                    "  1. cmake -DFTAPI_HOME=/path/to/FTAPI4CPP ..\n"
                    "  2. export FTAPI_HOME=/path/to/FTAPI4CPP && cmake ..\n"
                    "  3. ./build.sh --ftapi-home /path/to/FTAPI4CPP")
        endif()
    else()
        message(STATUS "Using FTAPI_HOME from CMake option: ${FTAPI_HOME}")
    endif()
    
    # 检查 FTAPI 是否存在
    if(NOT EXISTS ${FTAPI_HOME})
        message(FATAL_ERROR "FTAPI4CPP not found at ${FTAPI_HOME}. Please verify the path is correct.")
    endif()
    
    # 根据系统和编译类型选择合适的库路径
    if(APPLE)
        if(CMAKE_BUILD_TYPE MATCHES Debug)
            set(FTAPI_LIB_PATH "${FTAPI_HOME}/Bin/Mac/Debug")
        else()
            set(FTAPI_LIB_PATH "${FTAPI_HOME}/Bin/Mac/Release")
        endif()
    elseif(UNIX AND NOT APPLE)
        if(CMAKE_SYSTEM_NAME MATCHES "Linux")
            # 根据你的 Linux 版本选择 Ubuntu 或 CentOS
            if(EXISTS "${FTAPI_HOME}/Bin/Ubuntu16.04")
                set(FTAPI_LIB_PATH "${FTAPI_HOME}/Bin/Ubuntu16.04")
            else()
                set(FTAPI_LIB_PATH "${FTAPI_HOME}/Bin/Centos7")
            endif()
        endif()
    endif()
    
    # 添加 include 目录
    include_directories(${FTAPI_HOME}/Include)
    
    # 添加库文件目录
    link_directories(${FTAPI_LIB_PATH})
    
    message(STATUS "FTAPI Include: ${FTAPI_HOME}/Include")
    message(STATUS "FTAPI Lib Path: ${FTAPI_LIB_PATH}")
endif()

if(ENABLE_IBKR)
    message(STATUS "IBKR Exchange: ENABLED")
    add_definitions(-DENABLE_IBKR)
    include_directories(${CMAKE_SOURCE_DIR}/ibkr-api/include)
    link_directories(${CMAKE_SOURCE_DIR}/ibkr-api/lib)
endif()

if(ENABLE_BINANCE)
    message(STATUS "Binance Exchange: ENABLED")
    add_definitions(-DENABLE_BINANCE)
    include_directories(${CMAKE_SOURCE_DIR}/binance-api/include)
    link_directories(${CMAKE_SOURCE_DIR}/binance-api/lib)
endif()

# 核心源文件（总是编译）
set(CORE_SOURCES
    src/main.cpp
    src/config/config_manager.cpp
    src/managers/position_manager.cpp
    src/managers/risk_manager.cpp
    src/managers/strategy_manager.cpp
    src/scanner/market_scanner.cpp
    src/data/data_subscriber.cpp
    src/strategies/strategy_base.cpp
    src/strategies/momentum_strategy.cpp
    src/trading/order_executor.cpp
    src/exchange/exchange_factory.cpp
    src/exchange/exchange_manager.cpp
    src/event/event_engine.cpp
    src/utils/logger.cpp
)

# 根据启用的交易所添加对应的源文件
set(EXCHANGE_SOURCES "")

if(ENABLE_FUTU)
    list(APPEND EXCHANGE_SOURCES src/exchange/futu_exchange.cpp)
endif()

if(ENABLE_IBKR)
    list(APPEND EXCHANGE_SOURCES src/exchange/ibkr_exchange.cpp)
endif()

if(ENABLE_BINANCE)
    list(APPEND EXCHANGE_SOURCES src/exchange/binance_exchange.cpp)
endif()

# 合并所有源文件
set(SOURCES ${CORE_SOURCES} ${EXCHANGE_SOURCES})

# Create executable
add_executable(quant-trading-system ${SOURCES})

# 基础链接库
set(BASE_LIBRARIES
    Threads::Threads
    pthread
    dl
)

# 根据启用的交易所添加对应的链接库
set(EXCHANGE_LIBRARIES "")

if(ENABLE_FUTU)
    # 链接 FTAPI 需要的库：libFTAPI.a、libprotobuf.a、libFTAPIChannel.dylib
    list(APPEND EXCHANGE_LIBRARIES FTAPI protobuf FTAPIChannel)
endif()

if(ENABLE_IBKR)
    list(APPEND EXCHANGE_LIBRARIES TwsApi)  # TWS API library
endif()

if(ENABLE_BINANCE)
    list(APPEND EXCHANGE_LIBRARIES binance-cpprest)  # Binance API library
endif()

# Link libraries
target_link_libraries(quant-trading-system
    ${BASE_LIBRARIES}
    ${EXCHANGE_LIBRARIES}
    nlohmann_json::nlohmann_json
)

# Compiler options
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(quant-trading-system PRIVATE -Wall -Wextra -O3)
endif()

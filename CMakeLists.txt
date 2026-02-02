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

# 交易所配置
include(cmake/futu.cmake)
if(ENABLE_FUTU)
    include_directories(${FUTU_INCLUDE_DIRS})
    link_directories(${FUTU_LINK_DIRECTORIES})
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
    src/utils/stringsUtils.cpp
)

# 根据启用的交易所添加对应的源文件
set(EXCHANGE_SOURCES "")

if(ENABLE_FUTU)
    list(APPEND EXCHANGE_SOURCES ${FUTU_SOURCES})
endif()

if(ENABLE_IBKR)
    list(APPEND EXCHANGE_SOURCES src/exchange/ibkr_exchange.cpp)
endif()

if(ENABLE_BINANCE)
    list(APPEND EXCHANGE_SOURCES src/exchange/binance_exchange.cpp)
endif()

# 合并所有源文件
set(SOURCES ${CORE_SOURCES} ${EXCHANGE_SOURCES})

# 收集头文件（用于在 Visual Studio 中显示）
file(GLOB_RECURSE PROJECT_HEADERS
    ${CMAKE_SOURCE_DIR}/include/*.h
    ${CMAKE_SOURCE_DIR}/include/*.hpp
    ${CMAKE_SOURCE_DIR}/src/*.h
    ${CMAKE_SOURCE_DIR}/src/*.hpp
)
list(APPEND SOURCES ${PROJECT_HEADERS})

# 在 Visual Studio 中按目录树组织显示（只在 MSVC/VS 下生效）
if(MSVC)
    source_group(TREE ${CMAKE_SOURCE_DIR} FILES ${SOURCES})
endif()

# Create executable
add_executable(quant-trading-system ${SOURCES})

# 基础链接库
set(BASE_LIBRARIES Threads::Threads)
if(NOT WIN32)
    list(APPEND BASE_LIBRARIES pthread dl)
endif()

# 根据启用的交易所添加对应的链接库
set(EXCHANGE_LIBRARIES "")

if(ENABLE_FUTU)
    list(APPEND EXCHANGE_LIBRARIES ${FUTU_LIBRARIES})
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
    target_compile_options(quant-trading-system PRIVATE -Wall -Wextra $<$<CONFIG:Release>:-O3>)
elseif(MSVC)
    # 在 MSVC 中，/O2 与 Debug 模式下的 /RTC1 冲突
    target_compile_options(quant-trading-system PRIVATE /W4 /utf-8 $<$<CONFIG:Release>:/O2>)
endif()

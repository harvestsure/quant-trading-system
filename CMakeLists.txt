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

# dylib (library) - prefer checked-in submodule under libraries/dylib
if(EXISTS ${CMAKE_SOURCE_DIR}/libraries/dylib/CMakeLists.txt)
    message(STATUS "Adding dylib subproject from libraries/dylib")
    add_subdirectory(${CMAKE_SOURCE_DIR}/libraries/dylib ${CMAKE_BINARY_DIR}/libraries_dylib_build)
else()
    message(STATUS "dylib not found in libraries/dylib; skipping dylib subproject")
endif()


# cpp-httplib (header-only) - prefer checked-in submodule under libraries/cpp-httplib
# If the submodule exists, expose an INTERFACE target providing the include directory
if(EXISTS ${CMAKE_SOURCE_DIR}/libraries/cpp-httplib/httplib.h)
    message(STATUS "Using cpp-httplib from libraries/cpp-httplib submodule")
    add_library(cpp_httplib INTERFACE)
    target_include_directories(cpp_httplib INTERFACE ${CMAKE_SOURCE_DIR}/libraries/cpp-httplib)
else()
    message(STATUS "cpp-httplib not found in libraries/cpp-httplib; skipping header include")
endif()


# 交易所配置
include(cmake/futu.cmake)
if(ENABLE_FUTU)
    include_directories(${FUTU_INCLUDE_DIRS})
    link_directories(${FUTU_LINK_DIRECTORIES})
endif()

# ========== 全局编译选项 ==========
# 为了避免 ABI 不兼容，整个项目统一使用 libc++
# 但 FUTU 模块使用较低的 macOS 版本要求以兼容 FTAPI 的编译环境
if(APPLE)
    # 全局：使用 libc++ 标准库（所有模块统一）
    add_compile_options(
        "-stdlib=libc++"
    )
    add_link_options(
        "-stdlib=libc++"
    )
    message(STATUS "Global: using libc++ standard library for all modules")
endif()

# 基础库（不依赖交易所模块）
set(BASE_LIBS
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
    src/notification/notification_queue.cpp
    src/notification/telegram_sender.cpp
    src/notification/notification_manager.cpp
    src/utils/logger.cpp
    src/utils/stringsUtils.cpp
)

add_library(project_base_libs STATIC ${BASE_LIBS})
target_include_directories(project_base_libs PUBLIC ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(project_base_libs PUBLIC nlohmann_json::nlohmann_json)

if(TARGET dylib)
    target_link_libraries(project_base_libs PUBLIC dylib)
endif()

if(TARGET cpp_httplib)
    target_link_libraries(project_base_libs PUBLIC cpp_httplib)
endif()

# 核心源文件（总是编译）
set(CORE_SOURCES
    src/main.cpp
)

# 合并所有源文件
set(SOURCES ${CORE_SOURCES})

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

# Some configurations (OBJECT libraries / generator expressions) may leave
# CMake unable to infer the link language. Force it to C++ explicitly.
set_target_properties(quant-trading-system PROPERTIES LINKER_LANGUAGE CXX)

# 基础链接库
set(BASE_LIBRARIES Threads::Threads)
if(NOT WIN32)
    list(APPEND BASE_LIBRARIES pthread dl)
endif()

# Link libraries
target_link_libraries(quant-trading-system PRIVATE
    ${BASE_LIBRARIES}
    nlohmann_json::nlohmann_json
)

# Link the project base static library into the executable so `main` and
# all object files are included in the final link.
target_link_libraries(quant-trading-system PRIVATE project_base_libs)

# Compiler options
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(quant-trading-system PRIVATE -Wall -Wextra $<$<CONFIG:Release>:-O3>)
elseif(MSVC)
    # 在 MSVC 中，/O2 与 Debug 模式下的 /RTC1 冲突
    target_compile_options(quant-trading-system PRIVATE /W4 /utf-8 $<$<CONFIG:Release>:/O2>)
endif()

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

    set(FTAPI_SRC_DIR "${FTAPI_HOME}/Src")
    set(PROTO_SRC_DIR "${FTAPI_SRC_DIR}/protobuf-3.5.1")

    # --- 汇总变量供主工程使用 ---
    set(FUTU_SOURCES 
        src/exchange/futu_exchange.cpp
        src/exchange/futu_spi.cpp
    )
    
    if(WIN32)
        # Windows: 编译 FTAPI 和 Protobuf 源码
        
        # 1. 配置并引入 Protobuf
        set(protobuf_BUILD_TESTS OFF CACHE BOOL "Build tests" FORCE)
        set(protobuf_MSVC_STATIC_RUNTIME OFF CACHE BOOL "Link static runtime" FORCE)
        
        # 解决 VS 2022 对旧版 Protobuf (3.5.1) 的兼容性报错
        add_definitions(-D_SILENCE_STDEXT_HASH_DEPRECATION_WARNINGS)
        
        message(STATUS "Integrating Protobuf from source: ${PROTO_SRC_DIR}")
        add_subdirectory("${PROTO_SRC_DIR}/cmake" "${CMAKE_BINARY_DIR}/futu/protobuf" EXCLUDE_FROM_ALL)
        
        # 强制 libprotobuf 在 Debug 模式下也不带 'd' 后缀，以匹配 FTAPI 头文件中的 #pragma comment(lib, "libprotobuf.lib")
        set_target_properties(libprotobuf PROPERTIES DEBUG_POSTFIX "")

        # 2. 定义 FTAPI 库 (基于源码编译，以匹配当前的编译器和运行时)
        set(FUTU_INCLUDE_DIRS "${FTAPI_HOME}/Include")
        
        file(GLOB_RECURSE FTAPI_CORE_SOURCES 
            "${FTAPI_SRC_DIR}/FTAPI/*.cpp" 
            "${FTAPI_SRC_DIR}/FTAPI/*.cc"
            "${FUTU_INCLUDE_DIRS}/*.cpp"
            "${FUTU_INCLUDE_DIRS}/*.cc"
        )
        
        add_library(ftapi_compat STATIC ${FTAPI_CORE_SOURCES})
        # 设置输出名称为 FTAPI，以匹配头文件中的 #pragma comment(lib, "FTAPI.lib")
        set_target_properties(ftapi_compat PROPERTIES OUTPUT_NAME "FTAPI")
        target_include_directories(ftapi_compat PUBLIC ${FUTU_INCLUDE_DIRS})
        
        # 链接到刚才编译生成的 libprotobuf 目标
        # 使用 PUBLIC 确保依赖 ftapi_compat 的主工程也能自动获得 libprotobuf 的包含路径和链接信息
        # CMake 会根据目标属性自动在 Debug 模式下链接 libprotobufd.lib，Release 模式下链接 libprotobuf.lib
        target_link_libraries(ftapi_compat PUBLIC libprotobuf)
        
        # 3. 创建 FUTU wrapper 库（分开编译）
        add_library(futu_wrapper STATIC ${FUTU_SOURCES})
        target_include_directories(futu_wrapper 
            PRIVATE ${FUTU_INCLUDE_DIRS}
            PRIVATE ${CMAKE_SOURCE_DIR}/include
        )
        target_link_libraries(futu_wrapper PRIVATE ftapi_compat libprotobuf)
        
        if(CMAKE_SIZEOF_VOID_P EQUAL 8)
            set(FT_ARCH_DIR "Windows-x64")
        else()
            set(FT_ARCH_DIR "Windows")
        endif()

        if(CMAKE_BUILD_TYPE MATCHES Debug)
            set(FTAPI_CH_LIB_PATH "${FTAPI_HOME}/Bin/${FT_ARCH_DIR}/Debug")
        else()
            set(FTAPI_CH_LIB_PATH "${FTAPI_HOME}/Bin/${FT_ARCH_DIR}/Release")
        endif()

        find_library(FTAPI_CHANNEL_LIB FTAPIChannel PATHS ${FTAPI_CH_LIB_PATH} REQUIRED)
        set(FUTU_LIBRARIES futu_wrapper ftapi_compat ${FTAPI_CHANNEL_LIB} Ws2_32 Rpcrt4)
        
        message(STATUS "FUTU wrapper library created (separate compilation)")
        message(STATUS "FTAPI integration completed with differentiated compilation")
    else()
        # Linux/macOS: 使用预编译库
        # 关键：为了避免编译选项冲突，将 FUTU wrapper 代码单独编译成静态库
        # FUTU wrapper 使用旧的编译选项（兼容 FTAPI 的老平台），主程序使用 C++17/C++20
        
        if(APPLE)
            set(TARGET_OS "Mac")
        else()
            # Linux - Ubuntu 16.04 预编译库在较新系统上需要兼容性处理
            set(TARGET_OS "Ubuntu16.04")

            add_compile_options(-no-pie)
            add_link_options(-no-pie)
            
            message(STATUS "Applied Linux compatibility flags for Ubuntu 16 precompiled libraries")
        endif()
        
        set(FTAPI_BIN_DIR "${FTAPI_HOME}/Bin/${TARGET_OS}")

        # macOS: FTAPI 的预编译库按配置放在 Debug/Release 子目录中
        if(APPLE)
            set(FTAPI_BIN_DIR_DEBUG "${FTAPI_BIN_DIR}/Debug")
            set(FTAPI_BIN_DIR_RELEASE "${FTAPI_BIN_DIR}/Release")

            if(NOT EXISTS "${FTAPI_BIN_DIR_DEBUG}" AND NOT EXISTS "${FTAPI_BIN_DIR_RELEASE}")
                message(FATAL_ERROR "FTAPI precompiled libraries not found at ${FTAPI_BIN_DIR_DEBUG} or ${FTAPI_BIN_DIR_RELEASE}. Please verify FTAPI_HOME is correct.")
            endif()

            # 优先使用与当前构建类型匹配的目录，但在查找时提供两个目录以提高健壮性
            set(FTAPI_SEARCH_DIRS "${FTAPI_BIN_DIR_DEBUG}" "${FTAPI_BIN_DIR_RELEASE}")
        else()
            # Linux: 预编译库直接放在 FTAPI_BIN_DIR
            if(NOT EXISTS "${FTAPI_BIN_DIR}")
                message(FATAL_ERROR "FTAPI precompiled libraries not found at ${FTAPI_BIN_DIR}. Please verify FTAPI_HOME is correct.")
            endif()

            set(FTAPI_SEARCH_DIRS "${FTAPI_BIN_DIR}")
        endif()

        # 设置 FTAPI 头文件路径（必须，所有操作系统都需要）
        set(FUTU_INCLUDE_DIRS "${FTAPI_HOME}/Include")

        # 查找预编译库（macOS 会在 Debug/Release 两个子目录中查找）
        find_library(FTAPI_LIB FTAPI PATHS ${FTAPI_SEARCH_DIRS} REQUIRED)
        find_library(FTAPI_CHANNEL_LIB FTAPIChannel PATHS ${FTAPI_SEARCH_DIRS} REQUIRED)
        find_library(PROTOBUF_LIB protobuf PATHS ${FTAPI_SEARCH_DIRS} REQUIRED)

        # ========== 创建 FUTU wrapper 静态库（分开编译） ==========
        # 这个库包含 futu_exchange.cpp 和 futu_spi.cpp，使用与 FTAPI 兼容的编译选项
        # 注：不编译 .pb.cc，因为 FTAPI 预编译库已经包含了 protobuf 的完整实现
        
        add_library(futu_wrapper STATIC ${FUTU_SOURCES})
        
        # 设置包含目录
        target_include_directories(futu_wrapper 
            PRIVATE ${FUTU_INCLUDE_DIRS}
            PRIVATE ${CMAKE_SOURCE_DIR}/include
        )
        
        # *** 关键：仅为这个库应用旧的编译选项，不影响主程序 ***
        # 这样主程序可以使用 C++17/C++20，而 FUTU wrapper 使用兼容老平台的选项
        if(APPLE)
            # macOS: 使用最低系统版本以兼容 FTAPI 的编译环境
            # 注：标准库已在全局 CMakeLists.txt 中统一设置为 libc++，避免 ABI 不兼容
            target_compile_options(futu_wrapper PRIVATE
                "-mmacosx-version-min=10.13"
                "-fPIC"
            )
            target_link_options(futu_wrapper PRIVATE
                "-mmacosx-version-min=10.13"
            )
            message(STATUS "FUTU wrapper: using macOS 10.13 compatibility (libc++ unified)")
        else()
            # Linux: 应用兼容性编译选项仅针对这个库
            target_compile_options(futu_wrapper PRIVATE
                -no-pie
                -fno-stack-protector
            )
            target_link_options(futu_wrapper PRIVATE
                -no-pie
            )
            message(STATUS "FUTU wrapper: using Ubuntu 16.04 compatibility flags")
        endif()
        
        # 链接到 FTAPI 库
        target_link_libraries(futu_wrapper PRIVATE 
            ${FTAPI_LIB} 
            ${FTAPI_CHANNEL_LIB} 
            ${PROTOBUF_LIB}
        )
        
        # 将 FUTU wrapper 库和原始 FTAPI 库都返回给主程序链接
        set(FUTU_LIBRARIES futu_wrapper ${FTAPI_LIB} ${FTAPI_CHANNEL_LIB} ${PROTOBUF_LIB})
        
        message(STATUS "FTAPI include directory: ${FUTU_INCLUDE_DIRS}")
        message(STATUS "FUTU wrapper library created (separate compilation)")
        message(STATUS "FTAPI libraries: ${FUTU_LIBRARIES}")
    endif()

    message(STATUS "FTAPI integration completed as sub-projects")
endif()


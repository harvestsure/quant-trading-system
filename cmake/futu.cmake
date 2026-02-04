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
        
        # 3. 查找 FTAPIChannel DLL 库
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
        
        # 4. 创建 FUTU exchange 库（分开编译，动态库以支持动态加载）
        add_library(futu_exchange SHARED ${FUTU_SOURCES})
        target_include_directories(futu_exchange 
            PRIVATE ${FUTU_INCLUDE_DIRS}
            PRIVATE ${CMAKE_SOURCE_DIR}/include
        )
        
        # 链接所有必需的库：ftapi_compat、protobuf、项目基础库、Windows 系统库和 FTAPIChannel DLL
        target_link_libraries(futu_exchange 
            PRIVATE ftapi_compat 
            PRIVATE libprotobuf
            PRIVATE project_base_libs
            PRIVATE ${FTAPI_CHANNEL_LIB}
            PRIVATE Ws2_32
            PRIVATE Rpcrt4
        )
        
        # NOTE: futu_exchange 库将独立编译，主进程不链接
        set(FUTU_WRAPPER_LIB futu_exchange)
        set(FUTU_DEPENDENCIES ftapi_compat ${FTAPI_CHANNEL_LIB} Ws2_32 Rpcrt4)
        
        # FUTU_LIBRARIES 现在为空，主进程不链接
        set(FUTU_LIBRARIES "")
        
        message(STATUS "FUTU exchange library created (separate compilation, not linked to main executable)")
        message(STATUS "FTAPI integration completed with differentiated compilation")
    else()
        # Linux/macOS: 编译 FTAPI 源码（必须编译以生成 PIC 代码，用于动态库）
        # 理由：FTAPI 预编译库是非 PIC 的，无法链接到动态库
        # 编译源码可以用 -fPIC 重新编译，解决这个问题
        
        set(FUTU_INCLUDE_DIRS "${FTAPI_HOME}/Include")
        
        # 1. 编译 Protobuf（需要 -fPIC 以支持动态库）
        message(STATUS "Integrating Protobuf from source: ${PROTO_SRC_DIR}")
        
        # Linux/macOS 上编译 Protobuf 时必须使用 -fPIC
        if(APPLE)
            # macOS：使用 -fvisibility=hidden 隐藏 Protobuf 符号，避免与 libFTAPIChannel.dylib 的冲突
            set(CMAKE_CXX_FLAGS_SAVE "${CMAKE_CXX_FLAGS}")
            set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fPIC -fvisibility=hidden -mmacosx-version-min=10.13")
            message(STATUS "macOS Protobuf build: enabling symbol hiding with -fvisibility=hidden")
        else()
            # Linux
            set(CMAKE_CXX_FLAGS_SAVE "${CMAKE_CXX_FLAGS}")
            set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fPIC")
        endif()
        
        add_subdirectory("${PROTO_SRC_DIR}/cmake" "${CMAKE_BINARY_DIR}/futu/protobuf" EXCLUDE_FROM_ALL)
        
        # 恢复编译选项
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS_SAVE}")
        
        # macOS：为 libprotobuf 设置默认符号可见性为隐藏，确保不会导出与 FTAPI 冲突的符号
        if(APPLE AND TARGET libprotobuf)
            set_target_properties(libprotobuf PROPERTIES
                C_VISIBILITY_PRESET hidden
                CXX_VISIBILITY_PRESET hidden
                VISIBILITY_INLINES_HIDDEN ON
            )
            message(STATUS "Applied symbol hiding to libprotobuf on macOS")
        endif()
        
        # 2. 编译 FTAPI 源码（用 -fPIC 编译以支持动态库）
        file(GLOB_RECURSE FTAPI_CORE_SOURCES 
            "${FTAPI_SRC_DIR}/FTAPI/*.cpp" 
            "${FTAPI_SRC_DIR}/FTAPI/*.cc"
            "${FUTU_INCLUDE_DIRS}/*.cpp"
            "${FUTU_INCLUDE_DIRS}/*.cc"
        )
        
        add_library(ftapi_compat STATIC ${FTAPI_CORE_SOURCES})
        set_target_properties(ftapi_compat PROPERTIES OUTPUT_NAME "FTAPI")
        target_include_directories(ftapi_compat PUBLIC ${FUTU_INCLUDE_DIRS})
        
        # 关键：编译时添加 -fPIC，使静态库能链接到动态库
        target_compile_options(ftapi_compat PUBLIC -fPIC)
        # ftapi_compat 需要链接 libprotobuf，但 libprotobuf 的符号会在 futu_exchange.dylib 中隐藏
        target_link_libraries(ftapi_compat PUBLIC libprotobuf)
        
        # 3. 查找 FTAPIChannel 动态库
        if(APPLE)
            set(TARGET_OS "Mac")
            set(FTAPI_BIN_DIR "${FTAPI_HOME}/Bin/${TARGET_OS}")
            set(FTAPI_BIN_DIR_DEBUG "${FTAPI_BIN_DIR}/Debug")
            set(FTAPI_BIN_DIR_RELEASE "${FTAPI_BIN_DIR}/Release")
            if(NOT EXISTS "${FTAPI_BIN_DIR_DEBUG}" AND NOT EXISTS "${FTAPI_BIN_DIR_RELEASE}")
                message(FATAL_ERROR "FTAPI libraries not found at ${FTAPI_BIN_DIR_DEBUG} or ${FTAPI_BIN_DIR_RELEASE}")
            endif()
            set(FTAPI_SEARCH_DIRS "${FTAPI_BIN_DIR_DEBUG}" "${FTAPI_BIN_DIR_RELEASE}")
        else()
            set(TARGET_OS "Ubuntu16.04")
            set(FTAPI_BIN_DIR "${FTAPI_HOME}/Bin/${TARGET_OS}")
            if(NOT EXISTS "${FTAPI_BIN_DIR}")
                message(FATAL_ERROR "FTAPI libraries not found at ${FTAPI_BIN_DIR}")
            endif()
            set(FTAPI_SEARCH_DIRS "${FTAPI_BIN_DIR}")
            
            # Linux: 全局禁用 PIE 兼容老库
            add_compile_options(-no-pie)
            add_link_options(-no-pie)
            message(STATUS "Applied Linux compatibility flags: -no-pie")
        endif()
        
        find_library(FTAPI_CHANNEL_LIB FTAPIChannel PATHS ${FTAPI_SEARCH_DIRS} REQUIRED)
        find_library(PROTOBUF_PREBUILT_LIB protobuf PATHS ${FTAPI_SEARCH_DIRS})
        find_library(ZLIB_LIB z)
        
        # ========== 创建 FUTU exchange 动态库（分开编译，独立动态加载） ==========
        # 这个库包含 futu_exchange.cpp 和 futu_spi.cpp
        # 编译为动态库，主进程通过 dlopen 动态加载，不在编译时链接
        
        # 为了避免符号未定义，将项目的基础库源文件也编译到 futu_exchange 中
        file(GLOB BASE_LIB_SOURCES
            "${CMAKE_SOURCE_DIR}/src/utils/logger.cpp"
            "${CMAKE_SOURCE_DIR}/src/utils/stringsUtils.cpp"
        )
        
        add_library(futu_exchange SHARED ${FUTU_SOURCES} ${BASE_LIB_SOURCES})
        
        # 设置包含目录
        target_include_directories(futu_exchange 
            PRIVATE ${FUTU_INCLUDE_DIRS}
            PRIVATE ${CMAKE_SOURCE_DIR}/include
        )
        
        # *** 关键：为这个库应用适当的编译选项 ***
        if(APPLE)
            # macOS: 使用最低系统版本以兼容 FTAPI 的编译环境，同时隐藏 libprotobuf 符号避免冲突
            target_compile_options(futu_exchange PRIVATE
                "-mmacosx-version-min=10.13"
                "-fPIC"
                "-fvisibility=hidden"
            )
            target_link_options(futu_exchange PRIVATE
                "-mmacosx-version-min=10.13"
            )
            set_target_properties(futu_exchange PROPERTIES
                CXX_VISIBILITY_PRESET hidden
                VISIBILITY_INLINES_HIDDEN ON
            )
            message(STATUS "FUTU exchange: using macOS 10.13 + symbol hiding to prevent FTAPI conflicts")
        else()
            # Linux: 动态库需要 fPIC（但整体项目已经 -no-pie）
            target_compile_options(futu_exchange PRIVATE -fPIC)
            message(STATUS "FUTU exchange: using Linux compatibility flags (dynamic library with -fPIC)")
        endif()
        
        # 链接到编译的 FTAPI 库、预编译的 FTAPIChannel 和 libprotobuf
        # 注意：libprotobuf 会被静态链接到 futu_exchange.dylib 中（因为是 PUBLIC 依赖）
        # 并且符号会被隐藏，避免与 libFTAPIChannel.dylib 中的 Protobuf 冲突
        target_link_libraries(futu_exchange PRIVATE 
            ftapi_compat
            libprotobuf
            ${FTAPI_CHANNEL_LIB}
        )
        
        # macOS: 不使用 -undefined suppress（会导致符号找不到）
        # 而是使用 -flat_namespace 允许运行时符号解析
        if(APPLE)
            target_link_options(futu_exchange PRIVATE
                "-flat_namespace"
            )
        endif()
        
        # 链接系统库
        if(NOT APPLE)
            if(ZLIB_LIB)
                target_link_libraries(futu_exchange PRIVATE ${ZLIB_LIB})
            endif()
            target_link_libraries(futu_exchange PRIVATE pthread dl)
        else()
            if(ZLIB_LIB)
                target_link_libraries(futu_exchange PRIVATE ${ZLIB_LIB})
            endif()
        endif()
        
        # NOTE: futu_exchange 库将独立编译，主进程通过 dlopen 动态加载，不在编译时链接
        set(FUTU_WRAPPER_LIB futu_exchange)
        set(FUTU_DEPENDENCIES ftapi_compat ${FTAPI_CHANNEL_LIB})
        
        # FUTU_LIBRARIES 为空，主进程不链接（动态加载）
        set(FUTU_LIBRARIES "")
        
        message(STATUS "FTAPI include directory: ${FUTU_INCLUDE_DIRS}")
        message(STATUS "FUTU exchange library created (dynamic library, compiled with -fPIC source)")
        message(STATUS "FUTU exchange library: ${FUTU_WRAPPER_LIB}")
        message(STATUS "FUTU dependencies: ${FUTU_DEPENDENCIES}")
    endif()

    message(STATUS "FTAPI integration completed as sub-projects")
endif()


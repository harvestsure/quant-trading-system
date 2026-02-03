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
        
        add_library(futu_api STATIC ${FTAPI_CORE_SOURCES})
        # 设置输出名称为 FTAPI，以匹配头文件中的 #pragma comment(lib, "FTAPI.lib")
        set_target_properties(futu_api PROPERTIES OUTPUT_NAME "FTAPI")
        target_include_directories(futu_api PUBLIC ${FUTU_INCLUDE_DIRS})
        
        # 链接到刚才编译生成的 libprotobuf 目标
        # 使用 PUBLIC 确保依赖 futu_api 的主工程也能自动获得 libprotobuf 的包含路径和链接信息
        # CMake 会根据目标属性自动在 Debug 模式下链接 libprotobufd.lib，Release 模式下链接 libprotobuf.lib
        target_link_libraries(futu_api PUBLIC libprotobuf)
        
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
        set(FUTU_LIBRARIES futu_api ${FTAPI_CHANNEL_LIB} Ws2_32 Rpcrt4)
    else()
        # Linux/macOS: 使用预编译库
        if(APPLE)
            set(TARGET_OS "Mac")
        else()
            # Linux - Ubuntu 16.04 预编译库在较新系统上需要兼容性处理
            set(TARGET_OS "Ubuntu16.04")
            
            # 添加兼容性编译选项
            # -no-pie: 禁用 PIE (Position Independent Executable)，与 Ubuntu 16 编译的库兼容
            # -fno-stack-protector: 禁用栈保护，与旧版库兼容
            add_compile_options(-no-pie)
            add_link_options(-no-pie)
            
            message(STATUS "Applied Linux compatibility flags for Ubuntu 16 precompiled libraries")
        endif()
        
        set(FTAPI_BIN_DIR "${FTAPI_HOME}/Bin/${TARGET_OS}")
        
        # 检查预编译库是否存在
        if(NOT EXISTS "${FTAPI_BIN_DIR}")
            message(FATAL_ERROR "FTAPI precompiled libraries not found at ${FTAPI_BIN_DIR}. Please verify FTAPI_HOME is correct.")
        endif()
        
        # 设置 FTAPI 头文件路径（必须，所有操作系统都需要）
        set(FUTU_INCLUDE_DIRS "${FTAPI_HOME}/Include")
        
        # 查找预编译库
        find_library(FTAPI_LIB FTAPI PATHS "${FTAPI_BIN_DIR}" REQUIRED)
        find_library(FTAPI_CHANNEL_LIB FTAPIChannel PATHS "${FTAPI_BIN_DIR}" REQUIRED)
        find_library(PROTOBUF_LIB protobuf PATHS "${FTAPI_BIN_DIR}" REQUIRED)
        
        set(FUTU_LIBRARIES ${FTAPI_LIB} ${FTAPI_CHANNEL_LIB} ${PROTOBUF_LIB} pthread)
        
        message(STATUS "FTAPI include directory: ${FUTU_INCLUDE_DIRS}")
        message(STATUS "FTAPI libraries: ${FUTU_LIBRARIES}")
    endif()

    message(STATUS "FTAPI integration completed as sub-projects")
endif()


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
    elseif(WIN32)
        # 根据编译器设置选择 MD 或 MT
        if(MSVC_RUNTIME_LIBRARY MATCHES "MultiThreaded$|MultiThreadedDebug$")
            set(FT_RUNTIME "MT")
        else()
            set(FT_RUNTIME "MD") # 默认为 MD
        endif()

        # 根据架构选择目录
        if(CMAKE_SIZEOF_VOID_P EQUAL 8)
            set(FT_ARCH_DIR "Windows-x64")
        else()
            set(FT_ARCH_DIR "Windows")
        endif()

        # 使用生成器表达式或在配置时确定路径
        if(CMAKE_BUILD_TYPE MATCHES Debug)
            set(FTAPI_LIB_PATH "${FTAPI_HOME}/Bin/${FT_ARCH_DIR}/Debug/${FT_RUNTIME}")
            set(FTAPI_CH_LIB_PATH "${FTAPI_HOME}/Bin/${FT_ARCH_DIR}/Debug")
        else()
            set(FTAPI_LIB_PATH "${FTAPI_HOME}/Bin/${FT_ARCH_DIR}/Release/${FT_RUNTIME}")
            set(FTAPI_CH_LIB_PATH "${FTAPI_HOME}/Bin/${FT_ARCH_DIR}/Release")
        endif()
        
        list(APPEND FUTU_LINK_DIRECTORIES ${FTAPI_CH_LIB_PATH})
    endif()
    
    # 汇总变量
    set(FUTU_INCLUDE_DIRS ${FTAPI_HOME}/Include)
    list(APPEND FUTU_LINK_DIRECTORIES ${FTAPI_LIB_PATH})
    set(FUTU_SOURCES src/exchange/futu_exchange.cpp)
    
    if(WIN32)
        set(FUTU_LIBRARIES FTAPI libprotobuf FTAPIChannel Ws2_32 Rpcrt4)
    else()
        set(FUTU_LIBRARIES FTAPI protobuf FTAPIChannel)
    endif()

    message(STATUS "FTAPI Include: ${FUTU_INCLUDE_DIRS}")
    message(STATUS "FTAPI Lib Path: ${FTAPI_LIB_PATH}")
endif()

@echo off
setlocal enabledelayedexpansion

echo ======================================
echo   Building Futu Quant Trading System
echo ======================================
echo.

:: 解析命令行参数
set ENABLE_FUTU=ON
set ENABLE_IBKR=OFF
set ENABLE_BINANCE=OFF
set BUILD_TYPE=Release
set FTAPI_HOME_ARG=

:parse_args
if "%~1"=="" goto end_parse
if "%~1"=="--enable-futu" (
    set ENABLE_FUTU=ON
    shift
    goto parse_args
)
if "%~1"=="--disable-futu" (
    set ENABLE_FUTU=OFF
    shift
    goto parse_args
)
if "%~1"=="--enable-ibkr" (
    set ENABLE_IBKR=ON
    shift
    goto parse_args
)
if "%~1"=="--enable-binance" (
    set ENABLE_BINANCE=ON
    shift
    goto parse_args
)
if "%~1"=="--ftapi-home" (
    set "FTAPI_HOME_ARG=%~2"
    shift
    shift
    goto parse_args
)
if "%~1"=="--debug" (
    set BUILD_TYPE=Debug
    shift
    goto parse_args
)
if "%~1"=="--release" (
    set BUILD_TYPE=Release
    shift
    goto parse_args
)
if "%~1"=="--help" (
    echo Usage: build.bat [options]
    echo.
    echo Options:
    echo   --enable-futu              Enable Futu Exchange (default: ON^)
    echo   --disable-futu             Disable Futu Exchange
    echo   --enable-ibkr              Enable IBKR Exchange (default: OFF^)
    echo   --enable-binance           Enable Binance Exchange (default: OFF^)
    echo   --ftapi-home ^<path^>        Set FTAPI4CPP home directory
    echo   --debug                    Build in Debug mode
    echo   --release                  Build in Release mode (default^)
    echo   --help                     Show this help message
    echo.
    echo Examples:
    echo   build.bat                                              # Build with Futu in Release mode
    echo   build.bat --debug                                      # Build with Futu in Debug mode
    echo   build.bat --ftapi-home C:\path\to\FTAPI4CPP --debug      # Build with specific FTAPI path
    echo   build.bat --enable-ibkr --debug                        # Build with Futu and IBKR in Debug mode
    echo   build.bat --disable-futu --enable-ibkr                 # Build with IBKR in Release mode
    echo.
    echo Environment Variables:
    echo   FTAPI_HOME                 Alternative way to set FTAPI4CPP home directory
    echo.
    exit /b 0
)

echo Unknown option: %1
echo Use --help for usage information
exit /b 1

:end_parse

echo Exchange Configuration:
echo   Futu:    %ENABLE_FUTU%
echo   IBKR:    %ENABLE_IBKR%
echo   Binance: %ENABLE_BINANCE%
echo.
echo Build Configuration:
echo   Build Type: %BUILD_TYPE%

:: 创建构建目录
if not exist build (
    mkdir build
)

cd build

:: 构建 CMake 命令
set CMAKE_CMD=cmake .. ^
    -DCMAKE_BUILD_TYPE=%BUILD_TYPE% ^
    -DENABLE_FUTU=%ENABLE_FUTU% ^
    -DENABLE_IBKR=%ENABLE_IBKR% ^
    -DENABLE_BINANCE=%ENABLE_BINANCE%

:: 如果指定了 FTAPI_HOME，添加到 CMake 命令
if not "%FTAPI_HOME_ARG%"=="" (
    set CMAKE_CMD=%CMAKE_CMD% -DFTAPI_HOME="%FTAPI_HOME_ARG%"
)

:: 运行 CMake
echo Running CMake...
%CMAKE_CMD%

if %ERRORLEVEL% neq 0 (
    echo CMake configuration failed!
    cd ..
    exit /b 1
)

:: 编译
echo Compiling...
cmake --build . --config %BUILD_TYPE% --parallel

if %ERRORLEVEL% neq 0 (
    echo Compilation failed!
    cd ..
    exit /b 1
)

echo.
echo ======================================
echo   Build completed successfully!
echo ======================================
echo.
echo Run the system with:
echo   .\build\%BUILD_TYPE%\quant-trading-system.exe config.json
echo   (Note: path might vary depending on generator, e.g., .\build\quant-trading-system.exe^)
echo.

cd ..

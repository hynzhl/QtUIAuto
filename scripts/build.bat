@echo off
chcp 65001 >nul
setlocal enabledelayedexpansion

:: ============================================================
:: QU — 一键构建脚本
:: ============================================================

set "PROJECT_DIR=%~dp0.."
set "QT_DIR=D:\Qt\5.15.2\msvc2019_64"

:: 自动查找 VS
set "VS_DIR="
for %%v in (Professional Community Enterprise BuildTools) do (
    for %%y in (2022 2019 2017) do (
        set "TEST_VS=C:\Program Files (x86)\Microsoft Visual Studio\%%y\%%v"
        if exist "!TEST_VS!\VC\Auxiliary\Build\vcvarsall.bat" (
            set "VS_DIR=!TEST_VS!"
            goto :vs_found
        )
        set "TEST_VS=C:\Program Files\Microsoft Visual Studio\%%y\%%v"
        if exist "!TEST_VS!\VC\Auxiliary\Build\vcvarsall.bat" (
            set "VS_DIR=!TEST_VS!"
            goto :vs_found
        )
    )
)
:vs_found
if "%VS_DIR%"=="" (
    echo [ERROR] Visual Studio 2017/2019/2022 not found!
    pause & exit /b 1
)
set "VCVARSALL=!VS_DIR!\VC\Auxiliary\Build\vcvarsall.bat"
call "!VCVARSALL!" x64 >nul
if %ERRORLEVEL% NEQ 0 (
    echo [ERROR] vcvarsall.bat failed!
    pause & exit /b 1
)

:: 验证 CMake
where cmake >nul 2>&1
if %ERRORLEVEL% NEQ 0 (
    echo [ERROR] CMake not found in PATH!
    pause & exit /b 1
)

set "BUILD_DIR=%PROJECT_DIR%\build"

:: 配置 CMake
echo ============================================================
echo  Configuring QU (Release)...
echo ============================================================
cmake -S "%PROJECT_DIR%" -B "%BUILD_DIR%" ^
    -DCMAKE_BUILD_TYPE=Release ^
    -DBUILD_SPIKE=ON
if %ERRORLEVEL% NEQ 0 (
    echo [ERROR] CMake configuration failed!
    pause & exit /b 1
)

:: 编译
echo.
echo ============================================================
echo  Building QU...
echo ============================================================
cmake --build "%BUILD_DIR%" --config Release
if %ERRORLEVEL% NEQ 0 (
    echo [ERROR] Build failed!
    pause & exit /b 1
)

echo.
echo ============================================================
echo  Build complete!
echo  - Main app: %BUILD_DIR%\src\Release\QU.exe
echo  - Inject DLL: %BUILD_DIR%\inject\Release\QU_Inject.dll
echo  - Spike: %BUILD_DIR%\spike\Release\QU_Spike.exe
echo ============================================================
endlocal
pause

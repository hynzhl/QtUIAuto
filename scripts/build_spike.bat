@echo off
chcp 65001 >nul
setlocal enabledelayedexpansion
set "VS_DIR=C:\Program Files (x86)\Microsoft Visual Studio\2019\Community"
set "VCVARSALL=!VS_DIR!\VC\Auxiliary\Build\vcvarsall.bat"
call "!VCVARSALL!" x64 >nul
if errorlevel 1 (
    echo [ERROR] vcvarsall.bat failed
    pause
    exit /b 1
)
set "BUILD_DIR=e:\AI\QtUIAuto\build"
echo Configuring Spike...
cmake -S e:\AI\QtUIAuto -B %BUILD_DIR% -DCMAKE_BUILD_TYPE=Release -DBUILD_SPIKE=ON
if errorlevel 1 (
    echo [ERROR] CMake configure failed
    pause
    exit /b 1
)
echo Building Spike...
cmake --build %BUILD_DIR% --config Release --target QtUIAuto_Spike
if errorlevel 1 (
    echo [ERROR] Build failed
    pause
    exit /b 1
)
echo Build complete!
endlocal

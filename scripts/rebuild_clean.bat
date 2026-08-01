@echo off
set "VS_DIR=C:\Program Files (x86)\Microsoft Visual Studio\2019\Community"
call "%VS_DIR%\VC\Auxiliary\Build\vcvarsall.bat" x64 >nul
echo Clean building QU_Spike...
cmake --build E:\AI\QtUIAuto\build --target QU_Spike --clean-first --config Release
echo EXIT_CODE=%ERRORLEVEL%

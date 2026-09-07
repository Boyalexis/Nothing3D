@echo off
setlocal

set "PROJECT_ROOT=%~dp0.."
set "CMAKE_EXE=%PROJECT_ROOT%\tools\cmake-4.4.3-windows-x86_64\bin\cmake.exe"
set "QT_ROOT=%PROJECT_ROOT%\tools\Qt\6.10.3\msvc2022_64"
set "VULKAN_SDK=%PROJECT_ROOT%\tools\VulkanSDK\1.4.357.0"
set "VS_DEV_CMD=D:\DevTools\VSBuildTools\2022\Common7\Tools\VsDevCmd.bat"

call "%VS_DEV_CMD%" -arch=x64 -host_arch=x64
if errorlevel 1 exit /b %errorlevel%

"%CMAKE_EXE%" -S "%PROJECT_ROOT%" -B "%PROJECT_ROOT%\build" -G "Visual Studio 17 2022" -A x64 -DCMAKE_PREFIX_PATH="%QT_ROOT%"

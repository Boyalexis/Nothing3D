@echo off
setlocal

set "PROJECT_ROOT=%~dp0.."
set "QT_BIN=%PROJECT_ROOT%\tools\Qt\6.10.3\msvc2022_64\bin"

call "%~dp0build.bat"
if errorlevel 1 exit /b %errorlevel%

set "PATH=%QT_BIN%;%PATH%"
set "VK_LAYER_PATH=%PROJECT_ROOT%\tools\VulkanSDK\1.4.357.0\Bin"
cd /d "%PROJECT_ROOT%"
"%PROJECT_ROOT%\build\Debug\Nothing3D.exe" %*

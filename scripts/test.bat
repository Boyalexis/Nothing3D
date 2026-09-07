@echo off
setlocal

set "PROJECT_ROOT=%~dp0.."
set "CMAKE_EXE=%PROJECT_ROOT%\tools\cmake-4.4.3-windows-x86_64\bin\cmake.exe"
set "CTEST_EXE=%PROJECT_ROOT%\tools\cmake-4.4.3-windows-x86_64\bin\ctest.exe"

call "%~dp0build.bat"
if errorlevel 1 exit /b %errorlevel%

"%CTEST_EXE%" --test-dir "%PROJECT_ROOT%\build" -C Debug --output-on-failure

@echo off
setlocal

set "PROJECT_ROOT=%~dp0.."
set "CMAKE_EXE=%PROJECT_ROOT%\tools\cmake-4.4.3-windows-x86_64\bin\cmake.exe"

call "%~dp0configure.bat"
if errorlevel 1 exit /b %errorlevel%

"%CMAKE_EXE%" --build "%PROJECT_ROOT%\build" --config Debug

@echo off
setlocal
call "%~dp0configure.bat"
if errorlevel 1 exit /b %errorlevel%
"%~dp0..\tools\cmake-4.4.3-windows-x86_64\bin\cmake.exe" --build "%~dp0..\build" --config Release --target Nothing3D

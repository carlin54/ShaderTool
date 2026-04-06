@echo off
setlocal enabledelayedexpansion

REM Bundle ShaderTool + Qt on Windows using windeployqt.
REM Usage:
REM   scripts\deploy_windows.bat [path\to\ShaderTool.exe] [output_dir]
REM Example:
REM   scripts\deploy_windows.bat build\ShaderTool.exe dist\windows

set "EXE=%~1"
if "%EXE%"=="" set "EXE=build\ShaderTool.exe"
set "OUT=%~2"
if "%OUT%"=="" set "OUT=dist\windows"

if not exist "%EXE%" (
  echo error: executable not found: %EXE%
  echo hint: build first with: cmake -S . -B build ^&^& cmake --build build -j
  exit /b 1
)

where windeployqt >nul 2>nul
if errorlevel 1 (
  echo error: windeployqt not found in PATH.
  echo hint: add your Qt bin folder to PATH before running this script.
  exit /b 1
)

if exist "%OUT%" rmdir /s /q "%OUT%"
mkdir "%OUT%" || exit /b 1

copy /y "%EXE%" "%OUT%\ShaderTool.exe" >nul || exit /b 1

echo Running windeployqt...
windeployqt --release "%OUT%\ShaderTool.exe" || exit /b 1

REM Copy DXC next to app if available.
if defined SHADERTOOL_DXC (
  if exist "%SHADERTOOL_DXC%" (
    copy /y "%SHADERTOOL_DXC%" "%OUT%\dxc.exe" >nul
  )
)
if not exist "%OUT%\dxc.exe" (
  if defined VULKAN_SDK (
    if exist "%VULKAN_SDK%\Bin\dxc.exe" (
      copy /y "%VULKAN_SDK%\Bin\dxc.exe" "%OUT%\dxc.exe" >nul
    )
  )
)

echo Packaging complete: %OUT%
exit /b 0

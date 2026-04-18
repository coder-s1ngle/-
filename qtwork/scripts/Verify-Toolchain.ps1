$ErrorActionPreference = "Stop"

. (Join-Path $PSScriptRoot "Set-QtEnv.ps1") -EchoSummary

$WorkspaceRoot = Split-Path -Parent $PSScriptRoot
Set-Location $WorkspaceRoot

Write-Host ""
Write-Host "cmake:"
& cmake --version

Write-Host ""
Write-Host "ninja:"
& ninja --version

Write-Host ""
Write-Host "g++:"
& g++ --version

Write-Host ""
Write-Host "Qt version:"
Get-Content (Join-Path $env:QT_ROOT "mkspecs\\qconfig.pri") |
    Select-String "QT_VERSION|QT_EDITION|QT_GCC_MAJOR_VERSION|QT_GCC_MINOR_VERSION|QT_GCC_PATCH_VERSION"

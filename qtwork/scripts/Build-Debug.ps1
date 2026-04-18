$ErrorActionPreference = "Stop"

. (Join-Path $PSScriptRoot "Set-QtEnv.ps1")

$WorkspaceRoot = Split-Path -Parent $PSScriptRoot
Set-Location $WorkspaceRoot
$BinaryPath = Join-Path $WorkspaceRoot "build\debug-mingw\carrier_sync_lab.exe"
$DeployRoot = Split-Path -Parent $BinaryPath
$WinDeployQt = Join-Path $env:QT_ROOT "bin\windeployqt.exe"
$SdkPluginsRoot = Join-Path $env:QT_ROOT "plugins"
$BundledPlatforms = Join-Path $DeployRoot "platforms"
$MinGwBin = Join-Path $env:MINGW_ROOT "bin"
$RuntimeDlls = @(
    "libgcc_s_seh-1.dll",
    "libstdc++-6.dll",
    "libwinpthread-1.dll"
)

& cmake --preset windows-mingw-debug
& cmake --build --preset windows-mingw-debug

if (-not (Test-Path $BinaryPath)) {
    throw "Build completed but executable was not found: $BinaryPath"
}

if (-not (Test-Path $WinDeployQt)) {
    throw "windeployqt.exe was not found: $WinDeployQt"
}

& $WinDeployQt --force --no-translations $BinaryPath

New-Item -ItemType Directory -Force $BundledPlatforms | Out-Null
Copy-Item -Path (Join-Path $SdkPluginsRoot "platforms\*.dll") -Destination $BundledPlatforms -Force

foreach ($DllName in $RuntimeDlls) {
    $SourcePath = Join-Path $MinGwBin $DllName
    if (-not (Test-Path $SourcePath)) {
        throw "Missing MinGW runtime DLL: $SourcePath"
    }
    Copy-Item -Path $SourcePath -Destination $DeployRoot -Force
}

Write-Host ""
Write-Host "Build finished: $BinaryPath"

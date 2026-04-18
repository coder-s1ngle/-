param(
    [Parameter(ValueFromRemainingArguments = $true)]
    [string[]]$AppArgs
)

$ErrorActionPreference = "Stop"

. (Join-Path $PSScriptRoot "Set-QtEnv.ps1")

$WorkspaceRoot = Split-Path -Parent $PSScriptRoot
$BinaryPath = Join-Path $WorkspaceRoot "build\debug-mingw\carrier_sync_lab.exe"
$DeployRoot = Split-Path -Parent $BinaryPath
$BundledPlatforms = Join-Path $DeployRoot "platforms"
$SdkPluginsRoot = Join-Path $env:QT_ROOT "plugins"
$SdkPlatforms = Join-Path $SdkPluginsRoot "platforms"
Set-Location $WorkspaceRoot

if (-not (Test-Path $BinaryPath)) {
    & (Join-Path $PSScriptRoot "Build-Debug.ps1")
}

if (Test-Path $BundledPlatforms) {
    $env:QT_QPA_PLATFORM_PLUGIN_PATH = $BundledPlatforms
    $env:QT_PLUGIN_PATH = $DeployRoot
} else {
    $env:QT_QPA_PLATFORM_PLUGIN_PATH = $SdkPlatforms
    $env:QT_PLUGIN_PATH = $SdkPluginsRoot
}

& $BinaryPath @AppArgs

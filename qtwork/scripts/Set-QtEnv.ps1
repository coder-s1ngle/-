param(
    [switch]$EchoSummary
)

$ErrorActionPreference = "Stop"

$WorkspaceRoot = Split-Path -Parent $PSScriptRoot
$QtRoot = Join-Path $WorkspaceRoot "tools\Qt6\6.5.3\mingw_64"
$MinGwRoot = Join-Path $WorkspaceRoot "tools\Qt6\Tools\mingw1120_64\mingw64"
$VenvScripts = Join-Path $WorkspaceRoot ".venv\Scripts"

$RequiredPaths = @(
    $QtRoot,
    $MinGwRoot,
    $VenvScripts
)

foreach ($PathItem in $RequiredPaths) {
    if (-not (Test-Path $PathItem)) {
        throw "Missing required path: $PathItem"
    }
}

$QtBin = Join-Path $QtRoot "bin"
$MinGwBin = Join-Path $MinGwRoot "bin"

$env:QT_ROOT = $QtRoot
$env:MINGW_ROOT = $MinGwRoot
$env:CMAKE_PREFIX_PATH = $QtRoot
$PathItems = ($QtBin, $MinGwBin, $VenvScripts) + ($env:Path -split ";" | Where-Object { $_ })
$env:Path = (($PathItems | Select-Object -Unique) -join ";")

if ($EchoSummary) {
    Write-Host "WorkspaceRoot = $WorkspaceRoot"
    Write-Host "QT_ROOT       = $QtRoot"
    Write-Host "MINGW_ROOT    = $MinGwRoot"
    Write-Host "VenvScripts   = $VenvScripts"
}

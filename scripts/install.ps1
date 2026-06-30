# Registers the staged ds4vr driver tree with SteamVR via vrpathreg.
# Usage: .\scripts\install.ps1 [-DriverPath ...] [-SteamVrRoot ...]
param(
    [string]$DriverPath  = (Join-Path $PSScriptRoot ".." | Join-Path -ChildPath "build\ds4vr"),
    [string]$SteamVrRoot = (Join-Path ${env:ProgramFiles(x86)} "Steam\steamapps\common\SteamVR")
)

$ErrorActionPreference = "Stop"

if (-not (Test-Path $DriverPath)) {
    throw "Driver tree not found at $DriverPath. Run .\scripts\build.ps1 first."
}
$DriverPath = (Resolve-Path $DriverPath).Path

$vrpathreg = Join-Path $SteamVrRoot "bin\win64\vrpathreg.exe"
if (-not (Test-Path $vrpathreg)) {
    throw "vrpathreg.exe not found at $vrpathreg. Pass -SteamVrRoot pointing at your SteamVR install."
}

Write-Host "Registering driver tree: $DriverPath"
& $vrpathreg adddriver $DriverPath
if ($LASTEXITCODE -ne 0) { throw "vrpathreg adddriver failed" }

Write-Host ""
Write-Host "Done. Start SteamVR; two 'ds4vr' Touch controllers should appear."
Write-Host "To remove later: .\scripts\uninstall.ps1"

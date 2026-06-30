# Removes the ds4vr driver tree from SteamVR's registered driver list.
param(
    [string]$DriverPath  = (Join-Path $PSScriptRoot ".." | Join-Path -ChildPath "build\ds4vr"),
    [string]$SteamVrRoot = (Join-Path ${env:ProgramFiles(x86)} "Steam\steamapps\common\SteamVR")
)

$ErrorActionPreference = "Stop"

if (Test-Path $DriverPath) {
    $DriverPath = (Resolve-Path $DriverPath).Path
}

$vrpathreg = Join-Path $SteamVrRoot "bin\win64\vrpathreg.exe"
if (-not (Test-Path $vrpathreg)) {
    throw "vrpathreg.exe not found at $vrpathreg. Pass -SteamVrRoot pointing at your SteamVR install."
}

Write-Host "Removing driver tree: $DriverPath"
& $vrpathreg removedriver $DriverPath
if ($LASTEXITCODE -ne 0) { throw "vrpathreg removedriver failed" }

# Packages the built ds4vr driver into a release zip.
# Usage: .\scripts\package.ps1 [-BuildDir build] [-Config Release]
#
# Produces ds4vr-release.zip in the repo root containing:
#   ds4vr/                    <- driver tree (register this with vrpathreg)
#     driver.vrdrivermanifest
#     resources/...
#     bin/win64/driver_ds4vr.dll
#     ds4vr.ini               <- user-editable config
#   install.ps1               <- one-click install
#   uninstall.ps1             <- one-click uninstall
#   README.txt                <- quick-start instructions
param(
    [string]$BuildDir = (Join-Path $PSScriptRoot ".." | Join-Path -ChildPath "build"),
    [string]$Config   = "Release"
)

$ErrorActionPreference = "Stop"

$repoRoot  = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$stageDir  = Join-Path $BuildDir "ds4vr"
$dllPath   = Join-Path $stageDir "bin\win64\driver_ds4vr.dll"

if (-not (Test-Path $dllPath)) {
    throw "Driver DLL not found at $dllPath. Run .\scripts\build.ps1 first."
}

# --- Assemble a clean staging area for the zip ---
$pkgDir = Join-Path $BuildDir "package"
if ((Test-Path $pkgDir) -and (Test-Path (Join-Path $pkgDir "ds4vr"))) {
    Remove-Item -Recurse -Force $pkgDir
} elseif (Test-Path $pkgDir) {
    throw "Unexpected contents in $pkgDir -refusing to delete. Remove it manually."
}
New-Item -ItemType Directory -Path $pkgDir | Out-Null

# Copy the driver tree (already staged by CMake post-build).
Copy-Item -Recurse -Path $stageDir -Destination (Join-Path $pkgDir "ds4vr")

# Write the user-facing install script.
$installScript = @'
# ds4vr -Install
# Run this script to register the driver with SteamVR.
# Requires SteamVR to be installed. Restart SteamVR after running.

$ErrorActionPreference = "Stop"
$driverPath = Join-Path $PSScriptRoot "ds4vr"

if (-not (Test-Path (Join-Path $driverPath "driver.vrdrivermanifest"))) {
    throw "driver.vrdrivermanifest not found. Make sure this script is next to the ds4vr/ folder."
}

# Locate vrpathreg.exe
$steamVrPaths = @(
    (Join-Path ${env:ProgramFiles(x86)} "Steam\steamapps\common\SteamVR")
    (Join-Path $env:ProgramFiles "Steam\steamapps\common\SteamVR")
)
$vrpathreg = $null
foreach ($p in $steamVrPaths) {
    $candidate = Join-Path $p "bin\win64\vrpathreg.exe"
    if (Test-Path $candidate) { $vrpathreg = $candidate; break }
}
if (-not $vrpathreg) {
    throw "vrpathreg.exe not found. Is SteamVR installed?"
}

$absPath = (Resolve-Path $driverPath).Path
Write-Host "Registering ds4vr driver: $absPath"
& $vrpathreg adddriver $absPath
if ($LASTEXITCODE -ne 0) { throw "vrpathreg adddriver failed (exit $LASTEXITCODE)" }

Write-Host ""
Write-Host "Done! Start (or restart) SteamVR."
Write-Host "Edit ds4vr\ds4vr.ini to change settings, then restart SteamVR."
Write-Host ""
Write-Host "IMPORTANT: Do not move or delete this folder -SteamVR loads"
Write-Host "the driver from this location. Run uninstall.ps1 before moving."
Write-Host ""
Write-Host "To uninstall later, run uninstall.ps1"
Read-Host "Press Enter to close"
'@

$uninstallScript = @'
# ds4vr -Uninstall
# Removes the ds4vr driver registration from SteamVR.

$ErrorActionPreference = "Stop"
$driverPath = Join-Path $PSScriptRoot "ds4vr"

$steamVrPaths = @(
    (Join-Path ${env:ProgramFiles(x86)} "Steam\steamapps\common\SteamVR")
    (Join-Path $env:ProgramFiles "Steam\steamapps\common\SteamVR")
)
$vrpathreg = $null
foreach ($p in $steamVrPaths) {
    $candidate = Join-Path $p "bin\win64\vrpathreg.exe"
    if (Test-Path $candidate) { $vrpathreg = $candidate; break }
}
if (-not $vrpathreg) {
    throw "vrpathreg.exe not found. Is SteamVR installed?"
}

$absPath = (Resolve-Path $driverPath).Path
Write-Host "Removing ds4vr driver: $absPath"
& $vrpathreg removedriver $absPath

Write-Host "Done. Restart SteamVR to complete removal."
Read-Host "Press Enter to close"
'@

$readmeText = @'
ds4vr -DualShock 4 / DualSense to SteamVR Touch Controller Driver
===================================================================

Presents a DS4 or DualSense gamepad as two emulated Oculus Touch
controllers with gyro-based 3DoF aiming via the shoulder bumpers.

INSTALL
-------
1. Extract this zip anywhere (e.g. Desktop, Documents, etc.).
2. Right-click install.ps1 -> "Run with PowerShell".
3. Start SteamVR. Two ds4vr controllers should appear.
4. Plug in your DS4 or DualSense via USB.

CONTROLS
--------
  L1 / R1 (bumpers)     Hold to aim that hand (gyro-driven).
                         Hold both = clutch (reposition controller).
                         Double-tap = reset hand to side.

  L2 / R2 (triggers)    Trigger (analog).
  Left / Right stick    Joystick (captured for reach while aiming).
  D-pad (left hand)     Up=Y, Left=X, Right=Grip (configurable).
  Face btns (right)     Cross=A, Circle=B, Square=Grip (configurable).
  Share / Options       System/menu button (left / right).

CONFIGURATION
-------------
Edit ds4vr\ds4vr.ini and restart SteamVR. All arm model, reach,
deadzone, button mapping, and haptic settings are tunable.

UNINSTALL
---------
Right-click uninstall.ps1 -> "Run with PowerShell".
Then restart SteamVR (or just close it).
You can delete the extracted folder afterward.
'@

Set-Content -Path (Join-Path $pkgDir "install.ps1")   -Value $installScript   -Encoding UTF8
Set-Content -Path (Join-Path $pkgDir "uninstall.ps1") -Value $uninstallScript -Encoding UTF8
Set-Content -Path (Join-Path $pkgDir "README.txt")    -Value $readmeText      -Encoding UTF8

# --- Zip it ---
$zipPath = Join-Path $repoRoot "ds4vr-release.zip"
if (Test-Path $zipPath) { Remove-Item -Force $zipPath }

Compress-Archive -Path (Join-Path $pkgDir "*") -DestinationPath $zipPath -CompressionLevel Optimal

Write-Host ""
Write-Host "Package created: $zipPath"
Write-Host "Contents:"
$pkgAbs = (Resolve-Path $pkgDir).Path
Get-ChildItem -Recurse $pkgAbs | ForEach-Object {
    $rel = $_.FullName.Substring($pkgAbs.Length + 1)
    if ($_.PSIsContainer) { Write-Host "  $rel/" } else { Write-Host "  $rel  ($([math]::Round($_.Length/1KB, 1)) KB)" }
}

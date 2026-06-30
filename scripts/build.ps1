# Configures and builds the ds4vr driver against the free Microsoft toolchain
# (Visual Studio Build Tools 2022 — no IDE required).
#
# Locates the toolchain with vswhere, imports the developer environment so
# cl.exe + the Windows SDK are on PATH, then drives CMake. Prefers Ninja
# (single-config, faster) when available and falls back to the VS multi-config
# generator otherwise.
#
# Usage: .\scripts\build.ps1                       # auto-detect generator, Release
#        .\scripts\build.ps1 -Config Debug
#        .\scripts\build.ps1 -Generator VS         # force VS generator
#        .\scripts\build.ps1 -Clean                # wipe build dir first
param(
    [string]$BuildDir  = (Join-Path $PSScriptRoot ".." | Join-Path -ChildPath "build"),
    [ValidateSet("Release","Debug","RelWithDebInfo","MinSizeRel")]
    [string]$Config    = "Release",
    [ValidateSet("auto","ninja","VS")]
    [string]$Generator = "auto",
    [switch]$Clean
)

$ErrorActionPreference = "Stop"

$InstallHint = @"
Install Visual Studio Build Tools 2022 (free, no IDE) via winget:

    winget install --id Microsoft.VisualStudio.2022.BuildTools `
        --override "--quiet --wait --norestart --add Microsoft.VisualStudio.Workload.VCTools --includeRecommended"

Or download manually: https://visualstudio.microsoft.com/downloads/?q=build+tools
"@

function Find-VsInstall {
    $vswhere = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\Installer\vswhere.exe"
    if (-not (Test-Path $vswhere)) {
        throw "vswhere.exe not found at $vswhere.`n$InstallHint"
    }
    $vsPath = & $vswhere -latest -products * `
        -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 `
        -property installationPath
    if (-not $vsPath) {
        throw "No Visual Studio 2022 install with C++ tools found.`n$InstallHint"
    }
    return ($vsPath | Select-Object -First 1).Trim()
}

function Import-VsDevEnv([string]$vsPath) {
    # Use vcvarsall.bat (lighter than VsDevCmd) to set up the MSVC environment.
    $vcvarsall = Join-Path $vsPath "VC\Auxiliary\Build\vcvarsall.bat"
    if (-not (Test-Path $vcvarsall)) {
        throw "vcvarsall.bat not found at $vcvarsall"
    }

    # vcvarsall.bat appends to PATH. If the current PATH is already near
    # cmd.exe's ~8191-char per-variable limit, the SET inside vcvarsall
    # overflows and you get "The input line is too long." Fix: start the
    # child with a minimal PATH containing only the Windows essentials.
    # vcvarsall will add the MSVC/SDK dirs itself; we merge its output back
    # into our full process env afterward.
    $minimalPath = @(
        "$env:SystemRoot\system32"
        "$env:SystemRoot"
        "$env:SystemRoot\System32\Wbem"
    ) -join ";"

    $psi = New-Object System.Diagnostics.ProcessStartInfo
    $psi.FileName               = "cmd.exe"
    $psi.Arguments              = "/c `"`"$vcvarsall`" x64 && set`""
    $psi.UseShellExecute        = $false
    $psi.RedirectStandardOutput = $true
    $psi.RedirectStandardError  = $true
    $psi.CreateNoWindow         = $true
    # Inherit current env then override PATH to the minimal one.
    $psi.Environment["PATH"]    = $minimalPath

    $proc = [System.Diagnostics.Process]::Start($psi)
    $stdout = $proc.StandardOutput.ReadToEnd()
    $proc.WaitForExit()

    if ($proc.ExitCode -ne 0) {
        $stderr = $proc.StandardError.ReadToEnd()
        throw "vcvarsall.bat failed (exit $($proc.ExitCode)): $stderr"
    }

    # Collect the child's env. We'll merge PATH specially: take all the
    # entries vcvarsall added (relative to the minimal PATH we gave it)
    # and prepend them to our real process PATH.
    $childPath  = $null
    $childVars  = @{}
    $count      = 0
    foreach ($line in $stdout -split "`r?`n") {
        if ($line -match '^([^=]+)=(.*)$') {
            $name  = $matches[1]
            $value = $matches[2]
            if ($name -ieq "PATH") {
                $childPath = $value
            } else {
                $childVars[$name] = $value
            }
            $count++
        }
    }
    if ($count -eq 0) { throw "No env vars captured from vcvarsall" }

    # Apply non-PATH vars.
    foreach ($kv in $childVars.GetEnumerator()) {
        [Environment]::SetEnvironmentVariable($kv.Key, $kv.Value, 'Process')
    }

    # Merge PATH: entries vcvarsall added (not in our minimal set) go in
    # front of the current process PATH, avoiding duplicates.
    if ($childPath) {
        $minimalSet  = [System.Collections.Generic.HashSet[string]]::new(
            $minimalPath -split ";",
            [StringComparer]::OrdinalIgnoreCase)
        $newEntries  = ($childPath -split ";") | Where-Object {
            $_ -ne "" -and -not $minimalSet.Contains($_)
        }
        $currentPath = $env:PATH
        $merged      = (($newEntries -join ";") + ";" + $currentPath).TrimEnd(";")
        [Environment]::SetEnvironmentVariable("PATH", $merged, 'Process')
    }
}

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path

if ($Clean -and (Test-Path $BuildDir)) {
    Write-Host "Cleaning $BuildDir"
    Remove-Item -Recurse -Force $BuildDir
}
if (-not (Test-Path $BuildDir)) {
    New-Item -ItemType Directory -Path $BuildDir | Out-Null
}
$BuildDir = (Resolve-Path $BuildDir).Path

# Verify CMake itself is reachable. (Build Tools ships a copy under
# Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin which VsDevCmd
# normally adds to PATH; a separately-installed CMake also works.)
if (-not (Get-Command cmake.exe -ErrorAction SilentlyContinue)) {
    # Try after VsDevCmd import below
    $needCmakeRecheck = $true
}

$vsPath = Find-VsInstall
Write-Host "Using VS install: $vsPath"
Import-VsDevEnv $vsPath

if (-not (Get-Command cmake.exe -ErrorAction SilentlyContinue)) {
    throw @"
cmake.exe not on PATH even after sourcing VsDevCmd.
Install CMake (the Build Tools 'C++ CMake tools for Windows' component
should include it) or grab a standalone copy:
    winget install --id Kitware.CMake
"@
}

if ($Generator -eq "auto") {
    $Generator = if (Get-Command ninja.exe -ErrorAction SilentlyContinue) { "ninja" } else { "VS" }
}

# Auto-wipe the build dir if a previous run configured it with a different
# generator (CMake refuses to reuse such caches). This is local + reversible
# (just regenerated on next configure), so we do it automatically with a warning.
$wantGen = if ($Generator -eq "ninja") { "Ninja" } else { "Visual Studio 17 2022" }
$cache = Join-Path $BuildDir "CMakeCache.txt"
if (Test-Path $cache) {
    $match = Select-String -LiteralPath $cache -Pattern '^CMAKE_GENERATOR:INTERNAL=(.*)$' | Select-Object -First 1
    if ($match) {
        $cachedGen = $match.Matches[0].Groups[1].Value
        if ($cachedGen -ne $wantGen) {
            Write-Warning "Build dir was configured with '$cachedGen'; reconfiguring with '$wantGen'. Wiping $BuildDir."
            Remove-Item -Recurse -Force $BuildDir
            New-Item -ItemType Directory -Path $BuildDir | Out-Null
            $BuildDir = (Resolve-Path $BuildDir).Path
        }
    }
}

if ($Generator -eq "ninja") {
    Write-Host "Configuring with Ninja ($Config)"
    & cmake -S $repoRoot -B $BuildDir -G Ninja "-DCMAKE_BUILD_TYPE=$Config"
    if ($LASTEXITCODE -ne 0) { throw "cmake configure failed" }
    & cmake --build $BuildDir
    if ($LASTEXITCODE -ne 0) { throw "cmake build failed" }
} else {
    Write-Host "Configuring with Visual Studio 17 2022 generator ($Config)"
    & cmake -S $repoRoot -B $BuildDir -G "Visual Studio 17 2022" -A x64
    if ($LASTEXITCODE -ne 0) { throw "cmake configure failed" }
    & cmake --build $BuildDir --config $Config
    if ($LASTEXITCODE -ne 0) { throw "cmake build failed" }
}

$stage = Join-Path $BuildDir "ds4vr"
Write-Host ""
Write-Host "Staged driver tree: $stage"
Write-Host "Register with SteamVR: .\scripts\install.ps1"

<#
.SYNOPSIS
    Build MegaDirStat and launch it, in Debug or Release.

.DESCRIPTION
    Closes a running instance, configures if needed, builds one build preset,
    then starts the binary with Qt's DLL directory on PATH. The configuration is
    read out of CMakePresets.json, so an added preset needs no edit here.

.PARAMETER Config
    Debug (default) or Release. Shorthand for -Preset msvc-debug / msvc-release.

.PARAMETER Preset
    Build preset name, for anything that is not the two above. Overrides -Config.

.PARAMETER Theme
    light | dark | system -- sets MEGADIRSTAT_COLOR_SCHEME for this run only.

.PARAMETER AppArgs
    Arguments forwarded to MegaDirStat.exe, e.g. '--mock-generate','50000'.

.EXAMPLE
    scripts\run.ps1 -AppArgs '--mock-generate','20000'
.EXAMPLE
    scripts\run.ps1 -Config Release -Theme dark -AppArgs '--mock','tests\fixtures\sample.json'
#>
[CmdletBinding()]
param(
    [ValidateSet('Debug', 'Release')]
    [string]$Config = 'Debug',

    [string]$Preset,

    [switch]$Reconfigure,
    [switch]$NoBuild,
    [switch]$NoRun,

    [ValidateSet('light', 'dark', 'system')]
    [string]$Theme,

    [string[]]$AppArgs = @()
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$RepoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path

# The `cmake` on PATH is Strawberry Perl's 3.29: too old for this MSVC, and it
# overwrites CMakeCache.txt before failing. Always the full path.
$CMake = if ($env:MEGADIRSTAT_CMAKE) { $env:MEGADIRSTAT_CMAKE } else { 'C:/Qt/Tools/CMake_64/bin/cmake.exe' }
if (-not (Test-Path $CMake)) { throw "cmake not found at $CMake (override with `$env:MEGADIRSTAT_CMAKE)" }

$buildPreset = if ($Preset) { $Preset } else { 'msvc-' + $Config.ToLower() }
$presets = Get-Content -Raw -LiteralPath (Join-Path $RepoRoot 'CMakePresets.json') | ConvertFrom-Json
$bp = $presets.buildPresets | Where-Object { $_.name -eq $buildPreset }
if (-not $bp) {
    $known = ($presets.buildPresets | ForEach-Object { $_.name }) -join ', '
    throw "build preset '$buildPreset' is not in CMakePresets.json (known: $known)"
}
$configurePreset = $bp.configurePreset
$configuration = if ($bp.PSObject.Properties['configuration']) { $bp.configuration } else { $Config }

# binaryDir is named after the *configure* preset, so a Release build lands
# under build/msvc-debug/Release.
$buildDir = Join-Path $RepoRoot "build/$configurePreset"
$exePath = Join-Path $buildDir "$configuration/MegaDirStat.exe"

if (-not $NoBuild) {
    # A running MegaDirStat.exe holds its own .exe open; the link dies LNK1104.
    $running = Get-Process -Name MegaDirStat -ErrorAction SilentlyContinue
    if ($running) {
        $running | Stop-Process -Force
        $running | Wait-Process -Timeout 10 -ErrorAction SilentlyContinue
    }

    if ($Reconfigure -or -not (Test-Path (Join-Path $buildDir 'CMakeCache.txt'))) {
        & $CMake --preset $configurePreset
        if ($LASTEXITCODE -ne 0) { throw "configure failed ($LASTEXITCODE)" }
    }
    & $CMake --build --preset $buildPreset
    if ($LASTEXITCODE -ne 0) { throw "build failed ($LASTEXITCODE)" }
}

if ($NoRun) { return }
if (-not (Test-Path $exePath)) { throw "$exePath not found -- build first" }

$qtBin = if ($env:MEGADIRSTAT_QT_DIR) { Join-Path $env:MEGADIRSTAT_QT_DIR 'bin' } else { 'C:/Qt/6.11.1/msvc2022_64/bin' }
$dirs = @($qtBin)
# Only present once the MEGA SDK (and so vcpkg) is part of the build.
$vcpkgBin = Join-Path $buildDir ('vcpkg_installed/x64-windows-mega/' + $(if ($configuration -eq 'Debug') { 'debug/bin' } else { 'bin' }))
if (Test-Path $vcpkgBin) { $dirs += $vcpkgBin }
$env:PATH = ($dirs -join ';') + ";$env:PATH"

if ($Theme) {
    $env:MEGADIRSTAT_COLOR_SCHEME = if ($Theme -eq 'system') { $null } else { $Theme }
}

$startArgs = @{ FilePath = $exePath; WorkingDirectory = $RepoRoot }
if ($AppArgs.Count -gt 0) { $startArgs.ArgumentList = $AppArgs }
Start-Process @startArgs

<#
.SYNOPSIS
    Build MegaDirStat in Release, zip it, and check the zip works on its own.

.DESCRIPTION
    Release build -> CPack's ZIP (the install rules deploy Qt through
    windeployqt and add the MSVC runtime) -> check the archive holds what a
    machine without Qt needs -> unpack it and launch the exe from there, with
    Qt's directories stripped from PATH, until a window appears.

    Every failure this guards against -- a missing Qt DLL or plugin, a missing
    licence file -- builds and packages cleanly and then dies on someone else's
    desktop.

.PARAMETER SkipBuild
    Package whatever is already built. Fails if the binary is missing.

.EXAMPLE
    scripts\package.ps1
#>
[CmdletBinding()]
param(
    [switch]$SkipBuild
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$RepoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path

# The `cmake` on PATH is Strawberry Perl's 3.29: too old for this MSVC, and it
# overwrites CMakeCache.txt before failing. Always the full path.
$CMake = if ($env:MEGADIRSTAT_CMAKE) { $env:MEGADIRSTAT_CMAKE } else { 'C:/Qt/Tools/CMake_64/bin/cmake.exe' }
if (-not (Test-Path $CMake)) { throw "cmake not found at $CMake (override with `$env:MEGADIRSTAT_CMAKE)" }
$CPack = Join-Path (Split-Path $CMake) 'cpack.exe'
if (-not (Test-Path $CPack)) { throw "cpack not found at $CPack" }

# binaryDir is named after the *configure* preset, so Release lands under
# build/msvc-debug/Release.
$buildDir = Join-Path $RepoRoot 'build/msvc-debug'
$exePath = Join-Path $buildDir 'Release/MegaDirStat.exe'

if (-not $SkipBuild) {
    # A running MegaDirStat.exe holds its own .exe open; the link dies LNK1104.
    $running = Get-Process -Name MegaDirStat -ErrorAction SilentlyContinue
    if ($running) {
        $running | Stop-Process -Force
        $running | Wait-Process -Timeout 10 -ErrorAction SilentlyContinue
    }
    if (-not (Test-Path (Join-Path $buildDir 'CMakeCache.txt'))) {
        & $CMake --preset msvc-debug
        if ($LASTEXITCODE -ne 0) { throw "configure failed ($LASTEXITCODE)" }
    }
    & $CMake --build --preset msvc-release --target MegaDirStat
    if ($LASTEXITCODE -ne 0) { throw "build failed ($LASTEXITCODE)" }
}
if (-not (Test-Path $exePath)) { throw "$exePath not found -- build first" }

# --------------------------------------------------------------------- package
$outDir = Join-Path $buildDir 'package'
# CPack only ever adds to this directory; an older version's zip left in it
# would be the one picked below if this run produced a differently named one.
Remove-Item -Path (Join-Path $outDir '*.zip') -Force -ErrorAction SilentlyContinue
& $CPack --config (Join-Path $buildDir 'CPackConfig.cmake') -C Release -B $outDir
if ($LASTEXITCODE -ne 0) { throw "cpack failed ($LASTEXITCODE)" }

$zip = Get-ChildItem -LiteralPath $outDir -Filter '*.zip' | Select-Object -First 1
if (-not $zip) { throw "no zip produced in $outDir" }

# ---------------------------------------------------------------- contents
Add-Type -AssemblyName System.IO.Compression.FileSystem
$archive = [IO.Compression.ZipFile]::OpenRead($zip.FullName)
try {
    $entries = @($archive.Entries | ForEach-Object { $_.FullName })
} finally {
    $archive.Dispose()
}

# One entry per thing that can go missing on its own: Qt's libraries and the
# platform plugin (windeployqt), the MSVC runtime (InstallRequiredSystemLibraries),
# and the two text files a binary distribution is obliged to carry.
$required = @(
    '/MegaDirStat.exe',
    '/qt.conf',
    '/Qt6Core.dll',
    '/Qt6Widgets.dll',
    '/platforms/qwindows.dll',
    '/VCRUNTIME140.dll',
    '/MSVCP140.dll',
    '/LICENSE',
    '/THIRD-PARTY-NOTICES.txt'
)
# OrdinalIgnoreCase: the redist ships its DLLs lower-cased, Qt its own capitalised.
$missing = $required | Where-Object {
    $suffix = $_
    -not ($entries | Where-Object { $_.EndsWith($suffix, [StringComparison]::OrdinalIgnoreCase) })
}
if ($missing) { throw "zip is missing: $($missing -join ', ')" }

# ------------------------------------------------------------ launch check
$dest = Join-Path ([IO.Path]::GetTempPath()) "MegaDirStat-package-check"
Remove-Item -Recurse -Force $dest -ErrorAction SilentlyContinue
Expand-Archive -LiteralPath $zip.FullName -DestinationPath $dest
$exe = Get-ChildItem -Recurse -Filter MegaDirStat.exe $dest | Select-Object -First 1

# With Qt on PATH a missing DLL would be found there, and the check would pass
# on exactly the machine it is meant to stand in for.
$savedPath = $env:PATH
$env:PATH = (($env:PATH -split ';') | Where-Object { $_ -and $_ -notmatch '[\\/]Qt[\\/]' }) -join ';'
try {
    # No arguments: the real (non-mock) path, which stops at the login view.
    $p = Start-Process -FilePath $exe.FullName -WorkingDirectory $exe.DirectoryName -PassThru
} finally {
    $env:PATH = $savedPath
}
$deadline = (Get-Date).AddSeconds(20)
do {
    Start-Sleep -Milliseconds 500
    $p.Refresh()
} until ($p.HasExited -or $p.MainWindowTitle -or (Get-Date) -gt $deadline)
if ($p.HasExited) { throw "the unpacked exe exited with $($p.ExitCode)" }
$title = $p.MainWindowTitle
if (-not $title) {
    $p | Stop-Process -Force
    throw 'the unpacked exe showed no window within 20s'
}
$p.CloseMainWindow() | Out-Null
if (-not $p.WaitForExit(10000)) {
    $p | Stop-Process -Force
    throw 'the unpacked exe did not close within 10s'
}
Remove-Item -Recurse -Force $dest -ErrorAction SilentlyContinue

$sizeMb = [math]::Round($zip.Length / 1MB, 1)
Write-Host ("{0} ({1} MB, {2} entries; launched as '{3}')" -f $zip.FullName, $sizeMb, $entries.Count, $title) -ForegroundColor Green

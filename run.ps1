<#
.SYNOPSIS
    Launches the most recently built vbird.exe.

.DESCRIPTION
    Searches the CMake preset output directories and picks the newest exe, so it
    works whichever preset you last built. For a shared-Qt build it puts $env:QT_DIR
    on PATH if the Qt DLLs have not been copied next to the exe; the dist (static)
    build needs neither.
#>
[CmdletBinding()]
param(
    # Preset to launch. Omit to use whichever build is newest.
    [ValidateSet('dist', 'release', 'dev')]
    [string] $Preset
)

$ErrorActionPreference = 'Stop'

$candidates = if ($Preset) {
    @("build\$Preset\bin\vbird.exe")
} else {
    @('build\dist\bin\vbird.exe', 'build\release\bin\vbird.exe', 'build\dev\bin\vbird.exe', 'bin\vbird.exe')
}

$exe = $candidates |
    ForEach-Object { Join-Path $PSScriptRoot $_ } |
    Where-Object   { Test-Path $_ } |
    Get-Item |
    Sort-Object LastWriteTime -Descending |
    Select-Object -First 1

if (-not $exe) {
    Write-Error @"
No vbird.exe found. Build one first:

  cmake --preset release
  cmake --build --preset release

See BUILDING.md for the single-file dist build.
"@
    exit 1
}

Write-Host "Launching $($exe.FullName)" -ForegroundColor Cyan

# A static build imports no Qt DLLs, and a deployed shared build has them in the
# same folder. Only fall back to QT_DIR when neither is true.
$needsQtOnPath = -not (Test-Path (Join-Path $exe.Directory 'Qt6Core.dll'))
if ($needsQtOnPath -and $env:QT_DIR) {
    $qtBin = Join-Path $env:QT_DIR 'bin'
    if (Test-Path $qtBin) {
        $env:Path = "$qtBin;$env:Path"
        Write-Host "Added $qtBin to PATH for this launch" -ForegroundColor DarkGray
    }
}

Start-Process -FilePath $exe.FullName -Wait

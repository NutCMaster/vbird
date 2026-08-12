<#
.SYNOPSIS
    Builds a static Qt 6 (qtbase) with MinGW, which is what vBird's single-file
    dist build links against.

.DESCRIPTION
    Qt does not distribute static builds through the online installer, so a
    self-contained vbird.exe requires compiling Qt from source once. This builds
    qtbase only -- the module providing Core, Gui and Widgets -- which is all
    vBird currently uses and takes far less time than a full Qt build.

    Budget 30-60 minutes and roughly 10 GB of disk for the source and build
    trees. The result is reusable: you build it once, then every vBird dist
    build is fast.

    Licensing note: vBird is GPL-3.0 and Qt's open-source build is LGPL-3.0.
    Static linking is permitted, and publishing vBird's full source together
    with this script satisfies the obligation to let users relink against a
    modified Qt.

.PARAMETER QtVersion
    Qt release tag to build. Must be 6.5 or newer.

.PARAMETER Prefix
    Install directory for the finished static Qt.

.PARAMETER WorkDir
    Where the source and build trees go. Deletable once the build finishes.

.PARAMETER Jobs
    Parallel compile jobs. Defaults to the processor count.

.EXAMPLE
    .\tools\build-static-qt.ps1
    .\tools\build-static-qt.ps1 -QtVersion 6.9.1 -Prefix D:\Qt\static-6.9.1
#>
[CmdletBinding()]
param(
    [string] $QtVersion = '6.9.1',
    [string] $Prefix    = "$env:USERPROFILE\Qt\static-$QtVersion",
    [string] $WorkDir   = "$env:USERPROFILE\Qt\src",
    [int]    $Jobs      = $env:NUMBER_OF_PROCESSORS
)

$ErrorActionPreference = 'Stop'

function Write-Step($msg) { Write-Host "`n==> $msg" -ForegroundColor Cyan }
function Write-Warn($msg) { Write-Host "    $msg" -ForegroundColor Yellow }

# ---------------------------------------------------------------- prerequisites
Write-Step 'Checking prerequisites'

$required = @{
    'git'    = 'winget install Git.Git'
    'cmake'  = 'winget install Kitware.CMake'
    'ninja'  = 'winget install Ninja-build.Ninja'
    'g++'    = 'winget install BrechtSanders.WinLibs.POSIX.MSVCRT'
    'perl'   = 'winget install StrawberryPerl.StrawberryPerl'
    'python' = 'winget install Python.Python.3.12'
}

$missing = @()
foreach ($tool in $required.Keys | Sort-Object) {
    $cmd = Get-Command $tool -ErrorAction SilentlyContinue

    # The Windows Store alias in WindowsApps is a stub that is not a usable
    # Python for Qt's build scripts.
    if ($cmd -and $tool -eq 'python' -and $cmd.Source -like '*\WindowsApps\*') {
        $cmd = $null
    }

    if ($cmd) {
        Write-Host ("    {0,-8} {1}" -f $tool, $cmd.Source)
    } else {
        Write-Host ("    {0,-8} MISSING" -f $tool) -ForegroundColor Red
        $missing += $tool
    }
}

if ($missing.Count -gt 0) {
    Write-Host ''
    Write-Warn 'Install the missing tools, reopen the terminal, then re-run:'
    foreach ($tool in $missing) { Write-Warn "  $($required[$tool])" }
    throw "Missing prerequisites: $($missing -join ', ')"
}

# Qt's build fails in confusing ways if sh.exe (from Git for Windows) shadows the
# MinGW toolchain on PATH.
$sh = Get-Command sh.exe -ErrorAction SilentlyContinue
if ($sh) {
    Write-Warn "sh.exe on PATH ($($sh.Source)) can break Qt's MinGW build."
    Write-Warn 'If configure fails, drop Git usr\bin from PATH for this session.'
}

# ---------------------------------------------------------------------- source
$srcDir   = Join-Path $WorkDir "qtbase-$QtVersion"
$buildDir = Join-Path $WorkDir "qtbase-$QtVersion-build"

if (-not (Test-Path $srcDir)) {
    Write-Step "Cloning qtbase $QtVersion"
    New-Item -ItemType Directory -Force -Path $WorkDir | Out-Null
    & git clone --depth 1 --branch "v$QtVersion" `
        https://code.qt.io/qt/qtbase.git $srcDir
    if ($LASTEXITCODE -ne 0) { throw "git clone failed (is v$QtVersion a real tag?)" }
} else {
    Write-Step "Reusing existing source at $srcDir"
}

# --------------------------------------------------------------------- configure
Write-Step "Configuring static Qt -> $Prefix"

# -static is the flag that makes this a static Qt. -no-opengl avoids pulling in
# an ANGLE/OpenGL runtime that would have to ship beside the exe; Widgets renders
# fine on the software raster path.
$configureArgs = @(
    '-static'
    '-release'
    '-prefix',   $Prefix
    '-opensource'
    '-confirm-license'
    '-nomake',   'examples'
    '-nomake',   'tests'
    '-no-opengl'
    '-qt-zlib'
    '-qt-pcre'
    '-qt-freetype'
    '-qt-harfbuzz'
    '-no-icu'
    '-no-dbus'
    '--'
    "-DCMAKE_INSTALL_PREFIX=$Prefix"
    '-G', 'Ninja'
)

$configure = Join-Path $srcDir 'configure.bat'
New-Item -ItemType Directory -Force -Path $buildDir | Out-Null

Push-Location $buildDir
try {
    & $configure @configureArgs
    if ($LASTEXITCODE -ne 0) { throw "Qt configure failed with exit code $LASTEXITCODE" }

    Write-Step "Building qtbase with $Jobs jobs (this is the slow part)"
    & cmake --build . --parallel $Jobs
    if ($LASTEXITCODE -ne 0) { throw "Qt build failed with exit code $LASTEXITCODE" }

    Write-Step "Installing to $Prefix"
    & cmake --install .
    if ($LASTEXITCODE -ne 0) { throw "Qt install failed with exit code $LASTEXITCODE" }
} finally {
    Pop-Location
}

# ------------------------------------------------------------------------- done
Write-Step 'Static Qt ready'
Write-Host ''
Write-Host '    Point vBird at it and build the single-file exe:' -ForegroundColor Green
Write-Host ''
Write-Host "      `$env:QT_STATIC_DIR = '$Prefix'"
Write-Host '      cmake --preset dist'
Write-Host '      cmake --build --preset dist'
Write-Host '      cmake --build --preset dist-check'
Write-Host ''
Write-Host "    Persist it so new terminals pick it up:" -ForegroundColor Green
Write-Host ''
Write-Host "      [Environment]::SetEnvironmentVariable('QT_STATIC_DIR','$Prefix','User')"
Write-Host ''
Write-Host "    $WorkDir can be deleted now to reclaim disk space."
Write-Host ''

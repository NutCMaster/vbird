# Run wrapper to ensure Qt bin is on PATH and launch the built exe
$qtBin = 'D:\Qt\6.11.1\mingw_64\bin'
$env:Path = "$qtBin;" + $env:Path
$exe = Join-Path -Path $PSScriptRoot -ChildPath 'bin\vbird.exe'
if (Test-Path $exe) {
    Start-Process -FilePath $exe -NoNewWindow -Wait
} else {
    Write-Error "Executable not found: $exe"
    exit 1
}

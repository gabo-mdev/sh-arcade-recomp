# Build from any PowerShell prompt (sets up MSVC + Ninja).
param(
  [switch]$NoSdl,
  [switch]$RelWithDebInfo
)

$ErrorActionPreference = "Stop"
$Root = Split-Path -Parent $MyInvocation.MyCommand.Path
Set-Location $Root

$vcvars = "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat"
if (-not (Test-Path $vcvars)) {
  $found = Get-ChildItem "C:\Program Files\Microsoft Visual Studio" -Recurse -Filter vcvars64.bat -ErrorAction SilentlyContinue |
    Select-Object -First 1 -ExpandProperty FullName
  if (-not $found) { throw "vcvars64.bat not found. Install VS C++ tools." }
  $vcvars = $found
}

$ninjaDir = "C:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja"
$config = if ($RelWithDebInfo) { "RelWithDebInfo" } else { "Release" }
$sdl = if ($NoSdl) { "OFF" } else { "ON" }

$cmd = "call `"$vcvars`" >nul && set `"PATH=$ninjaDir;%PATH%`" && cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=$config -DSH_HOST_SDL=$sdl && cmake --build build --parallel"

cmd /c $cmd
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

Write-Host ""
Write-Host "OK -> build\splatterhouse.exe"
Write-Host "  Window:  .\build\splatterhouse.exe"
Write-Host "  Headless: .\build\splatterhouse.exe --headless --frames 5"

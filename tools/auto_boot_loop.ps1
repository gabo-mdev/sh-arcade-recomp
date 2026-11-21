# Rebuild + probe + PNG snapshot loop for boot bring-up.
param(
  [int]$Frames = 900,
  [int]$Iters = 1
)

$ErrorActionPreference = "Stop"
$Root = Split-Path -Parent $PSScriptRoot
Set-Location $Root

function Build {
  $vcvars = "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat"
  $ninja = "C:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja"
  cmd /c "call `"$vcvars`" >nul && set `"PATH=$ninja;%PATH%`" && cmake --build build --parallel"
  if ($LASTEXITCODE -ne 0) { throw "build failed" }
}

function Probe([int]$n) {
  $out = "build/probe"
  New-Item -ItemType Directory -Force -Path $out | Out-Null
  Get-Process splatterhouse -ErrorAction SilentlyContinue | Stop-Process -Force
  & .\build\splatterhouse.exe --headless --probe $n --probe-out $out
  python -c @"
from pathlib import Path
from PIL import Image
import json
st=json.loads(Path('build/probe/status.json').read_text())
print('STATUS', st)
p=Path('build/probe/last_frame.ppm')
if p.exists():
  Image.open(p).resize((576,448), Image.NEAREST).save('build/probe/last_frame.png')
  print('wrote build/probe/last_frame.png')
"@
}

Build
for ($i = 1; $i -le $Iters; $i++) {
  Write-Host "=== probe iter $i frames=$Frames ==="
  Probe $Frames
}

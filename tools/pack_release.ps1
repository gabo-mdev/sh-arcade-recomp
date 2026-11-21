# Pack a Windows x64 release zip (no ROMs).
param(
  [string]$Version = "0.1.0",
  [string]$Exe = "",
  [string]$OutDir = "dist"
)

$ErrorActionPreference = "Stop"
$Root = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
Set-Location $Root

if (-not $Exe) {
  $Exe = Join-Path $Root "build\splatterhouse.exe"
}
if (-not (Test-Path $Exe)) {
  throw "Missing $Exe — build first (.\build.ps1)."
}

$stageName = "Splatterhouse-Arcade-v$Version-Windows-x64"
$stage = Join-Path $Root (Join-Path $OutDir $stageName)
$zip = Join-Path $Root (Join-Path $OutDir "$stageName.zip")

if (Test-Path $stage) { Remove-Item -Recurse -Force $stage }
New-Item -ItemType Directory -Force -Path $stage | Out-Null
Copy-Item $Exe (Join-Path $stage "splatterhouse.exe")
Copy-Item (Join-Path $Root "README.md") (Join-Path $stage "README.md")
Copy-Item (Join-Path $Root "LICENSE") (Join-Path $stage "LICENSE")

if (Test-Path $zip) { Remove-Item -Force $zip }
Compress-Archive -Path (Join-Path $stage "*") -DestinationPath $zip -Force

Write-Host "OK -> $zip"
Write-Host "Contains: splatterhouse.exe, README.md, LICENSE"
Write-Host "Attach this file to a GitHub release tagged v$Version."

# =============================================================================
#  Deploy-Client.ps1
#
#  Copies the freshly built client into Client\ and archives the symbols that
#  belong to it.
#
#  Why the archive: a crash dump names the exact PDB it needs by GUID + age, and
#  the linker bumps the age on every incremental link while always writing to
#  the same Build\Client\Main.pdb. Without a per-deploy copy, yesterday's dump
#  can no longer be symbolised - which is exactly what happened to the
#  2026-07-31 crashes.
#
#  Usage:  .\Deploy-Client.ps1
# =============================================================================

$ErrorActionPreference = 'Stop'

$root    = $PSScriptRoot
$build   = Join-Path $root 'Build\Client'
$client  = Join-Path $root 'Client'
$symbols = Join-Path $root 'Build\Symbols'

$exe = Join-Path $build 'Main.exe'
$pdb = Join-Path $build 'Main.pdb'
$map = Join-Path $build 'Main'          # the linker map: no extension by design

if (-not (Test-Path $exe)) {
    throw "No build output at $exe - build Main.sln (Release|x86) first."
}

# --- 1. archive the symbols for this exact binary ---------------------------
# Archived before the copy on purpose: if the client is running, the deploy
# fails but the symbols are still worth keeping.
# Stamp the folder with the PE TimeDateStamp, which is what the dump records,
# so a dump can be matched to its build without guesswork.
$bytes = [System.IO.File]::ReadAllBytes($exe)
$peOff = [BitConverter]::ToInt32($bytes, 0x3C)
$stamp = [BitConverter]::ToUInt32($bytes, $peOff + 8)
$built = [DateTimeOffset]::FromUnixTimeSeconds($stamp).UtcDateTime

$name = '{0:yyyy-MM-dd_HHmmss}_{1:x8}' -f $built, $stamp
$dest = Join-Path $symbols $name

New-Item -ItemType Directory -Path $dest -Force | Out-Null

Copy-Item $exe $dest -Force
if (Test-Path $pdb) { Copy-Item $pdb $dest -Force }
if (Test-Path $map) { Copy-Item $map (Join-Path $dest 'Main.map') -Force }

# --- 3. keep the archive from growing without bound -------------------------
$keep = 30
Get-ChildItem $symbols -Directory |
    Sort-Object Name -Descending |
    Select-Object -Skip $keep |
    ForEach-Object { Remove-Item $_.FullName -Recurse -Force }

$size = (Get-ChildItem $dest | Measure-Object Length -Sum).Sum / 1MB

Write-Output ("symbols  : Build\Symbols\{0}   ({1:N0} MB)" -f $name, $size)

# --- 2. deploy --------------------------------------------------------------
try {
    Copy-Item $exe (Join-Path $client 'Main.exe') -Force -ErrorAction Stop

    if (Test-Path $pdb) {
        Copy-Item $pdb (Join-Path $client 'Main.pdb') -Force -ErrorAction SilentlyContinue
    }

    Write-Output ("deployed : Client\Main.exe   (built {0:yyyy-MM-dd HH:mm:ss} UTC)" -f $built)
}
catch {
    Write-Output "deployed : SKIPPED - Client\Main.exe is locked, close the game and run this again."
}

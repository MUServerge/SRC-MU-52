[CmdletBinding()]
param(
    [string]$RepositoryRoot = (Get-Location).Path,
    [ValidateSet("Release", "Debug")]
    [string[]]$Configuration = @("Release"),
    [ValidateSet("Win32", "x86")]
    [string]$Platform = "Win32",
    [string]$Solution = "SRCMainGS\Source\Main5.2\Main.sln",
    [string]$EvidenceRoot = "",
    [switch]$SkipRebuild
)

$ErrorActionPreference = "Stop"
$root = (Resolve-Path -LiteralPath $RepositoryRoot).Path
$solutionPath = Join-Path $root $Solution
if (-not (Test-Path -LiteralPath (Join-Path $root "AGENTS.md")) -or
    -not (Test-Path -LiteralPath $solutionPath)) {
    throw "RepositoryRoot/Solution does not identify SRC-MU-52."
}

if (-not $EvidenceRoot) {
    $EvidenceRoot = Join-Path ([IO.Path]::GetTempPath()) "MU52-BuildEvidence"
}
$stamp = Get-Date -Format "yyyyMMdd-HHmmss"
$output = Join-Path $EvidenceRoot $stamp
New-Item -ItemType Directory -Path $output -Force | Out-Null

$vswhere = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\Installer\vswhere.exe"
if (-not (Test-Path -LiteralPath $vswhere)) {
    throw "vswhere.exe was not found. Install Visual Studio Build Tools/Visual Studio."
}
$msbuild = & $vswhere -latest -products * -requires Microsoft.Component.MSBuild -find "MSBuild\**\Bin\MSBuild.exe" | Select-Object -First 1
if (-not $msbuild -or -not (Test-Path -LiteralPath $msbuild)) {
    throw "MSBuild.exe was not found by vswhere."
}

$head = (& git -C $root rev-parse HEAD).Trim()
$branch = (& git -C $root branch --show-current).Trim()
$status = @(& git -C $root status --short)
$remote = (& git -C $root remote get-url origin 2>$null)
$msbuildVersion = (& $msbuild -version -nologo | Select-Object -Last 1).Trim()
$results = @()

foreach ($config in $Configuration) {
    $log = Join-Path $output "Main-$config-$Platform.log"
    $target = if ($SkipRebuild) { "Build" } else { "Rebuild" }
    $arguments = @(
        $solutionPath,
        "/t:$target",
        "/m",
        "/nologo",
        "/p:Configuration=$config",
        "/p:Platform=$Platform",
        "/fl",
        "/flp:logfile=$log;verbosity=normal"
    )
    & $msbuild @arguments
    $exitCode = $LASTEXITCODE
    $results += [ordered]@{
        configuration = $config
        platform = $Platform
        target = $target
        exit_code = $exitCode
        succeeded = ($exitCode -eq 0)
        log = $log
    }
}

$artifacts = @()
$candidates = @(
    (Join-Path $root "Build\Client\Main.exe"),
    (Join-Path $root "Client\Main.exe")
) | Select-Object -Unique
foreach ($file in $candidates) {
    if (Test-Path -LiteralPath $file) {
        $item = Get-Item -LiteralPath $file
        $artifacts += [ordered]@{
            path = $item.FullName
            length = $item.Length
            modified_utc = $item.LastWriteTimeUtc.ToString("o")
            sha256 = (Get-FileHash -LiteralPath $item.FullName -Algorithm SHA256).Hash
        }
    }
}

$evidence = [ordered]@{
    generated_utc = [DateTime]::UtcNow.ToString("o")
    repository = $root
    remote = $remote
    branch = $branch
    commit = $head
    dirty = ($status.Count -gt 0)
    worktree_changes = $status
    solution = $solutionPath
    msbuild = $msbuild
    msbuild_version = $msbuildVersion
    results = $results
    artifacts = $artifacts
}
$json = Join-Path $output "build-evidence.json"
$evidence | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath $json -Encoding UTF8

$evidence | ConvertTo-Json -Depth 6
if ($results.Where({ -not $_.succeeded }).Count -gt 0) { exit 1 }


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
$stamp = Get-Date -Format "yyyyMMdd-HHmmss-fff"
$runId = "{0}-{1}" -f $stamp, ([Guid]::NewGuid().ToString("N").Substring(0, 8))
$output = Join-Path $EvidenceRoot $runId
if (Test-Path -LiteralPath $output) { throw "Evidence directory already exists: $output" }
New-Item -ItemType Directory -Path $output | Out-Null

$vswhere = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\Installer\vswhere.exe"
if (-not (Test-Path -LiteralPath $vswhere)) {
    throw "vswhere.exe was not found. Install Visual Studio Build Tools/Visual Studio."
}
$msbuild = & $vswhere -latest -products * -requires Microsoft.Component.MSBuild -find "MSBuild\**\Bin\MSBuild.exe" | Select-Object -First 1
if (-not $msbuild -or -not (Test-Path -LiteralPath $msbuild)) {
    throw "MSBuild.exe was not found by vswhere."
}

if (-not (Get-Command git -ErrorAction SilentlyContinue)) { throw "git.exe was not found." }
$gitRoot = (& git -C $root rev-parse --show-toplevel 2>$null)
if ($LASTEXITCODE -ne 0 -or -not $gitRoot) { throw "RepositoryRoot is not a Git worktree." }
$gitRoot = (Resolve-Path -LiteralPath $gitRoot.Trim()).Path
if ($gitRoot -ne $root) { throw "RepositoryRoot must be the Git worktree root: $gitRoot" }
$head = (& git -C $root rev-parse HEAD).Trim()
if ($LASTEXITCODE -ne 0 -or -not $head) { throw "Failed to resolve the Git commit." }
$branch = (& git -C $root branch --show-current).Trim()
if ($LASTEXITCODE -ne 0) { throw "Failed to resolve the Git branch." }
$statusBefore = @(& git -C $root status --short)
if ($LASTEXITCODE -ne 0) { throw "Failed to read Git worktree status." }
$remote = (& git -C $root remote get-url origin 2>$null)
if ($LASTEXITCODE -ne 0) { $remote = "" }
$msbuildVersion = (& $msbuild -version -nologo | Select-Object -Last 1).Trim()
$results = @()
$candidates = @(
    (Join-Path $root "Build\Client\Main.exe"),
    (Join-Path $root "Client\Main.exe")
) | Select-Object -Unique

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
    $startedUtc = [DateTime]::UtcNow
    $before = @{}
    foreach ($file in $candidates) {
        if (Test-Path -LiteralPath $file) {
            $before[$file] = (Get-FileHash -LiteralPath $file -Algorithm SHA256).Hash
        }
    }
    & $msbuild @arguments
    $exitCode = $LASTEXITCODE
    $configArtifacts = @()
    if ($exitCode -eq 0) {
        foreach ($file in $candidates) {
            if (-not (Test-Path -LiteralPath $file)) { continue }
            $item = Get-Item -LiteralPath $file
            $hash = (Get-FileHash -LiteralPath $item.FullName -Algorithm SHA256).Hash
            $changed = (-not $before.ContainsKey($file)) -or ($before[$file] -ne $hash) -or ($item.LastWriteTimeUtc -ge $startedUtc)
            if (-not $changed) { continue }
            $configArtifacts += [ordered]@{
                configuration = $config
                platform = $Platform
                path = $item.FullName
                length = $item.Length
                modified_utc = $item.LastWriteTimeUtc.ToString("o")
                sha256 = $hash
            }
        }
    }
    $artifactVerified = ($configArtifacts.Count -gt 0)
    $evidenceGatePassed = ($exitCode -eq 0 -and $artifactVerified)
    $results += [ordered]@{
        configuration = $config
        platform = $Platform
        target = $target
        exit_code = $exitCode
        msbuild_succeeded = ($exitCode -eq 0)
        artifact_verified = $artifactVerified
        evidence_gate_passed = $evidenceGatePassed
        log = $log
        artifacts = $configArtifacts
    }
}

$statusAfter = @(& git -C $root status --short)
if ($LASTEXITCODE -ne 0) { throw "Failed to read post-build Git worktree status." }

$evidence = [ordered]@{
    generated_utc = [DateTime]::UtcNow.ToString("o")
    repository = $root
    remote = $remote
    branch = $branch
    commit = $head
    dirty_before = ($statusBefore.Count -gt 0)
    worktree_changes_before = $statusBefore
    dirty_after = ($statusAfter.Count -gt 0)
    worktree_changes_after = $statusAfter
    solution = $solutionPath
    msbuild = $msbuild
    msbuild_version = $msbuildVersion
    results = $results
}
$json = Join-Path $output "build-evidence.json"
$evidence | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath $json -Encoding UTF8

$evidence | ConvertTo-Json -Depth 6
if ($results.Where({ -not $_.evidence_gate_passed }).Count -gt 0) { exit 1 }

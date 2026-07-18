[CmdletBinding()]
param(
    [Parameter(Mandatory = $false)]
    [string]$RepositoryRoot = (Get-Location).Path,
    [Parameter(Mandatory = $false)]
    [string]$OutputPath = ""
)

$ErrorActionPreference = "Stop"
$root = (Resolve-Path -LiteralPath $RepositoryRoot).Path
$agentGuide = Join-Path $root "AGENTS.md"
$clientRoot = Join-Path $root "SRCMainGS\Source\Main5.2"

if (-not (Test-Path -LiteralPath $agentGuide) -or -not (Test-Path -LiteralPath $clientRoot)) {
    throw "The selected directory is not the canonical SRC-MU-52 repository root."
}

$rg = Get-Command rg -ErrorAction SilentlyContinue
if (-not $rg) {
    throw "ripgrep (rg) is required for the deterministic static audit."
}

$patterns = [ordered]@{
    AbsolutePaths = '[A-Za-z]:\\Users\\|[A-Za-z]:\\Program Files'
    FrameTiming = 'GetTickCount|QueryPerformanceCounter|timeGetTime|Sleep\s*\(|CheckNormalizer|timefac|accumulatedTime'
    OpenGLLifecycle = 'glGen(Buffers|VertexArrays)|glDelete(Buffers|VertexArrays)|glUseProgram|GL_CURRENT_PROGRAM'
    ProjectLibraries = 'AdditionalDependencies|AdditionalLibraryDirectories|AdditionalIncludeDirectories'
    RiskMarkers = 'TODO|FIXME|HACK|XXX'
}

$report = New-Object System.Collections.Generic.List[string]
$report.Add("SRC-MU-52 static audit")
$report.Add("Root: $root")
$report.Add("Generated: $([DateTime]::UtcNow.ToString('u'))")

foreach ($entry in $patterns.GetEnumerator()) {
    $report.Add("")
    $report.Add("[$($entry.Key)]")
    $matches = & $rg.Source --line-number --hidden --glob '!Build/**' --glob '!Client/**' --glob '!MuServer/**' --glob '!MuServerTK/**' --glob '!**/.git/**' --glob '*.{cpp,h,hpp,c,vcxproj,props,targets}' -- $entry.Value $root 2>$null
    if ($LASTEXITCODE -gt 1) {
        throw "rg failed while scanning $($entry.Key)."
    }
    if ($matches) { $report.AddRange([string[]]$matches) } else { $report.Add("No matches") }
}

$text = $report -join [Environment]::NewLine
if ($OutputPath) {
    $destination = $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($OutputPath)
    [IO.File]::WriteAllText($destination, $text, (New-Object Text.UTF8Encoding($false)))
}
$text


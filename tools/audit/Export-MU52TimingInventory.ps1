[CmdletBinding()]
param(
    [string]$RepositoryRoot = (Get-Location).Path,
    [string]$OutputPath = ""
)

$ErrorActionPreference = "Stop"
$utf8 = New-Object System.Text.UTF8Encoding($false)
$OutputEncoding = $utf8
[Console]::OutputEncoding = $utf8
$root = (Resolve-Path -LiteralPath $RepositoryRoot).Path
$client = Join-Path $root "SRCMainGS\Source\Main5.2"
if (-not (Test-Path -LiteralPath $client)) { throw "Main5.2 client source was not found." }
$rg = Get-Command rg -ErrorAction SilentlyContinue
if (-not $rg) { throw "ripgrep (rg) is required." }

$categories = [ordered]@{
    WallClock = 'GetTickCount|GetTickCount64|timeGetTime|QueryPerformanceCounter|QueryPerformanceFrequency'
    WaitPacing = 'Sleep\s*\(|WaitForSingleObject|DwmFlush|SwapInterval|wglSwapInterval'
    DeltaFixedStep = 'timefac|CheckNormalizer|accumulatedTime|fixed.?step|interpolation|dropped.?step'
    FrameCounters = '(Frame|frame|Counter|counter|Tick|tick)\s*(\+\+|\+=|=\s*.*\+\s*1)'
    NetworkTimers = 'Send.*Time|Packet.*Time|Reconnect.*Time|Last.*Send|Last.*Recv|Timeout'
}

$rows = @()
foreach ($entry in $categories.GetEnumerator()) {
    $matches = & $rg.Source --json --glob '*.{cpp,c,h,hpp}' -- $entry.Value $client 2>$null
    if ($LASTEXITCODE -gt 1) { throw "rg failed for $($entry.Key)." }
    foreach ($line in $matches) {
        $record = $line | ConvertFrom-Json
        if ($record.type -ne "match") { continue }
        $rows += [pscustomobject]@{
            Category = $entry.Key
            File = $record.data.path.text.Substring($root.Length).TrimStart('\','/')
            Line = $record.data.line_number
            Text = $record.data.lines.text.Trim()
            Classification = "NeedsReview"
            Owner = "Unknown"
            Cadence = "Unknown"
            Evidence = "StaticInventory"
        }
    }
}

$rows = $rows | Sort-Object Category, File, Line -Unique
if ($OutputPath) {
    $destination = $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($OutputPath)
    $rows | Export-Csv -LiteralPath $destination -NoTypeInformation -Encoding UTF8
}
$rows | Format-Table -AutoSize

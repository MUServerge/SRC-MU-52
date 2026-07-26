[CmdletBinding()]
param(
    [Parameter(Mandatory = $false)]
    [string]$Base = "origin/main",
    [Parameter(Mandatory = $false)]
    [string]$Head = "HEAD",
    [Parameter(Mandatory = $false)]
    [string]$RepositoryRoot = (Get-Location).Path
)

$ErrorActionPreference = "Stop"
$root = (Resolve-Path -LiteralPath $RepositoryRoot).Path
if (-not (Test-Path -LiteralPath (Join-Path $root ".git"))) {
    throw "RepositoryRoot must be a Git checkout."
}

$files = & git -C $root diff --name-only --diff-filter=ACDMRT $Base $Head
if ($LASTEXITCODE -ne 0) { throw "git diff failed." }

$groups = [ordered]@{
    Build = @()
    Timing = @()
    Renderer = @()
    ProtocolPersistence = @()
    UI = @()
    Assets = @()
    Other = @()
}

foreach ($file in $files) {
    switch -Regex ($file) {
        '\.(vcxproj|props|targets|sln)$' { $groups.Build += $file; continue }
        '(ZzzScene|Time|Timer|FPS|Profiler|WINHANDLE)' { $groups.Timing += $file; continue }
        '(Shader|OpenGL|Render|BMD|Model|Terrain|Effect|glew)' { $groups.Renderer += $file; continue }
        '(Protocol|Packet|DataServer|GameServer|JoinServer|ConnectServer|Query|SQL)' { $groups.ProtocolPersistence += $file; continue }
        '(NewUI|LookAndFeel|Interface|Window|CGMFrame)' { $groups.UI += $file; continue }
        '(Data/|Texture|Bitmap|Sound|World|Object)' { $groups.Assets += $file; continue }
        default { $groups.Other += $file }
    }
}

foreach ($group in $groups.GetEnumerator()) {
    if ($group.Value.Count -eq 0) { continue }
    "[$($group.Key)]"
    $group.Value | Sort-Object -Unique
    ""
}

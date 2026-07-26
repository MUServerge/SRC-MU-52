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
    ContextLoader = 'wglCreateContext|wglCreateContextAttribs|glewInit|GLEW_|wglGetProcAddress'
    ProgramCreate = 'glCreateProgram|glCreateShader|glCompileShader|glLinkProgram'
    ProgramUseDelete = 'glUseProgram|GL_CURRENT_PROGRAM|glDeleteProgram|glDeleteShader'
    BufferCreate = 'glGenBuffers|glGenVertexArrays|glBufferData|glBufferSubData'
    BufferUseDelete = 'glBindBuffer|glBindVertexArray|glDeleteBuffers|glDeleteVertexArrays'
    TextureLifecycle = 'glGenTextures|glBindTexture|glTexImage|glTexSubImage|glDeleteTextures'
    FramebufferLifecycle = 'glGenFramebuffers|glBindFramebuffer|glFramebuffer|glDeleteFramebuffers|glGenRenderbuffers|glBindRenderbuffer|glRenderbufferStorage|glDeleteRenderbuffers'
    SamplerLifecycle = 'glGenSamplers|glBindSampler|glSamplerParameter|glDeleteSamplers'
    UniformState = 'glGetUniformLocation|glUniform|glGetAttribLocation|glVertexAttrib'
    FixedState = 'glEnable|glDisable|glBlendFunc|glBlendEquation|glDepthFunc|glDepthMask|glColorMask|glCullFace|glScissor|glViewport|glActiveTexture'
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
            Owner = "Unknown"
            Create = "NeedsTrace"
            Use = "NeedsTrace"
            Restore = "NeedsTrace"
            Release = "NeedsTrace"
        }
    }
}

$rows = $rows | Sort-Object Category, File, Line -Unique
if ($OutputPath) {
    $destination = $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($OutputPath)
    $rows | Export-Csv -LiteralPath $destination -NoTypeInformation -Encoding UTF8
}
$rows | Format-Table -AutoSize

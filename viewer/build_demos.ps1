
param(
    [string]$Demo    = "",
    [switch]$NoFrames,
    [int]$MaxFrameBytes = 3MB
)

$ErrorActionPreference = "Stop"
$viewer = $PSScriptRoot
$root   = Split-Path $viewer -Parent
$data   = Join-Path $viewer "data"
$exe    = Join-Path $root "fr_frontend.exe"

if (-not (Test-Path $exe)) { throw "fr_frontend.exe not found - run .\build.ps1 first" }
if (-not (Test-Path $data)) { New-Item -ItemType Directory $data | Out-Null }

$preferred = @("triangle", "koch", "dragon", "plant", "tree", "carpet")

$files = Get-ChildItem (Join-Path $root "*.fr") | Where-Object { -not (Select-String -Path $_.FullName -Pattern 'dimensions:\s*3' -Quiet) } | Sort-Object Name
if ($Demo) { $files = $files | Where-Object { $_.BaseName -eq $Demo } }
if (-not $files) { throw "no .fr files matched" }

$entries = @{}

Add-Type -AssemblyName System.Web.Extensions
$json = New-Object System.Web.Script.Serialization.JavaScriptSerializer
$json.MaxJsonLength = [int]::MaxValue

foreach ($f in $files) {
    $name = $f.BaseName
    $out  = Join-Path $data "$name.json"
    Write-Host "== $name"
    & $exe $f.FullName --json $out | Out-Null
    if ($LASTEXITCODE -ne 0) { & $exe $f.FullName --json $out; throw "$name failed to compile" }

    $doc   = $json.DeserializeObject([System.IO.File]::ReadAllText($out, [System.Text.Encoding]::UTF8))
    $iters = [int]$doc["ast"]["iterations"]
    Write-Host ("   {0} tokens, {1} symbols, {2} commands, iterations {3}" -f `
        $doc["tokens"].Count, $doc["expanded"]["fullLength"], $doc["programLength"], $iters)

    $frames = 0
    if (-not $NoFrames -and $iters -gt 0) {
        for ($k = 0; $k -le $iters; $k++) {
            $frameOut = Join-Path $data "$($name)_it$k.json"
            & $exe $f.FullName --json $frameOut --name "$($name)_it$k" --iterations $k | Out-Null
            if ($LASTEXITCODE -ne 0) { break }
            if ((Get-Item $frameOut).Length -gt $MaxFrameBytes) {
                Remove-Item $frameOut
                Write-Host "   frames capped at n=$($k-1) (next frame over $([int]($MaxFrameBytes/1MB)) MB)"
                break
            }
            $frames = $k
        }
        Write-Host "   frames it0..it$frames"
    }
    $entries[$name] = $frames
}

$ordered = @()
foreach ($p in $preferred) { if ($entries.ContainsKey($p)) { $ordered += $p } }
foreach ($k in ($entries.Keys | Sort-Object)) { if ($ordered -notcontains $k) { $ordered += $k } }

$parts = foreach ($n in $ordered) {
    '    {{ "name": "{0}", "iterationFrames": {1} }}' -f $n, $entries[$n]
}
$json = @"
{
  "generated": "$(Get-Date -Format 's')",
  "demos": [
$($parts -join ",`n")
  ]
}
"@
$manifest = Join-Path $viewer "manifest.json"
[System.IO.File]::WriteAllText($manifest, $json, (New-Object System.Text.UTF8Encoding($false)))
Write-Host "wrote $manifest ($($ordered.Count) demos: $($ordered -join ', '))"

Write-Host "`nNext: .\viewer\pack.ps1   (needed only to open index.html by double-click)"

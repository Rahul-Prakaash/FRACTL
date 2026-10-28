
param(
    [switch]$Test,
    [switch]$Demo,
    [switch]$Viewer,
    [string]$Cxx = "g++"
)

$ErrorActionPreference = "Stop"
$root  = $PSScriptRoot
$flags = @("-std=c++17", "-Wall", "-Wextra", "-O2")

Write-Host "building fr_frontend.exe ..."
& $Cxx @flags -o "$root\fr_frontend.exe" "$root\fr_frontend.cpp"
if ($LASTEXITCODE -ne 0) { throw "frontend build failed" }

if ($Test) {
    Write-Host "building tests\test_frontend.exe ..."
    & $Cxx @flags "-I$root" -o "$root\tests\test_frontend.exe" "$root\tests\test_frontend.cpp"
    if ($LASTEXITCODE -ne 0) { throw "test build failed" }

    & "$root\tests\test_frontend.exe" $root
    if ($LASTEXITCODE -ne 0) { throw "tests FAILED" }
}

if ($Demo) {
    Get-ChildItem "$root\*.fr" | ForEach-Object {
        Write-Host "=== $($_.Name) ==="
        & "$root\fr_frontend.exe" $_.FullName --ast --stats
    }
}

if ($Viewer) {
    & "$root\viewer\build_demos.ps1"
    & "$root\viewer\pack.ps1"
}

Write-Host "done."

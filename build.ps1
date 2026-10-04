$ErrorActionPreference = 'Stop'

$root = $PSScriptRoot
$src = @(Get-ChildItem -LiteralPath $root -Recurse -Filter *.cpp |
        Where-Object { $_.FullName -notmatch '\\tools\\' -and
                       $_.FullName -notmatch '\\out\\' -and
                       $_.FullName -notmatch '\\build\\' } |
        ForEach-Object { $_.FullName })
$out = Join-Path $root 'qlwt.exe'

Write-Host "compiling $($src.Count) sources -> $out"
& g++ -std=c++20 -O2 -Wall -Wextra @src -o $out -lraylib -lopengl32 -lgdi32 -lwinmm
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
Write-Host "build OK"

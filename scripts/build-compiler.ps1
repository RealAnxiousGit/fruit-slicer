# Builds dist\FruitSlicerCompiler.exe with Windows' own csc.exe, embedding the committed source tree.
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$dist = Join-Path $root 'dist'
New-Item -ItemType Directory -Force $dist | Out-Null
$zip = Join-Path $dist 'source.zip'
git -C $root archive --format=zip -o $zip HEAD
if (-not $?) { throw 'git archive failed' }
$fw = 'C:\Windows\Microsoft.NET\Framework64\v4.0.30319'
& "$fw\csc.exe" /nologo /optimize+ /target:exe "/out:$dist\FruitSlicerCompiler.exe" `
    "/resource:$zip,source.zip" `
    /reference:System.Windows.Forms.dll "/reference:$fw\System.IO.Compression.dll" "/reference:$fw\System.IO.Compression.FileSystem.dll" `
    (Join-Path $root 'compiler\FruitSlicerCompiler.cs')
if ($LASTEXITCODE -ne 0) { throw 'csc failed' }
Remove-Item $zip
Get-Item "$dist\FruitSlicerCompiler.exe" | Select-Object Name, Length

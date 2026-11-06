# Installs a portable copy of the Azahar emulator into .tools\azahar for testing the game.
$ErrorActionPreference = 'Stop'
$version = '2126.2'
$root = Split-Path $PSScriptRoot -Parent
$dest = Join-Path $root '.tools\azahar'
if (-not (Test-Path (Join-Path $dest 'azahar.exe'))) {
    $tmp = Join-Path $root '.tools\azahar.zip'
    New-Item -ItemType Directory -Force (Split-Path $tmp) | Out-Null
    [Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12
    $url = "https://github.com/azahar-emu/azahar/releases/download/$version/azahar-windows-msvc-$version.zip"
    Write-Output "Downloading $url"
    Invoke-WebRequest -UseBasicParsing -Uri $url -OutFile $tmp
    $unpacked = Join-Path $root '.tools\azahar-unpack'
    Expand-Archive -Force $tmp $unpacked
    $exe = Get-ChildItem $unpacked -Recurse -Filter 'azahar.exe' | Select-Object -First 1
    if (-not $exe) { throw 'azahar.exe was not in the download' }
    Move-Item $exe.Directory.FullName $dest
    Remove-Item -Recurse -Force $unpacked, $tmp -ErrorAction SilentlyContinue
}
# A "user" folder beside the exe makes Azahar portable: settings and the virtual SD card stay here.
New-Item -ItemType Directory -Force (Join-Path $dest 'user') | Out-Null
# Homebrew audio refuses to start without this file. Azahar's emulated DSP doesn't read its contents,
# so a blank placeholder is enough here. Real hardware needs a genuine dump (see README).
$sd3ds = Join-Path $dest 'user\sdmc\3ds'
New-Item -ItemType Directory -Force $sd3ds | Out-Null
$firm = Join-Path $sd3ds 'dspfirm.cdc'
if (-not (Test-Path $firm)) { [IO.File]::WriteAllBytes($firm, (New-Object byte[] 4096)) }
Write-Output "Azahar is ready in $dest"

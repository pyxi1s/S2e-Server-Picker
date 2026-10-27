# Downloads WinLibs (GCC, POSIX threads, UCRT) straight from GitHub and unpacks it with tar.
# Much faster than the winget package, which extracts file by file.
param([Parameter(Mandatory)][string]$Dest)
$ErrorActionPreference = 'Stop'
try {
    [Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12
    $ProgressPreference = 'SilentlyContinue'
    $api = 'https://api.github.com/repos/brechtsanders/winlibs_mingw/releases/latest'
    $rel = Invoke-RestMethod -Uri $api -Headers @{ 'User-Agent' = 's2e-setup' } -TimeoutSec 30
    $asset = $rel.assets | Where-Object {
        $_.name -match '^winlibs-x86_64-posix-seh-gcc-[\d.]+-mingw-w64ucrt-[\d.]+-r\d+\.zip$'
    } | Select-Object -First 1
    if (-not $asset) { throw 'No matching WinLibs zip in the latest release.' }

    New-Item -ItemType Directory -Force -Path $Dest | Out-Null
    $zip = Join-Path $Dest 'mingw.zip'
    $out = Join-Path $Dest 'mingw64'
    if (Test-Path $out) { cmd /c rmdir /s /q "`"$out`"" }

    & curl.exe -fL --retry 3 --connect-timeout 20 -o $zip $asset.browser_download_url
    if ($LASTEXITCODE -ne 0) { throw 'Download failed.' }

    Write-Host '      Extracting (this is the slow part, antivirus scans every file)...'
    & tar.exe -xf $zip -C $Dest
    if ($LASTEXITCODE -ne 0) { throw 'Extract failed.' }

    foreach ($f in 'g++.exe', 'mingw32-make.exe') {
        if (-not (Test-Path (Join-Path $out "bin\$f"))) { throw "$f missing after extract." }
    }
    New-Item -ItemType File -Path (Join-Path $out '.ok') -Force | Out-Null
    Remove-Item $zip -Force -ErrorAction SilentlyContinue
    exit 0
}
catch {
    Write-Host "      [warn] $($_.Exception.Message)"
    exit 1
}

param(
    [Parameter(Mandatory = $true)]
    [string]$Exe
)

$ErrorActionPreference = "Stop"

$Exe = (Resolve-Path -LiteralPath $Exe).Path
$Name = "Deadlock Server Picker"
$Wt = Get-Command wt.exe -ErrorAction SilentlyContinue

if ($Wt) {
    $Target = $Wt.Source
    $Arguments = "new-tab --title `"$Name`" `"$Exe`""
} else {
    $Target = $Exe
    $Arguments = ""
}

$Folders = @(
    [Environment]::GetFolderPath("Desktop"),
    [Environment]::GetFolderPath("Programs")
)
$Shell = New-Object -ComObject WScript.Shell

foreach ($Folder in $Folders) {
    $Path = Join-Path $Folder "$Name.lnk"
    $Link = $Shell.CreateShortcut($Path)
    $Link.TargetPath = $Target
    $Link.Arguments = $Arguments
    $Link.WorkingDirectory = Split-Path -Parent $Exe
    $Link.IconLocation = "$Exe,0"
    $Link.Description = "Block or allow Deadlock server locations"
    $Link.Save()

    $Bytes = [System.IO.File]::ReadAllBytes($Path)
    $Bytes[0x15] = $Bytes[0x15] -bor 0x20
    [System.IO.File]::WriteAllBytes($Path, $Bytes)

    Write-Host "Created $Path"
}

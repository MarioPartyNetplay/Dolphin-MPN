# Generate AchievementApprovedHash.h from ApprovedInis.json
param(
    [Parameter(Mandatory = $true)]
    [string]$JsonFile,
    [Parameter(Mandatory = $true)]
    [string]$TemplateFile,
    [Parameter(Mandatory = $true)]
    [string]$OutputFile
)

$ErrorActionPreference = "Stop"

$hashBytes = [System.Security.Cryptography.SHA1]::Create().ComputeHash(
    [System.IO.File]::ReadAllBytes($JsonFile))
$hashHex = [BitConverter]::ToString($hashBytes).Replace("-", "").ToLower()

$template = Get-Content $TemplateFile -Raw -Encoding UTF8
$output = $template.Replace("@ACHIEVEMENT_APPROVED_LIST_HASH@", $hashHex)

$outputDir = Split-Path -Parent $OutputFile
if ($outputDir -and -not (Test-Path $outputDir)) {
    New-Item -ItemType Directory -Path $outputDir -Force | Out-Null
}

$output | Out-File -FilePath $OutputFile -Encoding UTF8 -NoNewline

Write-Host "Generated $OutputFile from $JsonFile"

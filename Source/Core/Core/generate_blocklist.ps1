# Generate ChatBlocklist.h from blocklist.txt
param(
    [string]$InputFile = "blocklist.txt",
    [string]$OutputFile = "ChatBlocklist.h"
)

$ErrorActionPreference = "Stop"

# Read blocklist file
$lines = Get-Content $InputFile -Encoding UTF8

# Generate header content
$headerContent = "// Auto-generated from blocklist.txt`n"
$headerContent += "#pragma once`n`n"
$headerContent += "#include <unordered_set>`n"
$headerContent += "#include <string>`n`n"
$headerContent += "namespace NetPlay`n"
$headerContent += "{`n"
$headerContent += "inline const std::unordered_set<std::string>& GetChatBlocklist()`n"
$headerContent += "{`n"
$headerContent += "  static const std::unordered_set<std::string> blocked_words = {`n"

foreach ($line in $lines) {
    $stripped = $line.Trim()
    if ($stripped) {
        $headerContent += "    `"$stripped`",`n"
    }
}

$headerContent += "  };`n"
$headerContent += "  return blocked_words;`n"
$headerContent += "}`n"
$headerContent += "}  // namespace NetPlay`n"

# Ensure output directory exists
$outputDir = Split-Path -Parent $OutputFile
if ($outputDir -and -not (Test-Path $outputDir)) {
    New-Item -ItemType Directory -Path $outputDir -Force | Out-Null
}

# Write header file
$headerContent | Out-File -FilePath $OutputFile -Encoding UTF8 -NoNewline

Write-Host "Generated $OutputFile from $InputFile"


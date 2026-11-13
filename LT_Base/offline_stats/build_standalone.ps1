<#
.SYNOPSIS
  Erzeugt eine eigenständige 1-Datei-HTML-Version der Offline-Statistik (offline_stats_standalone.html).
#>
$ErrorActionPreference = 'Stop'

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$baseDir   = Split-Path -Parent $scriptDir
$dataDir   = Join-Path $baseDir 'data'

$indexPath      = Join-Path $scriptDir 'index.html'
$offlineCssPath = Join-Path $scriptDir 'offline_stats.css'
$offlineJsPath  = Join-Path $scriptDir 'offline_stats.js'
$styleCssPath   = Join-Path $dataDir   'style.css'
$statsJsPath    = Join-Path $dataDir   'stats.js'
$outPath        = Join-Path $scriptDir 'offline_stats_standalone.html'

foreach ($p in @($indexPath, $offlineCssPath, $offlineJsPath, $styleCssPath, $statsJsPath)) {
  if (-not (Test-Path -LiteralPath $p)) {
    throw "Benötigte Datei nicht gefunden: $p"
  }
}

$utf8 = [System.Text.UTF8Encoding]::new($false)
$html       = [System.IO.File]::ReadAllText($indexPath, $utf8)
$styleCss   = [System.IO.File]::ReadAllText($styleCssPath, $utf8) -replace '(?i)</style>', '<\/style>'
$offlineCss = [System.IO.File]::ReadAllText($offlineCssPath, $utf8) -replace '(?i)</style>', '<\/style>'
$statsJs    = [System.IO.File]::ReadAllText($statsJsPath, $utf8) -replace '(?i)</script>', '<\/script>'
$offlineJs  = [System.IO.File]::ReadAllText($offlineJsPath, $utf8) -replace '(?i)</script>', '<\/script>'

function Replace-TagWithInline {
  param(
    [string]$Source,
    [string]$Pattern,
    [string]$Replacement
  )
  $regex = [regex]::new($Pattern, [System.Text.RegularExpressions.RegexOptions]::IgnoreCase)
  if (-not $regex.IsMatch($Source)) {
    throw "Tag-Muster nicht in index.html gefunden: $Pattern"
  }
  return $regex.Replace($Source, { param($m) $Replacement }, 1)
}

$html = Replace-TagWithInline -Source $html `
  -Pattern '<link\s+rel="stylesheet"\s+href="\.\./data/style\.css[^"]*">' `
  -Replacement "<style>`n$styleCss`n  </style>"

$html = Replace-TagWithInline -Source $html `
  -Pattern '<link\s+rel="stylesheet"\s+href="offline_stats\.css[^"]*">' `
  -Replacement "<style>`n$offlineCss`n  </style>"

$html = Replace-TagWithInline -Source $html `
  -Pattern '<script\s+src="\.\./data/stats\.js[^"]*"></script>' `
  -Replacement "<script>`n$statsJs`n  </script>"

$html = Replace-TagWithInline -Source $html `
  -Pattern '<script\s+src="offline_stats\.js[^"]*"></script>' `
  -Replacement "<script>`n$offlineJs`n  </script>"

[System.IO.File]::WriteAllText($outPath, $html, $utf8)
$sizeKb = [Math]::Round((Get-Item -LiteralPath $outPath).Length / 1024, 1)
Write-Host "Erstellt: $outPath ($sizeKb KB)" -ForegroundColor Green

Set-StrictMode -Version 2
$ErrorActionPreference = "Stop"

$root = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot ".."))
$localManifestPath = Join-Path $PSScriptRoot "manifest.sha256"
$sourcePath = Join-Path $root "src\A-TAS 4.0.cpp"
$apiUrl = "https://api.github.com/repos/mint20170603/A-TAS/releases/latest"
$tempDir = $null

function Get-ATasVersion([string]$path) {
    $match = [regex]::Match([IO.File]::ReadAllText($path), '#define\s+A_TAS_VERSION\s+(\d+)')
    if (-not $match.Success) {
        throw "Cannot read A_TAS_VERSION from $path"
    }
    return [long]$match.Groups[1].Value
}

function Get-ManagedPath([string]$base, [string]$relative) {
    if ([IO.Path]::IsPathRooted($relative)) {
        throw "Invalid manifest path: $relative"
    }
    $basePrefix = [IO.Path]::GetFullPath($base).TrimEnd([char[]]'\/') + [IO.Path]::DirectorySeparatorChar
    $fullPath = [IO.Path]::GetFullPath((Join-Path $base $relative))
    if (-not $fullPath.StartsWith($basePrefix, [StringComparison]::OrdinalIgnoreCase)) {
        throw "Manifest path escapes the A-TAS directory: $relative"
    }
    return $fullPath
}

function Read-Manifest([string]$path) {
    $items = @{}
    foreach ($line in Get-Content -LiteralPath $path) {
        if ([string]::IsNullOrWhiteSpace($line)) {
            continue
        }
        if ($line -notmatch '^([0-9A-Fa-f]{64})\s{2}(.+)$') {
            throw "Invalid manifest line: $line"
        }
        $relative = $Matches[2].Trim().Replace('\', '/')
        Get-ManagedPath $root $relative | Out-Null
        if ($relative -match '^(?i:(\.git|replay)(/|$)|settings\.dat$|keybindings\.ini$)') {
            throw "Protected path in manifest: $relative"
        }
        if ($items.ContainsKey($relative)) {
            throw "Duplicate manifest path: $relative"
        }
        $items[$relative] = $Matches[1].ToUpperInvariant()
    }
    if ($items.Count -eq 0) {
        throw "Manifest is empty: $path"
    }
    return $items
}

function Assert-Manifest([string]$base, [hashtable]$items) {
    foreach ($item in $items.GetEnumerator()) {
        $path = Get-ManagedPath $base $item.Key
        if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
            throw "Managed file is missing: $($item.Key)"
        }
        if ((Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash -ne $item.Value) {
            throw "Managed file was modified: $($item.Key)"
        }
    }
}

try {
    if (Get-Process -Name "A-TAS-Manager" -ErrorAction SilentlyContinue) {
        Write-Warning "A-TAS Manager is already running. Update skipped."
        exit 2
    }
    if (Test-Path -LiteralPath (Join-Path $root ".git")) {
        Write-Host "Git checkout detected. Use git pull to update A-TAS."
        return
    }

    $localVersion = Get-ATasVersion $sourcePath
    [Net.ServicePointManager]::SecurityProtocol =
        [Net.ServicePointManager]::SecurityProtocol -bor [Net.SecurityProtocolType]::Tls12
    $release = Invoke-RestMethod -Uri $apiUrl -Headers @{ "User-Agent" = "A-TAS-Updater" } -TimeoutSec 10
    $tag = [string]$release.tag_name
    if ($tag -notmatch '^\d{12}$') {
        throw "Invalid release tag: $tag"
    }
    $remoteVersion = [long]$tag
    if ($remoteVersion -le $localVersion) {
        return
    }

    Write-Host "A-TAS update available: $localVersion -> $remoteVersion"
    if ($release.body) {
        Write-Host $release.body
    }
    $answer = Read-Host "Install this update? [y/N]"
    if (-not [string]::Equals($answer, "y", [StringComparison]::OrdinalIgnoreCase)) {
        return
    }

    if (-not (Test-Path -LiteralPath $localManifestPath -PathType Leaf)) {
        throw "Local manifest.sha256 is missing. Install the updater baseline manually."
    }
    $localManifest = Read-Manifest $localManifestPath
    Assert-Manifest $root $localManifest

    $zipAsset = $release.assets | Where-Object { $_.name -eq "A-TAS-4.0.zip" } | Select-Object -First 1
    $manifestAsset = $release.assets | Where-Object { $_.name -eq "manifest.sha256" } | Select-Object -First 1
    if ($null -eq $zipAsset -or $null -eq $manifestAsset) {
        throw "Release assets A-TAS-4.0.zip or manifest.sha256 are missing."
    }

    $tempDir = Join-Path ([IO.Path]::GetTempPath()) ("A-TAS-update-" + [guid]::NewGuid().ToString("N"))
    $packageDir = Join-Path $tempDir "package"
    $zipPath = Join-Path $tempDir "A-TAS-4.0.zip"
    $remoteManifestPath = Join-Path $tempDir "manifest.sha256"
    New-Item -ItemType Directory -Path $packageDir -Force | Out-Null
    Invoke-WebRequest -Uri $zipAsset.browser_download_url -Headers @{ "User-Agent" = "A-TAS-Updater" } -OutFile $zipPath -UseBasicParsing -TimeoutSec 120
    Invoke-WebRequest -Uri $manifestAsset.browser_download_url -Headers @{ "User-Agent" = "A-TAS-Updater" } -OutFile $remoteManifestPath -UseBasicParsing -TimeoutSec 30
    Expand-Archive -LiteralPath $zipPath -DestinationPath $packageDir -Force

    $remoteManifest = Read-Manifest $remoteManifestPath
    foreach ($required in @("app/A-TAS-Manager.exe", "app/libavz.dll", "src/A-TAS 4.0.cpp")) {
        if (-not $remoteManifest.ContainsKey($required)) {
            throw "Required file is not managed by the release: $required"
        }
    }
    Assert-Manifest $packageDir $remoteManifest
    if ((Get-ATasVersion (Join-Path $packageDir "src\A-TAS 4.0.cpp")) -ne $remoteVersion) {
        throw "Package version does not match release tag $tag."
    }
    foreach ($item in $remoteManifest.GetEnumerator()) {
        $destination = Get-ManagedPath $root $item.Key
        if (-not $localManifest.ContainsKey($item.Key) -and (Test-Path -LiteralPath $destination)) {
            throw "An unmanaged local file would be overwritten: $($item.Key)"
        }
    }

    $backupDir = Join-Path $tempDir "backup"
    New-Item -ItemType Directory -Path $backupDir -Force | Out-Null
    Copy-Item -LiteralPath $localManifestPath -Destination (Join-Path $backupDir "manifest.sha256")
    $backedUp = New-Object 'System.Collections.Generic.List[string]'
    $newPaths = New-Object 'System.Collections.Generic.List[string]'
    try {
        foreach ($item in $remoteManifest.GetEnumerator()) {
            $destination = Get-ManagedPath $root $item.Key
            if (Test-Path -LiteralPath $destination -PathType Leaf) {
                $backup = Get-ManagedPath $backupDir $item.Key
                New-Item -ItemType Directory -Path (Split-Path $backup) -Force | Out-Null
                Copy-Item -LiteralPath $destination -Destination $backup
                $backedUp.Add($item.Key)
            } else {
                $newPaths.Add($item.Key)
            }
        }
        foreach ($item in $remoteManifest.GetEnumerator()) {
            $source = Get-ManagedPath $packageDir $item.Key
            $destination = Get-ManagedPath $root $item.Key
            New-Item -ItemType Directory -Path (Split-Path $destination) -Force | Out-Null
            Copy-Item -LiteralPath $source -Destination $destination -Force
        }
        Copy-Item -LiteralPath $remoteManifestPath -Destination $localManifestPath -Force
    } catch {
        foreach ($relative in $backedUp) {
            Copy-Item -LiteralPath (Get-ManagedPath $backupDir $relative) -Destination (Get-ManagedPath $root $relative) -Force
        }
        foreach ($relative in $newPaths) {
            $path = Get-ManagedPath $root $relative
            if (Test-Path -LiteralPath $path -PathType Leaf) {
                Remove-Item -LiteralPath $path -Force
            }
        }
        Copy-Item -LiteralPath (Join-Path $backupDir "manifest.sha256") -Destination $localManifestPath -Force
        throw
    }
    Write-Host "A-TAS updated to $remoteVersion."
} catch {
    Write-Warning ("A-TAS update skipped: " + $_.Exception.Message)
} finally {
    if ($tempDir -and (Test-Path -LiteralPath $tempDir)) {
        Remove-Item -LiteralPath $tempDir -Recurse -Force
    }
}

#Requires -Version 7.0
[CmdletBinding(SupportsShouldProcess = $true, ConfirmImpact = 'Medium')]
param(
    [string]$PackagePath,
    [string]$InstallRoot,
    [string]$BackupRoot,
    [switch]$Rollback,
    [string]$BackupPath
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.IO.Compression.FileSystem

$script:ProductName = 'Modular FX Suite'
$script:VendorName = 'SkellAudio'
$script:BundleName = 'Modular FX Suite.vst3'
$script:ModuleRelativePath = 'Contents/x86_64-win/Modular FX Suite.vst3'
$script:ModuleInfoRelativePath = 'Contents/Resources/moduleinfo.json'
$script:BackupSchema = 'skell-modularfxsuite-backup/1'

function Get-LocalDataRoot {
    if (-not [string]::IsNullOrWhiteSpace($env:LOCALAPPDATA)) {
        return [IO.Path]::GetFullPath($env:LOCALAPPDATA)
    }
    $candidate = [Environment]::GetFolderPath([Environment+SpecialFolder]::LocalApplicationData)
    if ([string]::IsNullOrWhiteSpace($candidate)) { throw 'LOCALAPPDATA is not available for a per-user install.' }
    return [IO.Path]::GetFullPath($candidate)
}

function Assert-NoReparsePoints {
    param([Parameter(Mandatory)][string]$Path)
    $root = Get-Item -LiteralPath $Path -Force -ErrorAction Stop
    if (($root.Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0) {
        throw "Refusing a reparse-point path: $Path"
    }
    $pending = [System.Collections.Generic.Stack[string]]::new()
    $pending.Push($root.FullName)
    while ($pending.Count -gt 0) {
        $current = $pending.Pop()
        foreach ($item in (Get-ChildItem -LiteralPath $current -Force -ErrorAction Stop)) {
            if (($item.Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0) {
                throw "Refusing a tree containing a reparse point: $($item.FullName)"
            }
            if ($item.PSIsContainer) { $pending.Push($item.FullName) }
        }
    }
}

function Assert-PathInside {
    param(
        [Parameter(Mandatory)][string]$Root,
        [Parameter(Mandatory)][string]$Candidate,
        [string]$Description = 'Path'
    )
    $rootFull = [IO.Path]::GetFullPath($Root).TrimEnd([IO.Path]::DirectorySeparatorChar, [IO.Path]::AltDirectorySeparatorChar)
    $candidateFull = [IO.Path]::GetFullPath($Candidate)
    $prefix = $rootFull + [IO.Path]::DirectorySeparatorChar
    if (-not $candidateFull.StartsWith($prefix, [StringComparison]::OrdinalIgnoreCase)) {
        throw "$Description is outside its approved root."
    }
}

function Assert-NoDuplicateJsonKeys {
    param(
        [Parameter(Mandatory)][System.Text.Json.JsonElement]$Element,
        [string]$Location = '$'
    )
    switch ($Element.ValueKind) {
        Object {
            $seen = [System.Collections.Generic.HashSet[string]]::new([StringComparer]::Ordinal)
            foreach ($property in $Element.EnumerateObject()) {
                if (-not $seen.Add($property.Name)) {
                    throw "Duplicate JSON key '$($property.Name)' at $Location"
                }
                Assert-NoDuplicateJsonKeys -Element $property.Value -Location "$Location.$($property.Name)"
            }
        }
        Array {
            $index = 0
            foreach ($child in $Element.EnumerateArray()) {
                Assert-NoDuplicateJsonKeys -Element $child -Location "$Location[$index]"
                $index++
            }
        }
    }
}

function Read-StrictJsonObject {
    param(
        [Parameter(Mandatory)][string]$Text,
        [Parameter(Mandatory)][string]$Label
    )
    $document = $null
    try {
        $document = [System.Text.Json.JsonDocument]::Parse($Text)
        Assert-NoDuplicateJsonKeys -Element $document.RootElement -Location $Label
        $value = ConvertFrom-Json -InputObject $Text -AsHashtable -Depth 100 -ErrorAction Stop
        if ($value -isnot [System.Collections.IDictionary]) {
            throw "$Label JSON root must be an object."
        }
        return $value
    } catch {
        throw "Invalid $Label JSON: $($_.Exception.Message)"
    } finally {
        if ($null -ne $document) { $document.Dispose() }
    }
}

function Get-EntryText {
    param([Parameter(Mandatory)][System.IO.Compression.ZipArchiveEntry]$Entry)
    $stream = $Entry.Open()
    try {
        $reader = [IO.StreamReader]::new($stream, [Text.UTF8Encoding]::new($false, $true), $true)
        try { return $reader.ReadToEnd() }
        finally { $reader.Dispose() }
    } finally {
        $stream.Dispose()
    }
}

function Get-EntrySha256 {
    param([Parameter(Mandatory)][System.IO.Compression.ZipArchiveEntry]$Entry)
    $stream = $Entry.Open()
    $sha = [Security.Cryptography.SHA256]::Create()
    try {
        return ([Convert]::ToHexString($sha.ComputeHash($stream))).ToLowerInvariant()
    } finally {
        $sha.Dispose()
        $stream.Dispose()
    }
}

function Get-EntryPrefix {
    param(
        [Parameter(Mandatory)][System.IO.Compression.ZipArchiveEntry]$Entry,
        [int]$Count = 512
    )
    $stream = $Entry.Open()
    try {
        $buffer = [byte[]]::new($Count)
        $read = 0
        while ($read -lt $Count) {
            $chunk = $stream.Read($buffer, $read, $Count - $read)
            if ($chunk -le 0) { throw "Truncated Windows module: $($Entry.FullName)" }
            $read += $chunk
        }
        return ,$buffer
    } finally {
        $stream.Dispose()
    }
}

function Assert-Pe64Bytes {
    param(
        [Parameter(Mandatory)][byte[]]$Bytes,
        [string]$Label
    )
    if ($Bytes.Length -lt 512 -or $Bytes[0] -ne 0x4d -or $Bytes[1] -ne 0x5a) {
        throw "VST3 module is not a valid Windows PE file: $Label"
    }
    $peOffset = [BitConverter]::ToInt32($Bytes, 0x3c)
    if ($peOffset -lt 64 -or $peOffset + 26 -gt $Bytes.Length) { throw "Invalid PE header offset: $Label" }
    if ($Bytes[$peOffset] -ne 0x50 -or $Bytes[$peOffset + 1] -ne 0x45 -or
        $Bytes[$peOffset + 2] -ne 0 -or $Bytes[$peOffset + 3] -ne 0) {
        throw "PE signature is missing: $Label"
    }
    $machine = [BitConverter]::ToUInt16($Bytes, $peOffset + 4)
    $magic = [BitConverter]::ToUInt16($Bytes, $peOffset + 24)
    if ($machine -ne 0x8664 -or $magic -ne 0x20b) {
        throw "VST3 module must be AMD64 PE32+ (machine 0x8664, magic 0x20b): $Label"
    }
}

function Assert-ModuleIdentity {
    param(
        [Parameter(Mandatory)][System.Collections.IDictionary]$ModuleInfo,
        [string]$ExpectedVersion
    )
    if ($ModuleInfo['Name'] -ne $script:ProductName) { throw "VST3 identity must be $($script:ProductName)." }
    $version = [string]$ModuleInfo['Version']
    if ([string]::IsNullOrWhiteSpace($version)) { throw 'VST3 moduleinfo has no version.' }
    if (-not [string]::IsNullOrWhiteSpace($ExpectedVersion) -and $version -ne $ExpectedVersion) {
        throw "VST3 moduleinfo version $version does not match package version $ExpectedVersion."
    }
    $factory = $ModuleInfo['Factory Info']
    if ($factory -isnot [System.Collections.IDictionary] -or $factory['Vendor'] -ne $script:VendorName) {
        throw "VST3 Factory Info.Vendor must be $($script:VendorName)."
    }
    $classes = @($ModuleInfo['Classes'])
    if ($classes.Count -ne 2) { throw 'VST3 moduleinfo must contain exactly one processor and one controller class.' }
    $ids = [System.Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
    $categories = [System.Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
    foreach ($class in $classes) {
        if ($class -isnot [System.Collections.IDictionary]) { throw 'VST3 class entry must be an object.' }
        $cid = [string]$class['CID']
        if ($cid -notmatch '^[0-9A-Fa-f]{32}$') { throw 'VST3 class ID must contain exactly 32 hexadecimal characters.' }
        if (-not $ids.Add($cid)) { throw "Duplicate VST3 class ID: $cid" }
        if ($class['Name'] -ne $script:ProductName) { throw 'VST3 class name does not match the product.' }
        if (($class.Keys -contains 'Vendor') -and $class['Vendor'] -ne $script:VendorName) { throw 'VST3 class vendor does not match the product.' }
        if (($class.Keys -contains 'Version') -and [string]$class['Version'] -ne $version) { throw 'VST3 class version does not match the product.' }
        $category = [string]$class['Category']
        if ([string]::IsNullOrWhiteSpace($category)) { throw 'VST3 class category is missing.' }
        [void]$categories.Add($category)
    }
    if (-not $categories.Contains('Audio Module Class') -or
        -not $categories.Contains('Component Controller Class')) {
        throw 'VST3 metadata must contain one Audio Module Class and one Component Controller Class.'
    }
    return $version
}

function Get-TreeSha256 {
    param([Parameter(Mandatory)][string]$Path)
    $root = [IO.Path]::GetFullPath($Path)
    if (-not (Test-Path -LiteralPath $root -PathType Container)) { throw "Bundle directory missing: $root" }
    Assert-NoReparsePoints -Path $root
    $files = @(Get-ChildItem -LiteralPath $root -File -Recurse -Force)
    if ($files.Count -eq 0) { throw "Bundle contains no files: $root" }
    $records = foreach ($file in ($files | Sort-Object { [IO.Path]::GetRelativePath($root, $_.FullName) })) {
        $relative = [IO.Path]::GetRelativePath($root, $file.FullName).Replace('\', '/')
        $hash = (Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256).Hash.ToLowerInvariant()
        "$hash  $relative"
    }
    $payload = [Text.Encoding]::UTF8.GetBytes(($records -join [Environment]::NewLine) + [Environment]::NewLine)
    $sha = [Security.Cryptography.SHA256]::Create()
    try { return ([Convert]::ToHexString($sha.ComputeHash($payload))).ToLowerInvariant() }
    finally { $sha.Dispose() }
}

function Test-InstalledBundle {
    param([Parameter(Mandatory)][string]$BundlePath)
    Assert-NoReparsePoints -Path $BundlePath
    $moduleInfoPath = Join-Path $BundlePath $script:ModuleInfoRelativePath
    $modulePath = Join-Path $BundlePath $script:ModuleRelativePath
    if (-not (Test-Path -LiteralPath $moduleInfoPath -PathType Leaf)) { throw "VST3 moduleinfo.json is missing: $BundlePath" }
    if (-not (Test-Path -LiteralPath $modulePath -PathType Leaf)) { throw "Expected Windows x64 module is missing: $BundlePath" }
    $moduleInfo = Read-StrictJsonObject -Text ([IO.File]::ReadAllText($moduleInfoPath)) -Label 'moduleinfo'
    $version = Assert-ModuleIdentity -ModuleInfo $moduleInfo
    $bytes = [IO.File]::ReadAllBytes($modulePath)
    Assert-Pe64Bytes -Bytes $bytes -Label $modulePath
    return [pscustomobject]@{ Version = $version; TreeSha256 = Get-TreeSha256 -Path $BundlePath }
}

function Get-PackagePlan {
    param([Parameter(Mandatory)][string]$PackagePath)
    $packageFull = [IO.Path]::GetFullPath($PackagePath)
    if (-not (Test-Path -LiteralPath $packageFull -PathType Leaf)) { throw "Preview package not found: $packageFull" }
    $packageHash = (Get-FileHash -LiteralPath $packageFull -Algorithm SHA256).Hash.ToLowerInvariant()
    $manifestPath = [IO.Path]::ChangeExtension($packageFull, '.manifest.json')
    $sidecarPath = [IO.Path]::ChangeExtension($packageFull, '.sha256')
    if (-not (Test-Path -LiteralPath $manifestPath -PathType Leaf)) { throw 'Adjacent release manifest is missing.' }
    if (-not (Test-Path -LiteralPath $sidecarPath -PathType Leaf)) { throw 'Adjacent SHA-256 sidecar is missing.' }

    $sidecar = [IO.File]::ReadAllText($sidecarPath).Trim()
    if ($sidecar -notmatch '^([0-9a-fA-F]{64})  ([^\r\n]+)$') { throw 'Release SHA-256 sidecar is malformed.' }
    if ($Matches[2] -ne [IO.Path]::GetFileName($packageFull) -or $Matches[1].ToLowerInvariant() -ne $packageHash) {
        throw 'Release SHA-256 sidecar does not match the VST3 package.'
    }
    $releaseManifest = Read-StrictJsonObject -Text ([IO.File]::ReadAllText($manifestPath)) -Label 'release manifest'
    if ($releaseManifest['archive'] -ne [IO.Path]::GetFileName($packageFull) -or
        $releaseManifest['archive_sha256'] -ne $packageHash) {
        throw 'Release manifest does not match the VST3 package archive.'
    }

    $zip = [IO.Compression.ZipFile]::OpenRead($packageFull)
    try {
        $entries = [System.Collections.Generic.Dictionary[string, System.IO.Compression.ZipArchiveEntry]]::new([StringComparer]::Ordinal)
        $names = [System.Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
        $roots = [System.Collections.Generic.HashSet[string]]::new([StringComparer]::Ordinal)
        [long]$totalBytes = 0
        foreach ($entry in $zip.Entries) {
            $name = $entry.FullName
            if ([string]::IsNullOrWhiteSpace($name) -or $name.StartsWith('/') -or
                $name.Contains('\') -or $name.Contains(':') -or $name.EndsWith('/')) {
                throw "Unsafe or unexpected directory ZIP entry: $name"
            }
            $parts = $name.Split('/')
            if ($parts -contains '..' -or $parts -contains '.' -or $parts -contains '') {
                throw "Unsafe ZIP path: $name"
            }
            if (-not $names.Add($name)) { throw "Duplicate or case-colliding ZIP path: $name" }
            $unixType = ($entry.ExternalAttributes -shr 16) -band 0xF000
            if ($unixType -eq 0xA000) { throw "ZIP contains a symbolic link: $name" }
            if ($name -match '(?i)\.(exe|zip|7z)$') { throw "Unexpected executable or nested archive: $name" }
            $totalBytes += $entry.Length
            if ($entry.Length -gt 268435456 -or $totalBytes -gt 536870912) {
                throw 'Preview package exceeds the 256 MiB per-file / 512 MiB total safety limit.'
            }
            [void]$roots.Add(($parts[0]))
            $entries.Add($name, $entry)
        }

        foreach ($requiredRoot in @($script:BundleName, 'build-info.json', 'PREVIEW-ONLY.txt', 'SHA256SUMS.txt')) {
            if (-not $roots.Contains($requiredRoot)) { throw "Required package root is missing: $requiredRoot" }
        }
        if ($roots.Count -ne 4) { throw "Unexpected package roots: $($roots -join ', ')" }

        $checksumText = Get-EntryText -Entry $entries['SHA256SUMS.txt']
        $checksums = [System.Collections.Generic.Dictionary[string, string]]::new([StringComparer]::Ordinal)
        foreach ($line in ($checksumText -split '\r?\n')) {
            if ([string]::IsNullOrWhiteSpace($line)) { continue }
            if ($line -notmatch '^([0-9a-fA-F]{64})  (.+)$') { throw "Malformed SHA256SUMS entry: $line" }
            $relative = $Matches[2]
            $safeParts = $relative.Split('/')
            if ($relative.StartsWith('/') -or $relative.Contains('\') -or $relative.Contains(':') -or
                $safeParts -contains '..' -or $safeParts -contains '.' -or $safeParts -contains '') {
                throw "Unsafe checksum path: $relative"
            }
            if ($checksums.ContainsKey($relative)) { throw "Duplicate checksum path: $relative" }
            $checksums.Add($relative, $Matches[1].ToLowerInvariant())
        }
        if ($checksums.Count -ne ($entries.Count - 1)) {
            throw 'SHA256SUMS.txt must cover every package file except itself exactly once.'
        }
        foreach ($entryName in $entries.Keys) {
            if ($entryName -eq 'SHA256SUMS.txt') { continue }
            if (-not $checksums.ContainsKey($entryName)) { throw "Package file is not covered by SHA256SUMS.txt: $entryName" }
            if ((Get-EntrySha256 -Entry $entries[$entryName]) -ne $checksums[$entryName]) {
                throw "Package file SHA-256 mismatch: $entryName"
            }
        }

        $buildInfo = Read-StrictJsonObject -Text (Get-EntryText -Entry $entries['build-info.json']) -Label 'build-info'
        foreach ($pair in @(
            @('product', $script:ProductName),
            @('vendor', $script:VendorName),
            @('format', 'VST3'),
            @('platform', 'windows-x64'),
            @('architecture', 'AMD64')
        )) {
            if ($buildInfo[$pair[0]] -ne $pair[1]) { throw "Build provenance field $($pair[0]) is invalid." }
        }
        if ($buildInfo['preview_only'] -ne $true -or [int]$buildInfo['pluginval_strictness_required'] -lt 5 -or
            [int]$releaseManifest['pluginval_strictness_required'] -lt 5) {
            throw 'Build provenance and release manifest must identify a private preview validated at pluginval strictness 5 or higher.'
        }
        if ([string]$buildInfo['source_commit'] -notmatch '^[0-9a-fA-F]{40}$' -or
            [string]$buildInfo['ci_checkout_commit'] -notmatch '^[0-9a-fA-F]{40}$') {
            throw 'Build provenance is missing a full source or CI checkout commit.'
        }

        $previewText = Get-EntryText -Entry $entries['PREVIEW-ONLY.txt']
        if ($previewText -notmatch 'NOT APPROVED FOR COMMERCIAL DISTRIBUTION') {
            throw 'PREVIEW-ONLY.txt does not carry the required non-commercial warning.'
        }

        $moduleInfoPath = "$($script:BundleName)/$($script:ModuleInfoRelativePath)"
        $modulePath = "$($script:BundleName)/$($script:ModuleRelativePath)"
        if (-not $entries.ContainsKey($moduleInfoPath) -or -not $entries.ContainsKey($modulePath)) {
            throw 'Expected VST3 moduleinfo.json or Windows x64 module is missing.'
        }
        $moduleEntries = @($entries.Keys | Where-Object { $_.EndsWith('.vst3', [StringComparison]::OrdinalIgnoreCase) })
        if ($moduleEntries.Count -ne 1 -or $moduleEntries[0] -ne $modulePath) {
            throw 'Package must contain exactly one Windows VST3 module binary at the expected path.'
        }
        foreach ($entryName in $entries.Keys) {
            if ($entryName -match '/[^/]+\\.vst3/') { throw "Nested VST3 bundle is not allowed: $entryName" }
        }
        $moduleInfo = Read-StrictJsonObject -Text (Get-EntryText -Entry $entries[$moduleInfoPath]) -Label 'moduleinfo'
        $version = Assert-ModuleIdentity -ModuleInfo $moduleInfo -ExpectedVersion ([string]$buildInfo['version'])
        if ($releaseManifest['product'] -ne $script:ProductName -or
            $releaseManifest['vendor'] -ne $script:VendorName -or
            $releaseManifest['version'] -ne $version -or
            $releaseManifest['format'] -ne 'VST3' -or
            $releaseManifest['platform'] -ne 'windows-x64' -or
            $releaseManifest['architecture'] -ne 'AMD64' -or
            $releaseManifest['preview_only'] -ne $true -or
            $releaseManifest['source_commit'] -ne $buildInfo['source_commit'] -or
            $releaseManifest['bundle'] -ne $script:BundleName -or
            $releaseManifest['binary'] -ne $modulePath -or
            $releaseManifest['binary_sha256'] -ne $checksums[$modulePath]) {
            throw 'Release manifest identity, provenance or module checksum does not match the package.'
        }

        $manifestFiles = [System.Collections.Generic.Dictionary[string, string]]::new([StringComparer]::Ordinal)
        foreach ($record in @($releaseManifest['files'])) {
            $path = [string]$record['path']
            $hash = [string]$record['sha256']
            if ([string]::IsNullOrWhiteSpace($path) -or $hash -notmatch '^[0-9a-fA-F]{64}$' -or
                $manifestFiles.ContainsKey($path)) {
                throw 'Release manifest file list is malformed or contains duplicates.'
            }
            $manifestFiles.Add($path, $hash.ToLowerInvariant())
        }
        if ($manifestFiles.Count -ne $checksums.Count) { throw 'Release manifest file count does not match SHA256SUMS.txt.' }
        foreach ($path in $checksums.Keys) {
            if (-not $manifestFiles.ContainsKey($path) -or $manifestFiles[$path] -ne $checksums[$path]) {
                throw "Release manifest checksum does not match SHA256SUMS.txt: $path"
            }
        }

        Assert-Pe64Bytes -Bytes (Get-EntryPrefix -Entry $entries[$modulePath]) -Label $modulePath
        $prefix = "$($script:BundleName)/"
        $bundlePaths = [System.Collections.Generic.List[string]]::new()
        foreach ($path in $checksums.Keys) {
            if ($path.StartsWith($prefix, [StringComparison]::Ordinal)) { $bundlePaths.Add($path) }
        }
        $bundlePaths.Sort([StringComparer]::Ordinal)
        $treeRecords = foreach ($path in $bundlePaths) {
            "$($checksums[$path])  $($path.Substring($prefix.Length))"
        }
        if (@($treeRecords).Count -lt 2) { throw 'VST3 bundle contains too few files.' }
        $treePayload = [Text.Encoding]::UTF8.GetBytes(($treeRecords -join [Environment]::NewLine) + [Environment]::NewLine)
        $treeSha = [Security.Cryptography.SHA256]::Create()
        try { $bundleHash = ([Convert]::ToHexString($treeSha.ComputeHash($treePayload))).ToLowerInvariant() }
        finally { $treeSha.Dispose() }

        return [pscustomobject]@{
            PackagePath = $packageFull
            PackageSha256 = $packageHash
            Version = $version
            SourceCommit = [string]$buildInfo['source_commit']
            BundleTreeSha256 = $bundleHash
        }
    } finally {
        $zip.Dispose()
    }
}

function Copy-BundleFromPackage {
    param(
        [Parameter(Mandatory)][string]$PackagePath,
        [Parameter(Mandatory)][string]$StageRoot
    )
    $prefix = "$($script:BundleName)/"
    $bundleRoot = Join-Path $StageRoot $script:BundleName
    $bundleFull = [IO.Path]::GetFullPath($bundleRoot).TrimEnd([IO.Path]::DirectorySeparatorChar) + [IO.Path]::DirectorySeparatorChar
    $zip = [IO.Compression.ZipFile]::OpenRead($PackagePath)
    try {
        foreach ($entry in $zip.Entries) {
            if (-not $entry.FullName.StartsWith($prefix, [StringComparison]::Ordinal)) { continue }
            $relative = $entry.FullName.Substring($prefix.Length)
            $destination = [IO.Path]::GetFullPath((Join-Path $bundleRoot ($relative.Replace('/', [IO.Path]::DirectorySeparatorChar))))
            if (-not $destination.StartsWith($bundleFull, [StringComparison]::OrdinalIgnoreCase)) {
                throw "Extraction path escaped the staging bundle: $($entry.FullName)"
            }
            $parent = Split-Path -Parent $destination
            New-Item -ItemType Directory -Path $parent -Force | Out-Null
            $sourceStream = $entry.Open()
            $targetStream = [IO.File]::Open($destination, [IO.FileMode]::CreateNew, [IO.FileAccess]::Write, [IO.FileShare]::None)
            try { $sourceStream.CopyTo($targetStream) }
            finally {
                $targetStream.Dispose()
                $sourceStream.Dispose()
            }
        }
    } finally {
        $zip.Dispose()
    }
}

function Write-JsonFile {
    param([Parameter(Mandatory)][string]$Path, [Parameter(Mandatory)][System.Collections.IDictionary]$Value)
    $temporary = $Path + '.tmp'
    $json = ConvertTo-Json -InputObject $Value -Depth 12
    [IO.File]::WriteAllText($temporary, $json, [Text.UTF8Encoding]::new($false))
    Move-Item -LiteralPath $temporary -Destination $Path -Force
}

function Invoke-Install {
    param(
        [Parameter(Mandatory)][string]$PackagePath,
        [Parameter(Mandatory)][string]$InstallRoot,
        [Parameter(Mandatory)][string]$BackupRoot,
        [Parameter(Mandatory)][System.Management.Automation.PSCmdlet]$Cmdlet
    )
    $plan = Get-PackagePlan -PackagePath $PackagePath
    $installFull = [IO.Path]::GetFullPath($InstallRoot)
    $backupFull = [IO.Path]::GetFullPath($BackupRoot)
    $target = Join-Path $installFull $script:BundleName
    $previous = $null
    if (Test-Path -LiteralPath $target) {
        if (-not (Test-Path -LiteralPath $target -PathType Container)) { throw 'A non-directory item already occupies the VST3 destination.' }
        $previous = Test-InstalledBundle -BundlePath $target
        if ($previous.TreeSha256 -eq $plan.BundleTreeSha256) {
            Write-Host "$($script:ProductName) $($plan.Version) is already installed. No files changed."
            return
        }
    }
    if (-not $Cmdlet.ShouldProcess($target, "Install $($script:ProductName) $($plan.Version)")) { return }

    New-Item -ItemType Directory -Path $installFull -Force | Out-Null
    $id = [guid]::NewGuid().ToString('N')
    $backupDirectory = $null
    if ($null -ne $previous) {
        if (Test-Path -LiteralPath $backupFull) {
            if (-not (Test-Path -LiteralPath $backupFull -PathType Container)) { throw 'Backup root is not a directory.' }
            Assert-NoReparsePoints -Path $backupFull
        } else { New-Item -ItemType Directory -Path $backupFull -Force | Out-Null }
        $backupDirectory = Join-Path $backupFull ("previous-" + (Get-Date).ToUniversalTime().ToString('yyyyMMddTHHmmssfffZ') + '-' + $id)
        New-Item -ItemType Directory -Path $backupDirectory | Out-Null
        Copy-Item -LiteralPath $target -Destination $backupDirectory -Recurse
        $backupBundle = Join-Path $backupDirectory $script:BundleName
        if ((Get-TreeSha256 -Path $backupBundle) -ne $previous.TreeSha256) {
            Remove-Item -LiteralPath $backupDirectory -Recurse -Force
            throw 'Previous installation backup verification failed; the installed bundle was not touched.'
        }
    }

    $stageRoot = Join-Path $installFull ('.skelly-modularfxsuite-stage-' + $id)
    $swapPath = Join-Path $installFull ('.skelly-modularfxsuite-previous-' + $id)
    New-Item -ItemType Directory -Path $stageRoot | Out-Null
    $oldMoved = $false
    $newMoved = $false
    $committed = $false
    try {
        Copy-BundleFromPackage -PackagePath $plan.PackagePath -StageRoot $stageRoot
        $stageBundle = Join-Path $stageRoot $script:BundleName
        $staged = Test-InstalledBundle -BundlePath $stageBundle
        if ($staged.TreeSha256 -ne $plan.BundleTreeSha256 -or $staged.Version -ne $plan.Version) {
            throw 'Staged VST3 bundle does not match the validated package.'
        }
        if (Test-Path -LiteralPath $target) {
            Move-Item -LiteralPath $target -Destination $swapPath
            $oldMoved = $true
        }
        Move-Item -LiteralPath $stageBundle -Destination $target
        $newMoved = $true
        $installed = Test-InstalledBundle -BundlePath $target
        if ($installed.TreeSha256 -ne $plan.BundleTreeSha256) { throw 'Installed VST3 hash differs from the validated package.' }

        if ($null -ne $previous) {
            $backupManifest = [ordered]@{
                schema = $script:BackupSchema
                product = $script:ProductName
                bundle = $script:BundleName
                install_root = $installFull
                previous_version = $previous.Version
                previous_bundle_sha256 = $previous.TreeSha256
                installed_version = $installed.Version
                installed_bundle_sha256 = $installed.TreeSha256
                package_sha256 = $plan.PackageSha256
                source_commit = $plan.SourceCommit
                created_utc = (Get-Date).ToUniversalTime().ToString('o')
            }
            Write-JsonFile -Path (Join-Path $backupDirectory 'backup-manifest.json') -Value $backupManifest
        }
        $committed = $true
    } catch {
        if ($newMoved -and (Test-Path -LiteralPath $target)) {
            if ($null -ne $previous) {
                $failedRoot = Join-Path $backupFull ('failed-install-' + $id)
                New-Item -ItemType Directory -Path $failedRoot -Force | Out-Null
                Move-Item -LiteralPath $target -Destination $failedRoot
            } else {
                Remove-Item -LiteralPath $target -Recurse -Force
            }
        }
        if ($oldMoved -and (Test-Path -LiteralPath $swapPath) -and -not (Test-Path -LiteralPath $target)) {
            Move-Item -LiteralPath $swapPath -Destination $target
        }
        if (-not $committed -and $null -ne $backupDirectory -and (Test-Path -LiteralPath $backupDirectory)) {
            Remove-Item -LiteralPath $backupDirectory -Recurse -Force
        }
        throw
    } finally {
        if (Test-Path -LiteralPath $stageRoot) { Remove-Item -LiteralPath $stageRoot -Recurse -Force }
    }
    if ($committed -and (Test-Path -LiteralPath $swapPath)) {
        Remove-Item -LiteralPath $swapPath -Recurse -Force
    }
    Write-Host "Installed $($script:ProductName) $($plan.Version) to $target"
    if ($null -ne $backupDirectory) { Write-Host "Verified previous-version backup: $backupDirectory" }
    else { Write-Host 'No prior installation existed; rollback to an older version is not available.' }
}

function Invoke-Rollback {
    param(
        [Parameter(Mandatory)][string]$BackupPath,
        [Parameter(Mandatory)][string]$InstallRoot,
        [Parameter(Mandatory)][string]$BackupRoot,
        [Parameter(Mandatory)][System.Management.Automation.PSCmdlet]$Cmdlet
    )
    $backupFull = [IO.Path]::GetFullPath($BackupPath)
    $backupRootFull = [IO.Path]::GetFullPath($BackupRoot)
    Assert-PathInside -Root $backupRootFull -Candidate $backupFull -Description 'Backup path'
    if (Test-Path -LiteralPath $backupRootFull) { Assert-NoReparsePoints -Path $backupRootFull }
    if (-not (Test-Path -LiteralPath $backupFull -PathType Container)) { throw "Backup directory not found: $backupFull" }
    Assert-NoReparsePoints -Path $backupFull
    $manifestPath = Join-Path $backupFull 'backup-manifest.json'
    if (-not (Test-Path -LiteralPath $manifestPath -PathType Leaf)) { throw 'Backup manifest is missing.' }
    $manifest = Read-StrictJsonObject -Text ([IO.File]::ReadAllText($manifestPath)) -Label 'backup manifest'
    if ($manifest['schema'] -ne $script:BackupSchema -or $manifest['product'] -ne $script:ProductName -or
        $manifest['bundle'] -ne $script:BundleName) {
        throw 'Backup manifest schema or product identity is not supported.'
    }

    $installFull = [IO.Path]::GetFullPath($InstallRoot)
    if (-not [string]::Equals([IO.Path]::GetFullPath([string]$manifest['install_root']), $installFull, [StringComparison]::OrdinalIgnoreCase)) {
        throw 'Backup belongs to a different installation directory.'
    }
    $target = Join-Path $installFull $script:BundleName
    if (-not (Test-Path -LiteralPath $target -PathType Container)) { throw 'Current installed VST3 bundle is missing; refusing rollback.' }
    $current = Test-InstalledBundle -BundlePath $target
    if ($current.TreeSha256 -ne [string]$manifest['installed_bundle_sha256']) {
        throw 'Installed VST3 has changed since the backup was created; refusing to overwrite user or host changes.'
    }

    $backupBundle = Join-Path $backupFull $script:BundleName
    if (-not (Test-Path -LiteralPath $backupBundle -PathType Container)) { throw 'Previous VST3 bundle is missing from the backup.' }
    $previous = Test-InstalledBundle -BundlePath $backupBundle
    if ($previous.TreeSha256 -ne [string]$manifest['previous_bundle_sha256'] -or
        $previous.Version -ne [string]$manifest['previous_version']) {
        throw 'Previous VST3 backup does not match its recorded version and hash.'
    }

    if (-not $Cmdlet.ShouldProcess($target, "Roll back $($script:ProductName) to $($previous.Version)")) { return }
    New-Item -ItemType Directory -Path $installFull -Force | Out-Null
    New-Item -ItemType Directory -Path $backupRootFull -Force | Out-Null
    $id = [guid]::NewGuid().ToString('N')
    $forwardDirectory = Join-Path $backupRootFull ("forward-" + (Get-Date).ToUniversalTime().ToString('yyyyMMddTHHmmssfffZ') + '-' + $id)
    New-Item -ItemType Directory -Path $forwardDirectory | Out-Null
    Copy-Item -LiteralPath $target -Destination $forwardDirectory -Recurse
    $forwardBundle = Join-Path $forwardDirectory $script:BundleName
    if ((Get-TreeSha256 -Path $forwardBundle) -ne $current.TreeSha256) {
        Remove-Item -LiteralPath $forwardDirectory -Recurse -Force
        throw 'Current installation could not be preserved as a forward backup; rollback was not attempted.'
    }
    $forwardManifest = [ordered]@{
        schema = $script:BackupSchema
        product = $script:ProductName
        bundle = $script:BundleName
        install_root = $installFull
        previous_version = $current.Version
        previous_bundle_sha256 = $current.TreeSha256
        installed_version = $previous.Version
        installed_bundle_sha256 = $previous.TreeSha256
        created_utc = (Get-Date).ToUniversalTime().ToString('o')
    }
    Write-JsonFile -Path (Join-Path $forwardDirectory 'backup-manifest.json') -Value $forwardManifest

    $stageRoot = Join-Path $installFull ('.skelly-modularfxsuite-rollback-stage-' + $id)
    $swapPath = Join-Path $installFull ('.skelly-modularfxsuite-rollback-previous-' + $id)
    New-Item -ItemType Directory -Path $stageRoot | Out-Null
    $oldMoved = $false
    $newMoved = $false
    try {
        Copy-Item -LiteralPath $backupBundle -Destination $stageRoot -Recurse
        $stageBundle = Join-Path $stageRoot $script:BundleName
        if ((Get-TreeSha256 -Path $stageBundle) -ne $previous.TreeSha256) { throw 'Rollback staging hash mismatch.' }
        Move-Item -LiteralPath $target -Destination $swapPath
        $oldMoved = $true
        Move-Item -LiteralPath $stageBundle -Destination $target
        $newMoved = $true
        $restored = Test-InstalledBundle -BundlePath $target
        if ($restored.TreeSha256 -ne $previous.TreeSha256) { throw 'Restored VST3 hash mismatch.' }
    } catch {
        if ($newMoved -and (Test-Path -LiteralPath $target)) {
            $failedRoot = Join-Path $backupRootFull ('failed-rollback-' + $id)
            New-Item -ItemType Directory -Path $failedRoot -Force | Out-Null
            Move-Item -LiteralPath $target -Destination $failedRoot
        }
        if ($oldMoved -and (Test-Path -LiteralPath $swapPath) -and -not (Test-Path -LiteralPath $target)) {
            Move-Item -LiteralPath $swapPath -Destination $target
        }
        throw
    } finally {
        if (Test-Path -LiteralPath $stageRoot) { Remove-Item -LiteralPath $stageRoot -Recurse -Force }
    }
    if (Test-Path -LiteralPath $swapPath) { Remove-Item -LiteralPath $swapPath -Recurse -Force }
    Write-Host "Restored $($script:ProductName) $($previous.Version) to $target"
    Write-Host "Newer version preserved for forward recovery: $forwardDirectory"
}

try {
    $localRoot = Get-LocalDataRoot
    if ([string]::IsNullOrWhiteSpace($InstallRoot)) {
        $InstallRoot = Join-Path $localRoot 'Programs\Common\VST3'
    }
    if ([string]::IsNullOrWhiteSpace($BackupRoot)) {
        $BackupRoot = Join-Path $localRoot 'SkellAudio\Backups\Modular FX Suite'
    }
    if ($Rollback) {
        if ([string]::IsNullOrWhiteSpace($BackupPath)) { throw 'Use -Rollback with -BackupPath pointing to a verified previous-version backup directory.' }
        Invoke-Rollback -BackupPath $BackupPath -InstallRoot $InstallRoot -BackupRoot $BackupRoot -Cmdlet $PSCmdlet
    } else {
        if ([string]::IsNullOrWhiteSpace($PackagePath)) { throw 'Use -PackagePath to select a Windows VST3 preview ZIP.' }
        Invoke-Install -PackagePath $PackagePath -InstallRoot $InstallRoot -BackupRoot $BackupRoot -Cmdlet $PSCmdlet
    }
    exit 0
} catch {
    Write-Error $_ -ErrorAction Continue
    exit 1
}

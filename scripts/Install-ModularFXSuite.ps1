[CmdletBinding(SupportsShouldProcess=$true, ConfirmImpact='High', DefaultParameterSetName='Install')]
param(
    [Parameter(Mandatory=$true, ParameterSetName='Install')][string]$PackagePath,
    [Parameter(Mandatory=$true, ParameterSetName='Rollback')][switch]$Rollback,
    [Parameter(Mandatory=$true, ParameterSetName='Rollback')][string]$BackupPath,
    [string]$Vst3Directory = (Join-Path $env:LOCALAPPDATA 'Programs\Common\VST3'),
    [string]$BackupRoot = (Join-Path $env:LOCALAPPDATA 'Skell Audio\Backups\Modular FX Suite')
)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$script:Product = 'Modular FX Suite'
$script:Vendor = 'SkellAudio'
$script:Bundle = 'Modular FX Suite.vst3'
$script:Cids = @('ABCDEF019182FAEB536B656C4D667831','ABCDEF011234ABCD536B656C4D667831')

function Full([string]$p) { [IO.Path]::GetFullPath($p) }
function Under([string]$p,[string]$root) {
    $r=(Full $root).TrimEnd([char[]]@('\','/'))+[IO.Path]::DirectorySeparatorChar
    (Full $p).StartsWith($r,[StringComparison]::OrdinalIgnoreCase)
}
function UserPath([string]$p,[string]$label) {
    if ([string]::IsNullOrWhiteSpace($env:LOCALAPPDATA)) { throw 'LOCALAPPDATA is not set.' }
    $full=Full $p; $root=(Full $env:LOCALAPPDATA).TrimEnd([char[]]@('\','/'))+[IO.Path]::DirectorySeparatorChar
    if (-not $full.StartsWith($root,[StringComparison]::OrdinalIgnoreCase)) { throw "$label must be inside LOCALAPPDATA." }
    $full
}
function NoLinks([string]$p) {
    $probe=Full $p
    while (-not (Test-Path -LiteralPath $probe)) {
        $parent=[IO.Path]::GetDirectoryName($probe); if (-not $parent -or $parent -eq $probe) { break }; $probe=$parent
    }
    while ($probe -and (Test-Path -LiteralPath $probe)) {
        $item=Get-Item -LiteralPath $probe -Force
        if (($item.Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0) { throw "Reparse point not allowed: $probe" }
        $parent=[IO.Path]::GetDirectoryName($probe); if (-not $parent -or $parent -eq $probe) { break }; $probe=$parent
    }
}
function Separate([string]$install,[string]$backups) {
    $i=(Full $install).TrimEnd([char[]]@('\','/')); $b=(Full $backups).TrimEnd([char[]]@('\','/'))
    if ($b.StartsWith($i+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase) -or
        $i.StartsWith($b+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase) -or $i -eq $b) {
        throw 'Install and backup roots must be separate.'
    }
}
function TreeHash([string]$dir) {
    $root=Full $dir
    if (-not (Test-Path -LiteralPath $root -PathType Container)) { throw "Bundle directory missing: $root" }
    foreach ($item in Get-ChildItem -LiteralPath $root -Recurse -Force) { if (($item.Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0) { throw 'Reparse point in bundle.' } }
    $files=@(Get-ChildItem -LiteralPath $root -Recurse -File -Force | Sort-Object FullName)
    if (-not $files.Count) { throw 'Empty VST3 bundle.' }
    $lines=foreach ($f in $files) { $rel=$f.FullName.Substring($root.Length).TrimStart([char[]]@('\','/')).Replace('\','/'); "$rel  $((Get-FileHash -LiteralPath $f.FullName -Algorithm SHA256).Hash.ToLowerInvariant())" }
    $sha=[Security.Cryptography.SHA256]::Create(); try { [Convert]::ToHexString($sha.ComputeHash([Text.Encoding]::UTF8.GetBytes(($lines -join "`n")))).ToLowerInvariant() } finally { $sha.Dispose() }
}
function BundleEvidence([string]$dir) {
    $b=Full $dir; if ([IO.Path]::GetFileName($b) -ne $script:Bundle) { throw 'Unexpected VST3 bundle name.' }; NoLinks $b
    $infoPath=Join-Path $b 'Contents\Resources\moduleinfo.json'; $module=Join-Path $b 'Contents\x86_64-win\Modular FX Suite.vst3'
    if (-not (Test-Path -LiteralPath $infoPath -PathType Leaf) -or -not (Test-Path -LiteralPath $module -PathType Leaf)) { throw 'Required Windows VST3 files are missing.' }
    $modules=@(Get-ChildItem -LiteralPath (Join-Path $b 'Contents') -Recurse -File -Filter '*.vst3')
    if ($modules.Count -ne 1 -or (Full $modules[0].FullName) -ne (Full $module)) { throw 'Expected exactly one Windows VST3 module.' }
    $info=Get-Content -LiteralPath $infoPath -Raw | ConvertFrom-Json
    if ($info.Name -ne $script:Product -or $info.'Factory Info'.Vendor -ne $script:Vendor -or [string]$info.Version -notmatch '^\d+\.\d+\.\d+$') { throw 'VST3 name, vendor or version mismatch.' }
    if (@($info.Classes).Count -ne 2) { throw 'VST3 class list mismatch.' }
    $ids=@($info.Classes | ForEach-Object { ([string]$_.CID).ToUpperInvariant() } | Sort-Object)
    $expected=@($script:Cids | ForEach-Object { $_.ToUpperInvariant() } | Sort-Object)
    if (($ids -join '|') -ne ($expected -join '|')) { throw 'VST3 class IDs changed.' }
    $bytes=[IO.File]::ReadAllBytes($module); if ($bytes.Length -lt 128 -or $bytes[0] -ne 0x4D -or $bytes[1] -ne 0x5A) { throw 'Invalid PE module.' }
    $off=[BitConverter]::ToInt32($bytes,0x3C)
    if ($off -lt 64 -or $off+26 -gt $bytes.Length -or $bytes[$off] -ne 0x50 -or $bytes[$off+1] -ne 0x45 -or $bytes[$off+2] -ne 0 -or $bytes[$off+3] -ne 0 -or [BitConverter]::ToUInt16($bytes,$off+4) -ne 0x8664 -or [BitConverter]::ToUInt16($bytes,$off+24) -ne 0x20B) { throw 'VST3 module is not AMD64 PE32+.' }
    [pscustomobject]@{version=[string]$info.Version; binary=(Get-FileHash -LiteralPath $module -Algorithm SHA256).Hash.ToLowerInvariant(); tree=(TreeHash $b)}
}
function VerifyPackage([string]$path) {
    $zipPath=Full $path; if (-not (Test-Path -LiteralPath $zipPath -PathType Leaf)) { throw 'Package ZIP not found.' }
    $side=[IO.Path]::ChangeExtension($zipPath,'.sha256'); $manifestPath=[IO.Path]::ChangeExtension($zipPath,'.manifest.json')
    if (-not (Test-Path $side) -or -not (Test-Path $manifestPath)) { throw 'Package sidecar or provenance manifest missing.' }
    $sideText=[IO.File]::ReadAllText($side).Trim()
    if ($sideText -notmatch '^(?<hash>[0-9a-fA-F]{64})\s{2}(?<name>[^\r\n]+)$' -or $Matches.name -ne [IO.Path]::GetFileName($zipPath)) { throw 'Malformed or mismatched SHA-256 sidecar.' }
    $archiveHash=(Get-FileHash -LiteralPath $zipPath -Algorithm SHA256).Hash.ToLowerInvariant()
    if ($archiveHash -ne $Matches.hash.ToLowerInvariant()) { throw 'Package archive SHA-256 mismatch.' }
    $manifest=Get-Content -LiteralPath $manifestPath -Raw | ConvertFrom-Json
    if ($manifest.product -ne $script:Product -or $manifest.vendor -ne $script:Vendor -or $manifest.format -ne 'VST3' -or $manifest.platform -ne 'windows-x64' -or $manifest.architecture -ne 'AMD64' -or $manifest.preview_only -ne $true -or $manifest.archive -ne [IO.Path]::GetFileName($zipPath) -or $manifest.archive_sha256 -ne $archiveHash -or [long]$manifest.archive_size_bytes -ne (Get-Item $zipPath).Length) { throw 'Package provenance does not match.' }
    Add-Type -AssemblyName System.IO.Compression.FileSystem
    $z=[IO.Compression.ZipFile]::OpenRead($zipPath); try {
        $seen=[Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase); $roots=[Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase); $total=[long]0
        foreach ($entry in $z.Entries) {
            $n=$entry.FullName; $parts=$n.Split('/')
            if (-not $n -or $n.Contains('\') -or $n.Contains(':') -or $n.StartsWith('/') -or @($parts | Where-Object { -not $_ -or $_ -eq '.' -or $_ -eq '..' }).Count -gt 0) { throw "Unsafe ZIP path: $n" }
            if (-not $seen.Add($n)) { throw "Duplicate ZIP entry: $n" }
            if ((($entry.ExternalAttributes -shr 16) -band 0xF000) -eq 0xA000 -or $n -match '(?i)\.(exe|zip|7z)$') { throw "Unsafe archive entry: $n" }
            $total += [long]$entry.Length; if ($total -gt 536870912) { throw 'Package exceeds 512 MiB uncompressed.' }
            [void]$roots.Add($parts[0])
        }
        $expected=@($script:Bundle,'build-info.json','PREVIEW-ONLY.txt','SHA256SUMS.txt' | Sort-Object); $actual=@($roots | Sort-Object)
        if (($actual -join '|') -ne ($expected -join '|')) { throw 'Unexpected package top-level contents.' }
    } finally { $z.Dispose() }
    $temp=Join-Path ([IO.Path]::GetTempPath()) ('Skell-ModularFX-'+[guid]::NewGuid().ToString('N')); New-Item -ItemType Directory $temp | Out-Null
    try {
        Expand-Archive -LiteralPath $zipPath -DestinationPath $temp -Force
        $build=Get-Content -LiteralPath (Join-Path $temp 'build-info.json') -Raw | ConvertFrom-Json
        if ($build.product -ne $script:Product -or $build.vendor -ne $script:Vendor -or $build.version -ne $manifest.version -or $build.format -ne 'VST3' -or $build.platform -ne 'windows-x64' -or $build.architecture -ne 'AMD64' -or $build.preview_only -ne $true) { throw 'Embedded build provenance mismatch.' }
        $warning=Get-Content -LiteralPath (Join-Path $temp 'PREVIEW-ONLY.txt') -Raw
        if ($warning -notmatch 'PRIVATE PREVIEW ONLY' -or $warning -notmatch 'NOT APPROVED FOR COMMERCIAL DISTRIBUTION') { throw 'Private-preview warning missing.' }
        $checks=@{}; foreach ($line in [IO.File]::ReadAllLines((Join-Path $temp 'SHA256SUMS.txt'))) {
            if ($line -notmatch '^(?<hash>[0-9a-fA-F]{64})  (?<file>.+)$') { throw 'Malformed embedded SHA256SUMS.' }
            $rel=$Matches.file; if ($rel.Contains('\') -or $rel.Contains(':') -or $rel.Split('/') -contains '..' -or $checks.ContainsKey($rel)) { throw 'Unsafe or duplicate SHA256SUMS path.' }; $checks[$rel]=$Matches.hash.ToLowerInvariant()
        }
        $files=@(Get-ChildItem -LiteralPath $temp -Recurse -File | Where-Object { $_.Name -ne 'SHA256SUMS.txt' }); if ($files.Count -ne $checks.Count) { throw 'SHA256SUMS coverage mismatch.' }
        foreach ($f in $files) { $rel=$f.FullName.Substring($temp.Length).TrimStart([char[]]@('\','/')).Replace('\','/'); if (-not $checks.ContainsKey($rel) -or (Get-FileHash $f.FullName -Algorithm SHA256).Hash.ToLowerInvariant() -ne $checks[$rel]) { throw "Package file checksum failed: $rel" } }
        $bundle=Join-Path $temp $script:Bundle; $e=BundleEvidence $bundle; if ($e.version -ne [string]$build.version) { throw 'Bundle/build version mismatch.' }
        [pscustomobject]@{root=$temp; bundle=$bundle; version=$e.version; evidence=$e}
    } catch { Remove-Item -LiteralPath $temp -Recurse -Force -ErrorAction SilentlyContinue; throw }
}
function SaveBackup([string]$bundle,[string]$reason) {
    $e=BundleEvidence $bundle; New-Item -ItemType Directory -Path $BackupRoot -Force | Out-Null
    $folder=Join-Path $BackupRoot ('ModularFXSuite-'+$e.version+'-'+[DateTime]::UtcNow.ToString('yyyyMMddTHHmmssfffZ')+'-'+[guid]::NewGuid().ToString('N').Substring(0,8)); New-Item -ItemType Directory $folder | Out-Null
    try {
        $copy=Join-Path $folder $script:Bundle; Copy-Item -LiteralPath $bundle -Destination $copy -Recurse
        $copied=BundleEvidence $copy; if ($copied.tree -ne $e.tree) { throw 'Backup tree hash mismatch.' }
        [ordered]@{schema_version=1;product=$script:Product;version=$copied.version;binary_sha256=$copied.binary;bundle_tree_sha256=$copied.tree;created_utc=[DateTime]::UtcNow.ToString('o');reason=$reason} | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $folder 'backup-info.json') -Encoding utf8
        $folder
    } catch { Remove-Item -LiteralPath $folder -Recurse -Force -ErrorAction SilentlyContinue; throw }
}
function ReplaceBundle([string]$source,[string]$installRoot,[string]$reason) {
    $src=BundleEvidence $source; New-Item -ItemType Directory -Path $installRoot -Force | Out-Null; NoLinks $installRoot
    $target=Join-Path $installRoot $script:Bundle; $backup=$null; $hasOld=$false
    if (Test-Path -LiteralPath $target) { $oldEvidence=BundleEvidence $target; if ($oldEvidence.tree -eq $src.tree) { return [pscustomobject]@{status='already-installed';version=$src.version;target=$target;backup_path=$null;binary_sha256=$src.binary} }; $backup=SaveBackup $target $reason; $hasOld=$true }
    $id=[guid]::NewGuid().ToString('N'); $stageRoot=Join-Path $installRoot ('.ModularFX-stage-'+$id); $stage=Join-Path $stageRoot $script:Bundle; $old=Join-Path $installRoot ('.ModularFX-old-'+$id); $moved=$false; $new=$false
    try {
        New-Item -ItemType Directory $stageRoot | Out-Null; Copy-Item -LiteralPath $source -Destination $stage -Recurse
        if ((BundleEvidence $stage).tree -ne $src.tree) { throw 'Staged VST3 hash mismatch.' }
        if ($hasOld) { Move-Item -LiteralPath $target -Destination $old; $moved=$true }
        Move-Item -LiteralPath $stage -Destination $target; $new=$true
        $installed=BundleEvidence $target; if ($installed.tree -ne $src.tree) { throw 'Installed VST3 hash mismatch.' }
        if ($moved) { Remove-Item -LiteralPath $old -Recurse -Force }
    } catch {
        if ($new -and (Test-Path $target)) { Move-Item -LiteralPath $target -Destination (Join-Path $installRoot ('.ModularFX-failed-'+$id)) -ErrorAction SilentlyContinue }
        if ($moved -and (Test-Path $old) -and -not (Test-Path $target)) { Move-Item -LiteralPath $old -Destination $target -ErrorAction SilentlyContinue }
        throw
    } finally { if (Test-Path $stageRoot) { Remove-Item -LiteralPath $stageRoot -Recurse -Force -ErrorAction SilentlyContinue } }
    [pscustomobject]@{status='installed';version=$installed.version;target=$target;backup_path=$backup;binary_sha256=$installed.binary}
}

$installRoot=UserPath $Vst3Directory 'VST3 directory'; $backupRoot=UserPath $BackupRoot 'Backup directory'; Separate $installRoot $backupRoot; NoLinks $installRoot; NoLinks $backupRoot
if ($PSCmdlet.ParameterSetName -eq 'Rollback') {
    $selected=Full $BackupPath; if (-not (Under $selected $backupRoot) -or -not (Test-Path -LiteralPath $selected -PathType Container)) { throw 'Rollback backup must be inside BackupRoot.' }; NoLinks $selected
    $info=Get-Content -LiteralPath (Join-Path $selected 'backup-info.json') -Raw | ConvertFrom-Json; $bundle=Join-Path $selected $script:Bundle; $e=BundleEvidence $bundle
    if ($info.schema_version -ne 1 -or $info.product -ne $script:Product -or $info.version -ne $e.version -or $info.binary_sha256 -ne $e.binary -or $info.bundle_tree_sha256 -ne $e.tree) { throw 'Rollback backup failed integrity verification.' }
    $target=Join-Path $installRoot $script:Bundle; if (Test-Path $target) { if ((BundleEvidence $target).tree -eq $e.tree) { [pscustomobject]@{status='already-restored';version=$e.version;target=$target}; return } }
    if ($PSCmdlet.ShouldProcess($target,"Restore $script:Product $($e.version) from verified backup")) { ReplaceBundle $bundle $installRoot 'pre-rollback' } else { [pscustomobject]@{status='what-if';version=$e.version;target=$target} }
} else {
    $pkg=VerifyPackage $PackagePath
    try {
        $target=Join-Path $installRoot $script:Bundle
        if (Test-Path $target) { if ((BundleEvidence $target).tree -eq $pkg.evidence.tree) { [pscustomobject]@{status='already-installed';version=$pkg.version;target=$target}; return } }
        if ($PSCmdlet.ShouldProcess($target,"Install $script:Product $($pkg.version) with verified backup/rollback")) { ReplaceBundle $pkg.bundle $installRoot 'pre-install-or-update' } else { [pscustomobject]@{status='what-if';version=$pkg.version;target=$target} }
    } finally { Remove-Item -LiteralPath $pkg.root -Recurse -Force -ErrorAction SilentlyContinue }
}

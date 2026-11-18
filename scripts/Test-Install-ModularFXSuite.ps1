[CmdletBinding()]
param(
    [Parameter(Mandatory=$true)][string]$PackagePath,
    [Parameter(Mandatory=$true)][string]$InstallerPath,
    [string]$TestRoot = (Join-Path ([System.IO.Path]::GetTempPath()) ('Skell-ModularFX-Install-Test-' + [Guid]::NewGuid().ToString('N')))
)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$script:Assertions = 0

function Assert([bool]$Condition, [string]$Message) {
    $script:Assertions++
    if (-not $Condition) { throw $Message }
}

function Assert-Throws([scriptblock]$Action, [string]$Message) {
    $script:Assertions++
    $didThrow = $false
    try { & $Action | Out-Null } catch { $didThrow = $true }
    if (-not $didThrow) { throw $Message }
}

function Copy-Tree([string]$Source, [string]$Destination) {
    New-Item -ItemType Directory -Path $Destination -Force | Out-Null
    foreach ($item in Get-ChildItem -LiteralPath $Source -Force) {
        Copy-Item -LiteralPath $item.FullName -Destination $Destination -Recurse -Force
    }
}

$archive = [System.IO.Path]::GetFullPath($PackagePath)
$installer = [System.IO.Path]::GetFullPath($InstallerPath)
if (-not (Test-Path -LiteralPath $archive -PathType Leaf)) { throw "Package ZIP is missing: $archive" }
if (-not (Test-Path -LiteralPath $installer -PathType Leaf)) { throw "Installer script is missing: $installer" }
$oldLocalAppData = $env:LOCALAPPDATA
New-Item -ItemType Directory -Path $TestRoot -Force | Out-Null
$env:LOCALAPPDATA = [System.IO.Path]::GetFullPath($TestRoot)

try {
    # WhatIf validates the full package but must not create install or backup paths.
    $whatIfVst3 = Join-Path $TestRoot 'WhatIf\VST3'
    $whatIfBackups = Join-Path $TestRoot 'WhatIf\Backups'
    $dry = & $installer -PackagePath $archive -Vst3Directory $whatIfVst3 -BackupRoot $whatIfBackups -WhatIf
    Assert ($dry.status -eq 'what-if') 'WhatIf did not report a dry run.'
    Assert (-not (Test-Path -LiteralPath $whatIfVst3)) 'WhatIf created the VST3 install directory.'
    Assert (-not (Test-Path -LiteralPath $whatIfBackups)) 'WhatIf created the backup directory.'

    # Fresh install and idempotent repeat.
    $targetRoot = Join-Path $TestRoot 'Install\VST3'
    $backupRoot = Join-Path $TestRoot 'Install\Backups'
    $fresh = & $installer -PackagePath $archive -Vst3Directory $targetRoot -BackupRoot $backupRoot -Confirm:$false
    Assert ($fresh.status -eq 'installed' -and $fresh.version -eq '1.0.0') 'Fresh VST3 install failed.'
    Assert ($null -eq $fresh.backup_path) 'Fresh install unexpectedly created a backup.'
    $installedBundle = Join-Path $targetRoot 'Modular FX Suite.vst3'
    Assert (Test-Path -LiteralPath (Join-Path $installedBundle 'Contents\x86_64-win\Modular FX Suite.vst3') -PathType Leaf) 'Installed VST3 module is missing.'
    $same = & $installer -PackagePath $archive -Vst3Directory $targetRoot -BackupRoot $backupRoot -Confirm:$false
    Assert ($same.status -eq 'already-installed') 'Repeated install was not idempotent.'
    Assert (-not (Test-Path -LiteralPath $backupRoot)) 'Idempotent install created an unnecessary backup.'

    # Simulate a prior compatible version, then verify update backup and rollback.
    $moduleInfoPath = Join-Path $installedBundle 'Contents\Resources\moduleinfo.json'
    $moduleInfo = Get-Content -LiteralPath $moduleInfoPath -Raw | ConvertFrom-Json -ErrorAction Stop
    $moduleInfo.Version = '0.9.0'
    foreach ($class in $moduleInfo.Classes) { if ($class.PSObject.Properties['Version']) { $class.Version = '0.9.0' } }
    $moduleInfo | ConvertTo-Json -Depth 20 | Set-Content -LiteralPath $moduleInfoPath -Encoding utf8
    Set-Content -LiteralPath (Join-Path $installedBundle 'Contents\Resources\legacy-preview-note.txt') -Value 'Protected previous preview' -Encoding utf8

    $updated = & $installer -PackagePath $archive -Vst3Directory $targetRoot -BackupRoot $backupRoot -Confirm:$false
    Assert ($updated.status -eq 'installed' -and $updated.version -eq '1.0.0') 'Update to the package version failed.'
    Assert (-not [string]::IsNullOrWhiteSpace([string]$updated.backup_path)) 'Update did not create a verified backup.'
    Assert (Test-Path -LiteralPath (Join-Path $updated.backup_path 'backup-info.json') -PathType Leaf) 'Backup manifest is missing.'
    Assert (Test-Path -LiteralPath (Join-Path $updated.backup_path 'Modular FX Suite.vst3\Contents\Resources\legacy-preview-note.txt') -PathType Leaf) 'Backup did not preserve the previous bundle tree.'

    $rollbackDry = & $installer -Rollback -BackupPath $updated.backup_path -Vst3Directory $targetRoot -BackupRoot $backupRoot -WhatIf
    Assert ($rollbackDry.status -eq 'what-if') 'Rollback WhatIf did not report a dry run.'
    Assert ((Get-Content -LiteralPath $moduleInfoPath -Raw | ConvertFrom-Json).Version -eq '1.0.0') 'Rollback WhatIf modified the installed version.'

    $rolledBack = & $installer -Rollback -BackupPath $updated.backup_path -Vst3Directory $targetRoot -BackupRoot $backupRoot -Confirm:$false
    Assert ($rolledBack.status -eq 'rolled-back' -and $rolledBack.version -eq '0.9.0') 'Rollback did not restore the previous compatible version.'
    Assert (-not [string]::IsNullOrWhiteSpace([string]$rolledBack.backup_path)) 'Rollback did not preserve the replaced version.'
    Assert ((Get-Content -LiteralPath $moduleInfoPath -Raw | ConvertFrom-Json).Version -eq '0.9.0') 'Restored moduleinfo version is incorrect.'

    # A modified backup must be rejected without changing the current install.
    $tamperedBackup = Join-Path $backupRoot 'tampered-backup'
    Copy-Tree $updated.backup_path $tamperedBackup
    Add-Content -LiteralPath (Join-Path $tamperedBackup 'Modular FX Suite.vst3\Contents\Resources\legacy-preview-note.txt') -Value 'tampered'
    Assert-Throws {
        & $installer -Rollback -BackupPath $tamperedBackup -Vst3Directory $targetRoot -BackupRoot $backupRoot -Confirm:$false
    } 'Tampered rollback backup was accepted.'
    Assert ((Get-Content -LiteralPath $moduleInfoPath -Raw | ConvertFrom-Json).Version -eq '0.9.0') 'Rejected backup changed the installed bundle.'

    # A package whose archive bytes no longer match the external sidecar must be rejected.
    $tamperRoot = Join-Path $TestRoot 'TamperedPackage'
    New-Item -ItemType Directory -Path $tamperRoot | Out-Null
    $tamperedArchive = Join-Path $tamperRoot ([System.IO.Path]::GetFileName($archive))
    Copy-Item -LiteralPath $archive -Destination $tamperedArchive
    Copy-Item -LiteralPath ([System.IO.Path]::ChangeExtension($archive, '.sha256')) -Destination ([System.IO.Path]::ChangeExtension($tamperedArchive, '.sha256'))
    Copy-Item -LiteralPath ([System.IO.Path]::ChangeExtension($archive, '.manifest.json')) -Destination ([System.IO.Path]::ChangeExtension($tamperedArchive, '.manifest.json'))
    [System.IO.File]::AppendAllText($tamperedArchive, 'tampered')
    $tamperedTarget = Join-Path $TestRoot 'TamperedInstall\VST3'
    Assert-Throws {
        & $installer -PackagePath $tamperedArchive -Vst3Directory $tamperedTarget -BackupRoot (Join-Path $TestRoot 'TamperedInstall\Backups') -Confirm:$false
    } 'Archive with a stale SHA-256 sidecar was accepted.'
    Assert (-not (Test-Path -LiteralPath $tamperedTarget)) 'Rejected archive changed the install destination.'

    # Refuse system-wide locations and overlapping install/backup roots.
    $systemVst3 = Join-Path $env:ProgramFiles 'Common Files\VST3'
    Assert-Throws {
        & $installer -PackagePath $archive -Vst3Directory $systemVst3 -BackupRoot (Join-Path $TestRoot 'SystemBackups') -Confirm:$false
    } 'System-wide VST3 installation path was accepted.'
    $nestedVst3 = Join-Path $TestRoot 'Nested\VST3'
    Assert-Throws {
        & $installer -PackagePath $archive -Vst3Directory $nestedVst3 -BackupRoot (Join-Path $nestedVst3 'Backups') -Confirm:$false
    } 'Backup root nested inside the VST3 scan path was accepted.'

    Write-Host "Modular FX Suite per-user install/update/rollback tests: $script:Assertions assertions passed."
} finally {
    $env:LOCALAPPDATA = $oldLocalAppData
    if (Test-Path -LiteralPath $TestRoot) { Remove-Item -LiteralPath $TestRoot -Recurse -Force -ErrorAction SilentlyContinue }
}

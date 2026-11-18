# Install Modular FX Suite — private Windows preview

This guide applies only to the **Windows x64 VST3 private preview**. It is not a commercial release.

## Keep the artifact together

Extract the GitHub Actions preview artifact so these files remain in one directory:

- `ModularFXSuite-Windows-VST3.zip`
- `ModularFXSuite-Windows-VST3.manifest.json`
- `ModularFXSuite-Windows-VST3.sha256`
- `Install-ModularFXSuite.ps1`
- `INSTALL-WINDOWS.md`

The installer requires the adjacent manifest and SHA-256 sidecar; do not rename the files.

## Preview install

PowerShell 7.0 or later is required. Open PowerShell 7 in the extracted artifact directory.

First run the non-mutating validation:

```powershell
pwsh -NoProfile -File .\Install-ModularFXSuite.ps1 -PackagePath .\ModularFXSuite-Windows-VST3.zip -WhatIf
```

Then install:

```powershell
pwsh -NoProfile -File .\Install-ModularFXSuite.ps1 -PackagePath .\ModularFXSuite-Windows-VST3.zip
```

Default destination: `%LOCALAPPDATA%\Programs\Common\VST3`

Default backups: `%LOCALAPPDATA%\SkellAudio\Backups\Modular FX Suite`

No administrator elevation, registry edits, FL Studio configuration edits or system-wide plugin copies are performed.

## Update and rollback

When a valid Modular FX Suite bundle already exists, the installer verifies and copies it to a timestamped backup before swapping the new bundle into place. The installer prints the exact backup directory.

To roll back, use that printed `previous-...` directory:

```powershell
pwsh -NoProfile -File .\Install-ModularFXSuite.ps1 -Rollback -BackupPath "<printed previous-* backup directory>"
```

The rollback command refuses to overwrite the current install if its files differ from the hash recorded at update time. The newer bundle is preserved as a `forward-...` backup for recovery. A first install has no older version to restore.

Custom install and backup roots can be supplied with `-InstallRoot` and `-BackupRoot`; keep rollback paths under the selected backup root.

## Safety and limitations

- The package archive, external manifest, sidecar, internal SHA256SUMS, module identity, processor/controller class IDs, AMD64 PE32+ module and safe archive paths are checked before installation.
- Unknown products, malformed bundles, duplicate class IDs, modified installs, symbolic links/reparse points and checksum mismatches are rejected.
- This helper is not Authenticode-signed. Only use the exact private artifact produced by the repository's successful Actions run.
- CI installer tests do not prove installation on the owner's machine or FL Studio scan/load, project reopen, automation, preset recall, musical A/B, CPU/latency or commercial licensing.
- SkellyHub discovery/update integration remains separate and unverified.

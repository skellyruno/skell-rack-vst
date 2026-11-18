# Modular FX Suite — Windows VST3 preview release

**Status:** private preview only; not approved for commercial distribution.

## Product contract

- Windows x64 VST3 only. AU and Standalone are not part of the release target.
- The existing VST3 manufacturer/plugin codes and product identity remain unchanged.
- JUCE is pinned to 8.0.12. A local `JUCE_PATH` may be supplied for an offline Quick4 build when it contains the same pinned JUCE release.
- The current SkellCore Guided Tour dependency remains pinned to its existing immutable revision; it is not silently moved to a different SDK revision.

## Windows build

```powershell
cmake -S . -B build -A x64 -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --parallel 2
python -m unittest discover -s tests -p 'test_vst3_packager.py' -v
python tools/package_vst3.py --build-root 'build/ModularFXSuite_artefacts/Release/VST3' --output 'ModularFXSuite-Windows-VST3.zip' --expected-name 'Modular FX Suite' --expected-version '1.0.0' --expected-vendor 'SkellAudio'
```

The packager fails closed unless it finds exactly one VST3 bundle with a valid Windows AMD64 PE32+ module, matching product/version/vendor metadata, processor and controller classes with unique VST3 IDs, safe paths, and a valid archive checksum. JUCE trailing commas are normalized only in the staged package copy; the original build-tree bundle is left unchanged.

## Windows private-preview installation

The Actions artifact includes the VST3 ZIP, its provenance manifest and SHA-256 sidecar, `scripts/Install-ModularFXSuite.ps1`, and `docs/WINDOWS-INSTALL.txt`. Extract the outer Actions artifact once and keep these files together. The helper validates the archive sidecar and manifest, every embedded SHA256SUMS entry, exact VST3 identity/class IDs, AMD64 PE32+, and the package layout before installation.

Dry run:

```powershell
pwsh -NoProfile -File .\scripts\Install-ModularFXSuite.ps1 -PackagePath .\ModularFXSuite-Windows-VST3.zip -WhatIf
```

Install after reviewing the dry run:

```powershell
pwsh -NoProfile -File .\scripts\Install-ModularFXSuite.ps1 -PackagePath .\ModularFXSuite-Windows-VST3.zip
```

Rollback from a selected verified backup:

```powershell
pwsh -NoProfile -File .\scripts\Install-ModularFXSuite.ps1 -Rollback -BackupPath '<verified-backup-folder>'
```

The helper is per-user only, defaults to `%LOCALAPPDATA%\Programs\Common\VST3`, requires PowerShell 7, does not elevate or edit the registry, and verifies a timestamped backup before replacing an existing bundle. It stages and rechecks the new bundle before swapping it into place; if the swap fails, it restores the previous directory. Rollback verifies the selected backup and backs up the current installation before restoring it. The Windows CI workflow runs dry-run, fresh-install, idempotence, update, backup, rollback, tamper-rejection, system-path refusal and path-isolation checks against the actual built preview package.

## CI acceptance

The Windows workflow builds the VST3, runs the package-verifier regression suite, packages one flat bundle, exercises the per-user install/update/rollback helper, runs Tracktion pluginval strictness 5 against the extracted bundle that is actually uploaded, then uploads the ZIP, manifest, SHA-256 sidecar, helper and install guide. Preview artifacts are retained for 30 days. Only superseded pull-request checks may be cancelled; main and manual runs are protected.

## Gates that remain separate from CI

- FL Studio scan/load, project reopen, parameter automation, preset recall and saved-state compatibility.
- Musical A/B listening and target-hardware CPU/latency measurements.
- Product/source and dependency licensing/attribution review.
- Owner-approved vendor URL and support email.
- SkellyHub discovery/update integration and rollback across application-managed installs.

A green Windows build, installer self-test or pluginval run is not commercial release approval. Do not publish, sign, deploy, take payments, or change licensing terms from this workflow.

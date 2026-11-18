# Modular FX Suite — Windows VST3 preview release

**Status:** private preview only; not approved for commercial distribution.

## Product contract

- Windows x64 VST3 only. AU and Standalone are excluded from the distributable.
- Existing VST3 manufacturer/plugin codes, product identity, parameter IDs, automation IDs, preset formats and project-state migration remain unchanged.
- JUCE is pinned to 8.0.12. A local `JUCE_PATH` may be supplied for an offline Quick4 build when it contains the same pinned JUCE release.
- The SkellCore Guided Tour dependency remains pinned to its existing immutable revision.

## Reproducible Windows build

```powershell
cmake -S . -B build -A x64 -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --parallel 2
python -m unittest discover -s tests -p "test_vst3_packager.py" -v
python tools/package_vst3.py --build-root "build/ModularFXSuite_artefacts/Release/VST3" --output "ModularFXSuite-Windows-VST3.zip" --expected-name "Modular FX Suite" --expected-version "1.0.0" --expected-vendor "SkellAudio"
python tests/test_installer.py --package-path "ModularFXSuite-Windows-VST3.zip" -v
```

The packager fails closed unless it finds exactly one VST3 bundle with a valid Windows AMD64 PE32+ module, matching product/version/vendor metadata, processor and controller classes with unique VST3 IDs, safe paths, a valid ZIP CRC and per-file SHA-256 records. JUCE trailing commas are normalized only in the staged package copy; the original build-tree bundle is not modified.

The release workflow then runs the installer tests against the exact generated package, runs pluginval strictness 5 against the extracted bundle that will be uploaded, and retains the private preview artifact for 30 days. The separate Guided Tour workflow builds the real JUCE editor and captures four screenshots at the editor's actual aspect ratio.

## Per-user install, update and rollback

The preview artifact contains the package ZIP, external manifest, SHA-256 sidecar, installer script and install guide. Keep those files together when extracting the GitHub Actions artifact.

Preview install (PowerShell 7 or later):

```powershell
pwsh -NoProfile -File .\Install-ModularFXSuite.ps1 -PackagePath .\ModularFXSuite-Windows-VST3.zip -WhatIf
pwsh -NoProfile -File .\Install-ModularFXSuite.ps1 -PackagePath .\ModularFXSuite-Windows-VST3.zip
```

Default destination is the current user's `%LOCALAPPDATA%\Programs\Common\VST3`; no administrator elevation or registry changes are required. The script validates the adjacent archive SHA-256 sidecar and manifest, internal `SHA256SUMS.txt`, strict JSON identity, exact two-class processor/controller inventory, AMD64 PE32+ module, safe ZIP paths, and preview-only marker before writing anything.

When updating an existing valid Modular FX Suite installation, the script copies it to a timestamped backup under `%LOCALAPPDATA%\SkellAudio\Backups\Modular FX Suite`, verifies the backup hash, stages the new bundle, then swaps it into place. Unknown or modified bundles are never overwritten. Use the printed backup directory to roll back:

```powershell
pwsh -NoProfile -File .\Install-ModularFXSuite.ps1 -Rollback -BackupPath "<printed previous-* backup directory>"
```

Rollback checks that the current installation still matches the hash recorded by the update. If the installed files have changed, rollback refuses to overwrite them. The newer version is preserved as a forward-recovery backup. A fresh install has no older version to roll back to.

The installer is a private-preview helper, not a signed commercial installer. It does not modify FL Studio settings, install other products, deploy SkellyHub, or claim host acceptance.

## CI acceptance

- Windows x64 CMake configure and Release VST3 build.
- Packager regression tests.
- Installer `-WhatIf`, fresh install, idempotent repeat, update backup, rollback, forward backup, modified-install refusal, bad checksum, duplicate class ID, wrong product identity, non-AMD64 module and unknown-target preservation tests.
- pluginval strictness 5 against the exact packaged bundle.
- Separate real-editor Guided Tour screenshots at compact and default scale.

## Gates that remain separate from CI

- FL Studio scan/load, project save/reopen, parameter automation, preset recall and saved-state compatibility.
- Musical A/B listening and target-hardware CPU/latency measurements.
- DSP NaN/denormal/clipping/bypass/click regression coverage on real audio.
- Owner-machine installation, host discovery and rollback acceptance.
- Product/source and dependency licensing/attribution review.
- Owner-approved vendor URL and support email.
- SkellyHub install/update/backup/rollback integration.

A green Windows build, installer self-test or pluginval run is not FL Studio acceptance or commercial release approval. Do not publish, sign, deploy, take payments or change licensing terms from this workflow.

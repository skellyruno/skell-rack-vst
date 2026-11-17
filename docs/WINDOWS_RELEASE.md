# Modular FX Suite — Windows VST3 preview release

**Status:** private preview only; not approved for commercial distribution.

## Product contract

- Windows x64 VST3 only. AU and Standalone are not part of the release target.
- The existing VST3 manufacturer/plugin codes and product identity remain unchanged.
- JUCE is pinned to 8.0.12. A local \`JUCE_PATH\` may be supplied for an offline Quick4 build when it contains the same pinned JUCE release.
- The current SkellCore Guided Tour dependency remains pinned to its existing immutable revision; it is not silently moved to a different SDK revision.

## Windows build

\`\`\`powershell
cmake -S . -B build -A x64
cmake --build build --config Release --parallel 2
python -m unittest discover -s tests -p 'test_vst3_packager.py' -v
python tools/package_vst3.py \`
  --build-root 'build/ModularFXSuite_artefacts/Release/VST3' \`
  --output 'ModularFXSuite-Windows-VST3.zip' \`
  --expected-name 'Modular FX Suite' \`
  --expected-version '1.0.0' \`
  --expected-vendor 'SkellAudio'
\`\`\`

The packager fails closed unless it finds exactly one VST3 bundle with a valid Windows AMD64 PE32+ module, matching product/version/vendor metadata, processor and controller classes with unique VST3 IDs, safe paths, and a valid archive checksum. JUCE trailing commas are normalized only in the staged package copy; the original build-tree bundle is left unchanged.

## CI acceptance

The Windows workflow builds the VST3, runs the package-verifier regression suite, packages one flat bundle, runs Tracktion pluginval strictness 5 against the extracted bundle that is actually uploaded, then uploads the ZIP plus manifest and SHA-256 sidecar. Preview artifacts are retained for 30 days. Only superseded pull-request checks may be cancelled; main and manual runs are protected.

## Gates that remain separate from CI

- FL Studio scan/load, project reopen, parameter automation, preset recall and saved-state compatibility.
- Musical A/B listening and target-hardware CPU/latency measurements.
- Product/source and dependency licensing/attribution review.
- Owner-approved vendor URL and support email.
- Installer, SkellyHub installation/update, backup and rollback.

A green Windows build or pluginval run is not commercial release approval. Do not publish, sign, deploy, take payments, or change licensing terms from this workflow.

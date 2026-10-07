# Build and release

This repository contains source in `src/`, checked-in build headers in `third_party/`, and runtime assets in `native/`, `ui/`, `tools/`, `examples/`. The DLL is compiled, not committed.

## Local Windows x64 build

Install Visual Studio 2022 C++ tools, Windows SDK, CMake >=3.25, Python 3 and Node.js. Run from this repository root:

```powershell
cmake -S . -B build -A x64
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
node tests/ui_feedback_tests.cjs
python .github/scripts/package.py --dll build/bin/Endfield.Interaction.dll --output-dir ../artifacts
```

Packaging refuses to overwrite an existing version ZIP. Increment module.json before releasing, preserve old versions, and update the change log. The ZIP includes a freshly built DLL and runtime assets plus license/source notices. It excludes source, build scripts, documentation, private mods and tests.

## GitHub

Upload this directory as the repository root, including `.github`. Push and pull-request builds run on Windows. A successful default-branch push publishes a new version Release; an existing Release is left unchanged. Version tags `v<module.json version>` can also trigger a Release. Alpha versions are marked prerelease. No PAT is required; the release job uses GitHub's built-in token with contents:write.

The workflow compiles and tests before packaging. To make another version, increment the third component for a small update or the second for a major feature update; the first component changes only on the owner's instruction.

Every version update must synchronize the applicable version in the Chinese and English READMEs, including their documentation and repository copies. Update manual content only when user operations, controls or workflows change. Record purely technical changes and validation in updateInfo; keep the rest of the manual unchanged. Do not replace historical/example version numbers globally. Blender is required for FBX conversion on the user's PC, not on the CI build runner. EFMI tests use synthetic fixtures and do not require any installed/private mods.

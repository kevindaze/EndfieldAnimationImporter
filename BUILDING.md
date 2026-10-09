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

## MMD conversion worker

`tools/retarget_vmd.py` uses the pinned source packages in `tools/vendor` inside
an isolated Blender 4.2+ background process. These are packaged runtime dependencies.
`tools/standard_mmd.py` defines an EAI-authored mesh-free generic MMD skeleton.
The worker creates its temporary skeleton internally; no external PMX is required.
Private VMD/PMX/textures and generated output must never be packaged.

Optional integration test (requires Blender and local reference fixtures):
`python tests/vmd_conversion_tests.py --blender <blender.exe> --motion <motion.vmd>
--reference <skeleton-reference.json> --output <test-output>
--validator <build/Release/validate_animation.exe> [--actor controlled]`

This test adds known arm/finger keys and camera/light tracks to a private copy,
checks moving limbs, all 30 finger joints, normalized quaternions and bone-only
timeline duration, and validates generated JSON through the native parser.

Run `python tests/standard_mmd_tests.py` without Blender to check the authored
profile and PMX binary round-trip. The real Blender integration test additionally
checks IK-only motion and rejects camera-only files.

## Controlled / partner animation routing (1.2.0)

Both converters accept `--actor controlled|partner` (default partner). Bindpose
palettes and hierarchy are read from actor 0 or 1 in the game reference. Version
2 animations contain one actor and an explicit target character; controlled
assets must bind to a male/female Endministrator and have no manager action.

Presets keep `animation` / `support_mode` for the partner and add optional
`controlled_animation`, `controlled_pose`, `controlled_support`. Missing fields
default to `none`, `inherit`, `none`, preserving old clip manager poses. The
explicit `none` pose disables that old layer; `imported` uses the separate clip.
The same bank schema / calibration format remains readable by this version.

Playback owns two independent clocks, pause/scrub flags and clip ranges. Unity
writes and actor restoration execute only on Gameplay. Two active clips retain
shared scene placement until both are stopped; each actor's animation overrides
can be stopped separately. Clipping preserves the actor and target binding.

Web requests identify the actor with `actor: "controlled"` (default partner).
Timeline replies/events always use `timeline` for the partner and
`controlled_timeline` for the controlled actor. Numpad presets start both clocks
together. UI calibration preview preserves the other actor's current clock.


## GitHub 自動打包與發布

以 `G:/Project/endfield-mod/git-release` 作為 Git 儲存庫根目錄，上傳此目錄的內容，不要在 GitHub 再包一層 git-release。

1. 在 GitHub 建立空白儲存庫，記下 HTTPS URL。
2. 在 git-release 開啟 PowerShell，執行下列命令，將 URL 換成你的儲存庫：

```powershell
cd G:\Project\endfield-mod\git-release
git init -b main
git add .
git commit -m "Add EAI runtime package and automatic release"
git remote add origin https://github.com/YOUR-NAME/YOUR-REPO.git
git push -u origin main
```

每次分支 push 都會打包；預設分支會發布新版本至 Releases。其他分支僅供 Actions 下載測試 ZIP。也能在 Actions 選擇 Build and release EAI → Run workflow 手動執行。

版本取自 module.json 的 version。例如 1.0.0-Alpha 會產生 EndfieldAnimationImporter-1.0.0-Alpha-win-x64.zip 與 v1.0.0-Alpha Release。Alpha 等後綴自動標記為預發布。以後更新版本號、提交模組檔案並 push 即可。

既有同版本 Release 不修改、不覆蓋；若要發布更新，先更新版本號。每次 push 的 ZIP 仍可在 Actions 該次執行的 Artifacts 下載（下載的外層 artifact ZIP 內含真正模組 ZIP，請取出模組 ZIP 給 Better Endfield 使用）。

流程先在 Windows 編譯 C++ 與執行測試，再封裝新 DLL 及執行資源。原始碼、測試、docs 與外部模型描述不放進 ZIP；必要 motions 資料與第三方授權保留。詳細建置方式參考 repo 的 BUILDING.md。

若 Actions 因組織權限或標籤規則拒絕建立 Release，需在儲存庫／組織設定允許工作流程寫入 contents 與建立版本標籤。首次雲端執行仍須在上傳後確認。

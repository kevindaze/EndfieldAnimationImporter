"""Package only runtime files, always using the explicitly supplied compiled DLL."""
import argparse
import json
import os
from pathlib import Path
import re
import struct
import subprocess
import zipfile

ROOT = Path(__file__).resolve().parents[2]

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--dll', type=Path, required=True)
    parser.add_argument('--output-dir', type=Path, required=True)
    parser.add_argument('--eml-runtime', type=Path, required=True)
    parser.add_argument('--xxmi-dll', type=Path, required=True)
    args = parser.parse_args()
    manifest = json.loads((ROOT / 'module.json').read_text(encoding='utf-8-sig'))
    version = manifest['version']
    if not re.fullmatch(r'\d+\.\d+\.\d+(?:-[A-Za-z0-9]+(?:[.-][A-Za-z0-9]+)*)?', version):
        raise SystemExit('Invalid module version')
    if os.environ.get('GITHUB_REF', '').startswith('refs/tags/') and os.environ['GITHUB_REF'] != 'refs/tags/v' + version:
        raise SystemExit('Tag must match module.json version')
    dll = args.dll.resolve()
    binary = dll.read_bytes()
    if len(binary) < 64 or binary[:2] != b'MZ':
        raise SystemExit('Not a compiled Windows DLL')
    pe = struct.unpack_from('<I', binary, 60)[0]
    if pe + 24 > len(binary) or binary[pe:pe+4] != b'PE\0\0' or struct.unpack_from('<H', binary, pe+4)[0] != 0x8664:
        raise SystemExit('DLL must be Windows x64')
    if version.encode() not in binary:
        raise SystemExit('DLL version does not match module.json; rebuild first')
    executables = {path: dll.with_name(Path(path).name) for path in (
        'EAI.Launcher.exe', 'native/windows-x64/EAI.OfflineHost.exe', 'native/windows-x64/EAI.AnimationImport.exe')}
    for executable in executables.values():
        contents = executable.read_bytes()
        if len(contents) < 64 or contents[:2] != b'MZ':
            raise SystemExit(f'Not a compiled executable: {executable.name}')
        offset = struct.unpack_from('<I', contents, 60)[0]
        if offset + 24 > len(contents) or contents[offset:offset+6] != b'PE\0\0\x64\x86':
            raise SystemExit(f'Executable must be Windows x64: {executable.name}')
    if manifest['libraries']['windows-x64'] != 'native/windows-x64/Endfield.Interaction.dll' or manifest['ui'] != 'ui/index.html':
        raise SystemExit('Unexpected runtime paths')
    files = {'module.json': ROOT / 'module.json', 'native/windows-x64/Endfield.Interaction.dll': dll,
             **executables,
             'ui/index.html': ROOT / 'ui/index.html', 'ui/language-catalog.js': ROOT / 'ui/language-catalog.js', 'ui/language.js': ROOT / 'ui/language.js', 'tools/retarget_fbx.py': ROOT / 'tools/retarget_fbx.py',
             'UFBX-LICENSE.txt': ROOT / 'third_party/ufbx/LICENSE',
             'NLOHMANN-LICENSE.MIT': ROOT / 'third_party/nlohmann/LICENSE.MIT',
             'BETTER-ENDFIELD-LICENSE.txt': ROOT / 'third_party/better-endfield/LICENSE'}
    for name in ('EModLoader.Launcher.exe', 'EModLoader.Host.dll', 'EModLoader.OfflineHost.exe', 'EModLoader.XInputProxy.dll'):
        runtime_file = args.eml_runtime / name
        content = runtime_file.read_bytes()
        if len(content) < 64 or content[:2] != b'MZ':
            raise SystemExit(f'Missing compiled EML runtime: {name}')
        files['runtime/' + name] = runtime_file
    files['EML-LICENSE-AGPL-3.0.txt'] = ROOT / 'third_party/eml/LICENSE'
    files['EML-NOTICE.txt'] = ROOT / 'third_party/eml/NOTICE.txt'
    files['EML-THIRD-PARTY-NOTICES.md'] = ROOT / 'third_party/eml/THIRD_PARTY_NOTICES.md'
    files['MINHOOK-LICENSE.txt'] = ROOT / 'third_party/eml/src/native/vendor/minhook/LICENSE.txt'
    efmi = ROOT / 'third_party/efmi'
    for path in efmi.rglob('*'):
        if path.is_file() and path.name not in {'d3d11.dll', 'patch_live_buffers.py'}:
            files['runtime/efmi/' + path.relative_to(efmi).as_posix()] = path
    xxmi = args.xxmi_dll.read_bytes()
    xxmi_pe = struct.unpack_from('<I', xxmi, 60)[0] if len(xxmi) >= 64 else len(xxmi)
    if (xxmi[:2] != b'MZ' or xxmi[xxmi_pe:xxmi_pe+6] != b'PE\0\0\x64\x86'
            or '.eai-live-buffer'.encode('utf-16-le') not in xxmi
            or b'EaiSubmitMeshFollow' not in xxmi):
        raise SystemExit('Expected the compiled Windows x64 XXMI runtime with EAI live buffers')
    files['runtime/efmi/d3d11.dll'] = args.xxmi_dll
    files['SECONDARY-BODYPHYSICS-LICENSE.txt'] = ROOT / 'third_party/secondary-bodyphysics/LICENSE'
    files['SECONDARY-BODYPHYSICS-NOTICE.md'] = ROOT / 'third_party/secondary-bodyphysics/NOTICE.md'
    motions = ROOT / 'native/windows-x64/motions'
    files['tools/retarget_vmd.py'] = ROOT / 'tools/retarget_vmd.py'
    files['tools/standard_mmd.py'] = ROOT / 'tools/standard_mmd.py'
    # These Python packages are worker runtime dependencies, not user models.
    vendor = ROOT / 'tools/vendor'
    for path in vendor.rglob('*'):
        if path.is_file() and '__pycache__' not in path.parts and path.suffix != '.pyc':
            files[path.relative_to(ROOT).as_posix()] = path
    for name in ('18.asf', '19.asf', '18_01.amc', '18_02.amc', '19_01.amc', '19_02.amc', 'README.md'):
        files['native/windows-x64/motions/' + name] = motions / name
    for name in ('mouthAudio.wav', 'papaAudio.wav'):
        path = 'native/windows-x64/audio/' + name
        files[path] = ROOT / path
    for name in ('arm_raise_diagnostic.interaction-animation.json', 'arm_rotation_test.interaction-animation.json'):
        files['examples/' + name] = ROOT / 'examples' / name
    for name, path in files.items():
        if path.is_symlink() or not path.is_file():
            raise SystemExit('Missing or symlink runtime file: ' + name)
    args.output_dir.mkdir(parents=True, exist_ok=True)
    archive = args.output_dir / f'EndfieldAnimationImporter-{version}-win-x64.zip'
    source_archive = args.output_dir / f'EndfieldAnimationImporter-{version}-source.zip'
    if archive.exists() or source_archive.exists():
        raise SystemExit('Version artifacts already exist')
    source_names = subprocess.check_output(
        ['git', 'ls-files', '-z', '--cached', '--others', '--exclude-standard'],
        cwd=ROOT).decode('utf-8').split('\0')
    with zipfile.ZipFile(source_archive, 'x', compression=zipfile.ZIP_DEFLATED) as source:
        for name in sorted(set(source_names) - {''}):
            path = ROOT / name
            if not path.is_file() or path.is_symlink() or path.suffix.lower() in (
                    '.dll', '.exe', '.pdb', '.log', '.zip', '.fbx', '.blend', '.vmd'):
                continue
            source.write(path, name)
    # Exclusive creation: repeated versions never overwrite earlier artifacts.
    with zipfile.ZipFile(archive, 'x', compression=zipfile.ZIP_DEFLATED) as z:
        z.writestr('mod/', b'')
        z.writestr('runtime/efmi/EAI-LiveSidecars.txt', 'XXMI 1.2.2 with EAI Live Buffers\n')
        for name, path in sorted(files.items()):
            z.write(path, name)
    with zipfile.ZipFile(archive) as z:
        if z.testzip() is not None or set(z.namelist()) != set(files) | {'mod/', 'runtime/efmi/EAI-LiveSidecars.txt'}:
            raise SystemExit('Archive validation failed')
    print(archive)
    if os.environ.get('GITHUB_OUTPUT'):
        with open(os.environ['GITHUB_OUTPUT'], 'a', encoding='utf-8') as out:
            out.write(f'version={version}\narchive={archive.resolve().as_posix()}\nfilename={archive.name}\n')
            out.write(f'source_archive={source_archive.resolve().as_posix()}\n')
            out.write('prerelease=' + ('true' if '-' in version else 'false') + '\n')

if __name__ == '__main__':
    main()

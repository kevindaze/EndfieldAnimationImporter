"""Package only runtime files, always using the explicitly supplied compiled DLL."""
import argparse
import json
import os
from pathlib import Path
import re
import struct
import zipfile

ROOT = Path(__file__).resolve().parents[2]

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--dll', type=Path, required=True)
    parser.add_argument('--output-dir', type=Path, required=True)
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
    if manifest['libraries']['windows-x64'] != 'native/windows-x64/Endfield.Interaction.dll' or manifest['ui'] != 'ui/index.html':
        raise SystemExit('Unexpected runtime paths')
    files = {'module.json': ROOT / 'module.json', 'native/windows-x64/Endfield.Interaction.dll': dll,
             'ui/index.html': ROOT / 'ui/index.html', 'tools/retarget_fbx.py': ROOT / 'tools/retarget_fbx.py',
             'NLOHMANN-LICENSE.MIT': ROOT / 'third_party/nlohmann/LICENSE.MIT',
             'BETTER-ENDFIELD-LICENSE.txt': ROOT / 'third_party/better-endfield/LICENSE'}
    motions = ROOT / 'native/windows-x64/motions'
    for name in ('18.asf', '19.asf', '18_01.amc', '18_02.amc', '19_01.amc', '19_02.amc', 'README.md'):
        files['native/windows-x64/motions/' + name] = motions / name
    for name in ('arm_raise_diagnostic.interaction-animation.json', 'arm_rotation_test.interaction-animation.json'):
        files['examples/' + name] = ROOT / 'examples' / name
    for name, path in files.items():
        if path.is_symlink() or not path.is_file():
            raise SystemExit('Missing or symlink runtime file: ' + name)
    args.output_dir.mkdir(parents=True, exist_ok=True)
    archive = args.output_dir / f'EndfieldAnimationImporter-{version}-win-x64.zip'
    # Exclusive creation: repeated versions never overwrite earlier artifacts.
    with zipfile.ZipFile(archive, 'x', compression=zipfile.ZIP_DEFLATED) as z:
        for name, path in sorted(files.items()):
            z.write(path, name)
    with zipfile.ZipFile(archive) as z:
        if z.testzip() is not None or set(z.namelist()) != set(files):
            raise SystemExit('Archive validation failed')
    print(archive)
    if os.environ.get('GITHUB_OUTPUT'):
        with open(os.environ['GITHUB_OUTPUT'], 'a', encoding='utf-8') as out:
            out.write(f'version={version}\narchive={archive.resolve().as_posix()}\nfilename={archive.name}\n')
            out.write('prerelease=' + ('true' if '-' in version else 'false') + '\n')

if __name__ == '__main__':
    main()

"""Build the bundled XXMI D3D11 runtime from the pinned source archive."""
import argparse
from pathlib import Path
import re
import subprocess
import sys
import tarfile

ROOT = Path(__file__).resolve().parents[2]


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--build-dir', required=True, type=Path)
    parser.add_argument('--msbuild', default='msbuild')
    args = parser.parse_args()
    build = args.build_dir.resolve()
    build.mkdir(parents=True, exist_ok=False)
    with tarfile.open(ROOT / 'third_party/appearance-sources/XXMI-Libs-Package-1.2.2.tar.gz') as archive:
        archive.extractall(build, filter='data')
    source, = build.iterdir()
    subprocess.run([sys.executable, ROOT / 'third_party/efmi/patch_live_buffers.py', source], check=True)
    # The upstream version generator expects a git checkout. This source comes
    # from the pinned upstream tarball, so write its equivalent version header.
    (source / 'version.generated.h').write_text(
        '#define VERSION_MAJOR 1\n#define VERSION_MINOR 2\n'
        '#define VERSION_REVISION 2\n#define VERSION_BUILD 0\n'
        '#define VER_COMPANYNAME_STR "EndfieldAnimationImporter (XXMI a7847ea)"\n'
        '#define VER_COMMENTS_STR "https://github.com/SpectrumQT/XXMI-Libs-Package"\n',
        encoding='ascii')
    project = source / 'DirectX11/DirectX11.vcxproj'
    text = project.read_text(encoding='utf-8-sig')
    text, count = re.subn(r'^ *<Exec Command=".*generate-version\.ps1.*?/>\n', '', text, flags=re.M)
    if count != 1:
        raise SystemExit('Unexpected XXMI version generation step')
    # Embed compiler debug information in objects to avoid the shared PDB
    # writer for this project.
    text = text.replace('<MultiProcessorCompilation>true</MultiProcessorCompilation>',
                        '<DebugInformationFormat>OldStyle</DebugInformationFormat>')
    project.write_text(text, encoding='utf-8')
    subprocess.run([args.msbuild, 'StereovisionHacks.sln', '/t:DirectX11',
                    '/p:Configuration=Zip Release', '/p:Platform=x64', '/m:1',
                    '/nr:false', '/v:quiet', '/fl', '/flp:logfile=build.log;verbosity=normal'],
                   check=True, cwd=source)
    print(source / 'x64/Zip Release/d3d11.dll')


if __name__ == '__main__':
    main()

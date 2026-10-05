"""Package an already-built Windows simulator with its runtime assets."""
from pathlib import Path
import argparse
import hashlib
import os
import re
import shutil
import subprocess
import zipfile

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--toolchain-bin', type=Path)
    args = parser.parse_args()
    project = Path(__file__).resolve().parent.parent
    executable = project / 'bin/main.exe'
    if not executable.is_file():
        raise SystemExit('Build bin/main.exe before packaging')
    objdump = str(args.toolchain_bin / 'objdump.exe') if args.toolchain_bin else shutil.which('objdump')
    if not objdump:
        raise SystemExit('objdump must be available in the existing toolchain PATH')
    system = Path(os.environ['SystemRoot']) / 'System32'
    binaries = {executable.name: executable}
    pending = [executable]
    while pending:
        path = pending.pop()
        output = subprocess.check_output([objdump, '-p', str(path)], text=True)
        for name in re.findall(r'DLL Name:\s*(\S+)', output):
            if name.lower().startswith(('api-ms-', 'ext-ms-')) or (system / name).is_file():
                continue
            if name.lower() in {key.lower() for key in binaries}:
                continue
            local = executable.parent / name
            toolchain = args.toolchain_bin / name if args.toolchain_bin else None
            found = local if local.is_file() else (
                toolchain if toolchain and toolchain.is_file() else shutil.which(name))
            if not found:
                raise SystemExit(f'Missing runtime DLL: {name}')
            dependency = Path(found)
            binaries[name] = dependency
            pending.append(dependency)
    version = (project / 'VERSION').read_text().strip()
    output_dir = args.output.resolve()
    output_dir.mkdir(parents=True, exist_ok=True)
    archive = output_dir / f'BikeComputer-LVGL-v{version}-windows-x64.zip'
    entries = [(path, 'bin/' + name) for name, path in binaries.items()]
    for name in ['assets/maps', 'assets/generated', 'assets/background']:
        folder = project / name
        entries.extend((p, p.relative_to(project).as_posix())
                       for p in folder.rglob('*') if p.is_file())
    for name in ['VERSION', 'README.md', 'CHANGELOG.md', 'licence.txt',
                 '启动模拟器.cmd', f'docs/V{version}_RELEASE_NOTES.md', 'docs/RELEASE_WORKFLOW.md']:
        entries.append((project / name, name))
    entries.append((project / 'lvgl/LICENCE.txt', 'licenses/LVGL-LICENCE.txt'))
    # Public template only; never bundle runtime/ or a user's API key.
    for name in ['config/navigation-online.example.ini', 'docs/导航联网搜索_20261005.md',
                 'docs/导航搜索准确性_20261005.md', 'assets/fonts_src/MiSans-Semibold.ttf']:
        if (project / name).is_file():
            entries.append((project / name, name))
    # Keep the font's copyright notice and the official license with the
    # bundled application resource; these must not be silently omitted.
    for name in ['MiSans-NOTICE.txt', 'MiSans-IP-License.pdf']:
        entries.append((project / 'assets/fonts_src' / name, 'licenses/' + name))
    with zipfile.ZipFile(archive, 'w', zipfile.ZIP_DEFLATED, compresslevel=6) as package:
        for path, name in sorted(entries, key=lambda entry: entry[1]):
            package.write(path, name)
    with archive.open('rb') as stream:
        digest = hashlib.file_digest(stream, 'sha256').hexdigest()
    (output_dir / 'SHA256SUMS.txt').write_text(f'{digest}  {archive.name}\n', encoding='utf-8', newline='\n')
    print(f'Packaged {archive.name}: {archive.stat().st_size} bytes; {len(binaries)} runtime binaries')

if __name__ == '__main__':
    main()

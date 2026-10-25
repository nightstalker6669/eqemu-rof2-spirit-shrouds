#!/usr/bin/env python3
"""Install the spirit shrouds extension over the exact published housing + fellowship + mercenaries baseline."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[1]

def git(target, *args):
    return subprocess.check_output(['git', '-c', 'core.autocrlf=false', '-c', 'core.eol=lf', '-C', str(target), *args], text=True, encoding='utf-8').strip()

def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()

def relative(root, name):
    path = Path(name)
    if path.is_absolute() or '..' in path.parts:
        raise SystemExit('Unsafe package path: ' + name)
    result = root / path
    if result.is_symlink() or not result.resolve().is_relative_to(root.resolve()):
        raise SystemExit('Unsafe filesystem path: ' + name)
    return result

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('checkout', type=Path)
    modes = parser.add_mutually_exclusive_group(required=True)
    modes.add_argument('--check', action='store_true')
    modes.add_argument('--apply', action='store_true')
    args = parser.parse_args()
    manifest = json.loads((ROOT / 'source-manifest.json').read_text(encoding='utf-8'))
    for item in manifest['files']:
        path = relative(ROOT, item['path'])
        if digest(path) != item['sha256']:
            raise SystemExit('Package checksum mismatch: ' + item['path'])
    baseline = json.loads((ROOT / 'patches/base-manifest.json').read_text(encoding='utf-8'))
    target = args.checkout.resolve()
    if Path(git(target, 'rev-parse', '--show-toplevel')).resolve() != target:
        raise SystemExit('Target must be the root of its own EQEmu Git checkout.')
    if git(target, 'rev-parse', 'HEAD') != baseline['upstream_commit']:
        raise SystemExit('Checkout must be at the documented exact upstream commit.')
    expected = baseline['files']
    changed = set(git(target, 'diff', '--name-only', 'HEAD').splitlines())
    changed.update(git(target, 'ls-files', '--others', '--exclude-standard').splitlines())
    if changed != set(expected):
        raise SystemExit('Target must contain exactly the pinned housing + fellowship + mercenaries overlay; see docs/INSTALL.md.')
    for name, value in expected.items():
        path = relative(target, name)
        if not path.is_file() or digest(path) != value:
            raise SystemExit('Prerequisite baseline mismatch: ' + name)
    patch = ROOT / 'patches/eqemu-spirit-shrouds.patch'
    git(target, 'apply', '--check', str(patch))
    files = sorted(p for p in (ROOT / 'overlay').rglob('*') if p.is_file())
    for src in files:
        dest = relative(target, src.relative_to(ROOT / 'overlay').as_posix())
        if dest.exists():
            raise SystemExit('Spirit shroud overlay path already exists: ' + str(dest.relative_to(target)))
    if args.check:
        print('Compatible pinned housing + fellowship + mercenaries baseline; spirit shrouds patch and ' + str(len(files)) + ' production files ready.')
        return
    git(target, 'apply', str(patch))
    for src in files:
        dest = relative(target, src.relative_to(ROOT / 'overlay').as_posix())
        dest.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(src, dest)
    print('Spirit shroud source installed. Review docs/INSTALL.md for SQL and runtime deployment.')
    print('No database, server configuration, binaries or running service was changed.')

if __name__ == '__main__':
    main()

#!/usr/bin/env python3
"""Keep the single delivered Silente app; retire generated app bundles, without backups."""
from pathlib import Path
import shutil
import subprocess
import sys

root = Path(__file__).resolve().parent.parent
keep = root / 'releases/macOS/Silente-Universal.app'
registry = Path('/System/Library/Frameworks/CoreServices.framework/Frameworks/LaunchServices.framework/Support/lsregister')
apply = '--apply' in sys.argv
candidates = []
for name in ('build', 'build-universal', 'build-intel', 'releases/macOS'):
    folder = root / name
    if folder.exists():
        for app in folder.rglob('Silente*.app'):
            if app == keep or keep in app.parents:
                continue
            if not app.resolve().is_relative_to(root):
                raise RuntimeError(f'Unexpected app outside Silente: {app}')
            candidates.append(app)
if apply and not (keep / 'Contents/MacOS/Silente').is_file():
    raise RuntimeError('The delivered app must exist before retiring generated apps')
for app in sorted(candidates, key=lambda path: len(path.parts), reverse=True):
    if not app.exists():
        continue
    size = sum(file.stat().st_size for file in app.rglob('*') if file.is_file())
    print(f'{"Retirado" if apply else "Temporal"}: {app.relative_to(root)} ({size/1048576:.1f} MiB)')
    if apply:
        if registry.exists():
            subprocess.run([str(registry), '-u', str(app)], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        shutil.rmtree(app)
if apply:
    for name in ('build','build-universal','build-intel'):
        generated = root / name / 'Silente_artefacts/JuceLibraryCode/Silente'
        if generated.exists():
            shutil.rmtree(generated)
if apply and registry.exists():
    subprocess.run([str(registry), '-f', str(keep)], check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)

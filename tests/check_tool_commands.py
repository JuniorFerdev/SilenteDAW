#!/usr/bin/env python3
"""Audit the Tool reference against Cubase's read-only 15.0.30 category extract."""
import json
import re
from pathlib import Path

root = Path(__file__).resolve().parent.parent
reference = json.loads((root / 'docs/referencias/cubase15/herramientas-comandos-15.0.30.json').read_text())
source = (root / 'src/app/KeyCommands.cpp').read_text()
section = source.split('cat("Herramientas", "Tools");', 1)[1].split('cat("Transporte", "Transport");', 1)[0]
rows = re.findall(r'add\("([^"]+)", "([^"]+)", "([^"]+)", ([^;\n]+)\);', section)
commands = {identifier: (en, key) for identifier, es, en, key in rows}
assert len(rows) == len(commands) == len(reference['commands']) == 43, 'Duplicate or missing Tool commands'
assert commands.keys() == {row['silente_id'] for row in reference['commands']}, 'Tool catalog differs from Cubase extract'
aliases = {'Select Tool': 'Object Selection Tool', 'Range Tool': 'Range Selection Tool', 'Delete Tool': 'Erase Tool'}
assigned = set()
for row in reference['commands']:
    name, expression = commands[row['silente_id']]
    assert name == aliases.get(row['cubase_name'], row['cubase_name']), row['cubase_name']
    parts = expression.split(',')
    key = parts[0].strip()
    flags = {v.strip() for v in parts[1].split('|')} if len(parts) > 1 else set()
    actual = None if key == '0' else (key[1:-1] if key.startswith("'") else key.removeprefix('K::').removesuffix('Key'))
    if actual is not None:
        actual = ('Option+' if 'A' in flags else '') + ('Shift+' if 'S' in flags else '') + actual
        assert actual not in assigned, f'Duplicate key {actual}'
        assigned.add(actual)
    assert actual == row['key'], f"{name}: {actual!r} differs from Cubase {row['key']!r}"
assert len(assigned) == 13, 'Default Tool assignment set differs from official manual'
actions = set(re.findall(r'k\["([^"]+)"\]\s*=', (root / 'src/MainComponent.cpp').read_text()))
implemented = commands.keys() & actions
print(f'PASS: all {len(commands)} Tool commands present; 13 assignments match Cubase 15.0.30 and no duplicate keys')
print(f'INFO: {len(implemented)} commands have Silente actions; {len(commands) - len(implemented)} remain unavailable')

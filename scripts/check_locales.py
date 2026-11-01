#!/usr/bin/env python3
"""Check catalog parity and coverage of literal i18n::tr calls without using a model."""
from pathlib import Path
import json
import re
root = Path(__file__).resolve().parent.parent
catalogs = {code: json.loads((root / 'resources' / 'locales' / f'{code}.json').read_text()) for code in ('es', 'en')}
assert catalogs['es'].keys() == catalogs['en'].keys(), 'Catalog keys differ'
assert all(value for catalog in catalogs.values() for value in catalog.values()), 'Empty translation'
for source in (root / 'src').rglob('*'):
    if source.suffix not in ('.cpp', '.h'):
        continue
    for literal in re.findall(r'i18n::tr\(("(?:\\.|[^"\\])*")\)', source.read_text()):
        key = json.loads(literal)
        assert all(key in catalog for catalog in catalogs.values()), f'{source}: missing {key!r}'
print(f"PASS: {len(catalogs['es'])} entries per language; all literal calls covered")

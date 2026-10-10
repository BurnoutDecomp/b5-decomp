"""Validate compact geometry references across streaming reuse and owner rehash."""
from pathlib import Path
import argparse
import os
from fxgs_common import Tree, compile_and_run, report

parser = argparse.ArgumentParser()
parser.add_argument('--ignore-generation', action='store_true')
parser.add_argument('--wrap-generation', action='store_true')
args = parser.parse_args()
os.environ.pop('NoDefaultCurrentDirectoryInExePath', None)
path = 'src/pc/gcm/renderengine/GeometryEntryReferences.h'
source = Tree().read(path)
if args.ignore_generation:
    old = 'lrSlot.muGeneration == (luToken >> 32)'
    assert source.count(old) == 1
    source = source.replace(old, 'true')
if args.wrap_generation:
    old = 'if (lrSlot.muGeneration != KU_MAX_GENERATION)'
    assert source.count(old) == 1
    source = source.replace(old, 'if (true)')
    source = source.replace('++lrSlot.muGeneration;',
                            'lrSlot.muGeneration = lrSlot.muGeneration % KU_MAX_GENERATION + 1;')
result = compile_and_run(Path(__file__).with_name('PCGeometryEntryReferences.cpp'),
                         'unused.inc', '', 'PCGeometryEntryReferences', shadow={path: source})
raise SystemExit(report('run_pc_geometry_entry_references', [], result, 11))

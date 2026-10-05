"""Exercise real retained-map registration and source-page eviction, without GPU creation."""
from pathlib import Path
import argparse
import os
from fxgs_common import compile_and_run, report

parser = argparse.ArgumentParser()
parser.add_argument('--legacy', action='store_true')
args = parser.parse_args()
os.environ.pop('NoDefaultCurrentDirectoryInExePath', None)
os.environ['BRN_GEOMETRY_COMPACT_LIFETIME'] = '0' if args.legacy else '1'
result = compile_and_run(Path(__file__).with_name('PCGeometryLifetime.cpp'), 'unused.inc', '',
                         'PCGeometryLifetime', extra_flags='d3d9.lib user32.lib')
raise SystemExit(report('run_pc_geometry_lifetime', [], result, 14))

"""Exercise full identity and lifetime guards under deliberate fingerprint collisions."""
from pathlib import Path
import argparse
import os
from fxgs_common import Tree, compile_and_run, report

parser=argparse.ArgumentParser()
parser.add_argument('--ignore-identity',action='store_true')
parser.add_argument('--ignore-generation',action='store_true')
parser.add_argument('--stale-clear',action='store_true')
args=parser.parse_args()
os.environ.pop('NoDefaultCurrentDirectoryInExePath',None)
source=Tree().read('src/pc/gcm/renderengine/GeometryAssociativeFrontCachePCLeaf.h')
if args.ignore_identity:
    source=source.replace('if (lEqual(lrEntry)) return &lrEntry;', 'return &lrEntry;')
if args.ignore_generation:
    source=source.replace('if (lrSet.muGeneration != luGeneration) return nullptr;', '// negative: stale generation accepted')
if args.stale_clear:
    source=source.replace('void Clear() { maSets = {}; }', 'void Clear() {}')
result=compile_and_run(Path(__file__).with_name('PCGeometryAssociativeFrontCache.cpp'),
    'geometry_assoc_cache.inc',source,'PCGeometryAssociativeFrontCache')
raise SystemExit(report('run_pc_geometry_associative_front_cache',[],result,24))

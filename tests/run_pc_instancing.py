"""Actual D3D9 instancing versus individual-draw pixel and lifetime oracles."""
import argparse, os
from pathlib import Path
from fxgs_common import Tree, compile_and_run, report
os.environ.pop('NoDefaultCurrentDirectoryInExePath',None)
parser=argparse.ArgumentParser()
parser.add_argument('--uniform-matrix',action='store_true',help='negative control: all instances use matrix zero')
parser.add_argument('--explicit-operands-only',action='store_true',help='negative control: ignore implicit matrix register spans')
args=parser.parse_args()
shadow={}
if args.uniform_matrix or args.explicit_operands_only:
    path='src/pc/gcm/renderengine/Instancing.h'
    source=Tree().read(path)
    if args.uniform_matrix:
        before='matrices + static_cast<unsigned>(matrixIndex) * 16'
        assert source.count(before)==1
        source=source.replace(before,'matrices')
    if args.explicit_operands_only:
        before='    unsigned nextTemp = temps + result.fields;'
        assert source.count(before)==1
        source=source.replace(before,'    matrices.clear();\n'+before)
    shadow[path]=source
result=compile_and_run(Path(__file__).with_name('PCInstancing.cpp'),
    'pc_instancing_unused.inc','', 'PCInstancing',shadow=shadow,
    extra_flags='d3d9.lib d3dcompiler.lib user32.lib')
raise SystemExit(report('run_pc_instancing',[],result,18))

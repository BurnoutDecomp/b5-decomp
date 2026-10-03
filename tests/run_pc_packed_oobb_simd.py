"""Original packed-box equations against SSE2 and aliased public matrix products."""
import argparse
import os
import re
from pathlib import Path
from fxgs_common import Tree, compile_and_run, report

os.environ.pop('NoDefaultCurrentDirectoryInExePath', None)
parser = argparse.ArgumentParser()
parser.add_argument('--rev', help='production source revision; 20f522ee reproduces the alias bug')
parser.add_argument('--scalar-control', action='store_true')
parser.add_argument('--benchmark', action='store_true')
parser.add_argument('--skip-normalization', action='store_true')
args = parser.parse_args()
if args.scalar_control:
    os.environ['BRN_PACKED_OOBB_SIMD'] = '0'
else:
    os.environ.pop('BRN_PACKED_OOBB_SIMD', None)
path = 'src/GameShared/GameClasses/Graphics/Dispatch/CgsPackedOobb.cpp'
source = Tree().read(path)
reference = source[source.index('namespace CgsGraphics'):]
reference, guards = re.subn(r'(?ms)^#if defined\(_M_X64\).*?^#endif\s*\n', '', reference)
assert guards == 2
reference = reference.replace('namespace CgsGraphics', 'namespace CgsGraphics { namespace Reference', 1) + '\n}\n'
header = 'src/pc/geometric/PackedOobbSIMDPCLeaf.h'
simd = Tree().read(header)
if args.skip_normalization:
    needle = 'q = _mm_mul_ps(q, _mm_shuffle_ps(inverse, inverse, 0));'
    assert simd.count(needle) == 1
    simd = simd.replace(needle, '(void)inverse;')
flags = []
if args.scalar_control: flags.append('/DPC_PACKED_SCALAR_CONTROL')
if args.benchmark: flags.append('/DPC_PACKED_BENCHMARK')
result = compile_and_run(Path(__file__).with_name('PCPackedOobbSIMD.cpp'),
    'pc_packed_oobb_scalar.inc', reference, 'PCPackedOobbSIMD', extra_flags=' '.join(flags),
    shadow={header: simd}, extra_sources=[Path('packed_production.cpp')],
    extra_files={'packed_production.cpp': Tree(args.rev).read(path)})
raise SystemExit(report('run_pc_packed_oobb_simd', [], result, 13))

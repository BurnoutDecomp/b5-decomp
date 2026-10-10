"""Real four-lane sweep path against analytic cases and its established scalar lowering."""
from pathlib import Path
import argparse
import os
from fxgs_common import REPO, Tree, definition, compile_and_run, report

os.environ.pop('NoDefaultCurrentDirectoryInExePath',None)
parser=argparse.ArgumentParser()
parser.add_argument('--drop-valid-mask',action='store_true')
parser.add_argument('--zero-contact-time',action='store_true')
parser.add_argument('--benchmark',action='store_true')
parser.add_argument('--scalar-fallback',action='store_true')
args=parser.parse_args()
source=Tree().read('src/GameShared/GameClasses/Geometric/Intersection/CgsTriangleSphere.cpp')
start=source.index('namespace CgsGeometric',source.index('THE SWEPT (CONTINUOUS) CONTACT KERNEL'))
reference=source[start:]
start=reference.index('#if defined(_M_X64)')
end=reference.index('#endif',start)+len('#endif')
reference=reference[:start]+reference[end:]
reference=reference.replace('namespace CgsGeometric','namespace CgsGeometric { namespace Reference',1)+'\n}\n'
header='src/pc/geometric/SweptSphereSIMD.h'
text=Tree().read(header)
if args.drop_valid_mask:
    needle='_mm_and_ps(lbAccept.v, Load(lTriangles.mValidMasks).v)'
    assert text.count(needle)==1
    text=text.replace(needle,'lbAccept.v')
if args.zero_contact_time:
    needle='const Float4 lfHitTime = Select(lbFaceHit, lfTEnter, lfSweptTime);'
    assert text.count(needle)==1
    text=text.replace(needle,'const Float4 lfHitTime = 0.0f;')
flags=[]
if args.benchmark: flags.append('/DPC_SWEPT_BENCHMARK')
if args.scalar_fallback: flags.append('/DPC_SWEPT_SCALAR_FALLBACK')
result=compile_and_run(Path(__file__).with_name('PCSweptSphereSIMD.cpp'),'swept_scalar_reference.inc',reference,
    'PCSweptSphereSIMD',shadow={header:text},extra_flags=' '.join(flags),extra_sources=[
        REPO/'src/GameShared/GameClasses/Geometric/Intersection/CgsTriangleSphere.cpp',
        REPO/'src/GameShared/GameClasses/Geometric/Primitives/CgsSweptSphere.cpp',
        REPO/'src/GameShared/GameClasses/Geometric/Primitives/CgsSphere.cpp'])
raise SystemExit(report('run_pc_swept_sphere_simd',[],result,20))

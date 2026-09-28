"""Native filter ABI and original fused recurrences; no game data required."""
import argparse
from pathlib import Path
import sys
sys.dont_write_bytecode = True
from fxgs_common import Tree, definition, compile_and_run, report
parser=argparse.ArgumentParser()
parser.add_argument('--rev')
parser.add_argument('--source-root',type=Path)
args=parser.parse_args()
tree=Tree(args.rev)
def read(path):
    return (args.source_root/path).read_text(encoding='utf-8-sig') if args.source_root else tree.read(path)
source=read('vendor/renderware/src/rw/audio/core/ReverbFilters.cpp')
header=read('vendor/renderware/include/rw/audio/core/ReverbFilters.h')
parts=[definition(source,s) for s in (
 'AllPassFilter *AllPassFilter::AllPassFilter_ctor(',
 'void AllPassFilterFunc(', 'void *AllPassFilter::AllPassFilterApplyFunc(',
 'AllPassFilter *AllPassFilter::AllPassFilterResetFunc(',
 'AllPassFilter *AllPassFilter::SetGains(',
 'CombFilter *CombFilter::CombFilter_ctor(', 'f32 CombFilterFunc(',
 'void *CombFilter::CombFilterApplyFunc(', 'CombFilter *CombFilter::CombFilterResetFunc(',
 'CombFilter *CombFilter::SetGains(')]
numeric=compile_and_run(Path(__file__).with_name('FxReverbDsp.cpp'),'fx_reverb_dsp.inc',
 '\n'.join(parts),'FxReverbDsp',shadow={'src/rw/audio/core/ReverbFilters.h':header})
raise SystemExit(report('run_fx_reverb_dsp',[],numeric,18))

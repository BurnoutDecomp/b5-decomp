"""ARTIST826B4238 golden boost controls, extracted from production code."""
import argparse
from pathlib import Path
from fxgs_common import Tree,definition,compile_and_run,report
p=argparse.ArgumentParser();p.add_argument('--rev');p.add_argument('--source-root',type=Path);a=p.parse_args()
rel='GameSource/Sound/Vehicles/Engines/BrnBoostEffect.cpp'
cpp=(a.source_root/rel).read_text(encoding='utf-8-sig') if a.source_root else Tree(a.rev).read('src/'+rel)
body=definition(cpp,'void BoostEffect::UpdateParams(')
result=compile_and_run(Path(__file__).with_name('FxBoostControls.cpp'),'fx_boost_controls.inc',body,'FxBoostControls')
raise SystemExit(report('run_fx_boost_controls',[],result,28))

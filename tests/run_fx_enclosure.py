"""Run actual enclosure bodies against trigger, time, physics and message spies."""
import argparse
from pathlib import Path
import sys
sys.dont_write_bytecode=True
from fxgs_common import Tree, definition, compile_and_run, report
p=argparse.ArgumentParser();p.add_argument('--rev');p.add_argument('--source-root',type=Path);args=p.parse_args()
path='GameSource/Sound/Vehicles/Environment/BrnEnclosureControl'
tree=Tree(args.rev)
def read(ext):
    return (args.source_root/(path+ext)).read_text(encoding='utf-8-sig') if args.source_root else tree.read('src/'+path+ext)
cpp=read('.cpp');header=read('.h');parts=[]
signatures=[
    ('BrnTrigger::GenericRegion::Type EntityTriggerInfo::GetChangeType() const', '{ return static_cast<BrnTrigger::GenericRegion::Type>(19); }'),
    ('void EnclosureControl::UpdateParams(', 'float) {}'),
    ('void EnclosureControl::ProcessTriggerAction(', 'const BrnGameState::GameStateModuleIO::SoundTriggerAction&,eTriggerPosition) {}'),
    ('int EnclosureControl::ConvertRegionTypeToIndex(', None),
]
for signature,fallback in signatures:
    try:parts.append(definition(cpp,signature))
    except ValueError:
        # Previous controller inherits empty UpdateParams. The two missing
        # helpers are unreachable there; neutral placeholders expose those failures.
        if fallback is None:raise
        parts.append(signature+fallback)
result=compile_and_run(Path(__file__).with_name('FxEnclosure.cpp'),'fx_enclosure.inc','\n'.join(parts),'FxEnclosure')
raise SystemExit(report('run_fx_enclosure',[('controller declares per-frame override','void UpdateParams(' in header)],result,49))

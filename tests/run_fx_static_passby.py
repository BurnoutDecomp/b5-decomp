"""Exercise extracted static pass-by history, scene query and posting bodies."""
import argparse
from pathlib import Path
import re
import sys
sys.dont_write_bytecode = True
from fxgs_common import Tree, definition, compile_and_run, report, STRSTREAM_CPP

p = argparse.ArgumentParser()
p.add_argument('--rev')
p.add_argument('--source-root', type=Path)
args = p.parse_args()
rel = 'GameSource/Sound/Vehicles/Environment/BrnStaticPassbyControl.cpp'
cpp = ((args.source_root / rel).read_text(encoding='utf-8-sig')
       if args.source_root else Tree(args.rev).read('src/' + rel))
parts = re.findall(r'^(?:static const )?f32 KF_\w+\s*=.*?;', cpp, re.M)
parts += [definition(cpp, signature) for signature in (
    'bool StaticPassbyControl::PassbyHistory::Record(',
    'void StaticPassbyControl::PassbyHistory::Update(')]
for signature, empty in (
    ('s32 StaticPassbyControl::GetController(', 's32) { return -1; }'),
    ('void StaticPassbyControl::AttachController(', 'CgsSound::Logic::EffectBase*) {}'),
    ('bool StaticPassbyControl::Attach()', ' { return true; }'),
    ('void StaticPassbyControl::UpdateParams(', 'f32) {}'),
    ('void StaticPassbyControl::ProcessPassbys(', 'Vector3,f32,const PlayerVehicleStateManager*) {}'),
    ('void StaticPassbyControl::TriggerPassby(', 'const BrnSound::World::StaticSoundEntity&) {}'),
    ('void StaticPassbyControl::UpdateHistory(', 'f32) {}'),
):
    try:
        parts.append(definition(cpp, signature))
    except ValueError:
        # Old controller inherits empty EffectBase hooks; missing helpers cannot run.
        parts.append(signature + empty)
result = compile_and_run(Path(__file__).with_name('FxStaticPassby.cpp'),
    'fx_static_passby.inc', '\n'.join(parts), 'FxStaticPassby',
    extra_sources=(STRSTREAM_CPP,))
raise SystemExit(report('run_fx_static_passby', [], result, 27))

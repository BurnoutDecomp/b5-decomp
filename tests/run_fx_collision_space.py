"""Extract the production collision emitter attachment and test world/local space."""
import argparse
from pathlib import Path
from fxgs_common import Tree, definition, compile_and_run, report

p = argparse.ArgumentParser()
p.add_argument('--rev')
p.add_argument('--source', type=Path)
a = p.parse_args()
source = a.source.read_text(encoding='utf-8-sig') if a.source else Tree(a.rev).read(
    'src/GameSource/Sound/Module/LogicModule/Brn3DUserSpaceEffectControl.cpp')
code = '\n'.join(definition(source, sig) for sig in (
    'void Brn3DUserSpaceEffectControl::AttachTransform(',
    'void Brn3DUserSpaceEffectControl::AttachEmitterPosition(',
    'void Brn3DUserSpaceEffectControl::UpdateParams('))
result = compile_and_run(Path(__file__).with_name('FxCollisionSpace.cpp'),
                        'fx_collision_space.inc', code, 'FxCollisionSpace')
raise SystemExit(report('run_fx_collision_space', [], result, 32))

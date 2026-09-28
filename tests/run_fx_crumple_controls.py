"""Golden ARTIST deformation controls, extracted from production source."""
import argparse
from pathlib import Path
from fxgs_common import Tree, definition, compile_and_run, report

p = argparse.ArgumentParser()
p.add_argument('--rev')
p.add_argument('--source', type=Path)
a = p.parse_args()
rel = 'src/GameSource/Sound/Vehicles/Deformation/BrnDeformationEffect.cpp'
source = a.source.read_text(encoding='utf-8-sig') if a.source else Tree(a.rev).read(rel)
parts = []
for signature in ('f32 DeformationDisplacementSquared(', 'void DeformationEffect::UpdateParams('):
    try:
        parts.append(definition(source, signature))
    except ValueError:
        if signature.startswith('void '):
            raise
utils = Tree().read('src/GameShared/GameClasses/Sound/CgsSoundUtils.cpp')
util_code = '\n'.join(definition(utils, signature) for signature in (
    'void InterpolateLine::Initialize(', 'f32 InterpolateLine::GetValueFloat()',
    'void InterpolateLine::Update(f32 lfDeltaTime)'))
result = compile_and_run(Path(__file__).with_name('FxCrumpleControls.cpp'),
                        'fx_crumple_controls.inc', '\n'.join(parts), 'FxCrumpleControls',
                        extra_files={'fx_crumple_interpolate.inc': util_code})
raise SystemExit(report('run_fx_crumple_controls', [], result, 28))

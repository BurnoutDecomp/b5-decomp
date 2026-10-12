"""World review witnesses using complete production definitions and typed spies.

--rev ad62bcf8 selects the compile-clean source before the main review repairs.
Native layouts, full game activation and exact VMX estimate bits remain outside
this focused fixture. Other review findings retain their assembly/source review.
"""
import argparse
import os
from pathlib import Path
import fxgs_common as fx

p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--rev')
p.add_argument('--source-root', type=Path)
p.add_argument('--workflow-root', type=Path)
a = p.parse_args()
if a.source_root:
    fx.REPO = a.source_root.resolve()
fx.WORKFLOW = a.workflow_root.resolve() if a.workflow_root else next(
    x for x in fx.REPO.parents if (x / 'tools/build/msvc_flags.txt').is_file())
original_settings = fx.settings


def settings(name):
    values = original_settings(name)
    if name != 'msvc_includes.txt':
        return values
    return [str(fx.REPO.joinpath(*Path(x).parts[1:]))
            if Path(x).parts[:2] == ('b5-decomp', 'src')
            else str(fx.WORKFLOW / x) for x in values]


fx.settings = settings
t = fx.Tree(a.rev)
base = 'src/GameSource/World/'
prop = t.read(base + 'EntityModules/PropEntityModule/BrnPropEntityDebugComponent.cpp')
boost = t.read(base + 'EntityModules/RaceCarEntityModule/Boost/BrnBoostDebugComponent.cpp')
collision = t.read(base + 'DebugComponents/BrnCollisionDebugComponent.cpp')
world = t.read(base + 'EntityModules/WorldEntityModule/BrnWorldEntityModule.cpp')
cell = t.read('src/SharedClasses/Physics/Props/BrnPhysicsPropZoneData.cpp')
cell_body = fx.definition(cell, '    PropCellId PropZoneData::GetCellId(')
code = 'namespace BrnPhysics { namespace Props {\n' + cell_body + '\n}}\n'
code += 'namespace BrnWorld {\n'
for source, signatures in [
    (prop, ['        inline Vector3 MakeVector3(',
            '    void PropEntityDebugComponent::RenderCellGrid(',
            '    Vector2 PropEntityDebugComponent::ToCellGridScreenCoords(']),
    (boost, ['    void BoostDebugComponent::Construct(',
             '    void BoostDebugComponent::Destruct(',
             '    void BoostDebugComponent::OnAddRemoveChunkMode(']),
    (collision, ['    rw::RGBA RenderColour(', '    Vector3 AddPositions(',
                 '    Vector3 Scale(', 'u32 CollisionDebugComponent::GetTrafficColour(',
                 'u32 CollisionDebugComponent::GetSurfaceRenderColour(',
                 'u32 CollisionDebugComponent::GetFlagsRenderColour(',
                 'u32 CollisionDebugComponent::GetPolyColour(',
                 'void CollisionDebugComponent::DrawCollisionPolyCallback(']),
]:
    if source == prop and 'inline f32 RefinedCellGridReciprocal(' in source:
        code += fx.definition(source, '        inline f32 RefinedCellGridReciprocal(') + '\n'
    for signature in signatures:
        code += fx.definition(source, signature) + '\n'
code += '}\n' + fx.definition(world, 'static f32 GetMaximumScale(') + '\n'
config = '#define CELL_ID_INSTANCE ' + ('1' if ') const' in cell_body.split('{', 1)[0] else '0') + '\n'
os.environ.pop('NoDefaultCurrentDirectoryInExePath', None)
numeric = fx.compile_and_run(Path(__file__).with_name('DebugMenuWorldReview.cpp'),
    'debug_menu_world_review.inc', code, 'DebugMenuWorldReview',
    shadow={'src/pc/math/VmxFloat.h': t.read('src/pc/math/VmxFloat.h')},
    extra_files={'debug_menu_world_review_config.inc': config})
raise SystemExit(fx.report('run_debug_menu_world_review', [], numeric, 1))

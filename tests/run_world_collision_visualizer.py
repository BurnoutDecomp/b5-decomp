"""Exercise merged production drawing bodies with explicit engine-boundary doubles."""
import argparse
import os
from pathlib import Path
from fxgs_common import Tree, definition, compile_and_run, REPO

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--rev', help='Read the production drawing bodies from this revision.')
parser.add_argument('--visibility-rev', help='Run the counter regression against only this revision of IsPolyVisible.')
args = parser.parse_args()
os.environ.pop('NoDefaultCurrentDirectoryInExePath', None)
tree = Tree(args.rev)
paths = {
    'world': 'src/GameSource/World/DebugComponents/BrnCollisionDebugComponent.cpp',
    'scene': 'src/GameShared/GameClasses/SceneManager/TriangleCollision/CgsTriangleCollisionDebugComponent.cpp',
    'soup': 'src/GameShared/GameClasses/Geometric/Primitives/PolygonSoup/CgsPolygonSoup.cpp',
    'box': 'src/GameShared/GameClasses/Geometric/Primitives/CgsAxisAlignedBox.cpp',
    'box4': 'src/GameShared/GameClasses/Geometric/Primitives/CgsAxisAlignedBox4.cpp',
    'tag': 'src/SharedClasses/World/BrnCollisionTag.cpp',
    'owner': 'src/GameSource/World/EntityModules/WorldEntityModule/BrnWorldEntityModule.cpp',
}
sources = {key: tree.read(path) for key, path in paths.items()}
# Read fixture/header/dependency inputs through Tree as well so private-worktree
# launchers can freeze every compiled production input in their hash receipts.
tree.read('tests/WorldCollisionVisualizer.cpp')
headers = (
    'src/GameSource/World/DebugComponents/BrnCollisionDebugComponent.h',
    'src/GameShared/GameClasses/SceneManager/TriangleCollision/CgsTriangleCollisionDebugComponent.h',
)
shadow = {path: tree.read(path) for path in headers}
extra_sources = [
    REPO/'src/GameShared/GameClasses/RenderWare/RwRGBA.cpp',
    REPO/'src/GameShared/GameClasses/Development/CgsStrStream.cpp',
    REPO/'src/GameShared/GameClasses/Geometric/Primitives/PolygonSoup/CgsPolygonSoupPoly.cpp',
    REPO/'src/vendor/renderware/collision/Frustum.cpp',
]
for source in extra_sources:
    tree.read(source.relative_to(REPO).as_posix())

parts = []
try:
    # These are the original file-scoped state, palette and helpers, followed by
    # all the World component bodies. Keep their actual qualified ownership.
    world_start = sources['world'].index('namespace BrnWorld')
    parts.append(sources['world'][world_start:])
    parts += ['namespace BrnWorld {',
              definition(sources['owner'], 'void WorldEntityModule::GetSurface('),
              definition(sources['tag'], 'TrafficDirection CollisionTag::GetTrafficInfo('), '}']
    parts.append('namespace {')
    for sig in ('rw::RGBA RwRGBAFromPackedWord(',
                'void SinCosTriangleDebugAngle(',
                'Vector3 RotateTriangleDebugNormal('):
        parts.append(definition(sources['scene'], sig))
    parts += ['}', 'namespace CgsSceneManager {',
              'DrawCollisionPolyCallbackData TriangleCollisionDebugComponent::saDrawPolyCallbacks[4]={};',
              's32 TriangleCollisionDebugComponent::siNumDrawPolyCallbacks=0;']
    for sig in ('void DrawCollisionPolyCallbackData::Construct(',
                'DrawCollisionPolyCallbackData::DrawCollisionPolyCallback*\n    DrawCollisionPolyCallbackData::GetCallback(',
                'void* DrawCollisionPolyCallbackData::GetUserData(',
                'bool TriangleCollisionDebugComponent::RegisterDrawCollisionPolyCallback(',
                'void TriangleCollisionDebugComponent::Construct(',
                'void TriangleCollisionDebugComponent::DrawCollisionPoly(',
                'void TriangleCollisionDebugComponent::UpdateViewFrustum(',
                'bool TriangleCollisionDebugComponent::IsPolyVisible(',
                'void TriangleCollisionDebugComponent::DrawPolySoupPoly(',
                'void TriangleCollisionDebugComponent::DrawPolySoups(',
                'void TriangleCollisionDebugComponent::RenderWorld('):
        parts.append(definition(sources['scene'], sig))
    parts += ['}', 'namespace CgsGeometric {']
    for key, sig in (('soup', 'Vector3 PolygonSoup::GetVertexPosition('),
                     ('soup', 'u8* PolygonSoup::GetPolygon('),
                     ('box', 'bool AxisAlignedBox::ContainsPoint('),
                     ('box4', 'AxisAlignedBox AxisAlignedBox4::GetAxisAlignedBox(')):
        parts.append(definition(sources[key], sig))
    parts.append('}')
    parts.append('#define WORLD_COLLISION_VISIBILITY_COMPARISON '+str(int(bool(args.visibility_rev))))
    if args.visibility_rev:
        # Rename only the owner to let this frozen original method coexist with
        # the current production class. Its body and const qualification stay intact.
        visibility_source = Tree(args.visibility_rev).read(paths['scene'])
        alternate = definition(visibility_source, 'bool TriangleCollisionDebugComponent::IsPolyVisible(')
        alternate = alternate.replace('TriangleCollisionDebugComponent::IsPolyVisible(',
                                      'VisibilityComparison::IsPolyVisible(', 1)
        declaration = alternate[:alternate.index('{')].replace('VisibilityComparison::', '', 1).strip()+';'
        parts += ['namespace CgsSceneManager {',
                  'struct VisibilityComparison { rw::collision::Frustum mViewFrustum; '+declaration+' };',
                  alternate, '}']
        print('Counter regression IsPolyVisible revision:', args.visibility_rev)
except ValueError as error:
    print('Missing production drawing body:', error)
    raise SystemExit(1)

result = compile_and_run(Path(__file__).with_name('WorldCollisionVisualizer.cpp'),
    'world_collision_visualizer.inc', '\n'.join(parts), 'WorldCollisionVisualizer',
    shadow=shadow, extra_sources=extra_sources)
raise SystemExit(0 if result and result[1] == 0 else 1)

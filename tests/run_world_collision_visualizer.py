"""Exercise production collision-menu, soup traversal, callback and colour bodies."""
import argparse
import os
from pathlib import Path
from fxgs_common import Tree, definition, compile_and_run, REPO

parser = argparse.ArgumentParser()
parser.add_argument('--rev')
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
parts = []
try:
    parts += ['namespace BrnWorld {',
              'const WorldEntityModule* CollisionDebugComponent::spWorldEntityModule=nullptr;',
              's32 CollisionDebugComponent::siDrawMode=0, CollisionDebugComponent::siSurfaceAlpha=75;',
              'bool CollisionDebugComponent::sbDrawTags=false, CollisionDebugComponent::sbDrawNormals=false;']
    parts.append(definition(sources['owner'], 'void WorldEntityModule::GetSurface('))
    for sig in ('void CollisionDebugComponent::Construct(', 'void CollisionDebugComponent::OnActivate(',
                'u32 CollisionDebugComponent::GetTrafficColour(', 'u32 CollisionDebugComponent::GetFlagsRenderColour(',
                'u32 CollisionDebugComponent::GetSurfaceRenderColour(', 'u32 CollisionDebugComponent::GetPolyColour(',
                'void CollisionDebugComponent::DrawCollisionPolyCallback('):
        parts.append(definition(sources['world'], sig))
    parts += [definition(sources['tag'], 'TrafficDirection CollisionTag::GetTrafficInfo('), '}']
    parts += ['namespace CgsSceneManager {',
              'DrawCollisionPolyCallback TriangleCollisionDebugComponent::siDrawPolyCallbacks[4]={};',
              's32 TriangleCollisionDebugComponent::siNumDrawPolyCallbacks=0;']
    for sig in ('bool TriangleCollisionDebugComponent::RegisterDrawCollisionPolyCallback(',
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
except ValueError as error:
    print('Missing production drawing body:', error)
    raise SystemExit(1)
result = compile_and_run(Path(__file__).with_name('WorldCollisionVisualizer.cpp'),
    'world_collision_visualizer.inc', '\n'.join(parts), 'WorldCollisionVisualizer',
    extra_sources=[REPO/'src/GameShared/GameClasses/RenderWare/RwRGBA.cpp'])
raise SystemExit(0 if result and result[1] == 0 else 1)

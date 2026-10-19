"""ARTIST regression witnesses using extracted production consumer bodies.

Typed owner, renderer and collision call spies isolate the changed contracts.
No game object layout, GPU output, hardware estimate bits or native verdict is
claimed. --rev 43fbf001 is the pre-repair negative control.
"""
import argparse
import os
from pathlib import Path
import re
import sys
import fxgs_common as fx

p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--rev')
p.add_argument('--source-root', type=Path)
p.add_argument('--workflow-root', type=Path)
a = p.parse_args()
if a.source_root:
    fx.REPO = a.source_root.resolve()
fx.WORKFLOW = a.workflow_root.resolve() if a.workflow_root else next(
    x for x in fx.REPO.parents if (x/'tools/build/msvc_flags.txt').is_file())
original_settings = fx.settings

def settings(name):
    values = original_settings(name)
    if name != 'msvc_includes.txt':
        return values
    return [str(fx.REPO.joinpath(*Path(x).parts[1:]))
            if Path(x).parts[:2] == ('b5-decomp', 'src')
            else str(fx.WORKFLOW/x) for x in values]

fx.settings = settings
t = fx.Tree(a.rev)
base = 'src/GameSource/World/EntityModules/RaceCarEntityModule/'
scene = t.read('src/GameShared/GameClasses/SceneManager/CgsSceneManagerDebugComponent.cpp')
shadow = t.read('src/GameSource/World/ShadowMap/BrnShadowMap.cpp')
race = t.read(base+'BrnRaceCarEntityModuleDebugComponent.cpp')
near = t.read(base+'BrnRaceCarEntityModule_NearMissTailgate.cpp')
render = t.read(base+'BrnRaceCarEntityModule_Render.cpp')
world = t.read('src/GameSource/World/BrnWorldModule.cpp')
frustum = t.read('src/GameShared/GameClasses/Geometric/Primitives/CgsFrustum.cpp')
constants = t.read(base+'BrnRaceCarEntityModule_wBT_01.cpp')

def definitions(source, signatures):
    return '\n'.join(fx.definition(source, x) for x in signatures)

scene_code = definitions(scene, ['        rw::math::vpu::MaskScalar BroadcastMaskLane(',
    '        Vector3 CopyVector3Plus(', '        void DrawContactAxes(',
    '    void SceneManagerDebugComponent::DrawCachedTriangle(',
    '    void SceneManagerDebugComponent::DrawTriangle(',
    '    void SceneManagerDebugComponent::DrawTriangleTest('])
records = definitions(shadow, ['    struct TsmDebugRenderInfo',
    '    struct BoundingBoxDebugRenderInfo']).replace('}\n    struct', '};\n    struct') + ';'
shadow_code = definitions(shadow, ['        Vector4 TransformShadowDebugPointFma(',
    '        Vector3 ToShadowDebugVector3(', '        void DrawShadowDebugMatrixGuides(',
    '    void ShadowMap::DebugRender('])
race_constants = '\n'.join(re.findall(r'^\s*(?:const )?f32 KF_(?:MPH_TO_MPS|Z_OFFSET_FROM_CAR_CENTRE_TO_CONE_APEX|TAILGATING_\w+|MIN_TAILGATEE_SPEED)[^;]*;', constants, re.M))
race_code = definitions(race, ['void RaceCarEntityModuleDebugComponent::Update(',
    'void RaceCarEntityModuleDebugComponent::RenderTailgateCones(',
    'void RaceCarEntityModuleDebugComponent::RenderSingleDebugCone('])
near_code = fx.definition(near, 'bool RaceCarEntityModule::IsPlayerCarTailgatingOtherRaceCars(')
illum_start = render.index('    const Vector4 lv4DebugIllumination =')
illum_end = render.index('    // ---- the shadow-pass selector', illum_start)
illum = render[illum_start:illum_end]
lod = fx.definition(fx.definition(world, 'WorldModule::CalculateVehicleLODs('),
    '    for ( u32 luIndex = 0; luIndex < KU_NUM_VEHICLE_LODS; luIndex++ )')

frustum_helpers = definitions(frustum, ['        inline Vector3 MakeFrustumDebugVector(',
    '        inline Vector3 AddFrustumDebugVectors(', '        inline Vector3 ScaleFrustumDebugVector(',
    '        inline Vector3 AverageFrustumDebugFace(', '        inline Vector3 AddFrustumDebugPlane('])
code = ('namespace CgsSceneManager { static s32 siTriangleTestPerfMon=0;\n' + scene_code + '\n}\n'
    'namespace CgsGeometric {\n' + frustum_helpers + '\n' + fx.definition(frustum, '    void Frustum::DebugRenderCustomPlanes(') + '\n}\n'
    'namespace BrnWorld {\n' + records + '\n'
    'TsmDebugRenderInfo gaTsmDebugRenderInfo[3]{}; BoundingBoxDebugRenderInfo gBoundingBoxDebugRenderInfo[3]{};\n'
    + shadow_code + '\n' + race_constants
    + '\nconst f32 KF_TWO_PI=6.28318548f; const s32 KI_TAILGATE_CONE_SEGMENTS=32;\n'
    + race_code + '\n' + near_code + '\n'
    'Vector4 Illumination(Vector4 lv4LightState,f32 DEBUG_mfSelfIlluminationR, f32 DEBUG_mfSelfIlluminationG,f32 DEBUG_mfSelfIlluminationB) {\n'
    'const f32 KF_SELF_ILLUMINATION_INTENSITY=8.0f;\n' + illum + '\nreturn lv4SelfIlluminationMask; }\n'
    'f32 LodBand(f32 quality,f32 aggressive,f32 lfAlpha,f32 lfZoomFactor) {\n'
    'const u32 KU_NUM_VEHICLE_LODS=5; f32 KA_VEHICLE_QUALITY_LOD_DISTANCE[5]={quality,quality,quality,quality,quality};\n'
    'f32 KA_VEHICLE_AGGRESSIVE_LOD_DISTANCE[5]={aggressive,aggressive,aggressive,aggressive,aggressive}; f32 lafLODDistances[5];\n'
    + lod + '\nreturn lafLODDistances[0]; }\n}\n')
os.environ.pop('NoDefaultCurrentDirectoryInExePath', None)
headers = ['src/pc/math/VmxFloat.h']
header_shadow = {x: t.read(x) for x in headers if t.read(x)}
rgba = t.read('src/GameShared/GameClasses/RenderWare/RwRGBA.cpp')
code += '\nnamespace rw {\n' + fx.definition(rgba, 'RGBA::RGBA(') + '\n}\n'
numeric = fx.compile_and_run(Path(__file__).with_name('DebugMenuDrawConsumers.cpp'),
    'debug_menu_draw_consumers.inc', code, 'DebugMenuDrawConsumers', shadow=header_shadow)
raise SystemExit(fx.report('run_debug_menu_draw_consumers', [], numeric, 1))

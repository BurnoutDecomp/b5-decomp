"""Original camera IO/lifetime/lifecycle regression, replacing the rejected whitelist test.

The fixture executes actual production IO accessors, bridge bodies, camera latch,
loading Render, video/resource guards and World's pre-render camera latch. Native
draws and scene jobs are outside this CPU check. --negative runs bounded mutation
controls without changing any production file.
"""
from pathlib import Path
import argparse
import os
from fxgs_common import REPO, STRSTREAM_CPP, Tree, definition, code_only, compile_and_run, report
os.environ.pop('NoDefaultCurrentDirectoryInExePath',None)
parser=argparse.ArgumentParser()
parser.add_argument('--rev')
parser.add_argument('--negative',choices=['origin','times','stage7','fabricated','frame_step'])
args=parser.parse_args()
tree=Tree(args.rev)
game=tree.read('src/GameSource/Game/BrnGameModule.cpp')
bridge=tree.read('src/GameSource/Game/GameBridgeRendererToX.cpp')
world=tree.read('src/GameSource/World/BrnWorldModule.cpp')
flow=tree.read('src/GameSource/GameFlowController/TopLevel/BrnGameMainFlowStates.cpp')
camera=tree.read('src/GameSource/Director/Camera/Camera.cpp')
renderer=tree.read('src/GameSource/Graphics/BrnRendererModule.cpp')
dispatchio=tree.read('src/GameSource/Game/BrnDispatchThreadInputBuffer.cpp')
dispatch=code_only(definition(game,'int BrnGameModule::DoDispatch()'))
ordered=['mRenderModule.Update(', 'BridgeRendererToWorld(', 'GenerateFrustumQueries(',
         'mWorldModule.GenerateDispatchLists(', 'BridgeWorldToEffects_Dispatch(',
         'mEffectsModule.GenerateDispatchLists(']
positions=[dispatch.find(x) for x in ordered]
wiring=[('original renderer/world/frustum/effects call order',all(p>=0 for p in positions) and positions==sorted(positions)),
        ('no arbitrator/origin/tour or particle stand-in selects production camera',all(x not in dispatch for x in (
            'mbDirectorCameraLive','SetBringUpCameraOverride','GenerateDispatchListsBringUp',
            'PCBringUpProduceParticleRenderData','PCBringUpSetCameraInput','GetArbitrator','lfDistSq'))),
        ('original pre-render effects call precedes the resource-stall gate',
         0<=dispatch.find('mEffectsModule.PreRenderUpdate(')<dispatch.find('if (mbStalled)'))]
renderer_update=code_only(definition(renderer,'void BrnRendererModule::Update('))
wiring.append(('whole-camera publication precedes the corona-only FOV gate',
    0<=renderer_update.find('lpOutput->SetBrnCamera(lrCamera)')
    <renderer_update.find('if (lrCamera.GetFOV() > 0.1f)')))
try:
    latch=definition(game,'void BrnGameModule::LatchDispatchCamera()')
    getter=definition(game,'const BrnDirector::Camera::Camera* BrnGameModule::GetDispatchCamera() const')
    world_bridge=definition(bridge,'void BrnGameModule::BridgeRendererToWorld(')
    effects_bridge=definition(bridge,'void BrnGameModule::BridgeRendererToEffects(')
except ValueError:
    raise SystemExit(report('run_pc_paused_world_camera',wiring,None,252))

if args.negative=='origin':
    world_bridge=world_bridge.replace('    lpWorldDispatchInput->SetCameraInput(&lpRendererOutput->GetBrnCamera());',
        '    if (lpRendererOutput->GetBrnCamera().mTransform.wAxis.x > 1.0f)\n'
        '        lpWorldDispatchInput->SetCameraInput(&lpRendererOutput->GetBrnCamera());')
if args.negative=='times':
    for name in ('SetGameTime','SetSimTime'):
        start=world_bridge.index('    lpWorldDispatchInput->'+name+'(')
        end=world_bridge.index(';',start)+1
        world_bridge=world_bridge[:start]+world_bridge[end:]
if args.negative=='fabricated':
    getter=getter.replace('mbDispatchCameraPublished ? &mDispatchCamera : 0','&mDispatchCamera')

director_update=definition(game,'void BrnGameModule::DoUpdate_Director(')
director_prefix=director_update[director_update.index('{')+1:director_update.index('        BrnDirector::DirectorIO::InputBuffer*')]
dispatch_original=definition(game,'int BrnGameModule::DoDispatch()')
gui_render_position = dispatch_original.index('mGuiModule.Render(')
gui_gate_start = dispatch_original.rfind('        if (', 0, gui_render_position)
gui_gate_end = dispatch_original.index(';', gui_render_position) + 1
gui_gate = dispatch_original[gui_gate_start:gui_gate_end]
if args.negative == 'frame_step':
    gui_gate = gui_gate.replace('!mbSteppingFrames && ', '')
dispatch_prefix=dispatch_original[dispatch_original.index('{')+1:dispatch_original.index('        // Original cadence')]
world_dispatch=definition(world,'WorldModule::GenerateDispatchLists(')
world_prefix=world_dispatch[world_dispatch.index('{')+1:world_dispatch.index('    PerfMonCpu::StartMonitor( mGlobalCpuMonitors.miUT_RenderMainScreen );')]
flow_render='\n'.join(definition(flow, 'void ' + name + '::Render()') for name in (
    'LoadingScriptedState', 'MainGameFlowStateStartScreen', 'MainGameFlowStateMarketingScreens',
    'MainGameFlowStateCheckDiskSpace', 'MainGameFlowStateMemoryCard', 'MainGameFlowStateCompleteLoading'))
flow_render += '\n' + definition(tree.read(
    'src/GameSource/GameFlowController/TopLevel/BrnGameMainFlowInGameState.cpp'),
    'void MainGameFlowStateInGame::Render()')
if args.negative=='stage7': flow_render=flow_render.replace('gBrnScriptedLoadStage == 8','gBrnScriptedLoadStage >= 7')

parts=['namespace BrnGame {',world_bridge,effects_bridge,latch,getter,
       definition(dispatchio,'void DispatchThreadInputBuffer::SetIsStalled('),
       definition(dispatchio,'bool DispatchThreadInputBuffer::GetIsStalled()'),
       definition(dispatchio,'void DispatchThreadInputBuffer::SetRendererFlags('),'}',
       'namespace BrnDirector { namespace Camera {',definition(camera,'bool Camera::IsInJunkyard() const'),
       definition(camera,'void Camera::Construct()'),definition(camera,'void Camera::Clear()'),'} }',
       'namespace BrnGameMainFlowController {',flow_render,'}']
extra_files={'dispatch_lifecycle.inc':dispatch_prefix, 'director_lifecycle.inc':director_prefix,
             'dispatch_gui_gate.inc':gui_gate,
             'world_camera_latch.inc':world_prefix,
             'bridge_effects_call.inc':'        game.BridgeRendererToEffects(&effects,&rendererOutput);'}
result=compile_and_run(Path(__file__).with_name('PCOriginalCameraChain.cpp'),
    'original_camera_chain.inc','\n'.join(parts),'PCOriginalCameraChain',extra_flags='/Gy /Gw',
    extra_sources=[REPO/path for path in (
        'src/GameShared/GameClasses/Module/CgsIOBuffer.cpp',
        'src/GameSource/Graphics/BrnRendererModuleIO_OutputBuffer_Accessors.cpp',
        'src/GameSource/World/BrnWorldModuleIO_DispatchInputBuffer.cpp',
        'src/GameSource/Effects/SharedIO/BrnEffectsModuleIO_DispatchInputBuffer.cpp',
        'src/GameSource/Director/Camera/BrnCameraEffects.cpp',
        'src/GameSource/Director/Camera/BrnDepthOfField.cpp',
        'src/GameSource/Director/Camera/BrnCameraState.cpp',
        'src/GameSource/Director/Camera/BrnCameraValidityAccount.cpp',
        'src/GameShared/GameClasses/Graphics/Dispatch/CgsTextureScopeTable.cpp')]+[STRSTREAM_CPP],
    extra_files=extra_files)
raise SystemExit(report('run_pc_paused_world_camera',wiring,result,252))

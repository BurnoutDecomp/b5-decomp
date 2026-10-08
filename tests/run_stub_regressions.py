"""Execute production route-cache/minimap and loading-render code.

--rev reads an old b5 tree for a negative control; missing recovered bodies fail.
Run from the workflow with NoDefaultCurrentDirectoryInExePath unset.
"""
from pathlib import Path
import argparse
import sys
sys.dont_write_bytecode=True
from fxgs_common import Tree,definition,compile_and_run,code_only
from run_fxflow_rival_shutdown_gui import arm


def main():
    parser=argparse.ArgumentParser()
    parser.add_argument('--rev')
    args=parser.parse_args()
    tree=Tree(args.rev)
    failures=0
    try:
        cache=tree.read('src/GameSource/Gui/BrnGuiCache.cpp')
        satnav=tree.read('src/GameSource/Gui/CustomRenderer/Renderers/BrnSatNavRenderer.cpp')
        helper=definition(cache,'CgsID GuiCache::GetOriginalCarId(')
        cache_arm=arm(definition(cache,'void GuiCache::RecEvent('),'415')
        icon_arm=arm(definition(satnav,'void SatNavRenderer::GetIconInformation('),
                     r'GuiEventEnableSatNavIcons::E_ICON_DISPLAY_TYPE_OFFLINE_EVENTS')
        refresh=definition(satnav,'void SatNavRenderer::RefreshSatNavIconInfo(')
        translator=definition(tree.read('src/GameSource/Game/GameBridgeGameStateToX_StuntGuiEvents.cpp'),
                              'void BrnGameModule::TranslateGameActionsToGuiEvents(')
        bridge_arms='\n'.join(arm(translator,label) or '' for label in (
            '1','2',r'BrnGameState::GameStateModuleIO::E_ACTION_RESET_PLAYER_CAR'))
        if not cache_arm or not icon_arm:raise ValueError('missing recovered car/route arm')
        result=compile_and_run(Path(__file__).with_name('BurningRouteGui.cpp'),
            'route_bodies.inc',helper+'\n'+refresh,'BurningRouteGui',extra_files={
                'route_cache_arm.inc':cache_arm,'route_icon_arm.inc':icon_arm,
                'route_bridge_arms.inc':bridge_arms})
        failures+=1 if result is None else result[1]
    except ValueError as error:
        print('FAIL BurningRouteGui:',error);failures+=18

    loading=tree.read('src/GameSource/Game/BrnLoadingScreenRenderer.cpp')
    constants=loading[loading.index('namespace\n{'):loading.index('// Build a textured quad')]
    constants+='\n'+definition(loading,'void EmitQuad(')+'\n'+definition(loading,'void RotatedCorners(')+'\n}\n'
    result=compile_and_run(Path(__file__).with_name('LoadingBackground.cpp'),
        'loading_render.inc',definition(loading,'void LoadingScreenRenderer::Render('),
        'LoadingBackground',extra_files={'loading_constants.inc':constants})
    failures+=1 if result is None else result[1]
    storage=tree.read('src/GameShared/GameClasses/Gui/CgsSaveLoadPS3.cpp')
    result=compile_and_run(Path(__file__).with_name('SaveLoadCompletion.cpp'),
        'save_load_confirm.inc',definition(storage,'void SaveLoadSystem::LoadHandleConfirmLoad('),
        'SaveLoadCompletion',extra_files={
            'save_load_update.inc':definition(storage,'void SaveLoadSystem::Update()'),
            'save_cleanup.inc':definition(storage,'void FinishPendingSavePC(')})
    failures+=1 if result is None else result[1]
    print('run_stub_regressions:',failures,'failures')
    return 1 if failures else 0


if __name__=='__main__':sys.exit(main())

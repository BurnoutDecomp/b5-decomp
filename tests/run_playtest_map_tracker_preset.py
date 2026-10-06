"""Actual route sender,51/170/cache,93 and492 delivery regression."""
from pathlib import Path
import argparse,re,sys
sys.dont_write_bytecode=True
from fxgs_common import Tree,definition,code_only,compile_and_run,report

def main():
    ap=argparse.ArgumentParser();ap.add_argument('--rev');ap.add_argument('--tracker-rev');args=ap.parse_args();t=Tree(args.rev)
    try:
        cache=t.read('src/GameSource/Gui/BrnGuiCache.cpp')
        tracker=Tree(args.tracker_rev).read('src/GameSource/Gui/BrnGuiCache.cpp') if args.tracker_rev else cache
        cases='\n'.join(definition(tracker if n in [93,492] else cache,'case '+str(n)+':')+'\nbreak;' for n in [93,492,170])
        bodies='namespace BrnGui {\nvoid GuiCache::RecEvent(const CgsModule::Event* lpEvent,s32 id) {switch(id) {\n'+cases+'\n}}\n'+definition(cache,'const PresetRace* GuiCache::GetPresetRace(')+'\n}\n'
        bridge=t.read('src/GameSource/Game/GameBridgeGameStateToX_EventFlowGuiEvents.cpp')
        arm=definition(bridge,'case BrnGameState::GameStateModuleIO::E_ACTION_SET_LANDMARK_RACES:')
        bodies+='namespace BrnGame {bool TranslateRaces(s32 id,const CgsModule::Event* lpAction,FixtureQueue* lpGuiInput) {switch(id) {\n'+arm+'\ndefault:return false;}}}\n'
        sender=definition(t.read('src/GameSource/GameState/GameStateModule_SendSpecificPreSetRacesModes.cpp'),'void GameStateModule::SendSetLandmarkRacesAction(').replace('GameStateModuleIO::GameActionQueue*','FixtureQueue*')
        dispatch=definition(t.read('src/GameSource/GameState/GameStateModule_wX_00.cpp'),'void GameStateModule::ProcessGameEventsLandmarkRouteRequestBringUp(')
        # Remove the optional log witness; preserve the actual queue walk and all dispatch calls.
        start=dispatch.index('// [FLAG PC witness]');end=dispatch.index('        const CgsModule::Event* lpCurrent',start)
        dispatch=dispatch[:start]+'}\n\n'+dispatch[end:]
        dispatch=dispatch.replace('CgsModule::VariableEventQueue<1536, 16>','FixtureEventQueue').replace('GameStateModuleIO::GameActionQueue*','FixtureQueue*')
        getter=definition(t.read('src/GameSource/GameState/ModeManager/BrnModeManager.h'),'LandmarkIndex GetPlayerCurrentLandmark() const')
        getter=getter.replace('LandmarkIndex GetPlayerCurrentLandmark','LandmarkIndex FixtureModeManager::GetPlayerCurrentLandmark')
        bodies+='namespace BrnGameState {\n'+getter+'\n'+sender+'\n'+dispatch+'\n}\n'
        progression=definition(t.read('src/GameSource/GameState/Progression/BrnProgressionManager_Lifecycle.cpp'),'u32 ProgressionManager::GetRacesAtLandmark(')
        race=definition(t.read('src/SharedClasses/Progression/BrnRace.cpp'),'LandmarkIndex Race::GetStartLandmarkIndex() const')
        bodies+='namespace BrnProgression {\n'+race+'\n'+progression+'\n}\n'
    except ValueError as e:
        print('Missing original route data chain:',e)
        return report('run_playtest_map_tracker_preset',[],None,61)
    module=code_only(t.read('src/GameSource/Gui/BrnGuiModule.cpp'))
    start=module.index('case 93:');end=module.index('mGuiCache.RecEvent(',start)
    checks=[('GUI170 reaches original cache consumer',bool(re.search(r'case\s+170\s*:',module[start:end])))]
    numeric=compile_and_run(Path(__file__).with_name('PlaytestMapTrackerPreset.cpp'),
        'playtest_map_tracker_preset_bodies.inc',bodies,'PlaytestMapTrackerPreset')
    return report('run_playtest_map_tracker_preset',checks,numeric,60)
if __name__=='__main__':sys.exit(main())

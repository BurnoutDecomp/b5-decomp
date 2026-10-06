"""Actual action29 -> GUI159 -> cache -> map landmark classification regression."""
from pathlib import Path
import argparse,re,sys
sys.dont_write_bytecode=True
from fxgs_common import Tree,definition,code_only,compile_and_run,report

def main():
    ap=argparse.ArgumentParser();ap.add_argument('--rev');args=ap.parse_args();tree=Tree(args.rev)
    try:
        bridge=tree.read('src/GameSource/Game/GameBridgeGameStateToX_EventFlowGuiEvents.cpp')
        intro=definition(bridge,'case BrnGameState::GameStateModuleIO::E_ACTION_START_MODE_INTRO:')
        intro=intro[:intro.index('// FLAG PC diagnostic:')]+'return true;\n}'
        helper=definition(bridge,'inline bool IsOnlineLobbyOrShowtimeMode(')
        producer='namespace BrnGame {\n'+helper+'\nbool TranslateIntro(s32 id,const CgsModule::Event* lpAction,FixtureQueue* lpGuiInput) {switch(id) {\n'+intro+'\ndefault:return false;}}\n}'
        cache=tree.read('src/GameSource/Gui/BrnGuiCache.cpp')
        bind=definition(cache,'case 159:')
        bind=bind[:bind.index('// FLAG PC diagnostic:')]+'break;\n}'
        clear=cache[cache.index('case 162:'):cache.index('case 307:',cache.index('case 162:'))]
        consumer='namespace BrnGui {\nvoid GuiCache::RecEvent(const CgsModule::Event* lpEvent,s32 id) {switch(id) {\n'+bind+'\n'+clear+'\n}}\n'+definition(cache,'const PreEventInfo* GuiCache::GetPreEventInfo(')+'\n'
        manager=tree.read('src/GameSource/Gui/SatNav/BrnMapIconManager.cpp')
        for sig in ['bool MapIconManager::IsStartIcon(','bool MapIconManager::IsFinishIcon(']:
            consumer+=definition(manager,sig)+'\n'
        consumer+='}\n'
        shared=tree.read('src/GameSource/GameState/BrnGameStateSharedIO.cpp')
        dep='namespace BrnGameState {namespace GameStateModuleIO {\n'+definition(shared,'FlybyRivalData* FlybyData::GetFlybyRivalData(')+'\n}}'
    except ValueError as e:
        print('Missing original pre-race chain:',e)
        return report('run_playtest_pre_race_map',[],None,52)
    module=code_only(tree.read('src/GameSource/Gui/BrnGuiModule.cpp'))
    labels=module[module.index('case 93:'):module.index('mGuiCache.RecEvent(',module.index('case 93:'))]
    wiring=[('GUI159/162 route to the actual cache consumer',bool(re.search(r'case\s+159\s*:',labels)) and bool(re.search(r'case\s+162\s*:',labels)))]
    numeric=compile_and_run(Path(__file__).with_name('PlaytestPreRaceMap.cpp'),
        'playtest_pre_race_map_bodies.inc',dep+'\n'+producer+'\n'+consumer,'PlaytestPreRaceMap')
    return report('run_playtest_pre_race_map',wiring,numeric,52)
if __name__=='__main__':sys.exit(main())

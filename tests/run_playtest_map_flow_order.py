"""Exercise the production flow tick order against the shared map icon bank.

ARTIST registers HUD, SCREEN, OVERLAY in that order (82518570/80/90);
UpdateObservers82847BB8 walks it forwards. Direct map entry activates the
map's first-cache enable and the departing HUD's minimap hide in one update.
"""
from pathlib import Path
import argparse,re,sys
sys.dont_write_bytecode=True
from fxgs_common import Tree,definition,code_only,compile_and_run,report

def main():
    ap=argparse.ArgumentParser();ap.add_argument('--rev');args=ap.parse_args();tree=Tree(args.rev)
    module=tree.read('src/GameSource/Gui/BrnGuiModule.cpp')
    tick=module[module.index('// ---- 4. the flow ticks'):]
    calls=re.findall(r'm(?:Hud|Screen|Overlay)Flow\.Update\(\);',tick)[:3]
    manager=definition(tree.read('src/GameSource/Gui/SatNav/BrnMapIconManager.cpp'),
                       'void MapIconManager::SetIconsVisible(bool lbVisible)')
    manager='void MapIconManager::SetIconsVisible(bool lbVisible) {\n'+manager[manager.index('mbIconsVisible = lbVisible;'):]
    satnav=definition(tree.read('src/GameSource/Gui/SatNav/BrnSatNavComponent.cpp'),
                      'void SatNavComponent::RecvEvent(')
    show=definition(satnav,'case KI_EVENT_SHOW_HIDE:')
    satnav='void SatNavComponent::RecvEvent(const CgsModule::Event* lpEvent,s32 liEventId) {switch(liEventId) {\n'+show+'\n}}'
    hud=definition(tree.read('src/GameSource/Gui/Flow/HUD/States/BrnFBurnMainHudState.cpp'),
                   'void FBurnMainHudState::OnLeave()')
    hide=definition(hud,'if (mbSatNavEnabled)')
    hud='void FBurnMainHudState::OnLeave() {\n'+hide+'\n}'
    map_source=tree.read('src/GameSource/Gui/Flow/Screen/States/BrnCrashNavMap.cpp')
    begin=map_source.index('mpGuiCache = lpPayload->mpGuiCache;')
    end=map_source.index('mpIconManager->SetIconsVisible(true);',begin)+len('mpIconManager->SetIconsVisible(true);')
    bind='void CrashNavMap::Bind(const GuiCachePayload* lpPayload,const CgsModule::Event* lpEvent) {\n'+map_source[begin:end]+'\n}'
    bodies='namespace BrnGui {\n'+manager+'\n'+satnav+'\n'+hud+'\n'+bind+'\nvoid FixtureModule::Tick() {\n'+'\n'.join(calls)+'\n}\n}'
    numeric=compile_and_run(Path(__file__).with_name('PlaytestMapFlowOrder.cpp'),
                            'playtest_map_flow_order_bodies.inc',bodies,'PlaytestMapFlowOrder')
    return report('run_playtest_map_flow_order',[],numeric,16)
if __name__=='__main__':sys.exit(main())

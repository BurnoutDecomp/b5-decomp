"""Original LARGE-map rival state and naming in both live caller chains."""
from pathlib import Path
import argparse,re,sys
sys.dont_write_bytecode=True
from fxgs_common import Tree,definition,compile_and_run,report

def main():
    ap=argparse.ArgumentParser();ap.add_argument('--rev');args=ap.parse_args();tree=Tree(args.rev)
    source=tree.read('src/GameSource/Gui/SatNav/BrnMapIconManager.cpp')
    try:
        helper=definition(source,'MapIconBrnBase::IconState MapIconManager::GetCrashNavIconStateForRival(')
        table=re.search(r'const s32 KAE_LOBBY_COLOUR_TO_RIVAL_ICON\[12\][^;]+;',source)[0]
        snippets=[]
        for name,suffix in [('UpdateCrashNavIcons','CrashNav'),('UpdateSatNavIcons','SatNav')]:
            body=definition(source,'void MapIconManager::'+name+'()')
            begin=body.index('liState = GetCrashNavIconStateForRival(&lrRecord);')
            end=body.index('lrIcon.SetRotation(KF_PI - lrRecord.GetRotation());',begin)+len('lrIcon.SetRotation(KF_PI - lrRecord.GetRotation());')
            snippets.append('s32 MapIconManager::Caller'+suffix+'(SatNavIconInfo& lrRecord,FixtureDrawIcon& lrIcon) { s32 liState=0;\n'+body[begin:end]+'\nreturn liState; }')
    except (ValueError,TypeError) as e:
        print('Missing original rival caller/body:',e)
        return report('run_playtest_full_map_rivals',[],None,593)
    access=definition(tree.read('src/GameSource/Gui/BrnGuiEventTypeDefs.cpp'),
        'EActiveRaceCarIndex GuiEventUpdateSatNav::SatNavIconInfo::GetActiveRaceCarIndex() const')
    date=definition(tree.read('src/GameShared/GameClasses/System/Timer/PS3/CgsDateAndTimePS3.cpp'),
        'DateAndTime::DateAndTime()')
    deps='namespace BrnGui {\n'+access+'\n}\nnamespace CgsSystem {\n'+date+'\n}\n'
    numeric=compile_and_run(Path(__file__).with_name('PlaytestFullMapRivals.cpp'),
        'playtest_full_map_rivals_bodies.inc',table+'\nconst f32 KF_PI=3.1415927f;\n'+helper+'\n'+'\n'.join(snippets),
        'PlaytestFullMapRivals',extra_files={'playtest_full_map_rivals_deps.inc':deps})
    return report('run_playtest_full_map_rivals',[],numeric,593)
if __name__=='__main__':sys.exit(main())

"""Extract actual trigger sound producer bodies and test query/result lifecycle."""
import argparse
from pathlib import Path
import sys
sys.dont_write_bytecode=True
from fxgs_common import Tree,definition,compile_and_run,report,code_only
p=argparse.ArgumentParser();p.add_argument('--rev');p.add_argument('--source-root',type=Path);args=p.parse_args()
tree=Tree(args.rev)
def read(path):
    return (args.source_root/path).read_text(encoding='utf-8-sig') if args.source_root else tree.read('src/'+path)
base='GameSource/GameState/TriggerQueryManager/BrnTriggerQueryManager'
cpp=read(base+'.cpp');parts=[]
functions=[
('void TriggerQueryManager::SubmitTriggerQueries(', 'GameStateModuleIO::OutputBuffer*,const RCEntityActiveRaceCarOutputInterface*) {}'),
('void TriggerQueryManager::CacheSoundQueryPositions(', 'const RCEntityActiveRaceCarOutputInterface*) {}'),
('void TriggerQueryManager::PostSoundActions(', 'GameStateModuleIO::OutputBuffer*) {}'),
('bool TriggerQueryManager::IsSoundActionPresent(', 'EntityId, GameStateModuleIO::SoundTriggerAction::eType) const { return false; }'),
('void TriggerQueryManager::CheckSoundActions(', 'const RCEntityActiveRaceCarOutputInterface*) {}'),
('void TriggerQueryManager::PostWorldUpdate(', 'const GameStateModuleIO::PostWorldInputBuffer*, ModeManager*, EActiveRaceCarIndex) {}')]
for sig,fallback in functions:
    try:parts.append(definition(cpp,sig))
    except ValueError:
        if sig == 'void TriggerQueryManager::PostWorldUpdate(' and 'void TriggerQueryManager::PostWorldUpdateSoundActions(' in cpp:
            # Execute the actual old sound-only consumer under the restored
            # signature; the added unused mode argument changes no old logic.
            old=definition(cpp,'void TriggerQueryManager::PostWorldUpdateSoundActions(')
            old=old.replace('PostWorldUpdateSoundActions(', 'PostWorldUpdate(')
            old=old.replace('lpInput, EActiveRaceCarIndex', 'lpInput, ModeManager*, EActiveRaceCarIndex')
            parts.append(old)
        else:parts.append(sig+fallback)
# Use the real new packed identifier for both sides; old producers never used it.
idpath='GameSource/World/EntityModules/TriggerEntityModule/BrnTriggerQueryId.h'
idtext=read(idpath)
if not idtext:
    idtext='''#pragma once
#include "types.hpp"
namespace BrnWorld {struct TriggerQueryId {u32 id;void Set(u8 o,u32 i){id=(u32(o)<<24)|(i&0xffffff);}void SetIndex(u32 i){id=(id&0xff000000)|(i&0xffffff);}u8 GetOwner()const{return u8(id>>24);}u32 GetIndex()const{return id&0xffffff;}operator u32()const{return id;}};}
'''
numeric=compile_and_run(Path(__file__).with_name('FxSoundTriggers.cpp'),'fx_sound_triggers.inc','\n'.join(parts),'FxSoundTriggers',shadow={'src/'+idpath:idtext})
wiring=[('world registration enabled','KB_POST_TRIGGER_REGIONS_TO_WORLD = false' not in code_only(cpp)),
        ('sound queue constructed','maSoundActions.Construct();' in code_only(cpp)),
        ('pre-world submits motion query',
         'mTriggerQueryManager.PreWorldUpdate(' in code_only(read('GameSource/GameState/GameStateModule_gUI_00.cpp'))
         and 'SubmitTriggerQueries(lpOutput, lpActiveRaceCarInterface);' in code_only(cpp)),
        ('post-world forwards results','mTriggerQueryManager.PostWorldUpdate(' in code_only(read('GameSource/GameState/GameStateModule_wW_01.cpp')))]
raise SystemExit(report('run_fx_sound_triggers',wiring,numeric,45))

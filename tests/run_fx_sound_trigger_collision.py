"""Trigger event ABI, primitive fine queries and persistent-output retirement."""
import argparse
from pathlib import Path
import sys
sys.dont_write_bytecode=True
from fxgs_common import Tree,definition,compile_and_run,report,STRSTREAM_CPP
from fxrcem3_common import switch_arm
p=argparse.ArgumentParser();p.add_argument('--rev');p.add_argument('--source-root',type=Path);args=p.parse_args();tree=Tree(args.rev)
def read(path):
    return (args.source_root/path).read_text(encoding='utf-8-sig') if args.source_root else tree.read('src/'+path)
header='GameSource/World/EntityModules/TriggerEntityModule/SharedIO/BrnTriggerEntityModuleInputInterface.h'
module=read('GameSource/World/Trigger/BrnTriggerEntityModule.cpp')
box=switch_arm(definition(module,'void TriggerEntityModule::ProcessAddTriggerEvents('),'E_TRIGGERTYPE_BOX').replace('E_TRIGGERTYPE_BOX','2')
fine=definition(read('GameShared/GameClasses/SceneManager/FineIntersectionTestModule/CgsFineIntersectionTestModule_wSQ1.cpp'),'void FineIntersectionTestModule::ComputeLineTestFine(')
game=read('GameSource/Game/BrnGameModule.cpp')
retire='\n'.join(l for l in game.splitlines() if 'lpGameStateOutput->GetTrigger' in l and '.Clear();' in l or 'lpGameStateOutput->GetTriggerQueryInputInterface()->Clear();' in l)
# a83 has the old opaque line-query event. No query field is inspected in this test.
h=read(header)
registration=module[module.index('        // ---- 4. register the trigger volume'):module.index('        // advance',module.index('        // ---- 4. register the trigger volume'))]
numeric=compile_and_run(Path(__file__).with_name('FxSoundTriggerCollision.cpp'),'fx_sound_trigger_fine.inc',fine,'FxSoundTriggerCollision',shadow={'src/'+header:h},extra_sources=(STRSTREAM_CPP,),extra_files={'fx_sound_trigger_box.inc':box,'fx_sound_trigger_retire.inc':retire,'fx_sound_trigger_register.inc':registration})
raise SystemExit(report('run_fx_sound_trigger_collision',[],numeric,24))

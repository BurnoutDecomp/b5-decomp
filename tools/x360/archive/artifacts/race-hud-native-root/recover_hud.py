from pathlib import Path
import hashlib,json
ROOT=Path('D:/Reverse/BP-X360-Experimental-Worktrees/external-body-native-root')
HERE=Path(__file__).resolve().parent
path=ROOT/'src/GameSource/Gui/Flow/HUD/States/BrnRaceMainHudState.cpp'
before=path.read_text();s=before
def replace(old,new):
 global s
 assert s.count(old)==1,(old,s.count(old))
 s=s.replace(old,new)
def span(start,end,new=''):
 global s
 assert s.count(start)==1,start
 a=s.index(start);b=s.index(end,a)
 s=s[:a]+new+s[b:]
replace('#include "GameShared/GameClasses/Development/Log/CgsLog.h"              // CgsDev::Log (the deferral gap log)',
        '#include "GameShared/GameClasses/Development/Log/CgsLog.h"              // native filter-gated mugshot messages')
for line in ['#include <cstdio>    // std::snprintf (the one-shot deferral log)\n',
             '#include <cstdlib>   // std::getenv (BRN_PAYBACK_DIAG)\n']:
 replace(line,'')
start='        // ---- component deferral log (the FBurn LogDeferredComponent idiom)'
end='        const s32 KI_PAYBACK_DIAG_REPEAT = 4;'
span(start,end)
replace(end+'\n','')
replace('if (GuiCache_RoadRuleShotPending(mpCache))','if (mpCache->mbRoadRuleShotActive)')
span('            // [FLAG deferred] SetupComponent\'s remaining three arguments',
     '\n        }\n\n        return true;',
'''            mRoadRuleShotComponent.SetupComponent(
                RoadRuleShotComponent::E_PLAYER_STATE_RACING,
                mpCache->mbRoadRuleShotLocalPlayerNewRuler,
                lpOpponent->mPlayerName.GetPlayerName(), mpCache->mRoadRuleShotRoadId,
                mpCache->mbIsRoadRuleShotTime);
            if (mpCache->mbRoadRuleShotTaken)
                mRoadRuleShotComponent.Snap(true);
            else
                mRoadRuleShotComponent.Show(true);
''')
span('        // ---- GuiCache boundary (the far cache fields',
     '// (fold: an identical definition of LogDeferredComponent was dropped here')
for line in s.splitlines(keepends=True):
 if line.startswith('// (fold: an identical definition of LogDeferredComponent'):
  s=s.replace(line,'')
replace('GuiCache_SetSatNavZoomLevel(mpCache, 0);','mpCache->miSatNavZoomLevel = 0;')
span('                    // FLAG deferred: HandleMugshotEvent @0x82475CD0',
     '                }\n                break;',
     '                    HandleMugshotEvent(reinterpret_cast<const GuiMugshotControlEvent*>(lpEvent));\n')
replace('''            Vector3 lv3Camera;
            lv3Camera.x = lv4Camera.x;
            lv3Camera.y = lv4Camera.y;
            lv3Camera.z = lv4Camera.z;
            lv3Camera.w = lv4Camera.w;''',
        '            const Vector3 lv3Camera(lv4Camera.mV);')
replace('        const u32 KU_TICKER_PARAM_TEXT_LEN = 64;',
        '        const s32 KI_TICKER_MAX_PARAMS = 1 + BrnResource::ChallengeListEntry::KI_MAX_ACTIONS_PER_CHALLENGE;\n        const u32 KU_TICKER_PARAM_TEXT_LEN = 64;')
span('            // @0x824747D0: r3 = &mEventInfoComponent',
     '\n        }\n\n        if (mbBoostMessages)',
'''            mEventInfoComponent.HandleTrigger(
                reinterpret_cast<const CgsGui::GuiEventAptTrigger*>(lpEvent), mpCache);
''')
anchor='    // =======================================================================\n    //  SetExpectedComponent  @ 0x82473698'
methods=(HERE/'mugshot_methods.inc').read_text()
replace(anchor,(HERE/'mugshot_tables.inc').read_text()+'\n'+methods+'\n'+anchor)
assert not any(x in s for x in ['LogDeferredComponent(', 'GuiCache_RoadRuleShotPending(', 'GuiCache_SetSatNavZoomLevel(', 'std::snprintf'])
backup=HERE/'RaceMainHudState-before.cpp';assert not backup.exists();backup.write_text(before,encoding='utf-8')
path.write_text(s,encoding='utf-8')
(HERE/'source-change.json').write_text(json.dumps({'source':str(path),'before_sha256':hashlib.sha256(before.encode()).hexdigest(),'after_sha256':hashlib.sha256(s.encode()).hexdigest()},indent=2)+'\n',encoding='utf-8')
print('HUD native recovery source written',len(s.splitlines()))

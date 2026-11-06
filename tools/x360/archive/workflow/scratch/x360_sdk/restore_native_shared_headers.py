import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2] / 'b5-decomp'
assert json.loads((ROOT/'build/x360/full-native-render/compile-results.json').read_text(encoding='utf-8'))['state'] == 'complete'


def replace(path, old, new):
    p = ROOT / path
    text = p.read_text(encoding='utf-8')
    assert old in text, (path, old[:100])
    p.write_text(text.replace(old, new), encoding='utf-8', newline='\r\n')


speed = ROOT/'src/GameSource/Director/Camera/Utils/BrnSpeedResponder.h'
speed.write_text('''#pragma once
#include "types.hpp"
#include <ppcintrinsics.h>

namespace BrnDirector { namespace Camera { namespace Utils {
// ARTIST82251A7C..82251AB0; declaration BrnSpeedResponder.h:43.
struct SpeedResponder
{
    void Construct() { mfSpeedRatio = 0.0f; }
    void Update(f32 lfSpeed, f32 lfMaxSpeed)
    {
        f32 lfRatio = lfSpeed / lfMaxSpeed;
        lfRatio = static_cast<f32>(__fsel(-lfRatio, 0.0f, lfRatio));
        mfSpeedRatio = static_cast<f32>(__fsel(1.0f - lfRatio, lfRatio, 1.0f));
    }
    f32 GetSpeedRatio() { return mfSpeedRatio; }
private:
    f32 mfSpeedRatio;
};
}}}
''', encoding='utf-8', newline='\r\n')

astar = 'src/GameSource/World/AI/Route/BrnAStar.h'
replace(astar, 'static constexpr f32 KF_EUCLIDEAN_BIAS = 500.0f;', 'static const f32 KF_EUCLIDEAN_BIAS;')
replace(astar, 'static constexpr f32 KF_BUILDROUTE_DOT_SEED    = -2.0f;', 'static const f32 KF_BUILDROUTE_DOT_SEED;')
replace('src/GameSource/World/AI/Route/BrnAStar.cpp',
        'const f32 AStar::KF_DEFAULT_COST_WEIGHT = -1.0f;',
        '''const f32 AStar::KF_DEFAULT_COST_WEIGHT = -1.0f;
const f32 AStar::KF_EUCLIDEAN_BIAS = 500.0f;     // ARTIST820C478C
const f32 AStar::KF_BUILDROUTE_DOT_SEED = -2.0f; // ARTIST820C4358''')

road = 'src/GameSource/GameState/ModeManager/GameModes/BrnRoadRageMode.h'
replace(road, '''    // RoadRageMode member lands -- the base and the derived agree, no padding is needed. Host
    // offsets differ (x64 widening of the base); parity is by NAME, pinned order-only below.''',
        '''    // RoadRageMode member lands -- the base and the derived agree without padding.''')
p = ROOT/road
text = p.read_text(encoding='utf-8')
begin = text.index('    // ---- LAYOUT TRIPWIRE (order-only,')
end = text.index('\n};', begin)
members = [('miNumberOfTransmittedRivals',188),('miNumberOfAllowedRoadRageRivals',192),('miNumberOfRivals',196),
           ('mAddCarPreDelay',200),('mfRoadRageMadness',204),('mfRoadRageMadnessRatio',208),('mfHiddenTime',212),
           ('mbUpdateRivals',244),('mbUpdateMadnessLevel',245),('mfMadnessBroadcastLevel',248),('mfProgressionRankAsRatio',252)]
pins = ['    // Actual native offsets, attested by the ARTIST loads/stores cited above.',
        '    static void AssertLayout()', '    {',
        '        CGS_STATIC_ASSERT((sizeof(OfflineGameMode) == 188), "X360 offline mode base");',
        '        CGS_STATIC_ASSERT((sizeof(RoadRageMode) == 256), "X360 road rage mode");']
for member, offset in members:
    pins.append('        CGS_STATIC_ASSERT((offsetof(RoadRageMode, %s) == %d), "ARTIST road rage member");' % (member,offset))
pins += ['        CGS_STATIC_ASSERT((sizeof(((RoadRageMode*)0)->mfHiddenTime) == 8 * sizeof(f32)), "eight hidden-time slots");','    }']
p.write_text(text[:begin]+'\n'.join(pins)+text[end:],encoding='utf-8',newline='\r\n')

replace('src/GameSource/Sound/Module/BrnRootSoundModuleIo.h',
        '''            for (auto& lpSerialiser : mReplayRequestInterface.mapSerialisers)
                lpSerialiser = 0;''',
        '''            for (u32 luSerialiser = 0;
                 luSerialiser < sizeof(mReplayRequestInterface.mapSerialisers) / sizeof(mReplayRequestInterface.mapSerialisers[0]);
                 ++luSerialiser)
                mReplayRequestInterface.mapSerialisers[luSerialiser] = 0;''')

street = 'src/GameSource/GameState/StreetData/BrnGameStateStreetManager.h'
replace(street, '''                     // +0x1D10
        GameStateModule*                         mpGameStateModule    = 0;                     // +0x1D14
        RoadRulesManager*                        mpRoadRulesManager   = 0;                     // +0x1D18''',
        '''        BrnProgression::ProgressionManager*      mpProgressionManager;                        // +0x1D10
        GameStateModule*                         mpGameStateModule;                           // +0x1D14
        RoadRulesManager*                        mpRoadRulesManager;                          // +0x1D18''')
replace(street, '''            // +0x1DD0
        AILoadStage                              meAILoadStage          = E_AI_DATA_LOAD_NOT_STARTED;    // +0x1DD4
        DistrictMapLoadStage                     meDistrictMapLoadStage = E_DISTRICT_MAP_LOAD_REQUEST;   // +0x1DD8''',
        '''        LoadStage                                meLoadStage;                                 // +0x1DD0
        AILoadStage                              meAILoadStage;                               // +0x1DD4
        DistrictMapLoadStage                     meDistrictMapLoadStage;                      // +0x1DD8''')

resource = 'src/GameShared/GameClasses/Gui/Model/Resources/CgsGuiResourceModule.h'
replace(resource, '        static const s32 KI_RESOURCE_RECEIVER_QUEUE       = 8192;',
        '        static const s32 KI_RESOURCE_RECEIVER_QUEUE       = 8192;\n        static const s32 KI_MAX_BUNDLES_TO_LOAD            = 64;')
print('Restored seven native shared-header files.')

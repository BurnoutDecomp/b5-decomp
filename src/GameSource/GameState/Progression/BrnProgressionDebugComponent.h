#pragma once

#include "types.hpp"
#include "BrnCommonTypes.h"                                                          // Vector2
#include "GameShared/GameClasses/Development/DebugSystem/Core/CgsDebugComponent.h"   // CgsDev::DebugComponent (real base)
#include "GameShared/GameClasses/Development/DebugSystem/Core/UI/CgsTypes.h"         // CgsDev::DebugUI::StringList, CgsDev::RGBA
#include "GameSource/GameState/BrnGameStateSharedIO.h"                               // GameStateModuleIO::EGameModeType
#include "SharedClasses/Progression/BrnRaceEventData.h"                              // RaceEventData::EModeType

// BrnProgression::ProgressionDebugComponent -- the progression debug menu and HUD: profile and
// rank read-outs, the per-mode rank bars, the profile's events and rivals, the event junction
// list (and the event at the junction the player is waiting at), the rival roster and the car
// opponent sets, plus the win / rank / rival cheats. Member names and order follow the original
// declaration, gated on the console: its object has no room for that declaration's
// miAchievementToAward (the Construct stores run maGameModeOptions straight into the eight
// display flags at +0x70), so that member is left out. The ProgressionManager
// holds the object by value; this header is the class surface the debug bodies compile against.

namespace CgsDev
{
    struct Debug2DImmediateRender;
    struct StrStream;
}

namespace BrnGameState
{
    class ModeManager;
}

namespace BrnProgression
{
    class ProgressionManager;
    class Profile;

    // The display name of an event's mode (the runtime mode the event data maps to), "<none>" for
    // no mode and "<invalid>" for anything outside the mode table.
    const char* GetModeStringForEventData(u8 lu8EventDataMode);

    struct ProgressionDebugComponent : public CgsDev::DebugComponent
    {
    public:
        void RenderHUD(CgsDev::Debug2DImmediateRender* lpDisplay) override;

    private:
        void DrawRankAsBar(CgsDev::Debug2DImmediateRender* lpDisplay, BrnGameState::GameStateModuleIO::EGameModeType leGameModeType,
                           CgsDev::RGBA lColour, Vector2 lv2Position, const char* lpcName);
        void DrawTextWithOffsets(CgsDev::Debug2DImmediateRender* lpDisplay, CgsDev::StrStream* lpStream,
                                 f32 lfX, f32 lfY);
        void AddSpecialEventWins();
        void DefeatAllRivals();
        void RenderRaceEvents(CgsDev::Debug2DImmediateRender* lpDisplay);
        void RenderRaceEventData(CgsDev::Debug2DImmediateRender* lpDisplay, const RaceEventData* lpRaceEventData,
                                 CgsDev::StrStream& lrStream);
        void RenderProfileEvents(CgsDev::Debug2DImmediateRender* lpDisplay, const Profile* lpProfile);
        void RenderProfileRivals(CgsDev::Debug2DImmediateRender* lpDisplay, const Profile* lpProfile);

        static const s32 KI_GAME_MODE_OPTION_COUNT = 7;

        BrnGameState::ModeManager*   mpModeManager;                                 // +0x0C
        ProgressionManager*          mpProgressionManager;                          // +0x10
        s32                          miNumberOfRaceWinsToAdd;                       // +0x14
        s32                          miNumberOfRoadRageWinsToAdd;                   // +0x18
        s32                          miNumberOfMarkedManWinsToAdd;                  // +0x1C
        s32                          miNumberOfStuntWinsToAdd;                      // +0x20
        s32                          miNumberOfSpecialEventWinsToAdd;               // +0x24
        s32                          miRankToSkipTo;                                // +0x28
        s32                          miRivalIndex;                                  // +0x2C
        s32                          miEventStartIndex;                             // +0x30
        RaceEventData::EModeType     meDebugGameModeType;                           // +0x34
        CgsDev::DebugUI::StringList  maGameModeOptions[KI_GAME_MODE_OPTION_COUNT];  // +0x38
        bool                         mbShowProfile;                                 // +0x70
        bool                         mbShowProfileEventInfo;                        // +0x71
        bool                         mbShowProfileRivalInfo;                        // +0x72
        bool                         mbShowRankData;                                // +0x73
        bool                         mbShowRaceData;                                // +0x74
        bool                         mbShowRivalData;                               // +0x75
        bool                         mbShowPlayerOpponents;                         // +0x76
        bool                         mbProfileReadyForDisplay;                      // +0x77
    };
}

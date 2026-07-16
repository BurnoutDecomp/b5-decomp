#include "GameSource/Gui/BrnGuiHudMessageAnalyzer.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"          // CGS_ASSERT
#include "GameShared/GameClasses/Core/CgsStringUtils.h"      // CgsCore::StrCpy (the parked-name copy)
#include "GameSource/Gui/BrnGuiCache.h"                      // GuiCache accessors (frozen-header include set)
#include "GameSource/Gui/BrnGuiEventTypeDefs.h"              // GuiDirtyTrick* payloads + GuiHudMessage
#include "GameSource/Gui/BrnGuiDemangledEventTypes.h"        // remaining opaque event placeholders
#include "GameSource/Network/SharedIO/BrnNetworkModuleInGamePlayerStatusInterface.h" // InGamePlayerStatusData

// BrnGui::HudMessageAnalyzer -- reconstructed from BURNOUT_X360_ARTIST.XEX.
//
// Bodied here (wave-B group 3, the online dirty-trick / payback trio):
//   HudMessageAnalyzer::HandleDirtyTrickNew       @0x8251D868
//   HudMessageAnalyzer::HandleDirtyTrickTriggered @0x8251D988
//   HudMessageAnalyzer::HandleDirtyTrickEnded     @0x8251DF08

namespace BrnGui
{

namespace
{
    // The X360 payback handlers resolve an online-player record by active-race-car
    // index (the compiler inlined the lookup as a maPlayerInfo scan whose asserts name
    // "mpGuiCache->GetOnlinePlayerInfo( ... )"). It is the same scan GetOnlineName runs;
    // reproduced here from the committed GuiCache::GetOnlinePlayerInfo(index) accessor
    // + the record's meActiveRaceCarIndex, returning the matching record or NULL.
    const BrnNetwork::BrnNetworkModuleIO::InGamePlayerStatusData*
    GetOnlinePlayerInfoByActiveRaceCarIndex(const GuiCache* lpGuiCache,
                                            EActiveRaceCarIndex leActiveRaceCarIndex)
    {
        for (s32 liIndex = 0; liIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT; ++liIndex)
        {
            const BrnNetwork::BrnNetworkModuleIO::InGamePlayerStatusData* lpPlayerInfo =
                lpGuiCache->GetOnlinePlayerInfo(liIndex);

            if (static_cast<EActiveRaceCarIndex>(lpPlayerInfo->meActiveRaceCarIndex) == leActiveRaceCarIndex)
                return lpPlayerInfo;
        }
        return NULL;
    }
}

// @ 0x8251D868
// The aggressor has armed a dirty trick. Only the local aggressor gets the "arming"
// heads-up ("DTArming"); everyone else is silent.
void HudMessageAnalyzer::HandleDirtyTrickNew(const GuiDirtyTrickNewEvent* lpEvent)
{
    CGS_ASSERT(lpEvent != NULL, "Invalid Dirty Trick Event");
    CGS_ASSERT(lpEvent->meAggressorActiveRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0,
               "lpDTEvent->meAggressorActiveRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0");
    CGS_ASSERT(lpEvent->meAggressorActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT,
               "lpDTEvent->meAggressorActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT");

    const EActiveRaceCarIndex leLocalPlayer =
        static_cast<EActiveRaceCarIndex>(mpGuiCache->GetPlayerActiveRaceCarIndex());

    if (lpEvent->meAggressorActiveRaceCarIndex == leLocalPlayer)
    {
        GuiHudMessage lMessage;
        lMessage.Construct("DTArming");
        TriggerMessage(&lMessage);
    }
}

// @ 0x8251D988
// A dirty trick has fired. The local aggressor sees "PbkGdPayBack" tagged with the
// victim's name; the local victim sees "PbkBdPayBack" tagged with the aggressor's name
// (and parks the aggressor's name + trick type for the follow-up on-crash message).
void HudMessageAnalyzer::HandleDirtyTrickTriggered(const GuiDirtyTrickTriggerEvent* lpEvent)
{
    CGS_ASSERT(lpEvent != NULL, "Invalid Dirty Trick Event");
    CGS_ASSERT(lpEvent->meTrickType < 3, "Trick type invalid");
    CGS_ASSERT(lpEvent->meAggressorActiveRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0,
               "lpDTEvent->meAggressorActiveRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0");
    CGS_ASSERT(lpEvent->meAggressorActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT,
               "lpDTEvent->meAggressorActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT");
    CGS_ASSERT(lpEvent->meVictimActiveRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0,
               "lpDTEvent->meVictimActiveRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0");
    CGS_ASSERT(lpEvent->meVictimActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT,
               "lpDTEvent->meVictimActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT");

    const EActiveRaceCarIndex leLocalPlayer =
        static_cast<EActiveRaceCarIndex>(mpGuiCache->GetPlayerActiveRaceCarIndex());

    GuiHudMessage lMessage;

    if (lpEvent->meAggressorActiveRaceCarIndex == leLocalPlayer)
    {
        // The local player pulled off the dirty trick.
        lMessage.Construct("PbkGdPayBack");

        CGS_ASSERT(mpGuiCache != NULL, "mpGuiCache");
        const BrnNetwork::BrnNetworkModuleIO::InGamePlayerStatusData* lpVictimInfo =
            GetOnlinePlayerInfoByActiveRaceCarIndex(mpGuiCache, lpEvent->meVictimActiveRaceCarIndex);
        CGS_ASSERT(lpVictimInfo != NULL,
                   "mpGuiCache->GetOnlinePlayerInfo( lpDTEvent->meVictimActiveRaceCarIndex )");
        CGS_ASSERT(lpVictimInfo->mPlayerName.macName != NULL,
                   "mpGuiCache->GetOnlinePlayerInfo( lpDTEvent->meVictimActiveRaceCarIndex )->mPlayerName.GetPlayerName()");

        lMessage.AddParam(CgsGui::E_HUDMESSAGEPARAMTYPES_STRING, 1,
                          lpVictimInfo->mPlayerName.macName);
        TriggerMessage(&lMessage);
    }
    else if (lpEvent->meVictimActiveRaceCarIndex == leLocalPlayer)
    {
        // The local player was the target.
        lMessage.Construct("PbkBdPayBack");
        meDirtyTrickPending = lpEvent->meTrickType;

        // Park the aggressor's online name for the on-crash payback message.
        const BrnNetwork::BrnNetworkModuleIO::InGamePlayerStatusData* lpAggressorInfo =
            GetOnlinePlayerInfoByActiveRaceCarIndex(mpGuiCache, lpEvent->meAggressorActiveRaceCarIndex);
        CgsCore::StrCpy(macDTPendingVictim, sizeof(macDTPendingVictim),
                        lpAggressorInfo->mPlayerName.macName);

        CGS_ASSERT(mpGuiCache != NULL, "mpGuiCache");
        const BrnNetwork::BrnNetworkModuleIO::InGamePlayerStatusData* lpVictimInfo =
            GetOnlinePlayerInfoByActiveRaceCarIndex(mpGuiCache, lpEvent->meVictimActiveRaceCarIndex);
        CGS_ASSERT(lpVictimInfo != NULL,
                   "mpGuiCache->GetOnlinePlayerInfo( lpDTEvent->meVictimActiveRaceCarIndex )");
        CGS_ASSERT(lpVictimInfo->mPlayerName.macName != NULL,
                   "mpGuiCache->GetOnlinePlayerInfo( lpDTEvent->meVictimActiveRaceCarIndex )->mPlayerName.GetPlayerName()");

        lMessage.AddParam(CgsGui::E_HUDMESSAGEPARAMTYPES_STRING, 1,
                          lpAggressorInfo->mPlayerName.macName);
        TriggerMessage(&lMessage);
    }
}

// @ 0x8251DF08
// A dirty trick has resolved. The local aggressor sees whether the victim survived it
// ("DTSurvive") or which trick landed (KAPC_PAYBACK_TAKEDOWN_GD_MESSAGES); a local
// victim who survived sees "DTMlfStab". Either way the pending trick slot resets.
void HudMessageAnalyzer::HandleDirtyTrickEnded(const GuiDirtyTrickEndedEvent* lpEvent)
{
    CGS_ASSERT(lpEvent != NULL, "Invalid Dirty Trick Event");
    CGS_ASSERT(lpEvent->meTrickType < 3, "Trick type invalid");
    CGS_ASSERT(lpEvent->meAggressorActiveRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0,
               "lpDTEvent->meAggressorActiveRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0");
    CGS_ASSERT(lpEvent->meAggressorActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT,
               "lpDTEvent->meAggressorActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT");
    CGS_ASSERT(lpEvent->meVictimActiveRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0,
               "lpDTEvent->meVictimActiveRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0");
    CGS_ASSERT(lpEvent->meVictimActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT,
               "lpDTEvent->meVictimActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT");

    GuiHudMessage lMessage;
    bool lbNameValid = true;

    const EActiveRaceCarIndex leLocalPlayer =
        static_cast<EActiveRaceCarIndex>(mpGuiCache->GetPlayerActiveRaceCarIndex());

    if (lpEvent->meAggressorActiveRaceCarIndex == leLocalPlayer)
    {
        // The local player dealt the trick -- the pending slot is done.
        meDirtyTrickPending = static_cast<BrnNetwork::EPaybackType>(3); // X360 'none' sentinel

        if (lpEvent->mbSurvived)
        {
            lMessage.Construct("DTSurvive");
            lMessage.AddParam(CgsGui::E_HUDMESSAGEPARAMTYPES_STRING, 1,
                              GetOnlineName(lpEvent->meVictimActiveRaceCarIndex, &lbNameValid));
        }
        else
        {
            CGS_ASSERT(lpEvent->meTrickType >= 0,
                       "lpDTEvent->meTrickType >= BrnNetwork::E_PAYBACK_TYPE_START");
            CGS_ASSERT(lpEvent->meTrickType < 3,
                       "lpDTEvent->meTrickType < BrnNetwork::E_PAYBACK_TYPE_COUNT");

            lMessage.Construct(KAPC_PAYBACK_TAKEDOWN_GD_MESSAGES[lpEvent->meTrickType]);
            lMessage.AddParam(CgsGui::E_HUDMESSAGEPARAMTYPES_STRING, 1,
                              GetOnlineName(lpEvent->meVictimActiveRaceCarIndex, &lbNameValid));
        }
    }
    else if (lpEvent->meVictimActiveRaceCarIndex == leLocalPlayer && lpEvent->mbSurvived)
    {
        // The local player was the target and lived through it.
        meDirtyTrickPending = static_cast<BrnNetwork::EPaybackType>(3); // X360 'none' sentinel
        lMessage.Construct("DTMlfStab");
    }

    if (lbNameValid)
        TriggerMessage(&lMessage);
}

}

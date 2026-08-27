// ===================================================================================
// BrnGui::InstantResultsState  -- the offline post-event instant-results presentation state
//   class:BrnGui::InstantResultsState
//
//   InstantResultsState (ctor) @ 0x825006D8
//   GetNextSubstate            @ 0x824B3820
//   ResetStateTimer            @ 0x824B38C0
//   SetEventIconResource       @ 0x824B39B0
// Reconstructed store-for-store from the X360 asm.
// ===================================================================================
#include "GameSource/Gui/Flow/PostEvent/States/Offline/BrnOfflineInstantResults.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"
#include "GameShared/GameClasses/Development/MessageSystem/CgsMessage.h"
#include "GameSource/Gui/BrnGuiCache.h"
#include "GameSource/GameState/BrnGameStateSharedIO.h"   // BrnGameState::GameStateModuleIO::EGameModeType (assert bounds)

namespace BrnGui
{
    // ---- static resource list -------------------------------------------------------------
    // RECOVERED (was the "*** UNRECOVERED VALUES -- CONSOLIDATOR MUST FILL ***" placeholder: a
    // single { 0, 0 } tuple with count 1). Read out of the XEX image -- the IDA export set is
    // function-only, so no data symbol carries these.
    //
    // GetResourcesToLoad's asm @0x82500808 pins both addresses (`lis/addi unk_82F26AFC` -> *r4,
    // `lwz dword_82F26B0C` -> *r5). The extent is self-confirming: 0x82F26B0C - 0x82F26AFC ==
    // 0x10 == exactly two 8-byte tuples, and the count word itself reads 2. The table is bounded
    // on both sides by unrelated pointer data (0x820016C4 before, 0x82065268 after), so it is
    // neither longer nor shorter. Each id is named via off_82F278E0[id] from the same image --
    // the name table the RaceMainHudState 21-entry recovery used, re-checked here against its
    // published names (192 -> "B5RaceHud", 32 -> "Timer", 199 -> "SatNavMap" all reproduce).
    //
    // NOTE the committed count was 1, not 2: the placeholder omitted the SECOND entry, so the
    // offline instant-results screen never requested its manufacturer icon. Type 4 here is
    // E_GUI_RESOURCETYPE_APT, not the FLAPT_HD_BUNDLE the in-game HUD states use; that is what
    // the image says and it is kept as read.
    const CgsGui::sResourceTuple InstantResultsState::maResourcesToLoad[] =
    {
        { 217u, CgsGui::E_GUI_RESOURCETYPE_APT },   // Results
        {  55u, CgsGui::E_GUI_RESOURCETYPE_APT },   // B5ManufacturersIcon
    };
    const u32 InstantResultsState::muNumResourcesToLoad = 2;    // @0x82F26B0C

    // @0x825006D8 -- default constructor. The X360 image inlines this as a flat sequence of
    // vtable-pointer stores into every embedded GUI sub-component slot (each member's own
    // default construction) followed by two invalid-CgsID sentinel initialisations. On the host
    // the member default-constructors set those vtable pointers implicitly; the only explicit
    // member stores are the two -1 sentinels (X360 stw -1 at +0x22E8 and +0x2310), reproduced
    // here as invalid-id inits of the CgsID members DWARF places in that window.
    InstantResultsState::InstantResultsState()
    {
        mCarUnlockId    = -1;   // +0x22E8 (CgsID, DWARF h:443)
        mPendingRivalId = -1;   // +0x2310 (CgsID, DWARF h:448)
    }

    // @0x824B3820 -- return the next active sub-state whose enable flag is raised, scanning
    // forward from meActiveSubState+1; if none is enabled before E_ACTIVE_SUBSTATE_EVENT_COUNT
    // the state machine falls through to E_ACTIVE_SUBSTATE_EVENT_DONE.
    InstantResultsState::EResultsActiveSubStates InstantResultsState::GetNextSubstate()
    {
        CGS_ASSERT(meActiveSubState < E_ACTIVE_SUBSTATE_EVENT_COUNT,
                   "meActiveSubState < E_ACTIVE_SUBSTATE_EVENT_COUNT");

        for (s32 liNext = meActiveSubState + 1; liNext < E_ACTIVE_SUBSTATE_EVENT_COUNT; ++liNext)
        {
            if (mabSubStateFlags[liNext] == 1)
                return static_cast<EResultsActiveSubStates>(liNext);
        }

        return E_ACTIVE_SUBSTATE_EVENT_DONE;
    }

    // @0x824B38C0 -- prime mfTimeRemaining with the on-screen duration for the current active
    // sub-state. The X360 switches on meActiveSubState and loads a distinct .rdata float per
    // case; the E_ACTIVE_SUBSTATE_EVENT_LEAVING(8) case picks a longer dwell on a losing result.
    void InstantResultsState::ResetStateTimer()
    {
        switch (meActiveSubState)
        {
        case E_ACTIVE_SUBSTATE_EVENT_RESULTS:            // 0
            mfTimeRemaining = 4.5f;
            break;
        case E_ACTIVE_SUBSTATE_EVENT_RESULTS_TWO:        // 1
            mfTimeRemaining = 3.0f;
            break;
        case E_ACTIVE_SUBSTATE_EVENT_TAKE_PHOTO:         // 2
        case E_ACTIVE_SUBSTATE_EVENT_RANK_UP_TEXT:       // 3
            mfTimeRemaining = 2.0f;
            break;
        case E_ACTIVE_SUBSTATE_EVENT_RANK_UP_LICENSE:    // 4
            mfTimeRemaining = 9.0f;
            break;
        case E_ACTIVE_SUBSTATE_EVENT_CAR_UNLOCK:         // 5
            mfTimeRemaining = 7.4000001f;
            break;
        case E_ACTIVE_SUBSTATE_EVENT_LEAVING:            // 8
            if (meWinState == E_RESULTS_DETAILED_LOSS || meWinState == E_RESULTS_PLAIN_LOSS)
                mfTimeRemaining = 1.5f;
            else
                mfTimeRemaining = 0.5f;
            break;
        default:                                         // 6, 7 and out-of-range
            mfTimeRemaining = 0.0f;
            break;
        }
    }

    // @0x824B39B0 -- pick the large event-icon resource for the current offline game mode and
    // push it into mLargeEventIcon. Recognised modes map to a fixed BrnGuiResourceId (116..120);
    // any other mode falls back to the default icon (116) after a debug-only complaint.
    void InstantResultsState::SetEventIconResource()
    {
        CGS_ASSERT(mpGuiCache, "mpGuiCache");
        CGS_ASSERT(mpGuiCache->GetGameMode() >= BrnGameState::GameStateModuleIO::E_MODE_NONE,
                   "mpGuiCache->GetGameMode() >= BrnGameState::GameStateModuleIO::E_MODE_NONE");
        CGS_ASSERT(mpGuiCache->GetGameMode() < BrnGameState::GameStateModuleIO::E_MODE_OFFLINE_COUNT,
                   "mpGuiCache->GetGameMode() < BrnGameState::GameStateModuleIO::E_MODE_OFFLINE_COUNT");

        const s32 liGameMode = mpGuiCache->GetGameMode();
        switch (liGameMode)
        {
        case 3:
            muLargeEventIconResource = 118;
            break;
        case 5:
            muLargeEventIconResource = 120;
            break;
        case 7:
            muLargeEventIconResource = 119;
            break;
        case 8:
            muLargeEventIconResource = 117;
            break;
        default:
            if ((CgsDev::Message::gxMessageFilterFlags & CgsDev::Message::KX_FILTER_GLOBAL) != 0)
            {
                *CgsDev::Log::gpDebugPrint
                    << "Invalid game mode in InstantResultsState::SetEventIconResource- "
                    << liGameMode
                    << ", should really be an error but for debug purposes we'll play nice\n";
            }
            muLargeEventIconResource = 116;
            break;
        }
    }
}

// ============================================================================
// b5-decomp/src/GameSource/GameState/RichPresenceManager/X360/BrnGameStateRichPresenceManagerX360.cpp
// ============================================================================
// Bodies of BrnGameState::RichPresenceManagerX360 (see the header for the context-queue /
// presence-thread design). The context and property ids are the title's own presence schema;
// the four value tables were read out of the image. XUserSetContext / XUserSetProperty go
// through the PC platform layer (CgsXboxLivePC.cpp / CgsXboxLivePC_wBT_01.cpp).

#include "GameSource/GameState/RichPresenceManager/X360/BrnGameStateRichPresenceManagerX360.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"                          // CGS_ASSERT
#include "GameShared/GameClasses/Development/PerfMon/Cpu/CgsPerfMonCpu.h"   // CgsDev::PerfMonCpu::StartMonitor / StopMonitor

extern "C" void XUserSetContext(u32 luUserIndex, u32 luContextId, u32 luContextValue);
extern "C" void XUserSetProperty(u32 luUserIndex, u32 luPropertyId, u32 lcbValue, const void* lpvValue);

namespace BrnGameState
{
namespace
{
    // The presence contexts each writer sets.
    const u32 KU_CONTEXT_FINISH_POSITION = 0;
    const u32 KU_CONTEXT_LOBBY_TYPE      = 1;
    const u32 KU_CONTEXT_RANKED          = 4;
    const u32 KU_CONTEXT_DISTRICT        = 11;
    const u32 KU_CONTEXT_PRESENCE        = 0x8001;   // the system's presence-mode context

    // The 32-bit integer presence properties each writer sets.
    const u32 KU_PROPERTY_CURRENT_ROUND  = 0x10000002;
    const u32 KU_PROPERTY_TOTAL_ROUNDS   = 0x10000003;
    const u32 KU_PROPERTY_NUMBER_PLAYERS = 0x10000006;

    // The context values for the ranked flag.
    const u32 KU_CONTEXT_VALUE_RANKED   = 0;
    const u32 KU_CONTEXT_VALUE_UNRANKED = 1;

    // ERichPresenceStates -> presence-mode context value (read from the image).
    const u32 KAU_PRESENCE_STATE_CONTEXT_VALUES[RichPresenceManagerBase::E_PRESENCE_STATE_COUNT] =
    {
        0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15,
    };

    // ERichPresenceStates -> lobby-type context value; -1 sets no lobby context (the offline
    // states). Read from the image.
    const s32 KAI_LOBBY_TYPE_CONTEXT_VALUES[RichPresenceManagerBase::E_PRESENCE_STATE_COUNT] =
    {
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 3, 1, 1, 1, 5, 1,
    };
    const s32 KI_NO_LOBBY_CONTEXT = -1;

    // Finish position 1..8 (one per active race car) -> finish-position context value (read
    // from the image; indexed by position - 1).
    const s32 KI_NUM_FINISH_POSITIONS = 8;
    const u32 KAU_FINISH_POSITION_CONTEXT_VALUES[KI_NUM_FINISH_POSITIONS] =
    {
        0, 1, 2, 3, 4, 5, 6, 7,
    };

    // BrnWorld::EDistrict -> district context value (read from the image).
    const u32 KAU_DISTRICT_CONTEXT_VALUES[BrnWorld::E_DISTRICT_VALID_COUNT] =
    {
        0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17,
    };

    // The game thread waits 1 ms between attempts on a busy queue, the presence thread 16 ms.
    const s32 KI_CONTEXT_QUEUE_SLEEP_MS = 1;
    const s32 KI_PRESENCE_THREAD_SLEEP_MS = 16;

    // "PresenceThread" runs on hardware thread 1 at the default priority.
    const int  KI_PRESENCE_THREAD_PROCESSOR = 1;
    const int  KI_PRESENCE_THREAD_PRIORITY  = 0;
    const char KAC_PRESENCE_THREAD_NAME[]   = "PresenceThread";
}

// Build both context queues over their storage, put every record on the free queue, then start
// the presence thread on this manager.
void RichPresenceManagerX360::CreatePresenceThread()
{
    mFreePresenceContextQueue.Construct(mapFreePresenceContexts, KU_NUM_PRESENCE_CONTEXTS);
    mPendingPresenceContextQueue.Construct(mapPendingPresenceContexts, KU_NUM_PRESENCE_CONTEXTS);

    for (u16 luContext = 0; luContext < KU_NUM_PRESENCE_CONTEXTS; ++luContext)
    {
        PresenceContext* lpContext = &maPresenceContexts[luContext];
        mFreePresenceContextQueue.Post(&lpContext, true, KI_CONTEXT_QUEUE_SLEEP_MS);
    }

    EA::Thread::ThreadParameters lThreadParameters;
    lThreadParameters.mnPriority  = KI_PRESENCE_THREAD_PRIORITY;
    lThreadParameters.mnProcessor = KI_PRESENCE_THREAD_PROCESSOR;
    lThreadParameters.mpName      = KAC_PRESENCE_THREAD_NAME;
    mPresenceThread.Begin(&RichPresenceManagerX360::PresenceThread, this, &lThreadParameters,
                          EA::Thread::Thread::GetGlobalRunnableFunctionUserWrapper());
}

// The presence thread's body. It never returns.
intptr_t RichPresenceManagerX360::PresenceThread(void* lpRichPresenceManager)
{
    RichPresenceManagerX360* lpManager = static_cast<RichPresenceManagerX360*>(lpRichPresenceManager);
    for (;;)
    {
        PresenceContext* lpContext = 0;
        lpManager->mPendingPresenceContextQueue.Pop(&lpContext, true, KI_PRESENCE_THREAD_SLEEP_MS);
        XUserSetContext(lpContext->muUserIndex, lpContext->muContextId, lpContext->muContextValue);
        lpManager->mFreePresenceContextQueue.Post(&lpContext, true, KI_PRESENCE_THREAD_SLEEP_MS);
    }
}

// Hand one context write to the presence thread (timed under the "XUserSetContext" monitor).
void RichPresenceManagerX360::SetContext(u32 luUserIndex, u32 luContextId, u32 luContextValue)
{
    CgsDev::PerfMonCpu::StartMonitor(miSetContextPM);

    PresenceContext* lpContext = 0;
    mFreePresenceContextQueue.Pop(&lpContext, true, KI_CONTEXT_QUEUE_SLEEP_MS);
    lpContext->muUserIndex    = luUserIndex;
    lpContext->muContextId    = luContextId;
    lpContext->muContextValue = luContextValue;
    mPendingPresenceContextQueue.Post(&lpContext, true, KI_CONTEXT_QUEUE_SLEEP_MS);

    CgsDev::PerfMonCpu::StopMonitor(miSetContextPM);
}

// vtable +0x00: the presence mode.
void RichPresenceManagerX360::SetRichPresenceState(ERichPresenceStates leNewState)
{
    CGS_ASSERT(leNewState >= 0, "leNewState >= 0");
    CGS_ASSERT(leNewState < E_PRESENCE_STATE_COUNT, "leNewState < E_PRESENCE_STATE_COUNT");

    SetContext(static_cast<u32>(GetUserID()), KU_CONTEXT_PRESENCE, KAU_PRESENCE_STATE_CONTEXT_VALUES[leNewState]);
}

// vtable +0x04: the current round (a property).
void RichPresenceManagerX360::SetCurrentRound(s32 liRound)
{
    XUserSetProperty(static_cast<u32>(GetUserID()), KU_PROPERTY_CURRENT_ROUND, sizeof(liRound), &liRound);
}

// vtable +0x08: the number of rounds (a property).
void RichPresenceManagerX360::SetTotalRounds(s32 liTotalRounds)
{
    XUserSetProperty(static_cast<u32>(GetUserID()), KU_PROPERTY_TOTAL_ROUNDS, sizeof(liTotalRounds), &liTotalRounds);
}

// vtable +0x0C: the finish position, 1-based.
void RichPresenceManagerX360::SetCurrentPosition(s32 liFinishPosition)
{
    CGS_ASSERT(liFinishPosition >= 0, "liFinishPosition >= 0");
    CGS_ASSERT(liFinishPosition <= KI_NUM_FINISH_POSITIONS,
               "liFinishPosition <= BrnWorld::KI_MAX_ACTIVE_RACE_CARS");

    SetContext(static_cast<u32>(GetUserID()), KU_CONTEXT_FINISH_POSITION,
               KAU_FINISH_POSITION_CONTEXT_VALUES[liFinishPosition - 1]);
}

// vtable +0x10: the lobby type -- only for the online states, and only while the lobby context
// is enabled.
void RichPresenceManagerX360::SetLobbyType(ERichPresenceStates leLobbyState)
{
    const s32 liContextValue = KAI_LOBBY_TYPE_CONTEXT_VALUES[leLobbyState];
    if (liContextValue == KI_NO_LOBBY_CONTEXT)
    {
        return;
    }
    if (miLobbyContextEnabled == KI_NO_LOBBY_CONTEXT)
    {
        return;
    }

    SetContext(static_cast<u32>(GetUserID()), KU_CONTEXT_LOBBY_TYPE, static_cast<u32>(liContextValue));
}

// vtable +0x14: ranked or unranked.
void RichPresenceManagerX360::SetRankedStatus(EGameRankedType leRanked)
{
    switch (leRanked)
    {
        case E_GAME_RANKED:
            SetContext(static_cast<u32>(GetUserID()), KU_CONTEXT_RANKED, KU_CONTEXT_VALUE_RANKED);
            break;

        case E_GAME_UNRANKED:
            SetContext(static_cast<u32>(GetUserID()), KU_CONTEXT_RANKED, KU_CONTEXT_VALUE_UNRANKED);
            break;

        default:
            CGS_ASSERT(false, "Unexpected ranked game status");
            break;
    }
}

// vtable +0x18: the district.
void RichPresenceManagerX360::SetDistrict(BrnWorld::EDistrict leDistrict)
{
    CGS_ASSERT(leDistrict >= 0, "leDistrict >= 0");
    CGS_ASSERT(leDistrict < BrnWorld::E_DISTRICT_VALID_COUNT, "leDistrict < BrnWorld::E_DISTRICT_VALID_COUNT");

    SetContext(static_cast<u32>(GetUserID()), KU_CONTEXT_DISTRICT, KAU_DISTRICT_CONTEXT_VALUES[leDistrict]);
}

// vtable +0x1C: the player count (a property), written only when it changes.
void RichPresenceManagerX360::SetNumberPlayers(s32 liNumberPlayers)
{
    if (miNumberPlayers == liNumberPlayers)
    {
        return;
    }

    XUserSetProperty(static_cast<u32>(GetUserID()), KU_PROPERTY_NUMBER_PLAYERS, sizeof(liNumberPlayers),
                     &liNumberPlayers);
    miNumberPlayers = liNumberPlayers;
}

}

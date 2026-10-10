#pragma once

// ============================================================================
// b5-decomp/src/GameSource/GameState/RichPresenceManager/X360/BrnGameStateRichPresenceManagerX360.h
// ============================================================================
// BrnGameState::RichPresenceManagerX360 -- the Xbox 360 rich-presence writer under
// RichPresenceManagerBase. The base decides WHAT the presence shows; this class turns each field
// change into the system call that shows it:
//
//   * the presence state, finish position, lobby type, ranked flag and district are profile
//     CONTEXTS. XUserSetContext can block, so the game thread never calls it: SetContext takes a
//     free context record off one lockless queue, fills it, and posts it on another; a dedicated
//     "PresenceThread" pops each pending record, calls XUserSetContext, and returns the record.
//     Five records circulate (CreatePresenceThread seeds the free queue with all of them).
//   * the round, round count and player count are profile PROPERTIES, set directly with
//     XUserSetProperty (the player count only when it changed).
//
// Console layout (the base ends at +0x4C):
//   +0x4C  maPresenceContexts[5]          12-byte records
//   +0x88  mapFreePresenceContexts[5]     storage of the free queue
//   +0x9C  mapPendingPresenceContexts[5]  storage of the pending queue
//   +0xB0  mFreePresenceContextQueue      LocklessQueue (16 bytes)
//   +0xC0  mPendingPresenceContextQueue   LocklessQueue (16 bytes)
//   +0xD0  miNumberPlayers                last player count set (Prepare: 0)
//   +0xD4  miLobbyContextEnabled          -1 suppresses the lobby-type context (Prepare: -1)
//   +0xD8  mPresenceThread                EA::Thread::Thread
//   +0xDC  miSetContextPM                 "XUserSetContext" CPU perfmon handle
// Members are reached by name; the host widens the pointers.
//
// The construct / prepare halves of this class are inlined into GameStateModule::Construct and
// ::Prepare (perfmon registration, CreatePresenceThread, the base Construct; the two cache
// resets, the base Prepare).

#include "types.hpp"
#include "GameShared/GameClasses/Containers/CgsLocklessQueue.h"                  // CgsContainers::LocklessQueue
#include "GameSource/GameState/RichPresenceManager/BrnGameStateRichPresenceManagerBase.h"
#include "eathread/eathread_thread.h"                                           // EA::Thread::Thread

namespace BrnGameState
{

class RichPresenceManagerX360 : public RichPresenceManagerBase
{
    // The construct half (the perfmon registration into miSetContextPM) is inlined into
    // GameStateModule::Construct on the console.
    friend class GameStateModule;

public:
    // One XUserSetContext request.
    struct PresenceContext
    {
        u32 muUserIndex;      // +0x00
        u32 muContextId;      // +0x04
        u32 muContextValue;   // +0x08
    };

    // The number of context records in circulation (the size of both queues).
    static const u16 KU_NUM_PRESENCE_CONTEXTS = 5;

    // Seed the free queue with every context record and start "PresenceThread".
    void CreatePresenceThread();

    // The presence thread: forever pop a pending record, set that context, free the record.
    static intptr_t PresenceThread(void* lpRichPresenceManager);

    // Queue one XUserSetContext for the presence thread (blocks while every record is pending).
    void SetContext(u32 luUserIndex, u32 luContextId, u32 luContextValue);

protected:
    // The base's per-field presence writers (vtable slots +0x00..+0x1C, in order).
    void SetRichPresenceState(ERichPresenceStates leNewState) override;
    void SetCurrentRound(s32 liRound) override;
    void SetTotalRounds(s32 liTotalRounds) override;
    void SetCurrentPosition(s32 liFinishPosition) override;
    void SetLobbyType(ERichPresenceStates leLobbyState) override;
    void SetRankedStatus(EGameRankedType leRanked) override;
    void SetDistrict(BrnWorld::EDistrict leDistrict) override;
    void SetNumberPlayers(s32 liNumberPlayers) override;

private:
    typedef CgsContainers::LocklessQueue<PresenceContext*> PresenceContextQueue;

    PresenceContext      maPresenceContexts[KU_NUM_PRESENCE_CONTEXTS];          // +0x4C
    PresenceContext*     mapFreePresenceContexts[KU_NUM_PRESENCE_CONTEXTS];     // +0x88
    PresenceContext*     mapPendingPresenceContexts[KU_NUM_PRESENCE_CONTEXTS];  // +0x9C
    PresenceContextQueue mFreePresenceContextQueue;                             // +0xB0
    PresenceContextQueue mPendingPresenceContextQueue;                          // +0xC0
    s32                  miNumberPlayers;                                       // +0xD0
    s32                  miLobbyContextEnabled;                                 // +0xD4
    EA::Thread::Thread   mPresenceThread;                                       // +0xD8
    s32                  miSetContextPM;                                        // +0xDC
};

}

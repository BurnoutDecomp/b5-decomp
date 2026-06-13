#include "types.hpp"

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   CgsNetwork::StartTimeManager::StartTimeManager     @ 0x827E1428  (constructor)
//   CgsNetwork::StartTimeManager::AreAllPlayersReadyTo @ 0x82891ED0
//
// The manager holds an array of 7 per-player "ready" sub-records (256-byte stride from
// offset 0) plus a trailing block of 8 {int, float} time pairs. The constructor installs
// the two sub-record vtables (off_820CF48C / off_820CF4B4) and zeroes the counters/timers.
//
// AreAllPlayersReadyTo returns 2 if the candidate start time (pTime) is already past the
// target, otherwise walks every connected player: if any connected player has not yet
// acknowledged (its slot in the sub-record array is missing or not flagged ready) it
// returns 0; if all are ready it returns 1. Ported faithfully from the X360 control flow
// (PPC save/restore-GPR thunks dropped).

namespace CgsNetwork
{
    // Other TUs; declared for the compile-only gate.
    struct PlayerManager
    {
        int GetNextPlayerID(int* pPlayerId, int liStep);
    };
    struct PlayersConnectionManager
    {
        int GetConnectionStatus(int liPlayerId);
    };

    // Sub-record vtables (off_820CF48C / off_820CF4B4), defined with their classes.
    extern const u8 gStartTimeReadyVTable;     // off_820CF48C
    extern const u8 gStartTimeTimerVTable;     // off_820CF4B4

    class StartTimeManager
    {
    public:
        void* Construct();
        int   AreAllPlayersReadyTo(const void* pTime, const void* pUnused, const void* pTarget);
    };

    void* StartTimeManager::Construct()
    {
        uintptr_t lBase = reinterpret_cast<uintptr_t>(this);
        const u32 luReadyVT = static_cast<u32>(reinterpret_cast<uintptr_t>(&gStartTimeReadyVTable));
        const u32 luTimerVT = static_cast<u32>(reinterpret_cast<uintptr_t>(&gStartTimeTimerVTable));

        auto Word  = [lBase](int liOff) -> u32& { return *reinterpret_cast<u32*>(lBase + liOff); };
        auto Float = [lBase](int liOff) -> f32& { return *reinterpret_cast<f32*>(lBase + liOff); };

        // 7 per-player ready sub-records, 256-byte stride.
        for (int liRec = 0; liRec < 7; ++liRec)
        {
            int liB = liRec * 256;
            Word(liB + 0)   = luReadyVT;
            Word(liB + 44)  = 0;
            Float(liB + 48) = 0.0f;
            Word(liB + 84)  = luReadyVT;
            Word(liB + 128) = 0;
            Float(liB + 132) = 0.0f;
            Word(liB + 168) = luTimerVT;
            Word(liB + 208) = luTimerVT;
        }

        // Trailing time pairs: one at 1804, then seven at 1836 (8-byte stride).
        Word(1804) = 0;
        Float(1808) = 0.0f;
        for (int liPair = 0; liPair < 7; ++liPair)
        {
            int liO = 1836 + liPair * 8;
            Word(liO) = 0;
            Float(liO + 4) = 0.0f;
        }
        return this;
    }

    int StartTimeManager::AreAllPlayersReadyTo(const void* pTime, const void* /*pUnused*/, const void* pTarget)
    {
        uintptr_t lThis   = reinterpret_cast<uintptr_t>(this);
        uintptr_t lTime   = reinterpret_cast<uintptr_t>(pTime);
        uintptr_t lTarget = reinterpret_cast<uintptr_t>(pTarget);

        int liTime = *reinterpret_cast<const int*>(lTime + 16);
        if (liTime > *reinterpret_cast<const int*>(lTarget)
            || (liTime >= *reinterpret_cast<const int*>(lTarget)
                && *reinterpret_cast<const int*>(lTime + 20) >= *reinterpret_cast<const int*>(lTarget + 4)))
        {
            return 2;
        }

        int liPlayerId = -1;
        PlayerManager* lpPlayers = *reinterpret_cast<PlayerManager**>(lThis + 1812);

        while (lpPlayers->GetNextPlayerID(&liPlayerId, 1))
        {
            uintptr_t lMgr = *reinterpret_cast<uintptr_t*>(lThis + 1812);
            char lbReady = 0;
            int liSlot = 0;
            int liCount = *reinterpret_cast<int*>(lMgr + 9184);
            if (liCount > 0)
            {
                int liTargetId = liPlayerId;
                uintptr_t lEntry = lMgr + 9080;
                while (*reinterpret_cast<int*>(lEntry) != liPlayerId)
                {
                    ++liSlot;
                    lEntry += 12;
                    if (liSlot >= liCount)
                        goto NEXT_PLAYER;
                }

                uintptr_t lPlayer = *reinterpret_cast<uintptr_t*>(12 * liSlot + lMgr + 9076);
                if (lPlayer)
                {
                    PlayersConnectionManager* lpConn =
                        reinterpret_cast<PlayersConnectionManager*>(lMgr);
                    if (lpConn->GetConnectionStatus(liPlayerId) != 3)
                        return 0;

                    if (!*reinterpret_cast<char*>(lPlayer + 2985)
                        && !*reinterpret_cast<char*>(lPlayer + 2984))
                    {
                        int liSub = 0;
                        uintptr_t lRec = lThis + 248;
                        while (*reinterpret_cast<int*>(lRec) == -1
                               || *reinterpret_cast<int*>(lRec) != liTargetId
                               || !*reinterpret_cast<int*>(lRec + 4))
                        {
                            ++liSub;
                            lRec += 256;
                            if (liSub >= 7)
                                goto CHECK_READY;
                        }
                        lbReady = 1;
                    CHECK_READY:
                        if (!lbReady)
                            return 0;
                    }
                }
            }
        NEXT_PLAYER:;
        }
        return 1;
    }
}

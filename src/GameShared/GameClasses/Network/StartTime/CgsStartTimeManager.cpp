#include "types.hpp"

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   CgsNetwork::StartTimeManager::StartTimeManager     @ 0x827E1428  (constructor)
//   CgsNetwork::StartTimeManager::AreAllPlayersReadyTo @ 0x82891ED0
//
// The manager owns an array of 7 per-player "ready" sub-records (256-byte stride) plus a
// trailing set of {frame, time} pairs. Each sub-record embeds two ack sub-objects and two
// timer sub-objects whose vtables the constructor installs (0x820CF48C / 0x820CF4B4); the
// player id / ready flag live at the tail of the record (+248 / +252).
//
// AreAllPlayersReadyTo returns 2 when the candidate start point is already past the target,
// otherwise walks every connected player via the PlayerManager: any connected, in-game
// player that has not acknowledged ready yields 0; all-ready yields 1. Member access is by
// name; the PlayerManager connection table layout (offsets owned by CgsPlayerManager) is
// modelled as a typed view here purely to read it without raw-offset casts. PPC
// save/restore-GPR thunks dropped.

namespace CgsNetwork
{
namespace
{
const u32 KU_READY_SUBRECORD_VTABLE = 0x820CF48C;   // off_820CF48C
const u32 KU_TIMER_SUBRECORD_VTABLE = 0x820CF4B4;   // off_820CF4B4

const int KI_MAX_READY_RECORDS    = 7;
const int KI_CONNECTION_READY     = 3;   // GetConnectionStatus() value for "fully connected"
const int KI_INVALID_PLAYER_ID    = -1;
}

// ---- Foreign types reached into by this TU (real layouts owned by their own TUs) ----

// One slot of the PlayerManager connection table (12-byte stride, base +9076).
struct PlayerConnectionEntry
{
    void* mpPlayer;       // +0  (PlayerManager + 9076 + 12*i)
    int   miPlayerId;     // +4  (PlayerManager + 9080 + 12*i)
    int   miReserved;     // +8
};

// The connected player record carries two "left / not-in-game" flags near +2984.
struct ConnectedPlayer
{
    u8  mPad[2984];
    u8  mbHasLeft;        // +2984
    u8  mbNotInGame;      // +2985
};

struct PlayerManager
{
    int  GetNextPlayerID(int* pPlayerId, int liStep);
    int  GetConnectionStatus(int liPlayerId);

    // Connection table (recovered offsets; declared here only to read by name).
    PlayerConnectionEntry* ConnectionEntries() { return reinterpret_cast<PlayerConnectionEntry*>(reinterpret_cast<u8*>(this) + 9076); }
    int  ConnectionCount() { return *reinterpret_cast<int*>(reinterpret_cast<u8*>(this) + 9184); }
};

// Start-sync time stamps compared by AreAllPlayersReadyTo (external param types).
struct StartTimeStamp
{
    u8  mPad[16];
    int miFrame;          // +16
    int miTime;           // +20
};
struct StartTimeTarget
{
    int miFrame;          // +0
    int miTime;           // +4
};

// ---- StartTimeManager own layout (recovered) ----

struct StartTimeReadyRecord                 // 256-byte stride
{
    u32 muAckVTableA;     // +0
    u8  mPad0[40];
    int miAckCounterA;    // +44
    f32 mfAckTimerA;      // +48
    u8  mPad1[32];
    u32 muAckVTableB;     // +84
    u8  mPad2[40];
    int miAckCounterB;    // +128
    f32 mfAckTimerB;      // +132
    u8  mPad3[32];
    u32 muTimerVTableA;   // +168
    u8  mPad4[36];
    u32 muTimerVTableB;   // +208
    u8  mPad5[36];
    int miPlayerId;       // +248
    int miReadyFlag;      // +252
};

struct StartTimePair
{
    int miFrame;
    f32 mfTime;
};

class StartTimeManager
{
public:
    void* Construct();
    int   AreAllPlayersReadyTo(const StartTimeStamp* pNow, const void* pUnused, const StartTimeTarget* pTarget);

private:
    bool IsPlayerAcknowledged(int liPlayerId) const;

    StartTimeReadyRecord maRecords[KI_MAX_READY_RECORDS];   // +0..+1791
    u8             mPad0[12];                                // +1792
    StartTimePair  mLeadPair;                               // +1804
    PlayerManager* mpPlayerManager;                         // +1812
    u8             mPad1[20];
    StartTimePair  maPairs[7];                              // +1836
};

void* StartTimeManager::Construct()
{
    for (StartTimeReadyRecord& lRecord : maRecords)
    {
        lRecord.muAckVTableA   = KU_READY_SUBRECORD_VTABLE;
        lRecord.miAckCounterA  = 0;
        lRecord.mfAckTimerA    = 0.0f;
        lRecord.muAckVTableB   = KU_READY_SUBRECORD_VTABLE;
        lRecord.miAckCounterB  = 0;
        lRecord.mfAckTimerB    = 0.0f;
        lRecord.muTimerVTableA = KU_TIMER_SUBRECORD_VTABLE;
        lRecord.muTimerVTableB = KU_TIMER_SUBRECORD_VTABLE;
    }

    mLeadPair.miFrame = 0;
    mLeadPair.mfTime  = 0.0f;
    for (StartTimePair& lPair : maPairs)
    {
        lPair.miFrame = 0;
        lPair.mfTime  = 0.0f;
    }
    return this;
}

// True if one of the ready records names this player and is flagged ready.
bool StartTimeManager::IsPlayerAcknowledged(int liPlayerId) const
{
    for (const StartTimeReadyRecord& lRecord : maRecords)
    {
        if (lRecord.miPlayerId != KI_INVALID_PLAYER_ID
            && lRecord.miPlayerId == liPlayerId
            && lRecord.miReadyFlag)
        {
            return true;
        }
    }
    return false;
}

int StartTimeManager::AreAllPlayersReadyTo(const StartTimeStamp* pNow, const void* /*pUnused*/, const StartTimeTarget* pTarget)
{
    if (pNow->miFrame > pTarget->miFrame
        || (pNow->miFrame >= pTarget->miFrame && pNow->miTime >= pTarget->miTime))
    {
        return 2;
    }

    int liPlayerId = KI_INVALID_PLAYER_ID;
    while (mpPlayerManager->GetNextPlayerID(&liPlayerId, 1))
    {
        const int liCount = mpPlayerManager->ConnectionCount();
        if (liCount <= 0)
            continue;

        PlayerConnectionEntry* lpEntries = mpPlayerManager->ConnectionEntries();
        int liSlot = 0;
        while (liSlot < liCount && lpEntries[liSlot].miPlayerId != liPlayerId)
            ++liSlot;
        if (liSlot >= liCount)
            continue;

        ConnectedPlayer* lpPlayer = static_cast<ConnectedPlayer*>(lpEntries[liSlot].mpPlayer);
        if (!lpPlayer)
            continue;

        if (mpPlayerManager->GetConnectionStatus(liPlayerId) != KI_CONNECTION_READY)
            return 0;

        // A player that is still present and in-game must have acknowledged the start.
        if (!lpPlayer->mbNotInGame && !lpPlayer->mbHasLeft && !IsPlayerAcknowledged(liPlayerId))
            return 0;
    }
    return 1;
}
}

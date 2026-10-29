#include "GameSource/Replays/Serialisers/BrnReplayDirectorBridgeSerialiser.h"

// BrnReplays::DirectorBridgeSerialiser -- reconstructed from
// BURNOUT_X360_ARTIST.XEX. (This file's earlier Construct-only slice is
// DE-FORKED onto the real header home; its file-local BaseSerialiser fork is
// retired.)
//
// Bodied here (4 of the TU's ledger functions; the six getters are inline in
// the header, matching the original's h:193.. placement):
//   Construct @0x8264C498   Read @0x82657478   Write @0x82657210
//   SerialiseGameActionQueue   GetStaticLayout
//
// Read and Write stream the SAME ten snapshot spans (the Write order below);
// Read adds the mode-skip ladder up front (the batch-11 DirectorSerialiser
// pattern) while Write only null-checks the buffer. Each span re-reads the mode
// and dispatches RECORDING -> append / PLAYING -> restore, faithfully.

namespace BrnReplays
{

// @ 0x8264C498 -- the leaf forwarder: id 3, mode 0, the 0x8000 buffer pair,
// the channel name, flag 0.
s32 DirectorBridgeSerialiser::Construct()
{
    return BaseSerialiser::Construct(3, 0, 0x8000, 0x8000, "DirectorBridge", 0);
}

// @ 0x82657478 -- cpp:149. Lock, resolve the snapshot, run the mode-skip ladder
// (STALLED 3/6, PREPARING 1/4, RESTORING 7 no-op), then stream the ten spans.
// Always Unlocks.
void DirectorBridgeSerialiser::Read()
{
    BaseSerialiser::Lock();

    DirectorBridgeSerialiserStaticLayout* lpStatic = GetStaticLayout();
    CGS_ASSERT(lpStatic != 0, "lpStatic");   // :149 (non-gating)

    const EMode leMode = GetMode();
    bool lbSkip = (leMode == E_MODE_RECORDING_STALLED || leMode == E_MODE_PLAYING_STALLED);
    if (!lbSkip)
    {
        lbSkip = (leMode == E_MODE_RECORDING_PREPARING || leMode == E_MODE_PLAYING_PREPARING);
        if (!lbSkip && leMode != E_MODE_RESTORING)
        {
            BaseSerialiser::Serialise(&lpStatic->muActivePlayerIndex, sizeof(lpStatic->muActivePlayerIndex));
            BaseSerialiser::Serialise(&lpStatic->mabVehicleActive, sizeof(lpStatic->mabVehicleActive));
            BaseSerialiser::Serialise(&lpStatic->maVehicleInfos, sizeof(lpStatic->maVehicleInfos));
            BaseSerialiser::Serialise(&lpStatic->mPlayerCrashInfo, sizeof(lpStatic->mPlayerCrashInfo));
            BaseSerialiser::Serialise(&lpStatic->maCentreOfMassTransforms, sizeof(lpStatic->maCentreOfMassTransforms));
            BaseSerialiser::Serialise(&lpStatic->mPlayerScoreData, sizeof(lpStatic->mPlayerScoreData));
            BaseSerialiser::Serialise(&lpStatic->mbPlayerComboWarningActive, sizeof(lpStatic->mbPlayerComboWarningActive));
            BaseSerialiser::Serialise(&lpStatic->mfPlayerBoostPercentage, sizeof(lpStatic->mfPlayerBoostPercentage));
            BaseSerialiser::Serialise(&lpStatic->mbPlayerWasTakenDown, sizeof(lpStatic->mbPlayerWasTakenDown));
            BaseSerialiser::Serialise(&lpStatic->muPlayerKillerIndex, sizeof(lpStatic->muPlayerKillerIndex));
            SerialiseGameActionQueue();   // the queue stream step (own ledger fn)
        }
    }

    BaseSerialiser::Unlock();
}

// . The static buffer must hold the whole 24320-byte snapshot (non-gating
// tripwire); hands back the buffer as the layout.
DirectorBridgeSerialiserStaticLayout* DirectorBridgeSerialiser::GetStaticLayout()
{
    CGS_ASSERT(miStaticBufferSize >= static_cast<s32>(sizeof(DirectorBridgeSerialiserStaticLayout)),
               "Static buffer size is too small\n");
    return static_cast<DirectorBridgeSerialiserStaticLayout*>(mpStaticBuffer);
}

// . The game-action queue rides the stream as a packed variable queue:
// any recording mode writes it; a playing mode that is not stalled rebuilds it
// (Construct, then pop every event back in). Stalled-playing and the other
// modes leave the snapshot's queue untouched.
void DirectorBridgeSerialiser::SerialiseGameActionQueue()
{
    DirectorBridgeSerialiserStaticLayout* lpStaticLayout = GetStaticLayout();

    if (IsRecording())
    {
        WriteVariableQueue(&lpStaticLayout->mGameActionQueue);
    }
    else if (IsPlaying())
    {
        const EMode leMode = GetMode();
        const bool lbStalled = (leMode == E_MODE_RECORDING_STALLED || leMode == E_MODE_PLAYING_STALLED);
        if (!lbStalled)
        {
            lpStaticLayout->mGameActionQueue.Construct();
            ReadVariableQueue(&lpStaticLayout->mGameActionQueue);
        }
    }
}

// @ 0x82657210 -- lock, resolve, stream the same ten spans (only the null
// guard, no skip ladder). Always Unlocks.
void DirectorBridgeSerialiser::Write()
{
    BaseSerialiser::Lock();

    DirectorBridgeSerialiserStaticLayout* lpStatic = GetStaticLayout();
    if (lpStatic != 0)
    {
        BaseSerialiser::Serialise(&lpStatic->muActivePlayerIndex, sizeof(lpStatic->muActivePlayerIndex));
        BaseSerialiser::Serialise(&lpStatic->mabVehicleActive, sizeof(lpStatic->mabVehicleActive));
        BaseSerialiser::Serialise(&lpStatic->maVehicleInfos, sizeof(lpStatic->maVehicleInfos));
        BaseSerialiser::Serialise(&lpStatic->mPlayerCrashInfo, sizeof(lpStatic->mPlayerCrashInfo));
        BaseSerialiser::Serialise(&lpStatic->maCentreOfMassTransforms, sizeof(lpStatic->maCentreOfMassTransforms));
        BaseSerialiser::Serialise(&lpStatic->mPlayerScoreData, sizeof(lpStatic->mPlayerScoreData));
        BaseSerialiser::Serialise(&lpStatic->mbPlayerComboWarningActive, sizeof(lpStatic->mbPlayerComboWarningActive));
        BaseSerialiser::Serialise(&lpStatic->mfPlayerBoostPercentage, sizeof(lpStatic->mfPlayerBoostPercentage));
        BaseSerialiser::Serialise(&lpStatic->mbPlayerWasTakenDown, sizeof(lpStatic->mbPlayerWasTakenDown));
        BaseSerialiser::Serialise(&lpStatic->muPlayerKillerIndex, sizeof(lpStatic->muPlayerKillerIndex));
        SerialiseGameActionQueue();   // the queue stream step (own ledger fn)
    }

    BaseSerialiser::Unlock();
}

}

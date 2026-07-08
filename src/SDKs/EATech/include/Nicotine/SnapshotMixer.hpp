#pragma once

// ===========================================================================
//  Nicotine::SnapshotMixer -- the snapshot (mix-state) mixer that IDynamicMixer drives:
//  it holds the loaded snapshot header, the per-channel ramp array (SnapshotChannel),
//  and the snapshot/status array, and each frame ramps the active snapshot's channel
//  volumes into the NFS mix map.
//
//  LAYOUT IS ARTIST-DERIVED (NOT ProStreet): the ProStreet PDB SnapshotMixer [sizeof=28]
//  DIVERGES from ARTIST -- the ARTIST object has an extra owner pointer at +0x00 that
//  shifts every field. The layout below is cross-checked across the ARTIST bodies:
//    Construct @0x82B46CE8, GetChannel @0x82B47170 (idx < +0x14), GetSnapshot @0x82B46EE0
//    (idx < +0x18), GetSnapshotChannel @0x82B475B0 (+0x04 hdr, +0x14 count), AttachToMixMap
//    @0x82B47648 (+0x08 state==2, +0x0C channels stride 0x18, +0x14 count, +0x18 snaps).
//  Names align to the PDB where the field role matches. x64 widths; +0xNN = X360 offsets.
//  (Non-polymorphic: Construct installs no vtable; +0x00 is the owner, not a vptr.)
// ===========================================================================

#include "types.hpp"
#include "SDKs/EATech/include/NFSMix/MixerMemBase.hpp"
#include "SDKs/EATech/include/Nicotine/SnapshotChannel.hpp" // SnapshotChannel (stride 0x18)

class NFSMixMap;

namespace Nicotine
{

// ---------------------------------------------------------------------------
//  SnapshotStatus -- one entry of the mixer's runtime status array (mpSnapshots,
//  stride 8). Each element tracks whether its snapshot is active and, when a
//  timed snapshot is applied, how long it has left to hold. Derived from the
//  status loops in ActiveStatesChanged / Update / SetSnapshotOverride.
//      +0x00  mfTimeRemaining  hold timer; -1.0 == off, >0 counts down each Update
//      +0x04  mFlags           bit0 active(requested) / bit1 active-mirror / bit2 override-save
// ---------------------------------------------------------------------------
struct SnapshotStatus
{
    f32 mfTimeRemaining;  // [0x00]
    u8  mFlags;           // [0x04]
    u8  maPad05[3];       // [0x05] pad to the 8-byte stride
};

// A single snapshot's per-channel record inside the loaded header blob (stride 8):
// the destination volume (packed: hi16 = priority marker, lo16 = target millibels)
// and the ramp duration in seconds. GetSnapshot returns the base of one snapshot's
// miNumChannels-long array of these.
struct SnapshotChannelData
{
    u32 mTarget;     // [0x00] hi16 priority / lo16 target millibel volume
    f32 mDuration;   // [0x04] ramp duration (seconds)
};

// The per-channel descriptor table at header +0x10 (miNumChannels 12-byte entries).
struct SnapshotChannelDesc
{
    u32 mMixChannelId;  // [0x00] packed into the MIXCHINID handed to NFSMixMap
    u32 mChannelId;     // [0x04] logical channel id (GetSnapshotChannel / ...Id lookup)
    u32 mField08;       // [0x08] (unused by this TU)
};

// The loaded snapshot-mix header blob (serialised MixMap data): a fixed head, then
// the per-channel descriptor table, then the variable-stride snapshot data walked by
// GetSnapshot. Only the head + descriptor table are fixed-offset; the snapshot data
// stride is runtime (8*miNumChannels), so it is reached by documented blob pointer math.
class SnapshotHeader
{
public:
    u8  maHead[8];                     // +0x00 (owned by the loader / DMixIO)
    u32 meType;                        // +0x08  1 = active-state data, 2 = recall data
    u32 mField0C;                      // +0x0C
    SnapshotChannelDesc maChannels[1]; // +0x10  (miNumChannels entries; flexible)
};

class SnapshotMixer : public MixerMemBase
{
public:
    void Construct(void* lpOwner);                    // @0x82B46CE8
    SnapshotChannel*     GetChannel(int liIndex);      // @0x82B47170 (bounds vs miNumChannels)
    SnapshotChannelData* GetSnapshot(int liIndex);     // @0x82B46EE0 (bounds vs miNumSnapshots)
    // @0x82B475B0 -- &(snapshot liSnapshotIdx's data) for the channel whose descriptor
    // mChannelId == liChannelId, or null.
    SnapshotChannelData* GetSnapshotChannel(int liSnapshotIdx, int liChannelId);
    int  GetSnapshotChannelId(int liChannelIdx);       // @0x82B46F60 (descriptor mChannelId)
    void AttachToMixMap(NFSMixMap* lpMixMap);          // @0x82B47648 (bind each channel's mix proc)
    void Update(float lfDeltaTime);                    // @0x82B47C68 (per-frame ramp -> mix procs)

    bool ActiveStatesChanged();                        // @0x82B47550 (any snapshot toggled?)
    bool IsStatusActive(int liSnapshotIdx);            // @0x82B46FE0
    void UpdateMixerTargets();                         // @0x82B479A8 (pick + apply winning snapshot)
    void UpdateNicotineChannelData(int liMixChannelId);// @0x82B47760 (refresh cached proc volume)
    void UpdateSnapshotChannelData(int liSnapshotIdx, const void* lpSrc, int liChannelId); // @0x82B47708
    void Recall(int liSnapshotIdx);                    // @0x82B477C8 (apply recall-type snapshot)
    void SetSnapshot(int liSnapshotIdx, bool lbActive);// @0x82B46E30 (set active-state bit)
    void SetSnapshotOverride(bool lbOverride);         // @0x82B47020 (freeze/restore all snapshots)
    void TurnOnSnapshot(int liSnapshotIdx);            // @0x82B47120
    void TurnOffAllSnapshots();                        // @0x82B46DE0

    // ---- driven by Nicotine::IDynamicMixer (declared here; bodied by other TUs) ----
    void InitSnapshots();                            // (re)build the snapshot/channel arrays
    void DestroySnapshots();                         // tear down the snapshot/channel arrays
    void SetSnapshot();                              // apply the currently-selected snapshot

    void*            mpOwner;        // +0x00  owner (has the assert vtable; Construct arg)
    SnapshotHeader*  mpSnapshotHdr;  // +0x04
    int              meState;        // +0x08  1 = constructed, 2 = attached
    SnapshotChannel* mpChannels;     // +0x0c  per-channel ramps (stride 0x18)
    SnapshotStatus*  mpSnapshots;    // +0x10  per-snapshot status (stride 0x08)
    int              miNumChannels;  // +0x14
    int              miNumSnapshots; // +0x18
    u8               mbForceUpdate;  // +0x1c  targets dirty -> recompute next Update
    u8               mbOverride;     // +0x1d  snapshot-override active
};

} // namespace Nicotine

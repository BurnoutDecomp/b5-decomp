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
class SnapshotHeader;
class SnapshotStatus;

class SnapshotMixer : public MixerMemBase
{
public:
    void Construct(void* lpOwner);                 // @0x82B46CE8
    SnapshotChannel* GetChannel(int liIndex);       // @0x82B47170 (bounds vs miNumChannels)
    SnapshotStatus*  GetSnapshot(int liIndex);      // @0x82B46EE0 (bounds vs miNumSnapshots)
    int  GetSnapshotChannel(int liChannelId);       // @0x82B475B0 (-> index, or -1)
    void AttachToMixMap(NFSMixMap* lpMixMap);        // @0x82B47648 (FLAG: cascade -- below)
    void Update(float lfDeltaTime);                  // @0x82B47C68 (FLAG: rodata-blocked DSP)

    // ---- driven by Nicotine::IDynamicMixer (declared here; bodied by the SnapshotMixer TU) ----
    void InitSnapshots();                            // (re)build the snapshot/channel arrays
    void DestroySnapshots();                         // tear down the snapshot/channel arrays
    void SetSnapshot();                              // apply the currently-selected snapshot

    void*            mpOwner;        // +0x00  owner (has the assert vtable; Construct arg)
    SnapshotHeader*  mpSnapshotHdr;  // +0x04
    int              meState;        // +0x08  1 = constructed, 2 = attached
    SnapshotChannel* mpChannels;     // +0x0c  per-channel ramps (stride 0x18)
    SnapshotStatus*  mpSnapshots;    // +0x10
    int              miNumChannels;  // +0x14
    int              miNumSnapshots; // +0x18
    u8               mbForceUpdate;  // +0x1c
    u8               mbByte1D;       // +0x1d
};

} // namespace Nicotine

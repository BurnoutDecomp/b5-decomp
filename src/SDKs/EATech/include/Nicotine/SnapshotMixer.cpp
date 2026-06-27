#include "SDKs/EATech/include/Nicotine/SnapshotMixer.hpp"

// ===========================================================================
//  Nicotine::SnapshotMixer -- the clean, ARTIST-verified surface. The per-frame DSP
//  (Update) is rodata-blocked and AttachToMixMap cascades into NFSMixMap::
//  GetMasterMixChProc + a MIXCHINID bit-pack; both are FLAGGED below.
// ===========================================================================

namespace Nicotine
{

// off_83250014 -- the global SnapshotMixer singleton (sibling of off_83250008
// NFSMixMaster / off_83250004 mixer allocator). Construct installs `this` here.
SnapshotMixer* g_pSnapshotMixer = 0;

// ---------------------------------------------------------------------------
// Construct @0x82B46CE8
//   mpOwner = lpOwner; (+0x00)   meState = 1; (+0x08)
//   mpSnapshotHdr/mpChannels/mpSnapshots/miNumChannels/miNumSnapshots = 0;
//   mbForceUpdate = 0; mbByte1D = 0;   off_83250014 = this;
// ---------------------------------------------------------------------------
void SnapshotMixer::Construct(void* lpOwner)
{
    mpOwner        = lpOwner;   // +0x00
    meState        = 1;         // +0x08
    mpSnapshotHdr  = 0;         // +0x04
    mpChannels     = 0;         // +0x0C
    mpSnapshots    = 0;         // +0x10
    miNumChannels  = 0;         // +0x14
    miNumSnapshots = 0;         // +0x18
    mbForceUpdate  = 0;         // +0x1C
    mbByte1D       = 0;         // +0x1D
    g_pSnapshotMixer = this;    // off_83250014
}

// ---------------------------------------------------------------------------
// GetChannel @0x82B47170 -- the per-channel ramp at liIndex.
// FLAG: the X360 routes the range check through mpOwner's assert vtable (slot 4,
// "SnapshotMixer : Channel out of range"); the PC reproduces it as a bounds guard.
// ---------------------------------------------------------------------------
SnapshotChannel* SnapshotMixer::GetChannel(int liIndex)
{
    if (liIndex >= miNumChannels)   // +0x14 (owner-assert in the X360)
        return 0;
    return &mpChannels[liIndex];    // +0x0C, x64 array index (= X360 stride 0x18)
}

// ---------------------------------------------------------------------------
// GetSnapshot @0x82B46EE0 -- the snapshot/status record at liIndex (bounds vs +0x18).
// FLAG: the bounds check (owner-assert "Snapshot out of range") is reproduced as a
// guard; the &mpSnapshots[liIndex] index is deferred until SnapshotStatus is homed
// (its sizeof = the array stride), so this returns null for an out-of-range index and
// otherwise FLAG-returns null pending that type.
// ---------------------------------------------------------------------------
SnapshotStatus* SnapshotMixer::GetSnapshot(int liIndex)
{
    if (liIndex >= miNumSnapshots)  // +0x18 (bounds guard)
        return 0;
    return 0;                       // FLAG: &mpSnapshots[liIndex] pending SnapshotStatus home
}

// ---------------------------------------------------------------------------
// GetSnapshotChannel @0x82B475B0 -- FLAG (deferred): searches the snapshot header's
// channel-id table (mpSnapshotHdr+0x10) for liChannelId and returns its index (-1 if
// absent). The exact table stride needs a careful re-read of SnapshotHeader; bodied
// alongside SnapshotHeader's home.
// ---------------------------------------------------------------------------
int SnapshotMixer::GetSnapshotChannel(int /*liChannelId*/)
{
    return -1;
}

// ---------------------------------------------------------------------------
// AttachToMixMap @0x82B47648 -- FLAG (deferred -- cascades): for each channel it packs
// a MIXCHINID from the header's per-channel descriptor table, calls NFSMixMap::
// GetMasterMixChProc(@0x82B4A840, a 38-line search) and stores the proc into the
// SnapshotChannel (+0x08) plus the proc's channel count (+0x02). Needs NFSMixMap::
// GetMasterMixChProc bodied + the SnapshotChannel +0x02/+0x08 fields homed. The
// CONTROL FLOW is known (state==2 && map && miNumSnapshots && miNumChannels guards,
// loop miNumChannels over the header table); wired in with GetMasterMixChProc.
// ---------------------------------------------------------------------------
void SnapshotMixer::AttachToMixMap(NFSMixMap* /*lpMixMap*/)
{
}

// ---------------------------------------------------------------------------
// Update @0x82B47C68 -- FLAG (deferred -- RODATA-BLOCKED): the per-frame snapshot ramp.
// Guarded on meState==2; calls ActiveStatesChanged/UpdateMixerTargets then ramps each
// channel's volume using NFSMixShape conversion consts (flt_82001CC0 / flt_82147C24),
// which are not in the X360 export. Wired in once the NFSMixShape rodata is recovered.
// ---------------------------------------------------------------------------
void SnapshotMixer::Update(float /*lfDeltaTime*/)
{
}

} // namespace Nicotine

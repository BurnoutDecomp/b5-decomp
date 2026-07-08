#include "SDKs/EATech/include/Nicotine/SnapshotMixer.hpp"

#include "SDKs/EATech/include/NFSMix/NFSMixMap.hpp"      // NFSMixMap::GetMasterMixChProc
#include "SDKs/EATech/include/NFSMix/NFSMixRecords.hpp"  // stMasterMixChProc / ...SharedData / ...Params
#include "GameShared/GameClasses/Core/CgsAssert.h"        // CGS_ASSERT
#include "rw/core/stdc/stdc.h"                            // rw::core::stdc::MemCopy

// ===========================================================================
//  Nicotine::SnapshotMixer -- store-for-store reconstruction from
//  BURNOUT_X360_ARTIST.XEX. The snapshot mixer holds the loaded snapshot header
//  (mpSnapshotHdr), the per-channel ramp array (mpChannels), and the per-snapshot
//  status array (mpSnapshots). Each frame Update() ramps the winning snapshot's
//  channel volumes into the bound NFS master-mix channel procs.
//
//  The X360 routes each range check through mpOwner's assert vtable (slot 4); as
//  elsewhere in the NFS mix system (NFSMixMap::GetMasterMixChProc) that host-assert
//  collapses to CGS_ASSERT with the verbatim rodata message. The two float consts
//  the DSP loops use are flt_82147C24 == -1.0f (the "channel off" ramp value) and
//  flt_82001CC0 == 0.0f (the ramp-done threshold).
// ===========================================================================

namespace Nicotine
{

// off_83250014 -- the global SnapshotMixer singleton (sibling of off_83250008
// NFSMixMaster / off_83250004 mixer allocator). Construct installs `this` here.
SnapshotMixer* g_pSnapshotMixer = 0;

// ---------------------------------------------------------------------------
// Construct @0x82B46CE8
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
    mbOverride     = 0;         // +0x1D
    g_pSnapshotMixer = this;    // off_83250014
}

// ---------------------------------------------------------------------------
// GetChannel @0x82B47170 -- the per-channel ramp at liIndex.
// ---------------------------------------------------------------------------
SnapshotChannel* SnapshotMixer::GetChannel(int liIndex)
{
    CGS_ASSERT(liIndex < miNumChannels, "SnapshotMixer : Channel out of range\n");
    if (miNumChannels <= 0)
        return 0;
    return &mpChannels[liIndex];
}

// ---------------------------------------------------------------------------
// GetSnapshot @0x82B46EE0 -- base of snapshot liIndex's per-channel data inside the
// loaded header blob. Serialised-blob access: the snapshot data begins right after
// the miNumChannels-entry descriptor table (12 bytes each) and each snapshot spans
// miNumChannels SnapshotChannelData records (8 bytes each), so the stride is runtime.
// ---------------------------------------------------------------------------
SnapshotChannelData* SnapshotMixer::GetSnapshot(int liIndex)
{
    CGS_ASSERT(liIndex < miNumSnapshots, "SnapshotMixer : Snapshot out of range\n");

    u8* lpData = reinterpret_cast<u8*>(mpSnapshotHdr) + 16;      // +0x10 (past the head)
    lpData += (12 + 8 * liIndex) * miNumChannels;               // descriptor table + idx*stride
    return reinterpret_cast<SnapshotChannelData*>(lpData);
}

// ---------------------------------------------------------------------------
// GetSnapshotChannelId @0x82B46F60 -- the logical channel id of descriptor slot liChannelIdx.
// ---------------------------------------------------------------------------
int SnapshotMixer::GetSnapshotChannelId(int liChannelIdx)
{
    CGS_ASSERT(liChannelIdx < miNumChannels, "SnapshotMixer : Channel index out of range\n");
    return static_cast<int>(mpSnapshotHdr->maChannels[liChannelIdx].mChannelId);
}

// ---------------------------------------------------------------------------
// GetSnapshotChannel @0x82B475B0 -- find the descriptor whose mChannelId == liChannelId
// and return that channel's data within snapshot liSnapshotIdx (or null if absent).
// ---------------------------------------------------------------------------
SnapshotChannelData* SnapshotMixer::GetSnapshotChannel(int liSnapshotIdx, int liChannelId)
{
    // ARTIST guards on (&mpSnapshotHdr->maChannels != 0); a null header never reaches here.
    if (mpSnapshotHdr == 0)
        return 0;

    int liFound = -1;
    for (int i = 0; i < miNumChannels && liFound < 0; ++i)
    {
        if (mpSnapshotHdr->maChannels[i].mChannelId == static_cast<u32>(liChannelId))
            liFound = i;
    }

    if (liFound < 0)
        return 0;
    return GetSnapshot(liSnapshotIdx) + liFound;
}

// ---------------------------------------------------------------------------
// IsStatusActive @0x82B46FE0 -- is snapshot liSnapshotIdx currently flagged active?
// ---------------------------------------------------------------------------
bool SnapshotMixer::IsStatusActive(int liSnapshotIdx)
{
    if (meState != 2)
        return false;
    if (miNumSnapshots <= 0)
        return false;
    return (mpSnapshots[liSnapshotIdx].mFlags & 1) != 0;
}

// ---------------------------------------------------------------------------
// ActiveStatesChanged @0x82B47550 -- true if any snapshot's requested-active bit (bit0)
// differs from its applied-active mirror (bit1), i.e. a toggle is pending.
// ---------------------------------------------------------------------------
bool SnapshotMixer::ActiveStatesChanged()
{
    bool lbChanged = false;
    for (int i = 0; i < miNumSnapshots && !lbChanged; ++i)
    {
        const u8 lFlags = mpSnapshots[i].mFlags;
        if (((lFlags >> 1) ^ lFlags) & 1)
            lbChanged = true;
    }
    return lbChanged;
}

// ---------------------------------------------------------------------------
// TurnOffAllSnapshots @0x82B46DE0 -- clear the active bit and reset the timer on every snapshot.
// ---------------------------------------------------------------------------
void SnapshotMixer::TurnOffAllSnapshots()
{
    for (int i = 0; i < miNumSnapshots; ++i)
    {
        SnapshotStatus* lpSnap = &mpSnapshots[i];
        lpSnap->mfTimeRemaining = -1.0f;
        lpSnap->mFlags &= 0xFE;
    }
}

// ---------------------------------------------------------------------------
// TurnOnSnapshot @0x82B47120 -- set snapshot liSnapshotIdx active (active-state data only).
// ---------------------------------------------------------------------------
void SnapshotMixer::TurnOnSnapshot(int liSnapshotIdx)
{
    if (mpSnapshotHdr->meType != 1)     // active-state type
        return;
    if (meState != 2)
        return;
    if (liSnapshotIdx >= miNumSnapshots)
        return;

    SnapshotStatus* lpSnap = &mpSnapshots[liSnapshotIdx];
    lpSnap->mfTimeRemaining = -1.0f;
    lpSnap->mFlags |= 1;
}

// ---------------------------------------------------------------------------
// SetSnapshot @0x82B46E30 -- set/clear the active bit of snapshot liSnapshotIdx (unless
// an override is in force). Asserts the header carries active-state data.
// ---------------------------------------------------------------------------
void SnapshotMixer::SetSnapshot(int liSnapshotIdx, bool lbActive)
{
    CGS_ASSERT(mpSnapshotHdr->meType == 1,
               "SnapshotMixer::SetSnapshot : Snapshot data is not active state type.\n");

    if (!mbOverride && meState == 2 && liSnapshotIdx < miNumSnapshots)
    {
        SnapshotStatus* lpSnap = &mpSnapshots[liSnapshotIdx];
        if (lbActive)
            lpSnap->mFlags |= 1;
        else
            lpSnap->mFlags &= 0xFE;
        lpSnap->mfTimeRemaining = -1.0f;
    }
}

// ---------------------------------------------------------------------------
// SetSnapshotOverride @0x82B47020 -- freeze (lbOverride) or restore the snapshot set.
//   Entering: park every snapshot and stash its active bit into the override-save bit2.
//   Leaving : restore each snapshot's active bit from bit2.
// ---------------------------------------------------------------------------
void SnapshotMixer::SetSnapshotOverride(bool lbOverride)
{
    if (mbOverride == static_cast<u8>(lbOverride))
        return;

    if (lbOverride)
    {
        for (int i = 0; i < miNumSnapshots; ++i)
        {
            SnapshotStatus* lpSnap = &mpSnapshots[i];
            if (lpSnap->mfTimeRemaining > 0.0f)
            {
                lpSnap->mfTimeRemaining = -1.0f;
                lpSnap->mFlags &= 0xFE;
            }
            if (lpSnap->mFlags & 1)
                lpSnap->mFlags |= 4;
            else
                lpSnap->mFlags &= 0xFB;
        }
    }
    else
    {
        for (int i = 0; i < miNumSnapshots; ++i)
        {
            SnapshotStatus* lpSnap = &mpSnapshots[i];
            if (lpSnap->mFlags & 4)
                lpSnap->mFlags |= 1;
            else
                lpSnap->mFlags &= 0xFE;
            lpSnap->mfTimeRemaining = -1.0f;
        }
    }

    mbOverride = static_cast<u8>(lbOverride);
}

// ---------------------------------------------------------------------------
// Recall @0x82B477C8 -- snap every channel straight to snapshot liSnapshotIdx's stored
// volume (recall-type data). Only channels whose stored target has a non-zero hi16 are
// touched; each is re-based at its current volume with a zero ramp.
// ---------------------------------------------------------------------------
void SnapshotMixer::Recall(int liSnapshotIdx)
{
    CGS_ASSERT(mpSnapshotHdr->meType == 2,
               "SnapshotMixer::Recall : Snapshot data is not recall type.\n");

    if (meState != 2 || liSnapshotIdx >= miNumSnapshots)
        return;

    SnapshotChannelData* lpData = GetSnapshot(liSnapshotIdx);
    for (int i = 0; i < miNumChannels; ++i)
    {
        if (lpData[i].mTarget & 0xFFFF0000)
        {
            const f32 lfDuration = lpData[i].mDuration;
            const u32 lTarget    = lpData[i].mTarget;
            SnapshotChannel* lpChan = &mpChannels[i];
            const int liCurrent = lpChan->GetCurrentVolume();
            lpChan->mfElapsed        = 0.0f;
            lpChan->mfDuration       = lfDuration;
            lpChan->mi16BaseVolume   = static_cast<s16>(liCurrent);
            lpChan->mi16TargetVolume = static_cast<s16>(lTarget);
        }
    }
}

// ---------------------------------------------------------------------------
// UpdateSnapshotChannelData @0x82B47708 -- overwrite one channel's stored data (8 bytes)
// inside snapshot liSnapshotIdx and mark the mixer targets dirty.
// ---------------------------------------------------------------------------
void SnapshotMixer::UpdateSnapshotChannelData(int liSnapshotIdx, const void* lpSrc, int liChannelId)
{
    SnapshotChannelData* lpDest = GetSnapshotChannel(liSnapshotIdx, liChannelId);
    if (lpDest)
        rw::core::stdc::MemCopy(lpDest, lpSrc, 8);
    mbForceUpdate = 1;
}

// ---------------------------------------------------------------------------
// UpdateNicotineChannelData @0x82B47760 -- refresh the cached proc volume for every
// channel bound to master-mix id liMixChannelId (called when that mix channel's live
// MixData changes upstream).
// ---------------------------------------------------------------------------
void SnapshotMixer::UpdateNicotineChannelData(int liMixChannelId)
{
    for (int i = 0; i < miNumChannels; ++i)
    {
        if (mpSnapshotHdr->maChannels[i].mMixChannelId == static_cast<u32>(liMixChannelId))
        {
            SnapshotChannel* lpChan = &mpChannels[i];
            stMasterMixChProc* lpProc = lpChan->mpMixChProc;
            if (lpProc)
                lpChan->mi16ProcVolume =
                    static_cast<s16>(lpProc->pMixChData_S->pMapParams->MixData >> 16);
        }
    }
}

// ---------------------------------------------------------------------------
// AttachToMixMap @0x82B47648 -- bind each channel to its NFS master-mix proc: pack the
// descriptor's mMixChannelId into a MIXCHINID, resolve the proc through the map, cache
// it on the channel and seed the channel's proc volume.
// ---------------------------------------------------------------------------
void SnapshotMixer::AttachToMixMap(NFSMixMap* lpMixMap)
{
    if (meState != 2 || lpMixMap == 0 || miNumSnapshots == 0 || miNumChannels <= 0)
        return;

    for (int i = 0; i < miNumChannels; ++i)
    {
        const u32 luId = mpSnapshotHdr->maChannels[i].mMixChannelId;
        if (luId == 0)
            continue;

        const int liPacked = ((luId << 8) & 0xFF0000) | 0x20000000 | (luId & 0x100000FF);
        stMasterMixChProc* lpProc = lpMixMap->GetMasterMixChProc(liPacked);

        SnapshotChannel* lpChan = &mpChannels[i];
        lpChan->mpMixChProc = lpProc;
        if (lpProc)
            lpChan->mi16ProcVolume =
                static_cast<s16>(lpProc->pMixChData_S->pMapParams->MixData >> 16);
    }
}

// ---------------------------------------------------------------------------
// UpdateMixerTargets @0x82B479A8 -- rebuild each channel's ramp target from the winning
// active snapshot. Among the active snapshots, a channel prefers "priority" snapshots
// (stored target hi16 > 0), taking the loudest of those; with none it takes the quietest
// non-priority target. The winner re-bases the channel at its current volume.
// ---------------------------------------------------------------------------
void SnapshotMixer::UpdateMixerTargets()
{
    int laActive[66];
    int liNumActive = 0;
    for (int s = 0; s < miNumSnapshots; ++s)
    {
        if (mpSnapshots[s].mFlags & 1)
            laActive[liNumActive++] = s;
    }
    if (liNumActive == 0)
        return;

    for (int ch = 0; ch < miNumChannels; ++ch)
    {
        int  liBest  = 0;
        bool lbFound = false;   // seen a priority (hi16 > 0) snapshot yet?

        for (int i = 0; i < liNumActive; ++i)
        {
            const s16 liHi = static_cast<s16>(GetSnapshot(laActive[i])[ch].mTarget >> 16);
            const s16 liLo = static_cast<s16>(GetSnapshot(laActive[i])[ch].mTarget);

            if (!lbFound)
            {
                if (liHi > 0)
                {
                    lbFound = true;
                    liBest  = i;
                }
                else if (liLo < static_cast<s16>(GetSnapshot(laActive[liBest])[ch].mTarget))
                {
                    liBest = i;
                }
            }
            else if (liHi > 0)
            {
                if (liLo > static_cast<s16>(GetSnapshot(laActive[liBest])[ch].mTarget))
                    liBest = i;
            }
        }

        SnapshotChannelData* lpWin = GetSnapshot(laActive[liBest]);
        const u32 lTarget    = lpWin[ch].mTarget;
        const f32 lfDuration = lpWin[ch].mDuration;
        SnapshotChannel* lpChan = &mpChannels[ch];
        const int liCurrent = lpChan->GetCurrentVolume();
        lpChan->mfElapsed        = 0.0f;
        lpChan->mfDuration       = lfDuration;
        lpChan->mi16BaseVolume   = static_cast<s16>(liCurrent);
        lpChan->mi16TargetVolume = static_cast<s16>(lTarget);
    }
}

// ---------------------------------------------------------------------------
// Update @0x82B47C68 -- per-frame snapshot ramp (state==2 only):
//   1. recompute mixer targets when dirty or an active state toggled;
//   2. tick each snapshot's hold timer + mirror its active bit;
//   3. advance every channel's ramp clock;
//   4. resolve each channel's volume (+ optional ceiling) and push it, packed, into
//      the bound master-mix proc's MixData.
// ---------------------------------------------------------------------------
void SnapshotMixer::Update(float lfDeltaTime)
{
    if (meState != 2)
        return;

    if (mbForceUpdate || ActiveStatesChanged())
        UpdateMixerTargets();
    mbForceUpdate = 0;

    // (2) snapshot hold timers + active-bit mirror.
    for (int i = 0; i < miNumSnapshots; ++i)
    {
        SnapshotStatus* lpSnap = &mpSnapshots[i];
        u8 lFlags = lpSnap->mFlags;
        lFlags = (lFlags & 1) ? (lFlags | 2) : (lFlags & 0xFD);
        const f32 lfTimer = lpSnap->mfTimeRemaining;
        lpSnap->mFlags = lFlags;
        if (lfTimer > 0.0f)
        {
            const f32 lfNext = lfTimer - lfDeltaTime;
            lpSnap->mfTimeRemaining = lfNext;
            if (lfNext <= 0.0f)
            {
                lpSnap->mfTimeRemaining = -1.0f;
                lpSnap->mFlags = lFlags & 0xFE;
            }
        }
    }

    // (3) advance each channel's ramp clock.
    for (int i = 0; i < miNumChannels; ++i)
    {
        SnapshotChannel* lpChan = &mpChannels[i];
        if (lpChan->mfElapsed <= lpChan->mfDuration)
            lpChan->mfElapsed += lfDeltaTime;
    }

    // (4) push each channel's resolved volume into its master-mix proc.
    for (int i = 0; i < miNumChannels; ++i)
    {
        SnapshotChannel* lpChan = &mpChannels[i];
        stMasterMixChProc* lpProc = lpChan->mpMixChProc;
        if (!lpProc)
            continue;

        int liVolume;
        if (lpChan->mpCeiling)
        {
            const int liCeiling = lpChan->mpCeiling->GetFinalVolume();
            int liSum = static_cast<s16>(lpChan->GetCurrentVolume() + liCeiling);
            if (liSum <= -10000)
                liSum = -10000;
            liVolume = static_cast<s16>(liSum);
        }
        else
        {
            liVolume = lpChan->GetCurrentVolume();
        }

        lpProc->pMixChData_S->pMapParams->MixData =
            (lpChan->mi16ProcVolume + static_cast<s16>(liVolume)) << 16;
    }
}

} // namespace Nicotine

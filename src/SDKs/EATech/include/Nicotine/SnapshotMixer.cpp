#include "SDKs/EATech/include/Nicotine/SnapshotMixer.hpp"

#include "SDKs/EATech/include/NFSMix/NFSMixMap.hpp"      // NFSMixMap::GetMasterMixChProc
#include "SDKs/EATech/include/NFSMix/MixerAllocator.hpp" // g_pMixerAllocator (relocated InitSnapshots/DestroySnapshots)
#include <cstring>                                        // std::memset (relocated InitSnapshots)
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
    // FLAG (capacity unanchored, 2026-08-25 wave 1): the 66-slot stack array has no
    // recovered X360 anchor (the console body's fixed stack frame implies SOME cap;
    // 66 was carried over unannotated). Guarded against overrun until the real
    // capacity is read from the asm frame -- a snapshot blob with more than 66
    // active snapshots would otherwise smash the stack.
    const int KI_MAX_ACTIVE_SNAPSHOTS = 66;
    int laActive[KI_MAX_ACTIVE_SNAPSHOTS];
    int liNumActive = 0;
    for (int s = 0; s < miNumSnapshots && liNumActive < KI_MAX_ACTIVE_SNAPSHOTS; ++s)
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

// ============================================================================
// RELOCATED HOME (2026-08-25, audio-faithfulness wave 2): these bodies were
// reconstructed 2026-08-07 as a targeted export into SDKs/EATech/AptRenderLinkStubs.cpp
// (an artifact of that export's file set) -- moved here to their natural TU to
// retire the duplicate-definition hazard the stub-file placement guaranteed.
// ============================================================================

namespace Nicotine {
    // Nicotine::SnapshotMixer::InitSnapshots @0x82B47350 (targeted export 2026-08-07)
    // -- (re)build the per-channel ramp array + per-snapshot status array from the
    // loaded snapshot header. Store-for-store: teardown-if-built, bind the header +
    // counts, allocate/zero each array through the mixer allocator (off_83250004
    // vtbl+8, debug-named), then per-entry init (channels: all-zero + the ceiling
    // link; statuses: timer -1.0 == off, flags 0), and state -> 2 (built).
    //
    // FLAG (arg plumbing parked): the console entry receives the snapshot header
    // blob in r4 (stw r30,4(r31) == mpSnapshotHdr = r4), forwarded through
    // IDynamicMixer::InitSnapshots @0x82B44D68; the in-tree declaration chain is
    // no-arg (SnapshotMixer.hpp / IDynamicMixer.hpp -- out of this TU's file set),
    // so this body sources the header from mpSnapshotHdr, null-guarded, until the
    // argument is threaded through those headers.
    void SnapshotMixer::InitSnapshots()
    {
        SnapshotHeader* const lpHdr = mpSnapshotHdr;   // console: r4 (see the FLAG above)

        if (meState == 2)                    // +0x08: already built -> tear down first
            DestroySnapshots();

        if (lpHdr == 0)
            return;                          // FLAG: guard for the un-threaded arg (the console derefs r4)
        mpSnapshotHdr = lpHdr;               // stw 4(r31) -- re-stored after the teardown

        // The serialized head words: [0] = snapshot count, [1] = channel count (blob +0x00/+0x04).
        const s32* const lpCounts = reinterpret_cast<const s32*>(lpHdr->maHead);   // serialized blob head
        miNumChannels  = lpCounts[1];        // +0x14 (serialized blob word 1)
        miNumSnapshots = lpCounts[0];        // +0x18 (serialized blob word 0)

        if (miNumChannels > 0)
        {
            // sizeof stride (console 24 == X360 sizeof SnapshotChannel; x64 widens),
            // clamped to -1 past the console 0xAAAAAAA overflow guard.
            const u32 luSize = (miNumChannels > 0xAAAAAAA)
                ? 0xFFFFFFFFu
                : static_cast<u32>(sizeof(SnapshotChannel)) * static_cast<u32>(miNumChannels);
            SnapshotChannel* const lpChannels = static_cast<SnapshotChannel*>(
                g_pMixerAllocator->Allocate(luSize, 16, "SnapshotChannels"));   // off_83250004 vtbl+8
            if (lpChannels)
                std::memset(lpChannels, 0, luSize);
            mpChannels = lpChannels;         // +0x0C (null on alloc failure, as shipped)

            for (s32 li = 0; li < miNumChannels; ++li)
            {
                // Descriptor word 2 (serialized blob +0x18 + 12*li) = the CEILING channel index, -1 = none.
                const s32 liCeiling = static_cast<s32>(lpHdr->maChannels[li].mField08);
                SnapshotChannel& lrChannel = mpChannels[li];
                lrChannel.mfElapsed        = 0.0f;   // +0x10 (stfs flt_82001CC0)
                lrChannel.mi16TargetVolume = 0;      // +0x00 (sth)
                lrChannel.mfDuration       = 0.0f;   // +0x14
                lrChannel.mi16BaseVolume   = 0;      // +0x04 (sth)
                lrChannel.mi16ProcVolume   = 0;      // +0x02 (sth)
                lrChannel.mpMixChProc      = 0;      // +0x08
                lrChannel.mpCeiling        = (liCeiling == -1) ? 0 : &mpChannels[liCeiling];   // +0x0C
            }
        }

        if (miNumSnapshots > 0)
        {
            const u32 luSize = (miNumSnapshots > 0x1FFFFFFF)
                ? 0xFFFFFFFFu
                : static_cast<u32>(sizeof(SnapshotStatus)) * static_cast<u32>(miNumSnapshots);
            SnapshotStatus* const lpStatuses = static_cast<SnapshotStatus*>(
                g_pMixerAllocator->Allocate(luSize, 16, "SnapshotStatuses"));   // off_83250004 vtbl+8
            if (lpStatuses)
                std::memset(lpStatuses, 0, luSize);
            mpSnapshots = lpStatuses;        // +0x10

            for (s32 li = 0; li < miNumSnapshots; ++li)
            {
                mpSnapshots[li].mfTimeRemaining = -1.0f;   // stfs flt_82147C24 (hold timer off)
                mpSnapshots[li].mFlags          = 0;       // stb +4
            }
        }

        meState = 2;                         // +0x08: built
    }

    // Nicotine::SnapshotMixer::DestroySnapshots @0x82B46D20 (targeted export
    // 2026-08-07) -- tear the snapshot/channel arrays back down. Built (state 2):
    // free each non-empty array through the mixer allocator (off_83250004 vtbl+0xC,
    // flag 0), zero the array/count/header fields, state -> 1. Not built: just
    // state -> 1.
    void SnapshotMixer::DestroySnapshots()
    {
        if (meState == 2)                    // +0x08
        {
            if (miNumChannels > 0)           // +0x14
                g_pMixerAllocator->Free(mpChannels, 0);    // vtbl+0xC
            if (miNumSnapshots > 0)          // +0x18
                g_pMixerAllocator->Free(mpSnapshots, 0);   // vtbl+0xC
            mpChannels     = 0;              // +0x0C
            mpSnapshots    = 0;              // +0x10
            miNumChannels  = 0;              // +0x14
            miNumSnapshots = 0;              // +0x18
            mpSnapshotHdr  = 0;              // +0x04
            meState        = 1;              // +0x08
        }
        else
        {
            meState = 1;                     // +0x08
        }
    }

    // SetSnapshot (no-arg) is still un-addressed in the export set.
    void SnapshotMixer::SetSnapshot()      {}   // FLAG link-stub (X360 body un-exported)

    // The 512-entry snapshot volume-curve LUT (X360 rodata unk_82F86F88; dumped
    // bit-exact 2026-08-07, _data_volume_curve_lut: 2048 bytes of big-endian f32).
    // A monotonic 0.0 -> 1.0 ease table sampled at trunc(ratio * 511).
    static const f32 KF_SnapshotVolumeCurve[512] =
    {
        0.0f, 0.00307000009f, 0.00614000019f, 0.0092000002f, 0.0122699998f, 0.0153400004f, 0.0184099991f, 0.0214699991f,
        0.0245399997f, 0.0276100002f, 0.0306700002f, 0.0337399989f, 0.0368099995f, 0.0398700014f, 0.0429399982f, 0.0460000001f,
        0.0490700006f, 0.0521299988f, 0.0551999994f, 0.0582600012f, 0.0613199994f, 0.0643799976f, 0.0674400032f, 0.0705000013f,
        0.0735599995f, 0.0766199976f, 0.0796800032f, 0.0827400014f, 0.0857999995f, 0.088849999f, 0.0919099972f, 0.0949599966f,
        0.0980200022f, 0.101070002f, 0.104120001f, 0.107170001f, 0.11022f, 0.11327f, 0.116319999f, 0.119369999f,
        0.122409999f, 0.12545f, 0.1285f, 0.13154f, 0.134580001f, 0.137620002f, 0.140660003f, 0.143690005f,
        0.146730006f, 0.149759993f, 0.152799994f, 0.155829996f, 0.158859998f, 0.16189f, 0.164910004f, 0.167940006f,
        0.170959994f, 0.173979998f, 0.177000001f, 0.180020005f, 0.183039993f, 0.186049998f, 0.189070001f, 0.192080006f,
        0.195089996f, 0.198100001f, 0.201100007f, 0.204109997f, 0.207110003f, 0.210109994f, 0.21311f, 0.216110006f,
        0.219099998f, 0.222090006f, 0.225079998f, 0.228070006f, 0.231059998f, 0.234040007f, 0.237020001f, 0.239999995f,
        0.242980003f, 0.245949998f, 0.248930007f, 0.251899987f, 0.254869998f, 0.257829994f, 0.26078999f, 0.263749987f,
        0.266710013f, 0.26967001f, 0.272619992f, 0.275570005f, 0.278519988f, 0.281459987f, 0.28441f, 0.287349999f,
        0.290280014f, 0.293220013f, 0.296149999f, 0.299080014f, 0.30201f, 0.304930001f, 0.307850003f, 0.310770005f,
        0.313679993f, 0.316590011f, 0.319499999f, 0.322409987f, 0.325309992f, 0.328209996f, 0.331110001f, 0.333999991f,
        0.336890012f, 0.339780003f, 0.34266001f, 0.345539987f, 0.348419994f, 0.351289988f, 0.354160011f, 0.357030004f,
        0.359890014f, 0.362760007f, 0.365610003f, 0.368470013f, 0.371320009f, 0.374159992f, 0.377009988f, 0.37985f,
        0.382679999f, 0.385520011f, 0.388339996f, 0.391169995f, 0.39399001f, 0.396809995f, 0.399619997f, 0.402429998f,
        0.405239999f, 0.408039987f, 0.410840005f, 0.413639992f, 0.416429996f, 0.419220001f, 0.421999991f, 0.424780011f,
        0.427549988f, 0.430330008f, 0.433090001f, 0.435860008f, 0.438620001f, 0.44137001f, 0.44411999f, 0.446869999f,
        0.449609995f, 0.452349991f, 0.455080003f, 0.457810014f, 0.460539997f, 0.463259995f, 0.465979993f, 0.468690008f,
        0.471399993f, 0.474099994f, 0.476799995f, 0.479490012f, 0.482179999f, 0.484869987f, 0.48754999f, 0.490229994f,
        0.492900014f, 0.49555999f, 0.49823001f, 0.500880003f, 0.503539979f, 0.506190002f, 0.508830011f, 0.51147002f,
        0.514100015f, 0.516730011f, 0.519360006f, 0.521969974f, 0.524590015f, 0.527199984f, 0.529799998f, 0.532400012f,
        0.535000026f, 0.537590027f, 0.540170014f, 0.542750001f, 0.545319974f, 0.547890007f, 0.550459981f, 0.55302f,
        0.555570006f, 0.558120012f, 0.560660005f, 0.563199997f, 0.565729976f, 0.568260014f, 0.570779979f, 0.573300004f,
        0.575810015f, 0.578310013f, 0.58081001f, 0.583310008f, 0.585799992f, 0.588280022f, 0.590759993f, 0.593230009f,
        0.595700026f, 0.598160028f, 0.600619972f, 0.603070021f, 0.605509996f, 0.607949972f, 0.610379994f, 0.612810016f,
        0.615230024f, 0.617649972f, 0.620060027f, 0.622460008f, 0.624859989f, 0.627250016f, 0.629639983f, 0.632019997f,
        0.634389997f, 0.636759996f, 0.639119983f, 0.641480029f, 0.643830001f, 0.646179974f, 0.648509979f, 0.650849998f,
        0.65316999f, 0.655489981f, 0.657809973f, 0.660109997f, 0.662419975f, 0.664709985f, 0.666999996f, 0.669279993f,
        0.671559989f, 0.673829973f, 0.676090002f, 0.678349972f, 0.680599988f, 0.682850003f, 0.685079992f, 0.68730998f,
        0.689540029f, 0.691760004f, 0.693970025f, 0.696179986f, 0.698379993f, 0.700569987f, 0.702750027f, 0.704930007f,
        0.707109988f, 0.70927f, 0.711430013f, 0.713580012f, 0.715730011f, 0.717869997f, 0.720000029f, 0.722130001f,
        0.724250019f, 0.726360023f, 0.728460014f, 0.730560005f, 0.732649982f, 0.734740019f, 0.736819983f, 0.738889992f,
        0.740949988f, 0.743009984f, 0.745060027f, 0.747099996f, 0.749140024f, 0.751160026f, 0.753189981f, 0.755200028f,
        0.757210016f, 0.759209991f, 0.761200011f, 0.763189971f, 0.765169978f, 0.767139971f, 0.76910001f, 0.77105999f,
        0.773010015f, 0.774950027f, 0.77688998f, 0.778819978f, 0.780740023f, 0.782649994f, 0.784560025f, 0.786450028f,
        0.788349986f, 0.790229976f, 0.792110026f, 0.793980002f, 0.795840025f, 0.797689974f, 0.799539983f, 0.801379979f,
        0.80321002f, 0.805029988f, 0.806850016f, 0.808659971f, 0.810459971f, 0.812250018f, 0.814040005f, 0.815810025f,
        0.817579985f, 0.819350004f, 0.821099997f, 0.822849989f, 0.824590027f, 0.826319993f, 0.828040004f, 0.829760015f,
        0.831470013f, 0.833169997f, 0.834860027f, 0.836549997f, 0.83822f, 0.839890003f, 0.841549993f, 0.843209982f,
        0.844850004f, 0.846490026f, 0.848119974f, 0.849740028f, 0.851350009f, 0.852959991f, 0.854560018f, 0.856149971f,
        0.857729971f, 0.859300017f, 0.860870004f, 0.862420022f, 0.863969982f, 0.865509987f, 0.867049992f, 0.86857003f,
        0.870090008f, 0.871590018f, 0.873090029f, 0.87458998f, 0.876070023f, 0.877539992f, 0.879010022f, 0.880469978f,
        0.88191998f, 0.883360028f, 0.884800017f, 0.886219978f, 0.887639999f, 0.889050007f, 0.890450001f, 0.891839981f,
        0.893220007f, 0.894599974f, 0.895969987f, 0.897319973f, 0.898670018f, 0.900020003f, 0.901350021f, 0.902670026f,
        0.903989971f, 0.905300021f, 0.906599998f, 0.907890022f, 0.909169972f, 0.910440028f, 0.911710024f, 0.912959993f,
        0.914210021f, 0.915449977f, 0.916679978f, 0.917900026f, 0.91911f, 0.920319974f, 0.921509981f, 0.922699988f,
        0.923879981f, 0.92505002f, 0.926209986f, 0.927359998f, 0.92851001f, 0.929639995f, 0.93076998f, 0.931879997f,
        0.932990015f, 0.934090018f, 0.935180008f, 0.936269999f, 0.937340021f, 0.938399971f, 0.93945998f, 0.940509975f,
        0.941540003f, 0.942569971f, 0.943589985f, 0.944599986f, 0.945609987f, 0.94660002f, 0.947589993f, 0.948559999f,
        0.949530005f, 0.950489998f, 0.951430023f, 0.952369988f, 0.953310013f, 0.954230011f, 0.955139995f, 0.956040025f,
        0.956939995f, 0.957830012f, 0.958700001f, 0.959569991f, 0.960430026f, 0.961279988f, 0.962119997f, 0.962949991f,
        0.963779986f, 0.964590013f, 0.965390027f, 0.966189981f, 0.96697998f, 0.967750013f, 0.968519986f, 0.969280005f,
        0.97003001f, 0.970770001f, 0.971499979f, 0.972230017f, 0.972940028f, 0.973640025f, 0.974340022f, 0.975030005f,
        0.975700021f, 0.976369977f, 0.977029979f, 0.977680027f, 0.978320003f, 0.978950024f, 0.979569972f, 0.980180025f,
        0.980790019f, 0.981379986f, 0.981959999f, 0.982540011f, 0.983110011f, 0.983659983f, 0.984210014f, 0.984749973f,
        0.985279977f, 0.985800028f, 0.986310005f, 0.986810029f, 0.987299979f, 0.987779975f, 0.988259971f, 0.98872f,
        0.989180028f, 0.98961997f, 0.990059972f, 0.990480006f, 0.99089998f, 0.99131f, 0.991710007f, 0.9921f,
        0.99247998f, 0.992850006f, 0.993210018f, 0.993560016f, 0.993910015f, 0.994239986f, 0.994560003f, 0.994880021f,
        0.995180011f, 0.995480001f, 0.995769978f, 0.996039987f, 0.996309996f, 0.996569991f, 0.996819973f, 0.997060001f,
        0.997290015f, 0.997510016f, 0.997720003f, 0.99792999f, 0.99812001f, 0.998300016f, 0.998480022f, 0.998640001f,
        0.99879998f, 0.998939991f, 0.999080002f, 0.999199986f, 0.999319971f, 0.999430001f, 0.999530017f, 0.99962002f,
        0.99970001f, 0.999769986f, 0.999830008f, 0.999880016f, 0.999920011f, 0.999960005f, 0.999979973f, 1.0f,
    };

    // SnapshotVolumeCurve @0x82B453C0 (sub_82B453C0; targeted export 2026-08-07) --
    // the Nicotine volume-ramp curve evaluator. Selector map (even = 1 - odd twin):
    //   0/2/4/6/8 -> 1 - curve(ratio, type+1)         (fsubs 1.0 - recursion)
    //   1 -> LUT[trunc(ratio*511)]                    (console: fmuls ratio * -511.0
    //                                                  flt_820AD414, negative-indexed
    //                                                  off the table base)
    //   3 -> curve(ratio, 1)^2                        (fmuls)
    //   5 -> 1 - LUT[-trunc(ratio*511 - 511)]         (console: fmsubs flt_821478B0)
    //   7 -> curve(ratio, 5)^2                        (fmuls)
    //   9 -> ratio (raw)            default -> 0.0
    // The [0,1] bounds check only feeds the console's IDynamicMixer vtbl+0x10 assert
    // hook ("lfInput out of bounds. [0,1]") -- that virtual slot is un-modelled in
    // IDynamicMixer.hpp (out of this TU's file set) and is side-effect-free on the
    // shipped path, so it is documented rather than dispatched; the raw (unasserted)
    // ratio indexes the LUT exactly as on the console.
    double SnapshotVolumeCurve(double lfRatio, int liCurveType)
    {
        switch (liCurveType)
        {
            case 0: case 2: case 4: case 6: case 8:              // even = inverted odd twin
                return 1.0 - SnapshotVolumeCurve(lfRatio, liCurveType + 1);
            case 1:
            {
                const s32 liIndex = static_cast<s32>(static_cast<f32>(lfRatio) * -511.0f);   // flt_820AD414 (fctiwz)
                return KF_SnapshotVolumeCurve[-liIndex];         // base - 4*liIndex
            }
            case 3:
            {
                const f32 lfUp = static_cast<f32>(SnapshotVolumeCurve(lfRatio, 1));
                return lfUp * lfUp;                              // fmuls
            }
            case 5:
            {
                const s32 liIndex = static_cast<s32>(
                    static_cast<f32>(lfRatio) * 511.0f - 511.0f);   // flt_821478B0 (fmsubs; <= 0)
                return 1.0f - KF_SnapshotVolumeCurve[-liIndex];  // fsubs 1.0 - LUT
            }
            case 7:
            {
                const f32 lfDown = static_cast<f32>(SnapshotVolumeCurve(lfRatio, 5));
                return lfDown * lfDown;                          // fmuls
            }
            case 9:
                return lfRatio;                                  // identity (raw, unclamped)
            default:
                return 0.0;                                      // flt_82001CC0
        }
    }
} // namespace Nicotine (relocated block)

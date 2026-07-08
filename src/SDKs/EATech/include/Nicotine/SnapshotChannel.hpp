#pragma once

// ===========================================================================
//  Nicotine/SnapshotChannel.hpp  --  EATech "Nicotine" audio middleware:
//  per-snapshot mixer volume channel.
//
//  A SnapshotChannel holds one interpolating volume value used by the snapshot
//  mixer (Nicotine::SnapshotMixer / NFSLiveLink::UpdateActiveSnapshots). The
//  channel ramps from a base volume to a target volume over a duration; a
//  "ceiling" link (another SnapshotChannel) lets a channel's contribution be
//  summed onto a parent channel's final volume (and clamped to the DirectSound
//  minimum, -10000 millibels). Volumes are stored as signed-16-bit millibel
//  values (-10000 .. 0).
//
//  NO DWARF exists for this class (X360-only; not in the PS3 DecFIGS dump). The
//  shape below is derived STRICTLY from the X360 binary accessor bodies:
//      Nicotine::SnapshotChannel::GetCeilingVolume  @ 0x82B47338
//      Nicotine::SnapshotChannel::GetCurrentVolume  @ 0x82B47200
//      Nicotine::SnapshotChannel::GetFinalVolume    @ 0x82B472B8
//
//  LAYOUT (offsets reproduced from the accessor loads + the SnapshotMixer bodies
//  that drive the channel -- AttachToMixMap @0x82B47648 / Update @0x82B47C68 /
//  UpdateNicotineChannelData @0x82B47760):
//      +0x00  mi16TargetVolume  target / destination millibel volume (s16)
//      +0x02  mi16ProcVolume    cached master-mix-channel volume (s16; proc MixData hi16)
//      +0x04  mi16BaseVolume    base / starting millibel volume (s16)
//      +0x08  mpMixChProc       bound NFS master-mix-channel proc, or null
//      +0x0C  mpCeiling         parent "ceiling" channel, or null
//      +0x10  mfElapsed         seconds elapsed into the ramp (f32)
//      +0x14  mfDuration        total ramp duration in seconds (f32)
//  The +0x06..+0x07 hole is explicit padding so the named members land at the
//  binary offsets; its contents are owned/initialised by another TU.
// ===========================================================================

#include "types.hpp"

// The bound master-mix channel proc lives in the NFS mix record vocabulary; the
// channel holds it by pointer only, so a forward declaration suffices here (the
// SnapshotMixer TU includes NFSMixRecords.hpp to walk it).
struct stMasterMixChProc;

// FLAG (rwaudio/Nicotine PDB cross-check -- IDA Files/ProStreet08Milestone.pdb):
// ProStreet's Nicotine::SnapshotChannel [sizeof=20, : public MixerMemBase] is a DIFFERENT
// layout (mpChProc@0x00, mpCurrentTarget@0x04, mfTimeThroughLerp@0x08, mi16NicotineVolume@0x0C,
// mi16SnapshotVolume@0x0E, mi16LerpStartVolume@0x10). That is a genuine cross-build VERSION
// divergence -- NOT applicable here. Burnout's layout below is CONFIRMED by the ARTIST asm
// (GetCurrentVolume @0x82B47200: lfs 0x10=mfElapsed / lfs 0x14=mfDuration / lhz 0x04=base /
// lhz 0x00=target, then ratio = elapsed/duration interpolation). ARTIST wins; do NOT retrofit
// the ProStreet shape.

namespace Nicotine
{

// ---------------------------------------------------------------------------
// FLAG (un-homed helper): X360 sub_82B453C0 -- the Nicotine volume-ramp curve
// evaluator. Given a normalised ramp position lfRatio (clamped to [0,1] inside)
// and a curve-type selector, it returns the eased blend factor in [0,1]
// (selecting between a 512-entry curve table and squared/inverted variants).
// It is NOT present in any DWARF and is emitted in the NFSLiveLink/Nicotine
// mixer family; declared here so this TU compiles, defined in that TU.
//
// The X360 entry consumes the ratio as the first floating-point argument and
// the curve-type selector as the first integer argument (its recursive third
// parameter is the same selector under a different IDA name). The two snapshot
// curve selectors this channel uses are: 1 when ramping up (base <= target),
// 7 when ramping down (base > target).
// ---------------------------------------------------------------------------
double SnapshotVolumeCurve(double lfRatio, int liCurveType);

struct SnapshotChannel
{
    // ---- layout (offsets verified against the X360 accessors + mixer bodies) ----
    s16   mi16TargetVolume;  // [0x00] target millibel volume
    s16   mi16ProcVolume;    // [0x02] cached master-mix-channel volume (proc MixData hi16)
    s16   mi16BaseVolume;    // [0x04] base millibel volume
    u8    maPad06[2];        // [0x06] (owned elsewhere)
    stMasterMixChProc* mpMixChProc;  // [0x08] bound NFS master-mix channel proc, or null
    SnapshotChannel* mpCeiling;  // [0x0C] parent channel, or null
    f32   mfElapsed;         // [0x10] seconds into the ramp
    f32   mfDuration;        // [0x14] total ramp duration (seconds)

    // GetCurrentVolume @ 0x82B47200: the channel's own interpolated volume.
    //   if (mfElapsed >= mfDuration) return mi16TargetVolume;            // ramp done
    //   ratio = mfElapsed / mfDuration;
    //   curve = (mi16BaseVolume <= mi16TargetVolume) ? 1 : 7;            // up vs down
    //   blend = SnapshotVolumeCurve(ratio, curve);
    //   return mi16BaseVolume + (s16)trunc(blend * (mi16TargetVolume - mi16BaseVolume));
    int GetCurrentVolume();

    // GetFinalVolume @ 0x82B472B8: this channel's current volume, plus -- when a
    // ceiling channel is linked -- the ceiling's (recursive) final volume,
    // clamped to >= -10000 (the DirectSound minimum). With no ceiling it is just
    // GetCurrentVolume().
    int GetFinalVolume();

    // GetCeilingVolume @ 0x82B47338: the ceiling channel's final volume, or 0 if
    // no ceiling is linked.
    int GetCeilingVolume();
};

} // namespace Nicotine

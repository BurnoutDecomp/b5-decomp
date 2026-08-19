#pragma once

// CgsSceneManager::OverlappingPair -- one entry in the scene manager's overlap list:
// the pair of collision-volume INSTANCES that the broad phase found to be touching,
// queued for the narrow-phase overlap-culling pass to resolve.
//
// ===========================================================================
// 🔴 LAYOUT CORRECTED 2026-08-19 (wave Q5, cluster E3a -- the culler pair query).
//
// Until today this header declared
//     struct OverlappingPair { VolumeInstanceId mVolumeInstanceA, mVolumeInstanceB; };
// i.e. two PACKED 64-BIT HANDLES. That was inferred from nothing but the 16-byte
// stride of BaseEventQueue<OverlappingPair>::AddEvent @0x828B8B08 ("two u64 handles
// fill the 16 bytes"). It is WRONG, and it was wrong in the silent-corruption
// direction: the size matched, so nothing ever failed to compile, but every field
// role was misattributed. The element is four 32-bit lanes, not two 64-bit ones.
//
// The real field set is DWARF-attested VERBATIM -- and note the DWARF's own home for
// it is CgsSceneManagerTypes.h, not a file of its own:
//     references/DecFIGS/dwarfdump/GameShared/GameClasses/SceneManager/
//         CgsSceneManagerTypes.h:216
//         struct CgsSceneManager::OverlappingPair : public Event {
//             uint32_t muVolumeInstanceA;   // CgsSceneManagerTypes.h:87
//             uint32_t muVolumeInstanceB;   // CgsSceneManagerTypes.h:88
//             float_t  mfPadding;           // CgsSceneManagerTypes.h:89
//             bool     mbCull;              // CgsSceneManagerTypes.h:90
//         }
//
// ...and X360-attested THREE independent ways, producer and consumer:
//
//  (1) PRODUCER -- OverlapGenerationModule::OutputCollidingPairs @0x828C1F58 builds
//      each queued element from one SceneSweeper::CollidingPair (12-byte stride,
//      `addi r31,r31,0xC` per iteration) with exactly four stores into the staging
//      local at sp+var_40:
//          0x828C1FB0  lhz r11, -4(r31)   -> stw r11, var_40+0x00   mu16ObjectA
//          0x828C1FC8  lhz r11, -2(r31)   -> stw r11, var_40+0x04   mu16ObjectB
//          0x828C1FB8  lfs f0,   0(r31)   -> stfs f0, var_40+0x08   mfPadding
//          0x828C1FD0  lbz r11,  4(r31)   -> stb r11, var_40+0x0C   mbCull
//      A u16 object index ZERO-EXTENDED into a 32-bit lane, then a FLOAT lane, then a
//      single BYTE. Two u64 handles cannot produce that store pattern.
//
//  (2) CONSUMER A -- OverlapCullingModule::ProcessOverlapsQueue @0x828D0330 copies the
//      four words out (`lwz 0/4/8/0xC`) and gates the pair on the BYTE at +0x0C:
//          0x828D042C  lbz r11, var_40+0xC ; cmplwi ; bne <skip>
//      i.e. `if (!lrPair.mbCull) ProcessOverlap(...)`. (Hex-Rays renders that byte as
//      `HIBYTE(word3)`, which is only true because the console is big-endian.)
//
//  (3) CONSUMER B -- OverlapCullingModule::ProcessOverlap @0x828CAF38 reads +0x00 and
//      +0x04 as SIGNED 32-bit volume-INSTANCE indices (`cmpwi r25,-1` / `cmpwi r24,-1`
//      at 0x828CAF84/0x828CAF8C -- the -1 "no instance" sentinel), feeds each one
//      straight to EntityManager::GetVolumeInstance(s32), and reads +0x08 as a float
//      into f1 (`lfs f1, 8(r23)` @0x828CB078) to pass as DoPairQuery's padding.
//      SceneManagerModule::BridgeOverlapGenerationToOverlapCulling @0x828BA538
//      independently bounds-checks both lanes against KI_MAX_NUM_VOLUME_INSTANCES
//      (`cmpwi r31,0x13B8 ; bge <assert>` at 0x828BA5F4 and 0x828BA630).
//
// STRIDE, HOST AND CONSOLE: {u32,u32,f32,bool} is 13 bytes padded to 16 at alignment
// 4 -- the same 16-byte stride `slwi r11,r29,4` in BaseEventQueue<OverlappingPair>::
// GetEvent @0x828AE0D0, and the same 4-byte alignment the console proves twice (the
// queue is seated at InputBuffer+4, and EventQueue<OverlappingPair,16384>::Construct
// @0x828C4D40 puts maEvents at queue+0xC with no tail padding after the 12-byte
// {T*,s32,s32} header). The old two-u64 spelling forced 8-byte alignment and pushed
// maEvents to +0x10 on the host; that divergence is now gone as a side effect.
//
// ⚠️ EDITED OUT OF GRANT, DECLARED: cluster E3a's file grant is the culler module +
// its module-IO header/TU. This file is the ELEMENT TYPE of the two queues that
// cluster owns, and none of ProcessOverlapsQueue / ProcessOverlap / DoPairQuery can be
// landed faithfully against the old declaration (there is no honest way to read
// "mbCull" or "muVolumeInstanceA" out of a VolumeInstanceId). Grepped before editing:
// NO file anywhere in b5-decomp/src named mVolumeInstanceA/mVolumeInstanceB, and the
// only includers are CgsOverlapCullingModule.h, ContactGen/CgsOverlapCullingModuleIO.h,
// ContactGen/CgsOverlapGenerationModuleIO.h and
// EventQueue_OverlappingPair_AddEvent.cpp -- all of which use the type only as a queue
// element. Blast radius is therefore zero call sites; the change is reported in
// scratchpad/waveQ5/e3a.owner.md for the conductor to arbitrate.
//
// DWARF DELTA NOT TAKEN (recorded, not applied): the DWARF spells the struct
// `: public Event` (the empty CgsModule event base every queued payload derives from).
// The base is empty, so it costs nothing in layout, and adding it would drag
// CgsEventQueue.h into every includer of this header for no observable difference --
// left off deliberately, exactly as the sibling CgsSceneManager::Contact does.
// ===========================================================================

#include "types.hpp"

namespace CgsSceneManager
{
    struct OverlappingPair
    {
        // The two EntityManager volume-instance POOL indices (not VolumeInstanceIds,
        // and not VolumeManager volume indices -- that third index space is
        // VolumeInstance::miVolumeIndex, which ProcessOverlap looks up separately).
        // SIGNED in the consumer: -1 is the "no instance" sentinel it early-outs on.
        u32 muVolumeInstanceA;  // +0x00  DWARF CgsSceneManagerTypes.h:87
        u32 muVolumeInstanceB;  // +0x04  DWARF CgsSceneManagerTypes.h:88

        // The broad phase's per-pair padding, produced by SceneSweeper::
        // BuildCollidingPairs @0x828C2190 as the squared length of the difference of
        // the two instances' VolumeInstance::mPadding lanes, and consumed as the
        // narrow phase's fatness (rw::collision::VolumeVolumeQuery::m_padding /
        // PrimitivePairIntersect's `afPadding`).
        f32 mfPadding;          // +0x08  DWARF CgsSceneManagerTypes.h:89

        // The culling-table verdict BuildCollidingPairs stamped in
        // (`[groupOf(A)][groupOf(B)]`). TRUE means the pair is culled -- the culler
        // SKIPS it. Getting this polarity backwards resolves exactly the pairs that
        // should not collide and drops the ones that should.
        bool mbCull;            // +0x0C  DWARF CgsSceneManagerTypes.h:90
    };
}

// ===========================================================================
// CgsSceneManager::OverlapCullingModule -- the culler's INTERNAL-COLLISION half.
//   Wave Q5, cluster E3b (2026-08-19). Companion partfile to
//   ContactGen/CgsOverlapCullingModule.cpp (cluster E3a owns that file and the
//   shared header GameShared/GameClasses/SceneManager/CgsOverlapCullingModule.h).
//
// Three X360 ARTIST bodies, landed store-for-store from the per-address IDA
// export (.ida-exports/BURNOUT_X360_ARTIST.XEX/<addr>.json, `assembly` array --
// the pseudocode is a hint and is WRONG about IsInsideEscapeVolume's arity, see
// below):
//
//   OverlapCullingModule::ProcessInternalCollisions @ 0x828CB308  (206 insns)
//   OverlapCullingModule::IsInsideEscapeVolume      @ 0x828CB0A8  ( 75 insns)
//   OverlapCullingModule::DoInternalCollision       @ 0x828CB1D8  ( 76 insns)
//
// WHAT THIS HALF DOES. Separate from the broad-phase overlap pairs the sweeper
// produces, the culler keeps a per-volume-instance "internal collision"
// registration: a volume instance may name a second INTERNAL volume instance it
// must additionally be tested against every frame, plus a third ESCAPE volume
// instance that bounds how long the registration lives. Every frame
// ProcessInternalCollisions walks the registered set (a BitArray<5048> keyed by
// volume-instance index), and per registered instance either
//   * runs the internal pair query (DoInternalCollision -> DoPairQuery, which is
//     the SAME contact producer the overlap path uses), while the instance is
//     still inside its escape volume; or
//   * de-registers it (clears the bit) the first frame it is NOT.
// The queue that populates the three tables is
// OverlapCullingModule::ProcessAddInternalVolumeQueue (E3a's).
//
// ---------------------------------------------------------------------------
// MEMBER LAYOUT -- RE-DERIVED FROM THIS CLUSTER'S OWN ASM, NOT ASSUMED.
// Every table this file touches is reached BY NAME; the console byte offsets
// below are documentation only (gotcha 1 -- an X360 immediate is never a host
// value). They are quoted because they are what proves the header's member set
// is the right one, and all three bodies agree:
//
//   this + 0x230 (560)  mpVolumeManager        lwz r3,0x230(r31)  x2 per body
//   this + 0x234 (564)  mpEntityManager        lwz r3,0x234(r31)  x2 per body
//   this + 401984       mabIsUsingInternalCollision
//                         ProcessInternalCollisions: `addis r22,r14,6 ;
//                         addi r22,r22,0x2240` == 6*65536 + 0x2240 == 401984,
//                         and Prepare's own `memset(this+401984, 0, 632)`
//                         (632 == 79 u64 fields == BitArray<5048>).
//   this + 402616       mauInternalVolumeInstanceIndex[5048]
//                         DoInternalCollision: `addis r11,r29,2 ;
//                         addi r11,r11,-0x76D2 ; slwi r11,r11,2` ==
//                         (i + 131072 - 30418)*4 == i*4 + 402616.
//   this + 422808       mauEscapeVolumeInstanceIndex[5048]
//                         IsInsideEscapeVolume: `addis r11,r4,2 ;
//                         addi r11,r11,-0x631A ; slwi r11,r11,2` ==
//                         (i + 131072 - 25370)*4 == i*4 + 422808
//                         (== 402616 + 5048*4, i.e. the table straight after).
//   this + 443000       muNumPrimPrimPairs ... (the counter block; cross-checked
//                         from DoPairQuery @0x828C1A5C `addis r11,r27,7 ;
//                         addi r11,r11,-0x3D88` == 443000).
// So the header's order  mabIsUsingInternalCollision, mauInternalVolumeInstance-
// Index, mauEscapeVolumeInstanceIndex, <counters>  is attested end to end.
//
// ---------------------------------------------------------------------------
// THE BIT ARRAY IS DE-INLINED, DELIBERATELY. The X360 folds
// BitArray<5048>::GetFirstNonZeroBit / GetNextNonZeroBit / UnSetBit into
// ProcessInternalCollisions (that is most of its 206 instructions: the u64
// word scan, the `x & -x` lowest-set-bit isolate expressed as
// `cntlzd(w - ((w-1)&w))`, the in-word linear probe, and the `andc` clear).
// AGENTS "UNDO COMPILER OPTIMIZATIONS / inlining reversal" says to restore the
// calls, and CgsBitArray.h already carries all three methods with exactly these
// semantics. The two bounds ASSERTS the console emits inside those inlines stay
// at the call site, which is the convention CgsBitArray.h's own banner states
// ("emitted at the call sites by callers that own the CgsDev::Assert API"):
//   * CgsBitArray.h:241 ("luIndex < NUMBITS") -- reproduced below, it guards the
//     UnSetBit arm (X360 0x828CB42C `cmplwi r31,0x13B8 ; blt`).
//   * CgsBitArray.h:203 (the StrStream'd "invalid index : <i> < <N>") -- guards
//     the IsBitSet probe INSIDE the next-set-bit search (X360 0x828CB484..
//     0x828CB584). It has no expressible call site once the search is a single
//     GetNextNonZeroBit() call, and it is unreachable anyway (the search is
//     bounded by tuNumBits). NOT reproduced; recorded here instead of invented
//     somewhere it does not belong.
// PERFORMANCE DELTA, stated because it is real and not a defect: the committed
// GetNextNonZeroBit is a bit-at-a-time probe where the console skips whole
// all-zero u64 words. Same result, more loop trips when the registered set is
// sparse; fixing that belongs in CgsBitArray.h, not here.
//
// ---------------------------------------------------------------------------
// HEX-RAYS IS WRONG ABOUT IsInsideEscapeVolume'S SIGNATURE. The pseudocode
// renders it `IsInsideEscapeVolume(int a1)` -- one argument -- and then indexes
// `sub_828B9FD8(*(a1 + 564))` with no index at all. The asm has `addis r11,r4,2`
// in the third instruction: r4 is a live SECOND parameter (the volume-instance
// index), exactly as the DecFIGS DWARF declares it
// (CgsOverlapCullingModule.cpp:600 -> `bool IsInsideEscapeVolume(int32_t)`).
// The header's declaration is already right; the pseudocode is not.
// ===========================================================================

#include "GameShared/GameClasses/SceneManager/CgsOverlapCullingModule.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/SceneManager/CgsEntityManager.h"   // EntityManager, VolumeInstance
#include "GameShared/GameClasses/SceneManager/CgsVolumeManager.h"   // VolumeManager::GetRwVolume
#include "vendor/renderware/collision/GPInstance.hpp"               // PrimitivePairIntersect(+Result)

// NOT included, on purpose:
//   * ContactGen/CgsContactGenerationIO.h and ContactGen/CgsOverlapCullingModuleIO.h --
//     the OverlapCullingIO::OutputBuffer is only ever passed THROUGH this file
//     (to DoPairQuery), never dereferenced, so the forward declaration the
//     shared header already carries is enough. Including either would also make
//     this TU a second front in the still-open blob-vs-real OutputBuffer swap
//     that cluster D2 reported.
//   * vendor/renderware/collision/CollisionVolume.hpp -- only `const Volume*` is
//     needed and GPInstance.hpp forward-declares it. Pulling the full record in
//     would collide with the SECOND rw::collision::Volume that
//     SDKs/EATech/rwcollision/volume_debug_access.h defines (the fork
//     CollisionVolume.hpp's own banner reports).

namespace CgsSceneManager
{
namespace
{
    // ---------------------------------------------------------------------
    // The one type seam in this file, isolated so it is a single documented
    // place rather than four casts scattered through two bodies.
    //
    // VolumeManager::GetRwVolume returns `const CgsSceneManager::VolRef::Volume*`
    // (CgsVolumeStore.h:82 -- an opaque forward declaration) while the narrow
    // phase takes `const rw::collision::Volume*`. DecFIGS volume.h:39 typedefs
    // `VolRef::Volume` TO `rw::collision::Volume`, so these are one console
    // type described by two forward declarations in this tree; the conversion
    // reinterprets nothing.
    //
    // ⚠️ REPORTED, NOT UNILATERALLY FIXED (gotcha 7): unifying the two spellings
    // means editing CgsVolumeStore.h / CgsVolumeManager.h / CgsCollisionMeshData.h
    // / CgsOverlapCullingModule.h, none of which this cluster owns. The
    // VolumeManager owner already filed the same fork from the other side
    // (CgsVolumeManager.cpp's GetRwVolume banner) and casts identically there.
    // When the fork is collapsed, delete this helper and the calls become direct.
    inline const rw::collision::Volume* AsRwVolume(const VolRef::Volume* lpVolume)
    {
        return reinterpret_cast<const rw::collision::Volume*>(lpVolume);
    }
}

// ---------------------------------------------------------------------------
// OverlapCullingModule::IsInsideEscapeVolume @ 0x828CB0A8  (75 insns)
// DWARF home: CgsOverlapCullingModule.cpp:600
//
//   lpVolInst       = mpEntityManager->GetVolumeInstance( liVolumeInstanceIndex );
//   lpEscapeVolInst = mpEntityManager->GetVolumeInstance(
//                         mauEscapeVolumeInstanceIndex[ liVolumeInstanceIndex ] );
//   assert( lpVolInst,       "lpVolInst"       )   // .cpp:614   (li r5,0x266)
//   assert( lpEscapeVolInst, "lpEscapeVolInst" )   // .cpp:615   (li r5,0x267)
//   lpVol       = mpVolumeManager->GetRwVolume( lpVolInst->miVolumeIndex );
//   lpEscapeVol = mpVolumeManager->GetRwVolume( lpEscapeVolInst->miVolumeIndex );
//   assert( lpVol,       "lpVol"       )           // .cpp:619   (li r5,0x26B)
//   assert( lpEscapeVol, "lpEscapeVol" )           // .cpp:620   (li r5,0x26C)
//   return PrimitivePairIntersect( <stack result>, lpVol, lpVolInst->transform,
//                                  lpEscapeVol, lpEscapeVolInst->transform,
//                                  0.0f, NULL ) != 0;
//
// ORDER IS LOAD-BEARING AND PRESERVED: both GetVolumeInstance calls happen
// BEFORE either assert (r29/r27 are filled at 0x828CB0CC/0x828CB0DC, the asserts
// start at 0x828CB0F0), and `lwz r4,0x5C(r29)` -- the miVolumeIndex read -- is
// only reached AFTER them at 0x828CB134. CGS_ASSERT does not abort, so a NULL
// instance faults on the member read here exactly as it does on the console; no
// guard is added (adding one would be behaviour the binary does not have).
//
// THE `rw::collision::PrimitivePairIntersect` CALL, register by register
// (0x828CB19C-0x828CB1BC), against the committed declaration in
// vendor/renderware/collision/GPInstance.hpp:637:
//   r3  = sp + 0x50, an UNINITIALISED 1872-byte stack block -- exactly
//         sizeof(PrimitivePairIntersectResult) (0x750); the 0x7D0 frame is
//         0x50 linkage + this block + the __savegprlr_27 spill. No memset:
//         the callee writes every field it publishes.
//   r4  = lpVol                    -> apVolume1
//   r5  = lpVolInst                -> apMtx1     (see the transform note below)
//   r6  = lpEscapeVol              -> apVolume2
//   r7  = lpEscapeVolInst          -> apMtx2
//   f1  = flt_82001CC0             -> afPadding  (== 0.0f: an internal-collision
//         query runs with NO extra padding. Not guessed -- the value is pinned
//         three independent ways: the decrypted XEX image bytes are 0x00000000
//         (recorded at CgsModel.cpp:123), PrimitiveIntersect.cpp's own rodata
//         note for this exact symbol says 0.0f, and Hex-Rays renders the
//         argument `0.0` in both this body and DoInternalCollision.)
//   r9  = 0                        -> apSepDir   (NULL: no caller-supplied
//         separating direction, so the callee runs its own 6x6 SAT dispatch)
//   NOTE (gotcha 3): f1 consumes the r8 GPR slot, which is why apSepDir lands in
//   r9 and not r8. Hex-Rays drops the r9 argument entirely.
//   Return: `cntlzw ; extrwi 1,26 ; xori 1` == (result != 0) as a bool -- and the
//   caller reads it back with `clrlwi r11,r3,24`, i.e. one byte. bool is right.
//
// TRANSFORM ARGUMENT: the console passes the VolumeInstance pointer itself where
// the canonical prototype (rwccore.h:3001) takes `const Matrix44Affine*`. That is
// not a pun -- VolumeInstance::mWorldSpaceTransform is the record's FIRST member
// (CgsVolumeInstance.h, X360 +0x00), so the two addresses are identical. Written
// here as `&lpVolInst->mWorldSpaceTransform`: same address, and it says what the
// callee actually consumes instead of leaning on the offset being zero.
// ---------------------------------------------------------------------------
bool OverlapCullingModule::IsInsideEscapeVolume(s32 liVolumeInstanceIndex)
{
    // `lwzx r30, r11, r31` @0x828CB0C8 -- the escape registration is read one
    // instruction before the first GetVolumeInstance call, i.e. from the table
    // as it stands at entry, not after either lookup.
    const u32 luEscapeVolumeInstanceIndex =
        mauEscapeVolumeInstanceIndex[liVolumeInstanceIndex];

    VolumeInstance* lpVolInst =
        mpEntityManager->GetVolumeInstance(liVolumeInstanceIndex);
    VolumeInstance* lpEscapeVolInst =
        mpEntityManager->GetVolumeInstance(static_cast<s32>(luEscapeVolumeInstanceIndex));

    CGS_ASSERT(lpVolInst != NULL, "lpVolInst");              // .cpp:614
    CGS_ASSERT(lpEscapeVolInst != NULL, "lpEscapeVolInst");  // .cpp:615

    const VolRef::Volume* lpVol =
        mpVolumeManager->GetRwVolume(lpVolInst->miVolumeIndex);
    const VolRef::Volume* lpEscapeVol =
        mpVolumeManager->GetRwVolume(lpEscapeVolInst->miVolumeIndex);

    CGS_ASSERT(lpVol != NULL, "lpVol");              // .cpp:619
    CGS_ASSERT(lpEscapeVol != NULL, "lpEscapeVol");  // .cpp:620

    rw::collision::PrimitivePairIntersectResult lIntersectResult;

    return rw::collision::PrimitivePairIntersect(lIntersectResult,
                                                 AsRwVolume(lpVol),
                                                 &lpVolInst->mWorldSpaceTransform,
                                                 AsRwVolume(lpEscapeVol),
                                                 &lpEscapeVolInst->mWorldSpaceTransform,
                                                 0.0f,
                                                 NULL) != 0;
}

// ---------------------------------------------------------------------------
// OverlapCullingModule::DoInternalCollision @ 0x828CB1D8  (76 insns)
// DWARF home: CgsOverlapCullingModule.cpp:644
//
// The exact same four-step resolve as IsInsideEscapeVolume, against the INTERNAL
// table instead of the ESCAPE table, and ending in the shared contact producer
// DoPairQuery instead of the bare intersection test:
//
//   assert( lpVolInst,    "lpVolInst"    )   // .cpp:657  (li r5,0x291)
//   assert( lpIntVolInst, "lpIntVolInst" )   // .cpp:658  (li r5,0x292)
//   assert( lpVol,        "lpVol"        )   // .cpp:662  (li r5,0x296)
//   assert( lpIntVol,     "lpIntVol"     )   // .cpp:663  (li r5,0x297)
//
// DoPairQuery ARGUMENT ORDER, register by register (0x828CB2D4-0x828CB2FC):
//   r3  = this
//   r4  = r23 = the OutputBuffer this function was handed
//   r5  = r30 = lpVolInst
//   r6  = r29 = liVolumeInstanceIndex          <-- the VOLUME-INSTANCE index
//   r7  = r27 = lpVol
//   r8  = r26 = lpIntVolInst
//   r9  = r24 = mauInternalVolumeInstanceIndex[liVolumeInstanceIndex]
//                                              <-- also a VOLUME-INSTANCE index
//   r10 = r25 = lpIntVol
//   f1  = flt_82001CC0 == 0.0f (no padding, same constant as above)
//
// ⚠️ REPORTED TO THE HEADER OWNER (E3a): the declaration at
// CgsOverlapCullingModule.h:127-131 names r6/r9 `luVolumeIndexA`/`luVolumeIndexB`.
// They are NOT volume indices (those would be `lpVolInst->miVolumeIndex`, which
// is what GetRwVolume is fed two lines earlier and is a DIFFERENT index space) --
// they are volume-INSTANCE indices, and DoPairQuery's own body proves it: it
// stashes r6/r9 in r17/r16 and stores them straight into the
// CgsSceneManager::Contact it builds (0x828C1B20 `stw r17,var_7F0` /
// 0x828C1B28 `stw r16,var_7EC`), which is the pair of instance ids the world
// bridge later resolves. Suggested rename: luVolumeInstanceIndexA / ...B. A
// wrong name here is a live trap for whoever bodies DoPairQuery.
//
// RETURN TYPE: the asm tail-calls DoPairQuery and leaves its r3 in place, which
// is why Hex-Rays types this `int`. The DWARF declares
// `void DoInternalCollision(int32_t, OutputBuffer *)` (CgsOverlapCullingModule.h,
// .cpp:644) and the only caller (ProcessInternalCollisions @0x828CB424) discards
// r3 -- so void it is, and the result of DoPairQuery is deliberately not used.
// ---------------------------------------------------------------------------
void OverlapCullingModule::DoInternalCollision(s32 liVolumeInstanceIndex,
                                               OverlapCullingIO::OutputBuffer* lpOutputBuffer)
{
    // lwzx r24, r11, r31 -- the internal registration, read before the calls.
    const u32 luInternalVolumeInstanceIndex =
        mauInternalVolumeInstanceIndex[liVolumeInstanceIndex];

    VolumeInstance* lpVolInst =
        mpEntityManager->GetVolumeInstance(liVolumeInstanceIndex);
    VolumeInstance* lpIntVolInst =
        mpEntityManager->GetVolumeInstance(static_cast<s32>(luInternalVolumeInstanceIndex));

    CGS_ASSERT(lpVolInst != NULL, "lpVolInst");        // .cpp:657
    CGS_ASSERT(lpIntVolInst != NULL, "lpIntVolInst");  // .cpp:658

    const VolRef::Volume* lpVol =
        mpVolumeManager->GetRwVolume(lpVolInst->miVolumeIndex);
    const VolRef::Volume* lpIntVol =
        mpVolumeManager->GetRwVolume(lpIntVolInst->miVolumeIndex);

    CGS_ASSERT(lpVol != NULL, "lpVol");        // .cpp:662
    CGS_ASSERT(lpIntVol != NULL, "lpIntVol");  // .cpp:663

    DoPairQuery(lpOutputBuffer,
                lpVolInst, static_cast<u32>(liVolumeInstanceIndex), lpVol,
                lpIntVolInst, luInternalVolumeInstanceIndex, lpIntVol,
                0.0f);
}

// ---------------------------------------------------------------------------
// OverlapCullingModule::ProcessInternalCollisions @ 0x828CB308  (206 insns)
// DWARF home: CgsOverlapCullingModule.cpp:682
//
//   assert( lpOutputBuffer != NULL )   // .cpp:684  (li r5,0x2AC)
//   for ( i = mabIsUsingInternalCollision.GetFirstNonZeroBit();
//         i >= 0;
//         i = mabIsUsingInternalCollision.GetNextNonZeroBit( i ) )
//   {
//       if ( IsInsideEscapeVolume( i ) )  DoInternalCollision( i, lpOutputBuffer );
//       else                              mabIsUsingInternalCollision.UnSetBit( i );
//   }
//
// TERMINATION, as the asm actually writes it: the loop's back edge tests ONLY
// `i >= 0` (0x828CB630 `cmpwi cr6,r31,0 ; bge loc_828CB3FC`). The `>= 5048`
// bound is enforced inside the inlined searches, which `return` outright
// (0x828CB378, 0x828CB3A8, 0x828CB5C8, 0x828CB5D4, 0x828CB5FC). The committed
// GetFirstNonZeroBit / GetNextNonZeroBit hand back KI_INVALID_BITINDEX (-1) on
// every one of those paths, so `i >= 0` is the same condition -- and the
// entry `cmpwi r31,0x13B8 ; bge` / `cmpwi r31,0 ; blt` pair at 0x828CB3A4/A8/AC
// is the same -1-or-out-of-range early exit before the first iteration.
//
// THE ELSE ARM IS A DE-REGISTRATION, NOT AN ERROR PATH: the console clears the
// bit with `sld/andc/stdx` at 0x828CB44C-0x828CB460, i.e. once a volume instance
// has left its escape volume it stops being internally collided, permanently,
// until something re-posts it through the add-internal-volume queue. Clearing
// bit i does not disturb the search, which resumes at i+1.
//
// ⚠️ COUNTER NOTE for whoever wires the boot diagnostics (scene_collision_scout.md
// §6) -- DO NOT expect muNumPrimPrimPairs to show internal-collision work. The
// console's Update @0x828D1D50 runs, in this order:
//     LockForRead(in) ; LockForWrite(out)
//     ProcessAddInternalVolumeQueue(in)
//     ProcessInternalCollisions(out)          <-- this function
//     ProcessOverlapsQueue(out, in)
//     UnlockForRead(in) ; UnlockForWrite(out)
// and ProcessOverlapsQueue OPENS by zeroing six of the eight counters
// (0x828D0398-0x828D03E0 stores 0 to this+443000/443004/443008/443012/443024/
// 443028 == muNumPrimPrimPairs / muNumPrimAggPairs / muNumOtherPairs /
// muNumInstanceQueries / muNumPrimPrimContacts / muNumPrimAggContacts). So
// whatever DoPairQuery increments on this pass is WIPED before the frame ends.
// That is the console's own behaviour, preserved; the internal-collision path
// has no counter of its own and needs its own instrumentation if it needs any.
//
// ⚠️ AND THE MOUNTED Update() DOES NOT MATCH THAT SEQUENCE -- reported to the
// E3a owner rather than fixed here (CgsOverlapCullingModule.cpp is not this
// cluster's file). See scratchpad/waveQ5/e3b.owner.md, defects_found.
// ---------------------------------------------------------------------------
void OverlapCullingModule::ProcessInternalCollisions(OverlapCullingIO::OutputBuffer* lpOutputBuffer)
{
    CGS_ASSERT(lpOutputBuffer != NULL, "lpOutputBuffer != NULL");  // .cpp:684

    for (s32 liVolumeInstanceIndex = mabIsUsingInternalCollision.GetFirstNonZeroBit();
         liVolumeInstanceIndex >= 0;
         liVolumeInstanceIndex = mabIsUsingInternalCollision.GetNextNonZeroBit(liVolumeInstanceIndex))
    {
        if (IsInsideEscapeVolume(liVolumeInstanceIndex))
        {
            DoInternalCollision(liVolumeInstanceIndex, lpOutputBuffer);
        }
        else
        {
            // The bounds check the console emits from inside the inlined
            // UnSetBit (0x828CB42C); CgsBitArray.h keeps its asserts at the
            // call site by design. Unreachable in practice -- the search is
            // bounded by tuNumBits -- and preserved because the binary has it.
            CGS_ASSERT(static_cast<u32>(liVolumeInstanceIndex) < KU_MAX_NUM_VOLUME_INSTANCES,
                       "luIndex < NUMBITS");
            mabIsUsingInternalCollision.UnSetBit(static_cast<u32>(liVolumeInstanceIndex));
        }
    }
}

}  // namespace CgsSceneManager

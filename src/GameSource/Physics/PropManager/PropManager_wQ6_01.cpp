// GameSource/Physics/PropManager/PropManager_wQ6_01.cpp
//
// BrnPhysics::Props::PropManager -- breakable-props wave Q6, partfile 01.
// Reconstructed from BURNOUT_X360_ARTIST.XEX:
//   RemoveAllPropsAndParts @0x8260F010  (331 instructions, 0x8260F010..0x8260F538 --
//                                        COUNTED: (0x8260F538-0x8260F010)/4 + 1 == 331)
//
// A slice of the TU's own BrnPropManager.cpp, split out per the wave's partfile convention
// (same spirit as PropManager_wQ2_08.cpp, which reconstructed this function's only caller).
//
// ==========================================================================================
// ⚠️ PROVENANCE -- THIS FUNCTION IS A GENUINE EXPORT HOLE, CLOSED THIS WAVE.
//
// There is NO .ida-exports/BURNOUT_X360_ARTIST.XEX/0x8260F010.json, and no
// `BrnPhysics::Props::PropManager::RemoveAllPropsAndParts` row in progress/identity.json --
// which is why three separate committed banners (BrnPropManager.h:507/:939,
// PropManager_wQ2_08.cpp:113, BrnPhysicsConductorGates.cpp:500) could only name the address
// and the instruction count, taken from ProcessInputsPreScene's xrefs_from.
//
// The 331 instructions below were dumped 2026-08-19 by a headless `idat` run over a PRIVATE
// copy of "IDA Files/BURNOUT_X360_ARTIST.XEX.i64" (never the original):
//     scratchpad/waveQ6/ida_rmall/dump_rmall.py   -> scratchpad/waveQ6/ida_rmall/out.json
//     scratchpad/waveQ6/ida_rmall/rmall.asm.txt   (the 331 lines, one per instruction)
// IDA DOES name the function in that database (`BrnPhysics::Props::PropManager::
// RemoveAllPropsAndParts`, 0x8260F010..0x8260F53C) -- the hole was in the per-address EXPORT
// RUN, not in the analysis. AGENTS.md gotcha 6, third time in this subsystem.
//
// xrefs_to (measured, not inherited): 0x8263B228 inside ProcessInputsPreScene -- the live call
// at PropManager_wQ2_08.cpp:162 -- and NOTHING ELSE. The second xref IDA lists, 0x821CF120, was
// chased rather than assumed (scratchpad/waveQ6/ida_rmall/probe_xref.py): it is a `.pdata`
// UNWIND-TABLE slot (`.long BrnPhysics__Props__PropManager__RemoveAllPropsAndParts`, sitting
// between the Prepare and RemoveProp entries in the same table), not a caller. So the function
// has exactly ONE call site in the image. That table also cross-checks the span: the next entry
// is RemoveProp @0x8260F540, and 0x8260F540 - 0x8260F010 == 0x530 == 332 words == the 331
// instructions plus one alignment pad.
//
// ==========================================================================================
// WHAT THE 331 INSTRUCTIONS DO. All offsets quoted below are CONSOLE offsets read straight
// out of the asm; every one of them is reached BY NAME in the C++ (AGENTS.md gotcha 1 -- they
// are meaningless on the LLP64 host).
//
// Register map, MEASURED from the prologue (0x8260F010..0x8260F028):
//   r3 -> r30 = this                         (spilled to arg_14 at 0x8260F028 and reloaded
//                                             at 0x8260F2C8 / 0x8260F518)
//   r4 -> r3  = lpSimModuleInputBuffer       (consumed immediately, never saved)
//   r5 -> r26 = lpSceneInput
//   r14 = this + 0x80 == &mUsedProps         (set once at 0x8260F038, live to the very end)
//   r24 = 0                                  (the zero the six trailing stores use)
//   r15 = 1                                  (the IsBitSet mask seed)
//
// Body, in emission order:
//
//  1. 0x8260F02C..0x8260F048 -- the SIMULATION request, and it is a SINGLE event for the whole
//     owner, not one per body:
//         bl sub_825BD000   == InputBuffer::GetRemoveAllRigidBodiesQueue()  [WRITE side]
//         bl sub_825E4020   == BaseEventQueue<InRemoveAllRigidBodies>::AddEvent()  (no-arg
//                              slot reserve; returns the slot -- `add r3, mLength, mpEvents`
//                              with NO stride multiply, which is itself the proof that
//                              sizeof(InRemoveAllRigidBodies) == 1)
//         li r11, 3 ; stb r11, 0(r3)   == mu8OwnerId = E_ENTITYTYPE_PROP
//     Both callees were dumped in the same idat run (out.json "callee_bodies"):
//       * sub_825BD000, 42 insns: `lbz r11,0(r28) ; extrwi r11,r11,1,28` == status bit 3 ==
//         the WRITE lock, assert "Not locked for writing\n" with `li r5,0x43F` ==
//         CgsPhysicsSimulationModuleIO.h:1087, then `addis r3,r28,2 ; addi r3,r3,-0x5CD0` ==
//         this + 0x1A330 == +107312 == &mRemoveAllRigidBodiesQueue -- the SAME offset the
//         already-committed const accessor @0x8289E750 returns. The non-const overload was
//         missing from the tree and is added this wave (see the LINK-LEVEL note below).
//       * sub_825E4020, 40 insns: asserts "mpEvents != NULL" (line 0x168 == 360) and
//         "GetLength() < GetMaxLength()" (line 0x169 == 361) -- byte-for-byte the two source
//         lines the committed CgsBaseEventQueue.h:42-70 records for its no-arg reserve.
//     ⚠️ The tree spells that member `T& AddEvent()`. The DecFIGS DWARF's real name for it is
//     `T* AllocateEvent()` @CgsBaseEventQueue.h:356, and the DWARF for THIS function names its
//     local `InRemoveAllRigidBodies* lpEvent` -- i.e. the console source held a POINTER. That
//     name/return-type defect is already FILED by the header's owner (CgsBaseEventQueue.h:51-62,
//     "NAME DEFECT, FILED NOT FIXED 2026-08-18"); it is deliberately not re-discovered or
//     re-fixed here. The `&` below is what bridges the two, and it costs nothing.
//
//  2. 0x8260F040..0x8260F2C8 -- the PROP loop. The fully inlined BitArray<15>::
//     GetFirstNonZeroBit / GetNextNonZeroBit / IsBitSet walk over `addi r14, r30, 0x80` ==
//     &mUsedProps, re-rolled here into the named calls (AGENTS.md "inlining reversal"), exactly
//     as its caller ProcessInputsPreScene does with the identical inline.
//     Per live prop: `addi r11, r31, 0x1C` == KI_PROP_CACHE_START_INDEX + liPropIndex written
//     into a 4-byte stack record, then `bl 0x825E48C0` ==
//     BaseEventQueue<InEventRemoveFromCache>::AddEvent(const T*) on the queue at
//     `addis r27,r26,0xC ; addi r27,r27,0x77E0` == lpSceneInput + 0xC77E0 ==
//     &mRemoveFromCacheQueue.
//     ⚠️ The bit is NOT cleared inside the loop -- both bit-sets are wiped wholesale in step 4.
//     That is why GetNextNonZeroBit is handed the CURRENT index and still advances.
//
//  3. 0x8260F2CC..0x8260F518 -- the PART loop, the same shape over `addi r27, r30, 0x90` ==
//     &mUsedParts (the BitArray<30>; every bound in this half is 0x1E == 30), with
//     `addi r11, r31, 0x2B` == KI_PROP_PART_CACHE_START_INDEX + liPartIndex. The scene queue
//     pointer is re-derived into r26 this time (`addis r26,r26,0xC ; addi r26,r26,0x77E0`) --
//     same address, different register, because r27 has been retasked to &mUsedParts.
//
//  4. 0x8260F51C..0x8260F530 -- SIX stores, all of the r24 zero, and this is the whole tail:
//         std r24, 0(r14)     mUsedProps.UnSetAll()          (+0x80, one 64-bit field)
//         std r24, 0(r27)     mUsedParts.UnSetAll()          (+0x90, one 64-bit field)
//         stw r24, 0x88(r30)  muNumberOfPropInstances = 0
//         stw r24, 0x98(r30)  muNumberOfPartInstances = 0
//         std r24, 0x670(r30) mUsedPropJoints.UnSetAll()
//         std r24, 0x678(r30) mBreakPropJoints.UnSetAll()
//     `UnSetAll` (not `Prepare`) is the spelling this class already uses for these four
//     members in both Construct (BrnPropManager.cpp:393/:394/:399/:400) and Prepare
//     (PropManager_wQ2_06.cpp:259/:260) -- no second synonym is introduced.
//
//     ⭐ THIS RESOLVES A CAVEAT THE HEADER LEFT OPEN. BrnPropManager.h:502-513 states that no
//     PropManager function with a per-address export writes +0x88 / +0x98, and explicitly
//     scopes that to "the three export holes have no JSON to scan". One of those three holes is
//     THIS function, and it DOES write both -- with zero, on the teardown path only. So the two
//     counters still have no producer of a NON-ZERO value anywhere in the class; what changes is
//     that they now have an attested WRITER. Reported, and the header line should be narrowed to
//     "no non-zero producer" by its owner rather than left as-is.
//
//     ⚠️ WHAT IS *NOT* HERE, stated because its absence is load-bearing: the function does NOT
//     post a single InRemoveRigidBody (the owner-scoped remove-all covers every body -- the
//     drain PhysicsSimulationModule::ProcessRemoveAllRigidBodiesQueue @0x8289F1D8 sweeps all
//     200 slots matching the owner byte), does NOT touch mpaPropInstances / mpaPartInstances,
//     does NOT call RemoveProp or RemovePart, and does NOT clear mbPhysical on the world side.
//     Grepped over all 331 instructions: the ONLY `bl`s are the two in step 1, the two
//     InEventRemoveFromCache AddEvents, and the two unreachable assert blocks.
//
// ---- THE ONE ASSERT THAT IS EMITTED TWICE AND IS PROVABLY UNREACHABLE --------------------
// 0x8260F120..0x8260F218 (props) and 0x8260F370..0x8260F468 (parts) are two verbatim copies of
// IsBitSet's index precondition, inlined THROUGH GetNextNonZeroBit: the StrStream
// "invalid index : " << i << " < " << 15/30 with `li r5, 0xCB` == 203 (the same
// CgsBitArray.h:203 tripwire RemoveProp/RemovePart spell out at their own call sites).
// Neither copy can be entered, and the argument is the same one PropManager_wQ2_08.cpp:54-58
// makes for the identical inline:
//     r28 = min((r31 & ~63) + 64, LIMIT)          0x8260F0F8..0x8260F108 / 0x8260F348..0x8260F358
//     the inner-scan guard `cmplw r29,r28 ; bge` 0x8260F110/0x8260F250 (0x8260F360/0x8260F4A0)
//     means r29 < r28 <= LIMIT whenever `cmplwi r29,LIMIT ; blt` 0x8260F118 (0x8260F368) runs,
// so the fall-through into the assert block is dead code. Behaviour is identical, provably, so
// nothing is spelled here. (This is also why the function's callee list names
// CgsContainers::BasePriorityQueue::Clear @0x82815E58 twice -- both calls are INSIDE those dead
// blocks and are an ICF fold of the assert message-stream reset, not a container this TU uses.)
//
// ---- DWARF (DecFIGS) --------------------------------------------------------------------
// dwarfdump GameSource/Physics/PropManager/BrnPropManager.cpp:2265 (source BrnPropManager.cpp:420)
// and BrnPropManager.h:256/:540. The scope names the parameters and all three locals:
//     void RemoveAllPropsAndParts(InputBuffer* lpSimModuleInputBuffer,
//                                 InSceneUpdateInterface* lpSceneInput)
//     :422  InRemoveAllRigidBodies* lpEvent
//     :426  int32_t                 liPropIndex
//     :431  int32_t                 liPartIndex
// Those names are used verbatim below.
// ⚠️ DECLARATION NIT, REPORTED NOT CHANGED (BrnPropManager.h is another owner's file, and it
// MOVED under this reconstruction mid-session -- the declaration slid from :746 to :772 in the
// course of one afternoon, so no line number is quoted): the committed declaration
// (`void RemoveAllPropsAndParts(`, in the public block) names the parameters `lpSimInputBuffer` /
// `lpSceneInterface`. The DWARF names them `lpSimModuleInputBuffer` / `lpSceneInput`, which is
// also what the caller's committed body passes. Types and order are identical and MEASURED --
// `mr r5,r31 ; mr r4,r14` at 0x8263B21C/0x8263B220 hands over (simInputBuffer, sceneInput) in
// that order -- so this is a names-only divergence with no behavioural edge.
//
// ⚠️ ALSO: the DWARF's source-level call for the two cache evictions is
// InSceneUpdateInterface::RemoveCachedObject (dwarfdump BrnPropManager.cpp's ProcessInputsPreScene
// scope lists it twice, once per loop, because PS3 inlined this whole function there). That
// method is not declared in this tree, and the console emits the queue append directly -- the
// same situation, with the same committed resolution, as RemoveProp (PropManager_wQ2_04.cpp:599)
// and RemovePart (:513). The direct `mRemoveFromCacheQueue.AddEvent` form is used here to match
// its two siblings; a future RemoveCachedObject landing should convert all three together.
//
// ---- ⚠️ LINK-LEVEL, NOT VISIBLE TO `cl /c` (AGENTS.md gotcha 7 / 12) ----------------------
//   * RemoveAllPropsAndParts is ALSO defined -- inertly -- at
//     GameSource/Physics/BrnPhysicsConductorGates.cpp:514-518 (BRN_CONDUCTOR_GATE at :517).
//     That gates file IS mounted (build_game_exe.bat:1177). Mounting THIS partfile without
//     retiring that gate in the SAME change is an LNK2005 duplicate. Reported, NOT removed
//     here: gate retirement is conductor-only.
//   * This TU introduces NO new unresolved external. Its five callees are:
//       InputBuffer::GetRemoveAllRigidBodiesQueue()  -- bodied this wave in the already-mounted
//                                                      CgsPhysicsSimulationModuleIO_InputBuffer.cpp
//       BaseEventQueue<InRemoveAllRigidBodies>::AddEvent()          header inline
//       BaseEventQueue<InEventRemoveFromCache>::AddEvent(const T&)  header inline
//       BitArray<15>/<30>::{GetFirstNonZeroBit,GetNextNonZeroBit,UnSetAll}  header inline
//     Everything it touches on `this` is a data member. The only thing the linker still needs
//     from the wave is the retirement above.
// ==========================================================================================

#include "GameSource/Physics/PropManager/BrnPropManager.h"
#include "GameSource/World/BrnEntityTypes.h"                                  // E_ENTITYTYPE_PROP
#include "GameShared/GameClasses/Containers/CgsBitArray.h"                    // BitArray<>::KI_INVALID_BITINDEX
#include "GameShared/GameClasses/Physics/CgsPhysicsSimulationModuleIO.h"      // InputBuffer + the write-side queue getter
#include "GameShared/GameClasses/Physics/CgsPhysicsSimulationIO_Events.h"     // InRemoveAllRigidBodies
#include "GameShared/GameClasses/SceneManager/CgsSceneManagerIO_SceneUpdate.h"// InSceneUpdateInterface + mRemoveFromCacheQueue
#include "GameShared/GameClasses/SceneManager/CacheManager/CgsTriangleCacheManagerIO.h" // InEventRemoveFromCache
#include "GameShared/GameClasses/Development/Log/CgsLog.h"                     // [DIAG] gpDebugPrint only
#include <stdlib.h>                                                            // getenv -- [DIAG] BRN_PROP_DIAG only, host-side

namespace BrnPhysics
{
namespace Props
{
    // The two triangle-cache slot bases. Same file-static form and same DWARF attribution the
    // two sibling partfiles use (PropManager_wQ2_02.cpp:97-98, PropManager_wQ2_04.cpp:121-122):
    // BrnTriangleCacheConstants.h has no committed home in this tree yet, so each partfile that
    // needs them carries its own internal-linkage copy rather than forking a header.
    static const s32 KI_PROP_CACHE_START_INDEX      = 28;   // DWARF BrnTriangleCacheConstants.h:36
    static const s32 KI_PROP_PART_CACHE_START_INDEX = 43;   // DWARF BrnTriangleCacheConstants.h:37

    // ======================================================================================
    // RemoveAllPropsAndParts  --  X360 0x8260F010, 331 instructions.
    // DecFIGS: dwarfdump BrnPropManager.h:256 (declaration) / BrnPropManager.cpp:2265 (scope,
    // source line :420). Parameter names and all three locals are the DWARF's.
    //
    // Drain every live prop and part back out of the simulation and out of the triangle cache.
    // Reached from ProcessInputsPreScene (PropManager_wQ2_08.cpp:162) whenever the world side
    // has set PropInputInterface::mbRemoveAllPropsAndParts -- i.e. on every world unload.
    // ======================================================================================
    void
    PropManager::RemoveAllPropsAndParts(
        CgsPhysics::PhysicsSimulationIO::InputBuffer*            lpSimModuleInputBuffer,
        CgsSceneManager::SceneManagerIO::InSceneUpdateInterface* lpSceneInput )
    {
        // Step 1 -- one owner-scoped "remove every rigid body I own" request. 0x8260F02C..0x8260F048.
        // The `&` is the AddEvent/AllocateEvent return-type defect filed at
        // CgsBaseEventQueue.h:51-62; the DWARF local (:422) is a pointer, so a pointer is what
        // the console source held.
        CgsPhysics::PhysicsSimulationIO::InRemoveAllRigidBodies* const lpEvent =
            &lpSimModuleInputBuffer->GetRemoveAllRigidBodiesQueue()->AddEvent();

        lpEvent->mu8OwnerId = static_cast<u8>( BrnWorld::E_ENTITYTYPE_PROP );   // li r11,3 ; stb

        // [DIAG] NOT IN THE X360 BINARY. Opt in with BRN_PROP_DIAG; budgeted to the first four
        // teardowns (a world unload is a rare event, so a strict one-shot would hide the
        // second junkyard). Until this wave the whole function was an inert BRN_CONDUCTOR_GATE,
        // so the counts below are the first evidence that (a) the teardown request reaches
        // physics at all and (b) how many bodies and cache slots the gate was discarding with no trace.
        // They are also the direct cross-check on the three ~163 s assert families
        // (scout.md §0.3): a NON-ZERO count here on a frame the world is also posting
        // RemovePropInstance events is the double-retire the asserts are reporting.
        static const bool sbPropDiag    = ( getenv( "BRN_PROP_DIAG" ) != 0 );
        static s32        siDiagBudget  = 4;
        const bool        lbDiag        = sbPropDiag && CgsDev::Log::gpDebugPrint != 0
                                          && siDiagBudget > 0;
        s32               liDiagProps   = 0;
        s32               liDiagParts   = 0;

        // Step 2 -- evict every live PROP's triangle-cache slot. 0x8260F040..0x8260F2C8.
        // The loop guard is the console's own `cmpwi r31,-1 ; bne` at 0x8260F2C0/0x8260F2C4;
        // its `cmpwi r31,0xF ; bge` companion at 0x8260F0CC is the same exit, already folded
        // into the container's ">= tuNumBits returns KI_INVALID_BITINDEX" leg.
        for ( s32 liPropIndex = mUsedProps.GetFirstNonZeroBit();
              liPropIndex != CgsContainers::BitArray<15>::KI_INVALID_BITINDEX;
              liPropIndex = mUsedProps.GetNextNonZeroBit( liPropIndex ) )
        {
            CgsSceneManager::TriangleCacheManagerIO::InEventRemoveFromCache lRemoveFromCacheEvent;
            lRemoveFromCacheEvent.miCacheSlot = KI_PROP_CACHE_START_INDEX + liPropIndex;

            lpSceneInput->mRemoveFromCacheQueue.AddEvent( lRemoveFromCacheEvent );

            ++liDiagProps;   // [DIAG]
        }

        // Step 3 -- the same for every live PART. 0x8260F2CC..0x8260F518. Every bound in this
        // half is 0x1E == 30 == mUsedParts' capacity.
        for ( s32 liPartIndex = mUsedParts.GetFirstNonZeroBit();
              liPartIndex != CgsContainers::BitArray<30>::KI_INVALID_BITINDEX;
              liPartIndex = mUsedParts.GetNextNonZeroBit( liPartIndex ) )
        {
            CgsSceneManager::TriangleCacheManagerIO::InEventRemoveFromCache lRemoveFromCacheEvent;
            lRemoveFromCacheEvent.miCacheSlot = KI_PROP_PART_CACHE_START_INDEX + liPartIndex;

            lpSceneInput->mRemoveFromCacheQueue.AddEvent( lRemoveFromCacheEvent );

            ++liDiagParts;   // [DIAG]
        }

        // [DIAG] NOT IN THE X360 BINARY -- emitted before the wipe so the counts describe what
        // was actually live. `jointsUsed` is read here rather than counted in a loop because
        // the joint bit-set is cleared by step 4 without ever being walked, which is itself
        // worth seeing: any jointed prop (lamppost/pole) loses its joint on unload with nothing logging it.
        if ( lbDiag )
        {
            --siDiagBudget;
            *CgsDev::Log::gpDebugPrint
                << "[Q6-rmall] world unload: props=" << liDiagProps
                << " parts=" << liDiagParts
                << " jointsUsed=" << static_cast<s32>( mUsedPropJoints.CountSetBits() )
                << " (one InRemoveAllRigidBodies owner=3 posted)\n";
        }

        // Step 4 -- the six zero stores at 0x8260F51C..0x8260F530, in emission order.
        mUsedProps.UnSetAll();
        mUsedParts.UnSetAll();
        muNumberOfPropInstances = 0;
        muNumberOfPartInstances = 0;
        mUsedPropJoints.UnSetAll();
        mBreakPropJoints.UnSetAll();
    }
}
}

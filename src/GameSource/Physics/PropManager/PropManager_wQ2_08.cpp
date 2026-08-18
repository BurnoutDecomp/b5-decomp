// GameSource/Physics/PropManager/PropManager_wQ2_08.cpp
//
// BrnPhysics::Props::PropManager -- breakable-props wave Q, ROUND 2, partfile 08.
// Reconstructed from BURNOUT_X360_ARTIST.XEX:
//   ProcessInputsPreScene @0x8263AF30   (209 instructions, 0x8263AF30..0x8263B270 --
//                                        COUNTED: (0x8263B270-0x8263AF30)/4 + 1 == 209)
//
// A slice of the TU's own BrnPropManager.cpp, split out per the wave's partfile convention
// (same spirit as BrnPropManager_PropInstanceQueries.cpp / PropManager_wQ_02.cpp).
//
// The second function of this group -- ApplyPropRaceCarCollisionImpulse @0x825E3560 -- is
// NOT here. Its body is complete and PARKED at
//   scratchpad/waveQ2/parked/PropManager_08_ApplyPropRaceCarCollisionImpulse.cpp
// because it needs two declarations this partfile may not add (both are named in that file's
// banner and in the round-2 report): RaceCarPhysics::AddPropCollisionImpulse(Vector3) and
// BrnPhysics::Props::KVF_MAX_PROP_SPEED_MPS. Nothing is stubbed in its place.
//
// ==========================================================================================
// WHAT THE 209 INSTRUCTIONS DO  (all offsets below are CONSOLE offsets, quoted from the asm;
// every one of them is reached BY NAME in the C++ -- AGENTS.md gotcha 1, they are meaningless
// on the LLP64 host).
//
// Register map, MEASURED from the prologue (0x8263AF3C..0x8263AF54):
//   r3 -> r17 = this
//   r4 -> r30 = lpInput                  (spilled to arg_1C at 0x8263AF58)
//   r5 -> r31 = lpSceneInput             (spilled to arg_24 at 0x8263AF64)
//   r6 -> stb arg_2F                     = lbSimPaused  (a bool, kept on the stack across the
//                                          whole body and re-read ONCE at 0x8263B254)
//   r7 -> r14 = lpSimModuleInputBuffer
//
// Body, in emission order:
//
//  1. 0x8263AF4C `addi r21, r17, 0x80` == &mUsedProps, then the fully inlined
//     BitArray<15>::GetFirstNonZeroBit / ::GetNextNonZeroBit / ::IsBitSet walk
//     (0x8263AF68..0x8263B1E4). Per live prop: `lwz r9,0x7C(r17)` == mpaPropInstances,
//     `mulli r11, r10, 0x70` == the 112-byte PropInstance stride, then
//     `lbz r9,0x6F(r11) ; clrrwi r9,r9,1 ; stb r9,0x6F(r11)` -- clearing bit 0 of
//     PropInstance::mu8Flags, i.e. PropInstance::ClearAddedThisFrameFlag().
//     The DecFIGS scope for this function (dwarfdump BrnPropManager.cpp:2285, source :299)
//     names exactly ONE local -- `int32_t liPropIndex` (source :302) -- and lists exactly the
//     three BitArray<15u> methods plus PropInstance::ClearAddedThisFrameFlag, which is what
//     the loop below spells.
//     ⚠️ The console emission carries an inlined bounds ASSERT ("invalid index : %u < 0xF",
//     CgsDev::Assert::BeginAssert/FireAssert/EndAssert at 0x8263B038..0x8263B130). Precisely
//     (round-2 NIT -- the earlier wording got both the justification and the attribution wrong):
//       * it is IsBitSet's OWN index precondition, inlined THROUGH GetNextNonZeroBit -- not
//         GetNextNonZeroBit's own -- and it prints the candidate index against tuNumBits == 15;
//       * `li r5, 0xCB` @0x8263B120 == 203 IS measured; "CgsBitArray.h" is an INFERENCE from the
//         truncated path string (0x8263AFEC `lis r11, aDP4B5MainBurno_51@ha`, which IDA renders
//         only as "d:\p45_mainurnout\main\code\g...");
//       * the committed CgsBitArray.h does NOT "own the bounds behaviour" -- its IsBitSet (:28-33)
//         has no check at all, and the header states it is deliberately assert-free ("so this stays
//         assert-free like the rest of the header").
//     The MEASURED reason not to spell it is stronger than the old one: the assert is UNREACHABLE
//     in this emission. r28 = min(wordBase + 64, 15) (0x8263B004 `clrrwi` / 0x8263B008 `addi 0x40` /
//     0x8263B010 `cmplwi 0xF` / 0x8263B020 `li r28,0xF`) and the inner-scan guard 0x8263B028
//     `cmplw r29,r28` + `bge` means r29 < 15 whenever 0x8263B030 `cmplwi r29,0xF ; blt` is
//     evaluated, so the assert block can never be entered. Behaviour is identical, provably.
//     (The function's xrefs_from therefore lists `CgsContainers::BasePriorityQueue::Clear`
//     @0x82815E58 -- called at 0x8263B04C, INSIDE that assert block. It is an ICF fold, not a
//     priority queue: the call is the assert message-stream reset. Noted so the next reader
//     does not chase a container this TU never touches.)
//
//  2. 0x8263B1E8..0x8263B20C -- the two REMOVE drains, both with (r4,r5,r6) ==
//     (lpInput, lpSceneInput, lpSimModuleInputBuffer).
//
//  3. 0x8263B210..0x8263B228 -- `lbz r11, 0x2C00(r30)` is PropInputInterface::
//     mbRemoveAllPropsAndParts (the committed accessor ShouldRemoveAllPropsAndParts(), whose
//     own home records that same +0x2C00). When set, RemoveAllPropsAndParts is called -- and
//     ⚠️ ITS ARGUMENT ORDER IS THE OTHER WAY ROUND from the four drains:
//     `mr r5,r31 ; mr r4,r14` == (lpSimModuleInputBuffer, lpSceneInput), which is exactly the
//     committed declaration's (lpSimInputBuffer, lpSceneInterface). Measured, not assumed.
//
//  4. 0x8263B22C..0x8263B250 -- the two ADD drains, same (r4,r5,r6) as the removes.
//
//  5. 0x8263B254..0x8263B268 -- `lbz r11,arg_2F ; cmplwi r11,0 ; bne <skip> ; bl
//     UpdateJointedProps` with `mr r4,r14`, i.e. `if (!lbSimPaused)
//     UpdateJointedProps(lpSimModuleInputBuffer);`. This single byte read is the ONLY use of
//     the third parameter in the whole body.
//
// ⚠️ ONE DWARF/ARTIST DIVERGENCE, STATED RATHER THAN SMOOTHED OVER. The DecFIGS scope for
// this function also lists InSceneUpdateInterface::RemoveCachedObject and the BitArray<30u>
// methods. Those belong to RemoveAllPropsAndParts, which the PS3 compiler INLINED here and
// which ARTIST emits as a real `bl` to 0x8260F010. The ARTIST form is what is reconstructed
// (rung 1 arbitrates); nothing from the PS3 inline expansion is written into this body.
//
// ⚠️ LINK-LEVEL, NOT VISIBLE TO `cl /c` (AGENTS.md gotcha 7/12). Re-derived 2026-08-18 --
// an earlier revision of this banner got the mounting the wrong way round; what follows was
// read out of tools/build/build_game_exe.bat, not assumed:
//   * ProcessInputsPreScene is ALSO defined -- inertly -- at
//     GameSource/Physics/BrnPhysicsConductorGates.cpp:206 (BRN_CONDUCTOR_GATE at :212).
//     That gates file IS MOUNTED (build_game_exe.bat:1177). What is not mounted is THIS
//     partfile -- no PropManager_wQ*.cpp appears anywhere in build_game_exe.bat -- which is
//     the only reason nothing is duplicated today. The moment this file is mounted the gate
//     at :206 becomes an LNK2005 duplicate and must be retired in the SAME commit. Reported,
//     NOT removed here: gate retirement is conductor-only.
//   * ALL SIX callees below are declared in BrnPropManager.h; NONE of the six has a body in
//     any MOUNTED translation unit, so compile-green here is NOT link-green. That framing is
//     unchanged; the ENUMERATION under it was wrong and is corrected here (round-2 MUST_FIX --
//     two rows claimed "no body anywhere" for functions that a sibling lander had bodied
//     minutes earlier, and the parenthetical had wQ2_06 parking the wrong function).
//     Re-grepped 2026-08-18 (`PropManager::<name>` over b5-decomp/src/**/*.cpp):
//       - BODIED, but only in UNMOUNTED part-files (four of the six):
//           ProcessAddPartInstanceEvents    @0x826280F8  PropManager_wQ_02.cpp
//           ProcessRemovePartInstanceEvents @0x82627818  PropManager_wQ_02.cpp
//           ProcessAddPropInstanceEvents    @0x82632108  PropManager_wQ2_06.cpp  (that file's own
//                                                        banner says "BODIED BELOW"; the function
//                                                        it PARKS is OutputUpdatedProps)
//           UpdateJointedProps              @0x82631260  PropManager_wQ2_05.cpp
//       - NO body anywhere and no gate -- so mounting the whole wave yields TWO genuine
//         unresolved externals, not four:
//           ProcessRemovePropInstanceEvents @0x82627778
//           RemoveAllPropsAndParts          @0x8260F010
//         Both are ALSO export holes (no per-address JSON; their names/addresses come from this
//         function's own xrefs_from), which is what makes them the real end of the closure.
//     ⚠️ Line numbers are deliberately omitted: these are concurrent landers' files and they
//     move. RE-VERIFY this list at mount time rather than trusting it -- it went stale once
//     already, inside a single afternoon.
// ==========================================================================================

#include "GameSource/Physics/PropManager/BrnPropManager.h"
#include "GameSource/Physics/PropManager/PropPhysics/BrnPropInstance.h"     // ClearAddedThisFrameFlag
#include "GameSource/Physics/PropManager/SharedIO/BrnPropInputInterface.h"  // ShouldRemoveAllPropsAndParts
#include "GameShared/GameClasses/Containers/CgsBitArray.h"                  // BitArray<15>::KI_INVALID_BITINDEX
#include "GameShared/GameClasses/Physics/CgsPhysicsSimulationModuleIO.h"    // InputBuffer (by pointer)
#include "GameShared/GameClasses/SceneManager/CgsSceneManagerIO_SceneUpdate.h" // InSceneUpdateInterface

namespace BrnPhysics
{
namespace Props
{
    // ======================================================================================
    // ProcessInputsPreScene  --  X360 0x8263AF30, 209 instructions.
    // DecFIGS: dwarfdump BrnPropManager.h:147 (declaration) / BrnPropManager.cpp:2285
    // (scope, source line :299). Parameter names and the sole local are the DWARF's.
    // ======================================================================================
    void
    PropManager::ProcessInputsPreScene(
        const PropInputInterface*                                lpInput,
        CgsSceneManager::SceneManagerIO::InSceneUpdateInterface* lpSceneInput,
        bool                                                     lbSimPaused,
        CgsPhysics::PhysicsSimulationIO::InputBuffer*            lpSimModuleInputBuffer )
    {
        // Step 1 -- clear the "added this frame" latch on every live prop. The console walks
        // mUsedProps with the inlined BitArray<15> iterators; re-rolled here into the named
        // calls (AGENTS.md "inlining reversal"). The loop guard is the console's own
        // `cmpwi r10,-1 ; bne` at 0x8263B1E0/0x8263B1E4.
        for ( s32 liPropIndex = mUsedProps.GetFirstNonZeroBit();
              liPropIndex != CgsContainers::BitArray<15>::KI_INVALID_BITINDEX;
              liPropIndex = mUsedProps.GetNextNonZeroBit( liPropIndex ) )
        {
            mpaPropInstances[ liPropIndex ].ClearAddedThisFrameFlag();
        }

        // Step 2 -- drain the two remove queues.
        ProcessRemovePropInstanceEvents( lpInput, lpSceneInput, lpSimModuleInputBuffer );
        ProcessRemovePartInstanceEvents( lpInput, lpSceneInput, lpSimModuleInputBuffer );

        // Step 3 -- the world-side "tear everything down" request (console `lbz 0x2C00`).
        if ( lpInput->ShouldRemoveAllPropsAndParts() )
        {
            RemoveAllPropsAndParts( lpSimModuleInputBuffer, lpSceneInput );
        }

        // Step 4 -- drain the two add queues.
        ProcessAddPropInstanceEvents( lpInput, lpSceneInput, lpSimModuleInputBuffer );
        ProcessAddPartInstanceEvents( lpInput, lpSceneInput, lpSimModuleInputBuffer );

        // Step 5 -- re-solve the jointed props, unless the simulation is paused.
        if ( !lbSimPaused )
        {
            UpdateJointedProps( lpSimModuleInputBuffer );
        }
    }
}
}

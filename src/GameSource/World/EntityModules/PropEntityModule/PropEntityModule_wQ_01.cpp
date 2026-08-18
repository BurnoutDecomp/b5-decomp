// ============================================================================
// b5-decomp/src/GameSource/World/EntityModules/PropEntityModule/PropEntityModule_wQ_01.cpp
//
// BrnWorld::PropEntityModule -- WAVE Q KEYSTONE, GROUP 1: the replay entry/exit ladder.
// (spec: scratchpad/waveQ/PropEntityModule.spec.md, "GROUPS" row 1.)
//
// Ledger TUs covered (ONE class, TWO ledger ids -- the conductor closes them separately):
//   * GameSource/Unity/../World/EntityModules/PropEntityModule/BrnPropEntityModule.cpp
//   * class:BrnWorld::PropEntityModule
//
// Functions in this file (X360 BURNOUT_X360_ARTIST.XEX):
//   PropEntityModule::PrepareForReplay    @0x822A94B0  (55 insns)
//   PropEntityModule::RestoreFromReplay   @0x822A95A8  (58 insns)
//   PropEntityModule::LeaveReplay         @0x822C4810  (66 insns)
//
// COUNTED, not copied: PrepareForReplay's export prints 56 lines of which 1 is the collapsed
// `.long` jump table (0x822A94EC..0x822A9500) -> 55; RestoreFromReplay 59 - 1 -> 58;
// LeaveReplay 66 with no jump table. (The wave spec's section-6 count column said 32/36/45
// and is unreliable -- reported to the conductor, not inherited here.)
//
// All three are X360-ONLY: the DecFIGS (Dec-2007 PS3) DWARF declares none of them, and
// neither does the Feb-2007 leaked source -- both predate the prop replay path. That same
// merge-window delta is why the members they walk (miReplayState, mbStreamingSettled,
// muReplayPropsInScene, muReplayPartsInScene) are the header's X360-only block. Every
// statement below is therefore grounded on the RAW ASSEMBLY of the three per-address
// exports, never on the DWARF and never on Hex-Rays (which renders all three as `int`
// blobs and gives LeaveReplay a phantom `int result` first parameter).
//
// ---- WHAT IS MEASURED vs WHAT IS INFERENCE -------------------------------------------
// MEASURED (read off the asm listing, instruction by instruction):
//   * the whole state ladder: every case value, every store, every return value;
//   * the member offsets touched -- 0xD3330 miReplayState, 0xD3200 meStreamingMode,
//     0xD3204 mbStreamingSettled, 0xD320C muNumberOfLoadedZones, 0xD3338/0xD333C the two
//     replay-in-scene counters. These agree with the block documented in
//     BrnPropEntityModule.h, which the wave-Q spec re-verified against exactly these three
//     bodies (spec section 3.4);
//   * the two assert sites and their console line numbers (0x745 == 1861, 0x774 == 1908);
//   * LeaveReplay's scene-entity id arithmetic and its RemoveEntity(id, 0) flag word.
// INFERENCE (named as such where it appears):
//   * that meStreamingMode's stored `2` is E_RESET_UNLOADING -- the asm stores the raw
//     word 2; the enumerator name comes from the committed EPropStreamingMode in
//     BrnPropEntityModule.h. Value-identical either way.
//   * that the id's part field 0/1 is the Feb-2007 file-local { E_PROP, E_PART } (see the
//     enum below). The asm only shows the literals 0 and 1.
//
// ---- CONSOLE-LITERAL DISCIPLINE (gotcha 1) -------------------------------------------
// Not one console byte offset appears in the code. Every one is reached by named member.
// The only numeric literals that survive are (a) the miReplayState case values, which are
// a state machine's own alphabet and not addresses, and (b) the scene-entity OWNER byte
// 0x22, which is a packed-field VALUE, not an offset -- and it is passed through
// CgsSceneManager::EntityId::Set so the host does the packing (see LeaveReplay).
//
// NaN polarity (gotcha 4): NOT APPLICABLE. There is not one floating-point instruction in
// any of the three functions -- no fcmpu, no fsel, no FPR is touched.
// ============================================================================

#include "GameSource/World/EntityModules/PropEntityModule/BrnPropEntityModule.h"
#include "GameSource/World/EntityModules/PropEntityModule/BrnPropEntityModuleIO.h"   // PropEntityIO::OutputBuffer_PreScene

#include "GameShared/GameClasses/Core/CgsAssert.h"                                   // CGS_ASSERT
#include "GameShared/GameClasses/SceneManager/CgsEntityId.h"                         // CgsSceneManager::EntityId
#include "GameShared/GameClasses/SceneManager/CgsSceneManagerIO_SceneUpdate.h"       // InSceneUpdateInterface

namespace BrnWorld
{
    namespace
    {
        // The prop/part discriminator that rides the scene-entity id's 10-bit PART field.
        // GROUND TRUTH: the Feb-2007 leaked source declares exactly this unnamed file-scope
        // enum at references/Feb-2007/BrnEntityModuleUnity/GameSource/World/EntityModules/
        // PropEntityModule/BrnPropEntityModule.cpp:41-44 (`enum { E_PROP = 0, E_PART = 1 };`)
        // and uses E_PART as an EntityId part index at :894. The X360 bodies below emit the
        // folded literals 0 and 1 only, so the *names* are the Feb-2007 source's, the values
        // are the asm's. Kept file-local (anonymous namespace) exactly as the original is
        // file-local -- it must not leak into the class or another TU.
        enum EPropScenePartField
        {
            E_PROP = 0,
            E_PART = 1
        };

        // The scene-entity OWNER byte the replay prop path uses.
        //
        // ⚠️ UNNAMED IN THE TREE, DELIBERATELY LEFT AS A LITERAL. GameSource/World/
        // BrnEntityTypes.h names E_ENTITYTYPE_FIRST_REPLAY_TYPE = 32 and
        // E_ENTITYTYPE_REPLAY_RACECAR = 33, then stops at E_ENTITYTYPE_COUNT = 34 -- so 34
        // (0x22) is a replay-prop owner that the enum home does not name, and it COLLIDES
        // with _COUNT, which is used as a bound elsewhere. BrnEntityTypes.h is outside this
        // lane's ownership, so naming it there is a shared-header request (spec section 8,
        // item 2), not an edit made here. Until that lands, the literal + this citation is
        // the honest spelling.
        //
        // MEASURED at 0x822C4878 (`oris r30, r11, 0x2200` -> owner byte 0x22 in bits 31..24)
        // and at 0x822C48B4/B8 (`lis r11, 0x2200 ; ori r28, r11, 1` -> 0x22000001, i.e. the
        // same owner with part field 1). The same owner appears in ReplayUpdatePropsInScene
        // @0x822DB370 and ReplayUpdatePartsInScene @0x822DB900 (wave-Q group 3).
        const u32 KU_REPLAY_PROP_SCENE_OWNER = 0x22u;
    }

    // ------------------------------------------------------------------------------------
    // PropEntityModule::PrepareForReplay  @0x822A94B0  (55 insns, counted)
    //
    // Called only from PostPhysicsUpdate @0x823031D8. Drives the module from live play into
    // replay playback: it asks the streaming system to tear the live prop population down
    // (meStreamingMode = E_RESET_UNLOADING) and answers `true` only once the world has
    // actually settled, so the caller can spin on it across frames.
    //
    // ASM (jump table @0x822A94EC, 6 cases, `cmplwi r9,5 ; bgt default`):
    //   r11 = this ; r10 = this + 0xD3330 (miReplayState) ; r9 = *r10
    //   cases 0,1,4,5 (0x822A9504): stwx 2 -> this+0xD3200 (meStreamingMode)
    //                               stw  2 -> 0(r10)       (miReplayState)
    //                               li r3,0                -> return false
    //   case 2 (0x822A952C): lbzx r9, this+0xD3204 (mbStreamingSettled)
    //                        beq -> 0x822A9590 (li r3,0 ; return false)
    //                        lwzx r11, this+0xD320C (muNumberOfLoadedZones)
    //                        cmplwi r11,1 ; ble -> 0x822A9590 (return false)
    //                        stw 3 -> 0(r10) ; fall into case 3
    //   case 3 (0x822A955C): li r3,1 -> return true
    //   default (0x822A9570): assert "Shouldn't get here",
    //                         BrnPropEntityModule.cpp:1861 (r5 = 0x745), then return false.
    //
    // ⚠️ The `cmplwi r11, 1 ; ble` on muNumberOfLoadedZones is an UNSIGNED compare against
    // the immediate 1, i.e. `zones <= 1u` -> bail. Written below as `<= 1u` on the u32
    // member, which is the same test. (No float is involved, so gotcha 4 does not apply.)
    // ------------------------------------------------------------------------------------
    bool PropEntityModule::PrepareForReplay()
    {
        switch (miReplayState)
        {
        case 0:
        case 1:
        case 4:
        case 5:
            // INFERENCE (name only): the asm stores the raw word 2; EPropStreamingMode's
            // committed E_RESET_UNLOADING == 2 (BrnPropEntityModule.h). Value-identical.
            meStreamingMode = E_RESET_UNLOADING;
            miReplayState   = 2;
            return false;

        case 2:
            // Not settled yet, or the world is still down to a single loaded zone: stay in
            // state 2 and answer "not ready". Both legs branch to the SAME `li r3,0` exit.
            if (!mbStreamingSettled || muNumberOfLoadedZones <= 1u)
            {
                return false;
            }
            miReplayState = 3;
            return true;

        case 3:
            // Terminal "ready" state -- no store, just the answer (the asm falls into the
            // case-2 tail's `li r3,1` at 0x822A955C).
            return true;

        default:
            // X360: CgsDev::Assert::FireAssert("Shouldn't get here",
            //   "d:\p4\b5_main\burnout\main\code\gamesource\unity\../World/EntityModules/
            //    PropEntityModule/BrnPropEntityModule.cpp", 1861)
            CGS_ASSERT(false, "Shouldn't get here");
            return false;
        }
    }

    // ------------------------------------------------------------------------------------
    // PropEntityModule::RestoreFromReplay  @0x822A95A8  (58 insns, counted)
    //
    // Called only from PostPhysicsUpdate @0x823031D8. The mirror of the above: it walks the
    // module back out of replay playback and answers `true` when the live world is usable
    // again. NOTE it is NOT a symmetric copy -- the ladder is a different shape (state 0 is
    // the terminal "already live" answer here, and there is no muNumberOfLoadedZones gate).
    //
    // ASM (jump table @0x822A95E4, 6 cases, `cmplwi r9,5 ; bgt default`):
    //   r10 = this ; r11 = this + 0xD3330 (miReplayState) ; r9 = *r11
    //   case 0   (0x822A95FC): li r3,1 -> return true
    //   cases 1-3(0x822A9610): stw 4 -> 0(r11)   ... and FALLS THROUGH into case 4
    //   case 4   (0x822A9618): stwx 2 -> this+0xD3200 (meStreamingMode)
    //                          stw  5 -> 0(r11)  (miReplayState)
    //                          li r3,0 -> return false
    //   case 5   (0x822A9644): lbzx r10, this+0xD3204 (mbStreamingSettled)
    //                          beq -> 0x822A9694 (li r3,0 ; return false)
    //                          stw 0 -> 0(r11) ; li r3,1 -> return true
    //   default  (0x822A9674): assert "Shouldn't get here",
    //                          BrnPropEntityModule.cpp:1908 (r5 = 0x774), then return false.
    //
    // ⚠️ The cases-1-3 -> case-4 FALL-THROUGH is real, not a decompiler artifact: the
    // `stw r9(4), 0(r11)` at 0x822A9614 is immediately followed in address order by case 4's
    // first instruction at 0x822A9618 with no branch between them. The redundant-looking
    // `miReplayState = 4` then `miReplayState = 5` pair is what the shipped code does.
    // ------------------------------------------------------------------------------------
    bool PropEntityModule::RestoreFromReplay()
    {
        switch (miReplayState)
        {
        case 0:
            return true;

        case 1:
        case 2:
        case 3:
            miReplayState = 4;
            // FALL THROUGH -- measured, see the banner above.
            [[fallthrough]];

        case 4:
            // INFERENCE (name only): raw word 2 == E_RESET_UNLOADING, as in PrepareForReplay.
            meStreamingMode = E_RESET_UNLOADING;
            miReplayState   = 5;
            return false;

        case 5:
            if (!mbStreamingSettled)
            {
                return false;
            }
            miReplayState = 0;
            return true;

        default:
            // X360: FireAssert("Shouldn't get here", <same path>, 1908)
            CGS_ASSERT(false, "Shouldn't get here");
            return false;
        }
    }

    // ------------------------------------------------------------------------------------
    // PropEntityModule::LeaveReplay  @0x822C4810  (66 insns, counted)
    //
    // Called only from ReplayPreSceneUpdate @0x822EF878 (wave-Q group 3). Evicts every scene
    // entity the replay playback path published -- first the props, then the parts -- and
    // zeroes both published counts so the next replay entry starts from an empty population.
    //
    // ASM:
    //   r28 = this ; r26 = r4 (lpOutput) ; r29 = 0 (the constant zero reused as the loop
    //   seed, as the RemoveEntity flags word, and as both final stores)
    //   r25 = this + 0xD3338 (muReplayPropsInScene)
    //   loop 1 (0x822C4850..0x822C4898), entry guard `lwz r11,0(r25) ; cmpwi r11,0 ; ble`:
    //       cmplwi r31, 0x4000 ; blt -> skip assert
    //       assert "luEntityIndex < (1U << KU_NUM_BITS_FOR_ENTITY_NUM)", console CgsEntityId.h:116
    //       slwi r11, r31, 10 ; oris r30, r11, 0x2200        == (i << 10) | 0x22000000
    //       mr r3, r26 ; bl sub_822B9738                     == OutputBuffer_PreScene::
    //                                                           GetSceneInputInterface()
    //       mr r5, r29(0) ; mr r4, r30 ; bl InSceneUpdateInterface::RemoveEntity
    //       lwz r11, 0(r25) ; addi r31,r31,1 ; cmpw r31,r11 ; blt  (bound RE-READ each pass)
    //   loop 2 (0x822C48BC..0x822C4904): identical, over r27 = this + 0xD333C
    //       (muReplayPartsInScene), with r28 preloaded to 0x22000001 and the id built as
    //       `slwi r11,r31,10 ; or r30,r11,r28`   == (i << 10) | 0x22000001
    //   tail (0x822C4908): stw r29(0), 0(r25) ; stw r29(0), 0(r27)
    //
    // WHY THE ID GOES THROUGH EntityId::Set RATHER THAN THE PACKED LITERAL:
    // CgsSceneManager::EntityId is owner[31..24] | entityIndex[23..10] | partIndex[9..0]
    // (CgsEntityId.h, pinned on EntityId::Set @0x82277330), so
    // `Set(0x22, i, E_PROP)` == `(i << 10) | 0x22000000` and `Set(0x22, i, E_PART)` ==
    // `(i << 10) | 0x22000001` -- exactly the two words the asm builds. Set() also carries
    // the three tripwires; the shipped code emits only the entityIndex one because the other
    // two arguments are compile-time constants there (partIndex 0/1 < 0x400, owner
    // 0x22 < 128), so the compiler folded them. Routing through Set() reproduces that: the
    // same one assert survives, from the same header line (console CgsEntityId.h:116 --
    // the reconstructed header carries that assert at CgsEntityId.h:69-70; :116 there is the
    // unrelated mId assignment inside SetPartIndex).
    //
    // ⚠️ SIGNED LOOP COMPARE, MEASURED. Both the entry guard (`cmpwi r11,0 ; ble`) and both
    // back-edges (`cmpw r31,r11 ; blt`) are SIGNED compares -- an unsigned counter against
    // an unsigned bound would have compiled to cmplwi/cmplw. The two counters are declared
    // u32 in the committed header (this lane may not edit it), so the cast below preserves
    // the shipped comparison exactly rather than silently widening it. It is NOT decoration:
    // drop it and a count with the top bit set would iterate instead of being skipped.
    // (Unreachable in practice -- the entityIndex field is only 14 bits wide -- but the
    // faithful thing is to keep the compare the compiler actually emitted.)
    // ------------------------------------------------------------------------------------
    void PropEntityModule::LeaveReplay(PropEntityIO::OutputBuffer_PreScene* lpOutput)
    {
        // Pass 1 -- the replay props.
        for (s32 liProp = 0; liProp < static_cast<s32>(muReplayPropsInScene); ++liProp)
        {
            CgsSceneManager::EntityId lPropEntityId;
            lPropEntityId.Set(KU_REPLAY_PROP_SCENE_OWNER, static_cast<u32>(liProp), E_PROP);

            // The X360 re-fetches the write-locked scene interface INSIDE the loop (one
            // `bl sub_822B9738` per iteration, 0x822C487C); kept, because that getter is
            // also the buffer's "locked for writing" tripwire.
            lpOutput->GetSceneInputInterface()->RemoveEntity(lPropEntityId, 0);
        }

        // Pass 2 -- the replay parts. Same shape, different counter and part field.
        for (s32 liPart = 0; liPart < static_cast<s32>(muReplayPartsInScene); ++liPart)
        {
            CgsSceneManager::EntityId lPartEntityId;
            lPartEntityId.Set(KU_REPLAY_PROP_SCENE_OWNER, static_cast<u32>(liPart), E_PART);

            lpOutput->GetSceneInputInterface()->RemoveEntity(lPartEntityId, 0);
        }

        muReplayPropsInScene = 0;
        muReplayPartsInScene = 0;
    }
}

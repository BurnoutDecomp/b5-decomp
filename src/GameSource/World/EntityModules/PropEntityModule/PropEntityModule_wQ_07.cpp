// ============================================================================
// b5-decomp/src/GameSource/World/EntityModules/PropEntityModule/PropEntityModule_wQ_07.cpp
//
// BrnWorld::PropEntityModule -- WAVE Q KEYSTONE, GROUP 7: THE PRE-PHYSICS TICK and
// THE BROKEN-PROP RETIREMENT. (spec: scratchpad/waveQ/PropEntityModule.spec.md,
// "GROUPS" row 7.)
//
// Ledger TUs covered (ONE class, TWO ledger ids -- the conductor closes them separately):
//   * GameSource/Unity/../World/EntityModules/PropEntityModule/BrnPropEntityModule.cpp
//   * class:BrnWorld::PropEntityModule
//
// Functions in this file (X360 BURNOUT_X360_ARTIST.XEX):
//   PropEntityModule::ProcessBrokenProps        @0x822EEFA0  (363 insns)   [ledger]
//   PropEntityModule::PrePhysicsUpdate          @0x82303048  (100 insns)   [ledger]
//   PropCellManager::GetPhysicalPropSlot        @0x822E11C0  (NON-ledger; declared in
//                                                BrnPropCellManager.h since the keystone,
//                                                never bodied -> the group link-fails
//                                                without it. Spec section 1, the
//                                                "ledger-reviewed != implemented" table.)
//   PropCellManager::GetPhysicalPartSlot        @0x822E0DB0  (same, and it is the callee
//                                                ProcessBrokenProps' inner loop turns on)
//
// PrePhysicsUpdate is the DRIVER of ProcessBrokenProps (and of group 5's
// ProcessPotentialContacts); both decode InputBuffer_PrePhysics, which is why they pair.
//
// ---- GROUND TRUTH ---------------------------------------------------------------------
// All four bodies are reconstructed from the RAW ASSEMBLY of their per-address ARTIST
// exports, read instruction by instruction:
//   .ida-exports/BURNOUT_X360_ARTIST.XEX/0x822EEFA0.json
//   .ida-exports/BURNOUT_X360_ARTIST.XEX/0x82303048.json
//   .ida-exports/BURNOUT_X360_ARTIST.XEX/0x822E11C0.json
//   .ida-exports/BURNOUT_X360_ARTIST.XEX/0x822E0DB0.json
// The Hex-Rays pseudocode was used ONLY to read the assert string literals out in full and
// to cross-check the branch structure. It is wrong about the shapes -- it renders
// ProcessBrokenProps as `_DWORD *(_DWORD *, int, int)` and PrePhysicsUpdate as
// `int(int,int,int,_DWORD*,int,char)`, and it drops the second argument of
// PropZoneManager::GetProp/GetPart entirely. Declaration shapes are the DecFIGS DWARF ones
// already committed in BrnPropEntityModule.h (:1430 for ProcessBrokenProps) and
// BrnPropCellManager.h.
//
// ---- MEASURED vs INFERENCE ------------------------------------------------------------
// MEASURED (read off the asm listing, per function):
//   * ProcessBrokenProps: the whole control flow -- the Set walk over maRecentlyBrokenProps
//     (asserts at CgsSet.h:227/:274/:275, count word at this+0xCDE64), the `state == 5`
//     early-continue at 0x822EF184, the two smashable/smashed tripwires at :1470/:1471, the
//     slot release + RemovePropInstance + `--miNumPropsInSim` triple, the 1-based part-id
//     seed (`insrwi r27, 1, 10, 22` at 0x822EF2CC), the eviction leg, the VMX part-transform
//     cascade at 0x822EF3F4..0x822EF454, and the terminal `maRecentlyBrokenProps.Clear()`
//     (`stwx 0, this, 0xCDE64` at 0x822EF540).
//   * PrePhysicsUpdate: the two locks, the `lUpdateSet & 1` split at 0x82303074, the
//     paused leg's `miUpdatesSinceLastSimPause = 0` + potential-contact tripwire, the
//     running leg's SendTrafficLightRestoreEvents / reset-queue drain / ProcessBrokenProps /
//     ProcessPotentialContacts / `++miUpdatesSinceLastSimPause`, and the two unlocks.
//   * Both slot brokers: the find-first-clear-bit search, the >= capacity and == -1 escapes
//     into the eviction scan, the "largest mfTimeInSim wins" selection with its exact
//     `fcmpu` + `blt` polarity, and every store on all four exit paths.
// INFERENCE (each marked where it appears):
//   * the NAME KU_UPDATESET_SIM_PAUSED for update-set bit 0 (the bit itself is measured).
//   * that `if (state == 5) continue;` is `GetState() == E_MOVED` -- the asm compares the
//     raw byte 5 against a value that has just passed GetState()'s own
//     "mu8State < E_STATE_COUNT" tripwire; E_MOVED == 5 in the committed EPropState.
//   * that `lbz 0x4A; rlwinm 0,28,28` is PropEntityInstance::IsSmashable(). The assert
//     STRING is "lpInstance->IsSmashable()" verbatim, and mask 0x8 == KU_SMASHABLE_BIT,
//     but this class has no IsSmashable() member in the committed header, so the predicate
//     is spelled as the flag test -- exactly the idiom BrnPropCellManager.cpp:512/:558
//     already uses for the sibling IsAddedToScene().
//
// ---- CONSOLE-LITERAL DISCIPLINE (gotcha 1) -------------------------------------------
// Not one console byte offset, stride or object size appears in the code below. The traps
// this group carries, all defused:
//   (1) ProcessBrokenProps walks the part descriptors with `r24 += 0x30` (0x822EF4F4).
//       48 is the CONSOLE sizeof(PropPartTypeData); on the host it is 64. The loop below
//       subscripts `lpType->GetParts()[luPart]` and lets the host compiler pick the stride.
//   (2) Every queue length in the asm is `lwz r11, 8(queue)`. 8 is the CONSOLE offset of
//       BaseEventQueue<T>::miLength (mpEvents 4 + miMaxLength 4); on the host mpEvents
//       widens to 8 bytes and miLength moves to +0x10. Every length below goes through
//       GetLength().
//   (3) The two slot brokers address maPhysicalPropParams / maPhysicalPartParams as
//       `this + 8*(i + 0xF1)` / `this + 8*(i + 0x100)` and their time fields as
//       `+0x78C` / `+0x804`. Those are the CONSOLE offsets of members that sit AFTER the
//       two widening member pointers mpaProps/mpaPropParts (BrnPropCellManager.h's banner
//       says so explicitly), so they do not hold on the host at all. Both bodies subscript
//       the named arrays.
//   (4) The bit-array words are read with `ld`/`std` at `this + 0x778` / `+0x780` and the
//       shift is `1 << (i & 0x3F)`. Reached below through CgsContainers::BitArray's own
//       GetFirstClearBit / SetBit / IsBitSet, whose committed bodies are that same bit math
//       (GetFirstClearBit's banner cites the identical `~field` + lowest-set-bit idiom).
//   (5) The eviction miss writes the literal word 0x03000000 into the out-parameter. That
//       is a PropEntityID whose OWNER byte is E_ENTITYTYPE_PROP and whose index/part fields
//       are zero -- the compiler's fold of a default-constructed prop handle. Spelled below
//       through PropEntityID::KU_OWNER_BASE + E_ENTITYTYPE_PROP, the same way
//       BrnPropZoneManager.cpp:1092 and BrnPropEntityDebugComponent.cpp:446 already spell it.
//   The only numeric literals that survive are population deltas (the `1` in
//   Increment/DecrementNumberOf*InSim) and the part-index seed `1`.
//
// ---- POOL-CAPACITY SPELLING (changed 2026-08-18, round 2) ------------------------------
// The two slot brokers are PropCellManager members, and they now bound their loops and
// qualify their BitArrays with the CLASS'S OWN constants -- PropCellManager::
// KU_MAX_PHYSICAL_PROPS (15) and ::KU_MAX_PHYSICAL_PARTS (30), BrnPropCellManager.h:140-141,
// the same symbols that size `BitArray<15u> mPhysicalProps` / `maPhysicalPropParams[15]` /
// `BitArray<30u> mPhysicalParts` / `maPhysicalPartParams[30]`. Round 1 used
// BrnPhysics::Props::KU_MAX_PHYSICAL_PROPS / KU_MAX_PHYSICAL_PROP_PARTS
// (BrnPropInputInterface.h:52/:61) instead: all three spellings agree at 15/30 today, but
// that pair is explicitly marked there as temporary squatters scheduled to MOVE to
// BrnPropConstants.h, so a loop bound and an array extent were coupled through different
// headers -- a pool resized through one spelling only is a silent out-of-bounds walk over
// maPhysicalPartParams. The round-2 owner added the four static_asserts that tie the three
// spellings together (BrnPropCellManager.cpp:139-147), so a drift now fails to compile.
//
// ---- NaN POLARITY (gotcha 4) ----------------------------------------------------------
// ProcessBrokenProps and PrePhysicsUpdate: NOT APPLICABLE -- neither contains a single
// floating-point compare (ProcessBrokenProps' only FP work is the VMX multiply-add cascade).
// The two slot brokers: exactly one compare each, the recycle scan's
//     0x822E1524  fcmpu cr6, f0, f31        (f0 == maPhysical*Params[i].mfTimeInSim,
//     0x822E1528  blt   cr6, <skip update>   f31 == the running best, seeded 0.01f)
// `blt` after fcmpu is bc 12,LT -- taken ONLY on an ordered less-than, so an UNORDERED
// (NaN) compare falls through and TAKES the update. The faithful C++ is therefore
// `if ( !(lfTimeInSim < lfBest) )`, which is likewise true for NaN. Writing
// `if (lfTimeInSim >= lfBest)` would be WRONG: C++ `>=` is false for NaN and would skip a
// slot the console selects. (This is the mirror image of the bge/ble case in the gotcha
// list -- there the negation is needed because the branch IS taken on unordered; here it is
// needed because the branch is NOT.)
// ============================================================================

#include "GameSource/World/EntityModules/PropEntityModule/BrnPropEntityModule.h"
#include "GameSource/World/EntityModules/PropEntityModule/BrnPropEntityModuleIO.h"
#include "GameSource/World/EntityModules/PropEntityModule/BrnPropZoneManager.h"
#include "GameSource/World/EntityModules/PropEntityModule/BrnPropCellManager.h"
#include "GameSource/World/EntityModules/PropEntityModule/BrnPropEntityInstance.h"

#include "SharedClasses/Physics/Props/BrnPropEntityID.h"
#include "SharedClasses/Physics/Props/BrnPhysicsPropTypeData.h"
#include "SharedClasses/Physics/Props/BrnPropPhysicsDataHeader.h"

#include "GameSource/Physics/PropManager/SharedIO/BrnPropInputInterface.h"
#include "GameSource/World/AI/SharedIO/BrnAIModuleResultInterface.h"   // ResetOnTrackResult

#include "GameShared/GameClasses/Containers/CgsArray.h"
#include "GameShared/GameClasses/Containers/CgsSet.h"
#include "GameShared/GameClasses/Containers/CgsBitArray.h"
#include "GameShared/GameClasses/Module/CgsEventQueue.h"
#include "GameShared/GameClasses/Module/CgsIOBufferStack.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"

// Vector3 operator* / operator+ -- the part-transform cascade in ProcessBrokenProps. Same
// include, for the same reason, as PropEntityModule_wQ_04.cpp and BrnPropCellManager.cpp.
#include "rw/math/vpu/vector3_operation.h"

#include "GameShared/GameClasses/Development/Log/CgsLog.h"   // gpDebugPrint ([DIAG] BRN_PROP_DIAG)
#include <stdlib.h>                                          // getenv    ([DIAG] BRN_PROP_DIAG)

namespace BrnWorld
{
    namespace
    {
        // ------------------------------------------------------------------
        // Update-set bit 0.
        //
        // MEASURED: PrePhysicsUpdate splits on it at 0x82303074 with
        //     clrlwi r11, r30, 31 ; cmplwi cr6, r11, 0 ; bne cr6, <bit-set leg>
        // and the BIT-SET leg is the one that resets miUpdatesSinceLastSimPause and fires
        // "Received potential contacts in a paused update". So bit 0 SET == the simulation
        // did not step this frame.
        //
        // CORROBORATED, not invented: the committed WorldModule::Update already tests the
        // same bit and skips SceneModule::UpdateContactGeneration when it is set
        // (BrnWorldModule.cpp:2610, `if ( ( lUpdateSet & 1 ) == 0 )`) -- i.e. with bit 0 set
        // no contact generation runs, which is exactly why receiving potential contacts is
        // an assertable condition here. BrnPropEntityModule_PreScene.cpp's park P6 quotes a
        // third consumer of the same bit in this very class.
        //
        // ⚠️ INFERENCE: only the NAME. No enumerator for this bit exists in any committed
        // header and no assert string spells one; the name below describes the measured
        // role and nothing more. The DWARF-attested spellings that DO exist for other bits
        // live at their own call sites (KU_UPDATESET_RESOURCE_SYSTEM_STALLED == 0x400 in
        // BrnPropEntityModule_PreScene.cpp:289, KU_UPDATESET_USE_REPLAY_INTERFACE == 0x100
        // in WorldBridgeRaceCarToPropModule.cpp:96), and this follows their convention.
        const BrnUpdateSet KU_UPDATESET_SIM_PAUSED = 0x1;

        // ------------------------------------------------------------------
        // The seed of the recycle scan's running maximum in BOTH slot brokers
        // (0x822E0E14 and 0x822E1224, `lfs f31, flt_82002138`).
        //
        // VALUE MEASURED: flt_82002138 == 0.01f. It is a shared rodata float, decoded
        // identically (0.0099999998) in eight other ARTIST exports' pseudocode -- e.g.
        // BrnDirector::Camera::Utils::Looker::Parameters::Construct @0x821F8D80 and
        // CollisionPolicyAttachedToVehicle::UpdateRadius @0x8220E4D0 -- so the reading does
        // not rest on this one function's decompilation.
        //
        // ROLE MEASURED: the scan keeps the slot with the LARGEST mfTimeInSim, and this is
        // the initial best. A pool whose every resident has been in the sim for less than
        // 0.01 s therefore yields NO recycle candidate and the broker fails -- i.e. a prop
        // or part cannot be evicted in the same ~frame it was admitted.
        //
        // ⚠️ INFERENCE: only the NAME. No assert string or symbol attests one.
        const f32 KF_MINIMUM_TIME_IN_SIM_BEFORE_RECYCLE = 0.01f;

        // ------------------------------------------------------------------
        // The seed value the two slot brokers write into their `evicted id` out-parameter
        // when nothing was evicted: `lis rN, 0x300` == 0x03000000.
        //
        // That is a PropEntityID with owner E_ENTITYTYPE_PROP (== 3) at KU_OWNER_BASE and a
        // zero entity/part payload -- the compiler's fold of a default-constructed prop
        // handle, the same fold BrnPropZoneManager.cpp:1025 documents for the sibling
        // PropVolumeInstanceID. PropEntityID's committed default constructor is trivial (it
        // does NOT seed the owner byte), so the seed is written explicitly here rather than
        // relying on the declaration.
        //
        // NOTE the value is deliberately NOT built with the explicit PropEntityID(u32)
        // constructor: that one is a real out-of-line X360 symbol (0x822B7888) with an
        // owner tripwire, and the asm calls no such thing here -- it materialises a
        // constant.
        PropEntityID MakeDefaultPropEntityId()
        {
            PropEntityID lId;
            lId.mEntityId.muValue =
                static_cast<u32>( E_ENTITYTYPE_PROP ) << PropEntityID::KU_OWNER_BASE;
            return lId;
        }
    }

    // ========================================================================
    // PropCellManager::GetPhysicalPropSlot   @ 0x822E11C0
    // ------------------------------------------------------------------------
    // ⭐ NON-LEDGER, NEW 2026-08-18 (wave Q keystone, group 7). Declared in
    // BrnPropCellManager.h, bodied nowhere in the tree until now; ChangePropState (group 4,
    // via the PropZoneManager forwarder) and every future physical-prop promotion is an
    // unresolved external without it. `cl /c` cannot see that, which is exactly why the
    // spec lists it under "ledger-`reviewed` != implemented".
    //
    // Claim a slot in the 15-entry physical-prop pool for lEntityId:
    //   * if a slot is free, take it, seed its bookkeeping, and report "no eviction";
    //   * otherwise EVICT the prop that has been resident LONGEST (largest mfTimeInSim),
    //     hand its id back to the caller and re-seat the slot on lEntityId;
    //   * if the pool is full AND no resident qualifies, fail with a -1 slot.
    //
    // SIGNATURE (prologue 0x822E11D0..0x822E11E8, measured): r3 this, r4 lEntityId,
    // r5 -> lriSlot, r6 -> lrEvictedId, r7 -> lrbEvicted. That is the committed
    // declaration exactly.
    //
    // ⚠️ MEASURED ODDITY, reproduced: the console does NOT clear the pool bit on the
    // eviction path (it is already set) and it does NOT touch mfTimeInSim on the failure
    // path. Both are as-shipped.
    // ========================================================================
    bool PropCellManager::GetPhysicalPropSlot( PropEntityID lEntityId, s32& lriSlot,
                                               PropEntityID& lrEvictedId, bool& lrbEvicted )
    {
        // 0x822E11EC..0x822E1328. The console open-codes find-first-clear-bit over
        // mPhysicalProps' single 64-bit field (`ld` / `cmpdi -1` to skip an all-ones field,
        // then `not` + isolate-lowest-set-bit + `cntlzd`), and treats BOTH "ran past the
        // capacity" (`cmpwi r31, 0xF ; bge`) and "came back -1" (`cmpwi r31, -1 ; beq`) as
        // "no free slot". BitArray<15>::GetFirstClearBit is that same idiom and collapses
        // both escapes into its KI_INVALID_BITINDEX return.
        const s32 liFreeSlot = mPhysicalProps.GetFirstClearBit();

        if ( liFreeSlot != CgsContainers::BitArray<KU_MAX_PHYSICAL_PROPS>::KI_INVALID_BITINDEX )
        {
            // 0x822E132C -- SetBit's own bounds tripwire (CgsBitArray.h:222). Its X360
            // message is StrStream-built ("Index: " << i << ", Number of bits: " << 15);
            // collapsed to a plain CGS_ASSERT per this TU's convention, literal verbatim.
            CGS_ASSERT( static_cast<u32>( liFreeSlot ) < KU_MAX_PHYSICAL_PROPS,
                        "Index: " );

            // 0x822E13DC..0x822E1438, in asm store order.
            mPhysicalProps.SetBit( static_cast<u32>( liFreeSlot ) );
            maPhysicalPropParams[liFreeSlot].mfTimeInSim = 0.0f;
            maPhysicalPropParams[liFreeSlot].mEntityId   = lEntityId;

            lriSlot     = liFreeSlot;
            lrEvictedId = MakeDefaultPropEntityId();
            lrbEvicted  = false;
            return true;
        }

        // ---- the pool is full: pick the longest-resident slot ------------------------
        // 0x822E1210..0x822E1544. f31 is seeded from flt_82002138 == 0.01f, so a slot whose
        // mfTimeInSim has not yet reached 0.01 s is never recycled.
        f32 lfLongestTimeInSim = KF_MINIMUM_TIME_IN_SIM_BEFORE_RECYCLE;
        s32 liRecycleIndex     = -1;

        for ( u32 luSlot = 0; luSlot < KU_MAX_PHYSICAL_PROPS; ++luSlot )
        {
            // 0x822E1274 -- IsBitSet's bounds tripwire (CgsBitArray.h:203), StrStream-built
            // ("invalid index : " << i << " < " << 15); collapsed, literal verbatim. It is
            // unreachable with this loop bound, exactly as in the console.
            CGS_ASSERT( luSlot < KU_MAX_PHYSICAL_PROPS, "invalid index : " );

            // 0x822E14CC..0x822E1518 (BrnPropCellManager.cpp:1091). Every occupied-pool slot
            // must be marked in the bit array; the whole pool is occupied on this path.
            CGS_ASSERT( mPhysicalProps.IsBitSet( luSlot ),
                        "mPhysicalProps.IsBitSet( liRecycleIndex )" );

            // 0x822E1520..0x822E1530. See the NaN-polarity note in the banner: the console's
            // `blt` skips the update ONLY on an ordered less-than, so the negated ordered
            // predicate is the faithful spelling.
            const f32 lfTimeInSim = maPhysicalPropParams[luSlot].mfTimeInSim;
            if ( !( lfTimeInSim < lfLongestTimeInSim ) )
            {
                lfLongestTimeInSim = lfTimeInSim;
                liRecycleIndex     = static_cast<s32>( luSlot );
            }
        }

        if ( liRecycleIndex == -1 )
        {
            // 0x822E1550..0x822E1570 -- nothing recyclable.
            lriSlot     = -1;
            lrEvictedId = MakeDefaultPropEntityId();
            lrbEvicted  = false;
            return false;
        }

        // 0x822E1580..0x822E15EC. The console reads the resident id's OWNER byte and fires
        // the prop tripwire before handing it back (`lbzx r11, r31, r22 ; cmplwi r11, 3`);
        // that is PropEntityID::AssertIsProp folded at the point of the read. (Its part-slot
        // twin at 0x822E1170 does NOT carry this tripwire -- a real asymmetry between the
        // two shipped bodies, reproduced.)
        maPhysicalPropParams[liRecycleIndex].mEntityId.AssertIsProp();

        lrEvictedId = maPhysicalPropParams[liRecycleIndex].mEntityId;
        lrbEvicted  = true;

        maPhysicalPropParams[liRecycleIndex].mfTimeInSim = 0.0f;
        maPhysicalPropParams[liRecycleIndex].mEntityId   = lEntityId;

        lriSlot = liRecycleIndex;
        return true;
    }

    // ========================================================================
    // PropCellManager::GetPhysicalPartSlot   @ 0x822E0DB0
    // ------------------------------------------------------------------------
    // ⭐ NON-LEDGER, NEW 2026-08-18 (wave Q keystone, group 7). Same story as its whole-prop
    // twin above: declared, never bodied, and ProcessBrokenProps' inner loop turns on it.
    //
    // Structurally IDENTICAL to GetPhysicalPropSlot with three differences, all measured:
    //   * the pool is mPhysicalParts / maPhysicalPartParams, capacity 30 not 15;
    //   * the assert texts name mPhysicalParts (BrnPropCellManager.cpp:1014, not :1091);
    //   * the eviction leg does NOT run the owner tripwire on the resident id.
    // ========================================================================
    bool PropCellManager::GetPhysicalPartSlot( PropEntityID lEntityId, s32& lriSlot,
                                               PropEntityID& lrEvictedId, bool& lrbEvicted )
    {
        // 0x822E0DDC..0x822E0F18 -- find-first-clear-bit, both escapes folded (see the twin).
        const s32 liFreeSlot = mPhysicalParts.GetFirstClearBit();

        if ( liFreeSlot != CgsContainers::BitArray<KU_MAX_PHYSICAL_PARTS>::KI_INVALID_BITINDEX )
        {
            // 0x822E0F1C -- SetBit bounds tripwire (CgsBitArray.h:222).
            CGS_ASSERT( static_cast<u32>( liFreeSlot ) < KU_MAX_PHYSICAL_PARTS,
                        "Index: " );

            // 0x822E0FCC..0x822E1028, in asm store order.
            mPhysicalParts.SetBit( static_cast<u32>( liFreeSlot ) );
            maPhysicalPartParams[liFreeSlot].mfTimeInSim = 0.0f;
            maPhysicalPartParams[liFreeSlot].mEntityId   = lEntityId;

            lriSlot     = liFreeSlot;
            lrEvictedId = MakeDefaultPropEntityId();
            lrbEvicted  = false;
            return true;
        }

        // ---- the pool is full: pick the longest-resident slot ------------------------
        f32 lfLongestTimeInSim = KF_MINIMUM_TIME_IN_SIM_BEFORE_RECYCLE;   // flt_82002138
        s32 liRecycleIndex     = -1;

        for ( u32 luSlot = 0; luSlot < KU_MAX_PHYSICAL_PARTS; ++luSlot )
        {
            // 0x822E0E64 -- IsBitSet bounds tripwire (CgsBitArray.h:203).
            CGS_ASSERT( luSlot < KU_MAX_PHYSICAL_PARTS, "invalid index : " );

            // 0x822E10BC..0x822E1108 (BrnPropCellManager.cpp:1014).
            CGS_ASSERT( mPhysicalParts.IsBitSet( luSlot ),
                        "mPhysicalParts.IsBitSet( liRecycleIndex )" );

            // 0x822E110C..0x822E1120 -- negated ordered predicate; see the banner.
            const f32 lfTimeInSim = maPhysicalPartParams[luSlot].mfTimeInSim;
            if ( !( lfTimeInSim < lfLongestTimeInSim ) )
            {
                lfLongestTimeInSim = lfTimeInSim;
                liRecycleIndex     = static_cast<s32>( luSlot );
            }
        }

        if ( liRecycleIndex == -1 )
        {
            // 0x822E1140..0x822E1160.
            lriSlot     = -1;
            lrEvictedId = MakeDefaultPropEntityId();
            lrbEvicted  = false;
            return false;
        }

        // 0x822E1170..0x822E11B0. No owner tripwire here (contrast the whole-prop twin).
        lrEvictedId = maPhysicalPartParams[liRecycleIndex].mEntityId;
        lrbEvicted  = true;

        maPhysicalPartParams[liRecycleIndex].mfTimeInSim = 0.0f;
        maPhysicalPartParams[liRecycleIndex].mEntityId   = lEntityId;

        lriSlot = liRecycleIndex;
        return true;
    }

    // ========================================================================
    // PropEntityModule::ProcessBrokenProps   @ 0x822EEFA0   (363 insns)
    // DWARF BrnPropEntityModule.cpp:1430.
    // ------------------------------------------------------------------------
    // Retire the props the PHYSICS side reported broken last frame, and re-issue them as
    // PARTS. For each id in maRecentlyBrokenProps:
    //   * skip it entirely if it is already E_MOVED;
    //   * release the whole-prop physics slot, tell the prop manager to drop the whole-prop
    //     rigid body, and decrement the in-sim prop population;
    //   * then, for every part of the prop type in turn, broker a PART slot -- evicting the
    //     longest-resident part if the 30-slot pool is full (and appending the evicted id to
    //     maRecentlyRecycledParts so PreSceneUpdate can pull it out of the scene next frame)
    //     -- mark the part physical, seat it at the prop's pose offset by that part's
    //     design-time offset, and enqueue an AddPartInstance.
    // Finally clear the broken list.
    //
    // ⚠️ MEASURED: lpInput (r4) is NEVER READ. The prologue moves r3 -> r29 and r5 -> r22 and
    // leaves r4 untouched; no later instruction reads it before the first call clobbers it.
    // The parameter is declared because the CALLER passes it (PrePhysicsUpdate @0x82303100
    // sets r4 = lpInput, r5 = lpOutput) and because the DWARF declares it. Left unnamed.
    //
    // r23 == this + 0x280 == &mZoneManager throughout, and PropCellManager is embedded at
    // offset 0 inside it -- which is why the raw asm appears to call PropCellManager methods
    // on the zone manager's address (gotcha 2: three classes on one pointer). The calls below
    // go through the de-inlined PropZoneManager forwarders, per the keystone's convention.
    // ========================================================================
    void PropEntityModule::ProcessBrokenProps( const PropEntityIO::InputBuffer_PrePhysics* /*lpInput*/,
                                               PropEntityIO::OutputBuffer_PrePhysics* lpOutput )
    {
        // 0x822EF0C0..0x822EF530. A `while (true)` in the asm whose exit test is
        // `if (i >= GetLength()) break` -- i.e. this for. The bound is RE-READ every
        // iteration (0x822EF0C0 and 0x822EF0E4 both `lwz 0x80(r30)`); nothing in the body
        // adds to the set, so the re-read and the for are equivalent as well as same-shaped.
        // The three asserts the console fires per iteration are CgsSet.h:227 ("Set used
        // before Construct/Clear was called", from GetLength), :274 (the same, from GetItem)
        // and :275 ("Set index out of bounds"). GetLength()/operator[] carry the first two
        // verbatim. ⚠️ The THIRD differs in the committed container and is NOT introduced by
        // this body: the console bounds the index against the LIVE COUNT (`lwz 0x80(set)`,
        // `cmplw ; blt`) while CgsSet.h:97-100's GetItem bounds it against the CAPACITY N.
        // Same message, weaker predicate. Recorded, not worked around -- the loop bound above
        // is the live count, so neither form can fire here.
        for ( u32 luBroken = 0; luBroken < maRecentlyBrokenProps.GetLength(); ++luBroken )
        {
            const PropEntityID lPropEntityId = maRecentlyBrokenProps[luBroken];

            // [DIAG] NOT IN THE X360 BINARY. The last rung of the wave-Q4 break probe (set
            // BRN_PROP_DIAG): the prop reached maRecentlyBrokenProps and is being retired
            // from the simulation. Emitted at the TOP of the iteration, before the E_MOVED
            // early-out, so an entry that is skipped still shows up -- "the event exists" and
            // "the event was acted on" are different failures and the log must separate them.
            // The latch is evaluated ONCE (a static bool), not per iteration.
            {
                static const bool sbPropDiag = ( getenv( "BRN_PROP_DIAG" ) != 0 );
                if ( sbPropDiag && CgsDev::Log::gpDebugPrint != 0 )
                {
                    *CgsDev::Log::gpDebugPrint
                        << "[prop-diag] broken-event prop=" << lPropEntityId.GetValue()
                        << "\n";
                }
            }

            // 0x822EF150 `sub_822CDA28` == PropZoneManager::GetProp(PropEntityID) -- the
            // GLOBAL-index overload. Hex-Rays drops its second argument; the asm passes
            // r4 = the id.
            PropEntityInstance* lpInstance = mZoneManager.GetProp( lPropEntityId );

            // 0x822EF158..0x822EF184. GetState() folded (its "mu8State < E_STATE_COUNT"
            // tripwire at BrnPropEntityInstance.h:653), then `cmplwi 5 ; beq <loop tail>`.
            // INFERENCE (banner): 5 == E_MOVED. A prop that has already been shoved out of
            // its slot by an eviction is not retired again.
            if ( lpInstance->GetState() == E_MOVED )
            {
                continue;
            }

            // 0x822EF188 `lbz 0x4A ; rlwinm 0,28,28` == mu8Flags & KU_SMASHABLE_BIT.
            // INFERENCE (banner): this is IsSmashable(), which this class does not declare.
            CGS_ASSERT( ( lpInstance->mu8Flags & KU_SMASHABLE_BIT ) != 0,
                        "lpInstance->IsSmashable()" );                                    // :1470
            // 0x822EF1B0..0x822EF208. The second "mu8State < E_STATE_COUNT" plus the
            // branchless `state >= 6` carry trick IS PropEntityInstance::IsSmashed().
            CGS_ASSERT( lpInstance->IsSmashed(), "lpInstance->IsSmashed()" );              // :1471

            // 0x822EF20C..0x822EF274. `addis r3,r29,0xD ; addi r3,r3,-0x2278` == this +
            // 0xCDD88 == mpPropPhysicsDataHeader; the truncated `BrnPhysics__Props__Prop`
            // symbol is that ResourcePtr's GetMemoryResource. GetType's two bounds
            // tripwires (BrnPropPhysicsDataHeader.h:173/:174) are inlined at 0x822EF220 and
            // 0x822EF240 and come with the call.
            const BrnPhysics::Props::PropTypeData* lpType =
                mpPropPhysicsDataHeader.GetMemoryResource()->GetType( lpInstance->muTypeId );

            // 0x822EF268..0x822EF2A4. Release the whole-prop slot, drop the whole-prop rigid
            // body, and decrement the in-sim population. mu8PhysicsIndex is re-read between
            // the two calls (`lbz 0x4C` at 0x822EF268 and 0x822EF280); nothing in between can
            // change it, so the two reads are one value.
            mZoneManager.FreePhysicalPropSlot( lpInstance->mu8PhysicsIndex );
            lpOutput->GetPropInputInterface()
                    ->RemovePropInstance( lPropEntityId, lpInstance->mu8PhysicsIndex );
            // `lwz/addi -1/stw 0x904(this+0x280)` == a bare `--miNumPropsInSim` on the
            // embedded PropCellManager. The de-inlined forwarder takes the DWARF's int32_t
            // delta, so the folded form is the delta == 1 case.
            mZoneManager.DecrementNumberOfPropsInSim( 1 );

            // 0x822EF294..0x822EF2CC. `insrwi r27, 1, 10, 22` splices the literal 1 into the
            // low 10-bit part field, preceded by the hoisted owner tripwire -- i.e.
            // SetPartIndex(1). Part ids are 1-BASED (part 0 is the whole prop).
            PropEntityID lPartEntityId = lPropEntityId;
            lPartEntityId.SetPartIndex( 1 );

            // 0x822EF2C4..0x822EF4FC. The console guards the loop on `lbz 0x5D(type) != 0`
            // and then runs a do-while over 0-based part indices, RE-READING the part count
            // at the bottom of every iteration (0x822EF4EC). lpType is a const serialised
            // record, so the guarded do-while with a re-read bound is exactly this for.
            //
            // ⚠️ CONSOLE STRIDE TRAP: the descriptor walk is `r24 += 0x30` -- banner note (1).
            const Matrix44Affine& lrPropTransform = lpInstance->mWorldTransform;

            for ( s32 liPart = 0; liPart < static_cast<s32>( lpType->GetNumberOfParts() ); ++liPart )
            {
                // 0x822EF2E4 `sub_822CDB90` == PropZoneManager::GetPart(PropEntityID).
                PropPartEntityInstance* lpPart = mZoneManager.GetPart( lPartEntityId );
                CGS_ASSERT( lpPart != NULL, "lpPart != NULL" );                           // :1495

                // 0x822EF30C..0x822EF32C. Broker a part slot. The three out-parameters are
                // the stack slots var_12C / var_134 / var_140 (r5 / r6 / r7).
                s32          liSlot    = 0;
                PropEntityID lEvictedId;
                bool         lbEvicted = false;

                if ( !mZoneManager.GetPhysicalPartSlot( lPartEntityId, liSlot,
                                                        lEvictedId, lbEvicted ) )
                {
                    // 0x822EF32C `beq cr6, loc_822EF4EC` -- straight to the LOOP TAIL, which
                    // skips the part-index advance at 0x822EF474..0x822EF4E8. So a failed
                    // slot request leaves lPartEntityId pointing at the SAME part for the
                    // next iteration. That is as-shipped, not a transcription slip.
                    continue;
                }

                if ( lbEvicted )
                {
                    // 0x822EF340..0x822EF3B4. The slot was taken from another part: clear
                    // that part's physical flag (`stb 0, 0x47`), pull its rigid body, and
                    // record it so PreSceneUpdate can take it out of the scene next frame.
                    mZoneManager.GetPart( lEvictedId )->mbPhysical = false;
                    lpOutput->GetPropInputInterface()
                            ->RemovePartInstance( lEvictedId, liSlot );

                    // 0x822EF370..0x822EF3B0. `this + 0xCDEA8` == maRecentlyRecycledParts
                    // (::Array<PropEntityID,30>), count word at +0x78. A full list simply
                    // drops the record -- the asm has no else arm.
                    if ( !maRecentlyRecycledParts.IsFull() )
                    {
                        maRecentlyRecycledParts.Append( lEvictedId );
                    }
                }
                else
                {
                    // 0x822EF3B8. `lwz/addi 1/stw 0x908(this+0x280)` == `++miNumPartsInSim`.
                    mZoneManager.IncrementNumberOfPartsInSim( 1 );
                }

                // 0x822EF3C4..0x822EF3F0, in asm store order (flag first, index second).
                CGS_ASSERT( lpPart->mbPhysical == false, "lpPart->mbPhysical == false" );  // :1523
                lpPart->mbPhysical      = true;
                lpPart->mu8PhysicsIndex = static_cast<u8>( liSlot );

                // 0x822EF3F4..0x822EF454. The part inherits the prop's ORIENTATION verbatim
                // (three `lvx128`/`stvx128` row copies) and only its translation differs:
                //     partPos = M.Right()*off.x + M.Up()*off.y + M.At()*off.z + M.Pos()
                // The console stores the prop's own wAxis into the local first and then
                // overwrites it with the computed row; both stores are reproduced.
                //
                // IDA operand-order trap. The site HERE is the BASE-VMX form -- 0x822EF448
                // `vmaddfp v13, v12, v13, v8` / 0x822EF44C `vmaddfp v0, v11, v13, v0` (no
                // `128` suffix) -- which IDA prints in RAW FIELD order `vD,vA,vB,vC`
                // meaning vD = vA*vC + vB, i.e. the ADDEND (the running accumulator v13) is
                // the THIRD operand. Verified by decode, not assumed.
                // ⚠️ The VMX128 form prints DIFFERENTLY: `vmaddfp128 vD,vA,vC,vB` is ISA
                // mnemonic order and its addend is the LAST operand (see
                // PropEntityModule_wQ_04.cpp's BreakPropIntoParts, 0x822FB20C, which is the
                // same maths written with vmaddfp128). Do not carry either rule across.
                const BrnPhysics::Props::PropPartTypeData& lrPartType =
                    lpType->GetParts()[liPart];
                const Vector3& lrOffset = lrPartType.GetOffset();

                Matrix44Affine lPartTransform;
                lPartTransform.xAxis = lrPropTransform.xAxis;
                lPartTransform.yAxis = lrPropTransform.yAxis;
                lPartTransform.zAxis = lrPropTransform.zAxis;
                lPartTransform.wAxis = lrPropTransform.wAxis;

                Vector3 lPartPosition = lrPropTransform.xAxis * lrOffset.x;
                lPartPosition = lrPropTransform.yAxis * lrOffset.y + lPartPosition;
                lPartPosition = lrPropTransform.zAxis * lrOffset.z + lPartPosition;
                lPartPosition = lrPropTransform.wAxis + lPartPosition;

                lPartTransform.wAxis = lPartPosition;

                // 0x822EF458..0x822EF470. r5 is a FRESH `lhz 0x44(prop)` -- the PROP's type
                // id, not the part's; r6 is the 0-based loop counter; r8 is the brokered slot.
                lpOutput->GetPropInputInterface()
                        ->AddPartInstance( lPartEntityId,
                                           static_cast<s32>( lpInstance->muTypeId ),
                                           liPart,
                                           lPartTransform,
                                           liSlot );

                // 0x822EF474..0x822EF4E8. GetPartIndex() (owner tripwire + `clrlwi 22`),
                // +1, then SetPartIndex (owner tripwire + the `< 1<<10` bound at
                // CgsEntityId.h:167 + `clrrwi 10 ; or`). Advance to the next part id.
                lPartEntityId.SetPartIndex( lPartEntityId.GetPartIndex() + 1 );
            }
        }

        // 0x822EF534..0x822EF540. `stwx 0, r29, 0xCDE64` == maRecentlyBrokenProps' count
        // word == Set<PropEntityID,32>::Clear(). Unconditional, outside the loop.
        maRecentlyBrokenProps.Clear();
    }

    // ========================================================================
    // PropEntityModule::PrePhysicsUpdate   @ 0x82303048   (100 insns)
    // ------------------------------------------------------------------------
    // The prop module's pre-physics tick, and the DRIVER of both broken-prop retirement and
    // potential-contact routing. WorldModule::EntityModulePrePhysicsUpdate
    // (BrnWorldModule.cpp:1688) calls it once per frame inside miPhysicsPropPrePhysicsUpdatePM.
    //
    // ⚠️ DUPLICATE DEFINITION / HARD LINK BREAK -- RE-CONFIRMED 2026-08-18 (round 2, and
    // re-measured again in the round-3 fix pass), STILL PRESENT. There is an inert boot gate
    // for this exact function in b5-decomp/src/GameSource/World/WorldLinkStubs.cpp:
    //
    //     lines 1174-1178  the 5-line `// BOOT GATE (world-IO wave 2026-07-27):` comment
    //     line  1179       void BrnWorld::PropEntityModule::PrePhysicsUpdate(
    //                          struct CgsModule::IOBufferStack *, struct CgsModule::IOBufferStack *,
    //                          class BrnWorld::PropEntityIO::InputBuffer_PrePhysics *,
    //                          class BrnWorld::PropEntityIO::OutputBuffer_PrePhysics *,
    //                          unsigned short)
    //     lines 1180-1188  `{ ... one-shot "inert (body not reconstructed) [FLAG PC boot
    //                      gate]" log ... }`   (1188 is the closing brace)
    //
    // `BrnUpdateSet` is `typedef u16` (SharedClasses/BrnSharedConstants.h:12), so that
    // parameter list is TOKEN-IDENTICAL to the definition below -- same mangled symbol, two
    // definitions, LNK2005. `cl /c` cannot see it and coverage_check does not scan
    // WorldLinkStubs.cpp, so BOTH gates read green while the link is broken.
    //
    // MOUNT/RETIRE RULE (AGENTS.md gotcha 7) -- the gate is deliberately NOT retired here,
    // because retiring it before the mount leaves the symbol undefined for every already-
    // mounted caller. The conductor does all of this in ONE change, in this order:
    //   1. add this file to tools/build/build_game_exe.bat (re-checked 2026-08-18:
    //      `grep -c PropEntityModule_wQ tools/build/build_game_exe.bat` == 0, so NO wQ_* or
    //      wQ2_* partfile is mounted yet -- mounting this one alone will not link, because
    //      its siblings' externals are still open);
    //   2. delete WorldLinkStubs.cpp:1174-1188 exactly (that whole range and nothing more --
    //      1173 and 1189 are blank lines, 1172 closes the neighbouring PostSceneUpdate gate),
    //      replacing it with the house marker `// GATE RETIRED <date>:
    //      BrnWorld::PropEntityModule::PrePhysicsUpdate @0x82303048 is now REAL.` in the form
    //      used at WorldLinkStubs.cpp:1102 / :1110 / :1118 / :1126;
    //   3. re-LINK. A recompile proves nothing here -- only the linker sees this.
    // NOTE the two NEIGHBOURING gates in the same file belong to the wQ2_* landers, not to
    // this file: PostSceneUpdate at :1163 (block 1158-1172) and PostPhysicsUpdate at :3171.
    // Retiring any of them is a per-function decision tied to that function's own mount.
    //
    // REGISTER -> PARAMETER MAP (prologue 0x82303054..0x82303064, measured):
    //   r3 -> r31 this | r4 UNUSED | r5 UNUSED | r6 -> r24 lpInput | r7 -> r23 lpOutput
    //   | r8 -> r30 lUpdateSet
    // r4/r5 are the two IOBufferStacks; this body never touches them (it neither creates nor
    // destroys a buffer -- WorldModule does that around the call). They are declared because
    // the committed signature and every sibling entity module's PrePhysicsUpdate carry them,
    // and because the arguments after them depend on their positions. Left unnamed.
    // NOTE this is NOT the float-skips-its-GPR-slot case (gotcha 3): both dead registers hold
    // real pointer parameters that the body simply does not read.
    // ========================================================================
    void PropEntityModule::PrePhysicsUpdate( CgsModule::IOBufferStack* /*lpInputBufferStack*/,
                                             CgsModule::IOBufferStack* /*lpOutputBufferStack*/,
                                             PropEntityIO::InputBuffer_PrePhysics* lpInput,
                                             PropEntityIO::OutputBuffer_PrePhysics* lpOutput,
                                             BrnUpdateSet lUpdateSet )
    {
        // 0x82303068 / 0x82303070 -- write lock on the OUTPUT buffer, read lock on the INPUT
        // buffer, in that order.
        lpOutput->LockForWrite();
        lpInput->LockForRead();

        if ( ( lUpdateSet & KU_UPDATESET_SIM_PAUSED ) != 0 )
        {
            // ---- 0x82303134: the simulation did not step this frame -------------------
            // Reset the "frames since the sim last paused" counter, then assert that nobody
            // handed us contacts anyway (WorldModule skips UpdateContactGeneration under this
            // same bit -- BrnWorldModule.cpp:2610).
            miUpdatesSinceLastSimPause = 0;

            CGS_ASSERT( lpInput->GetPotentialContactQueue()->GetLength() == 0,
                        "Received potential contacts in a paused update" );               // :1171
        }
        else
        {
            // ---- 0x82303080: the normal frame -----------------------------------------
            // 0x82303084 `addi r3, r31, 0x280` == &mZoneManager. Traffic lights that were
            // knocked down and have served their time are queued for restoration.
            mZoneManager.SendTrafficLightRestoreEvents( lpOutput );

            // 0x82303090 (`BrnWorld::PropEnti`, truncated by IDA) == 0x822B9348 ==
            // InputBuffer_PrePhysics::GetResetOnTrackResultQueue.
            const PropEntityIO::InputBuffer_PrePhysics::ResetOnTrackResultQueue* lpResetQueue =
                lpInput->GetResetOnTrackResultQueue();

            // 0x8230309C..0x823030F8. `lwz r11, 8(queue)` is the CONSOLE miLength offset --
            // banner note (2); GetLength() below. The callee at 0x822AC500 (IDA
            // `BrnAI::AIModuleIO::`, truncated) is
            // BaseEventQueue<ResetOnTrackResult>::GetEvent(int).
            for ( s32 liResult = 0; liResult < lpResetQueue->GetLength(); ++liResult )
            {
                const BrnAI::AIModuleIO::ResetOnTrackResult& lrResult =
                    lpResetQueue->GetEvent( liResult );

                // 0x823030D0..0x823030E8. `lbz 0(r25)` with r25 == this + 0xD3210 ==
                // mu8PlayerIndex; `lwz 0x24(event)` == meGlobalRaceCarIndex; the position is
                // `lvx128 v0, r0, r3` == the event's +0 mResetPosition, loaded before the
                // compare and stored only on a match.
                if ( static_cast<s32>( lrResult.GetGlobalRaceCarIndex() )
                     == static_cast<s32>( mu8PlayerIndex ) )
                {
                    // `stbx 1, r31, 0xCDDE0` / `stvx128 v0, r31, 0xCDDD0`.
                    mbPlayerWasJustReset     = true;
                    mLastPlayerResetPosition = lrResult.GetResetPosition();
                }
            }

            // 0x823030FC..0x82303118. Both take (lpInput, lpOutput) in that order.
            ProcessBrokenProps( lpInput, lpOutput );
            ProcessPotentialContacts( lpInput, lpOutput );

            // 0x8230311C..0x8230312C. `++*(this + 0xD335C)`.
            ++miUpdatesSinceLastSimPause;
        }

        // 0x823031C0..0x823031CC -- released in the MIRROR order of the acquire: read lock
        // first, then the write lock.
        lpInput->UnlockForRead();
        lpOutput->UnlockForWrite();
    }
}

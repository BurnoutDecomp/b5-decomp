// ============================================================================
// b5-decomp/src/GameSource/World/EntityModules/PropEntityModule/PropEntityModule_wQ_05.cpp
//
// BrnWorld::PropEntityModule -- WAVE Q, GROUP 5: THE CONTACT FAN-OUT.
// One queue decode, one router, two handlers. This partfile lands two of the three:
//
//   PropEntityModule::ProcessPotentialContacts        @0x822FA538  (213 insns)  <- here
//   PropEntityModule::ProcessPotentialContactWithProp @0x822DB038  (206 insns)  <- here
//   PropEntityModule::ProcessPotentialContactWithPart @0x822EEDA8  (126 insns)  <- PARKED
//       in round 1, scratchpad/waveQ/parked/PropEntityModule_05_ProcessPotentialContactWithPart.cpp
//       ROUND-2 UPDATE (2026-08-18): its single blocker is RETIRED. The one declaration it
//       needed -- a reader of mZoneManager.mCellManager.miNumPartsInSim, the physical-PART
//       budget gate at 0x822EEE70..0x822EEE80 -- landed as
//       `PropZoneManager::IsInPhysicalBudgetForParts(s32)` (BrnPropZoneManager.h:488, DWARF
//       :312). The round-2 owner compiled the parked body against it unchanged
//       (STATUS=pass, scratchpad/waveQ2/worldio.owner.md §0). The body is landed by the
//       round-2 LANDER, not by this file.
//
// LEDGER NOTE: ProcessPotentialContactWithProp is ledger-attributed to the
// GameSource/World/EntityModules/PropEntityModule/BrnPropZoneManager.h catch-all TU, not to
// BrnPropEntityModule.cpp -- an inlining artefact. It is a member of PropEntityModule
// (declared BrnPropEntityModule.h:512, DWARF BrnPropEntityModule.cpp:1265) and it is bodied
// here with its sibling. Both ledger TU ids are owned by this lane.
//
// SOURCE-OF-TRUTH. Every line below is read off the X360 ARTIST **assembly** for the two
// addresses above (the Hex-Rays pseudocode is used only as a map). Declaration shape comes
// from the committed BrnPropEntityModule.h, whose entries quote the DecFIGS DWARF.
//
// ---- LAYOUT DISCIPLINE ------------------------------------------------------------------
// Not one console byte offset appears in the code. The offset -> member decode used to READ
// the asm is recorded per site, in comments, and every access goes through a named member or
// accessor. The console strides that DO appear in the asm and are NOT reproduced:
//   * PotentialContact stride 80 (BaseEventQueue<PotentialContact>::GetEvent @0x822AC2D8
//     computes idx*80 with `slwi 2 ; add ; slwi 4`) -- the host `sizeof` is whatever the
//     compiler picks; indexing goes through GetEvent().
//   * maRaceCarVelocity stride 16 (`slwi r11,r11,4` at 0x822DB100) -- read through
//     GetRaceCarSpeed(), the DWARF accessor for the splatted .w lane, instead.
//   * queue length at queue+8 (`lwz r11, 8(r24)`) -- that is the CONSOLE offset of
//     BaseEventQueue<T>::miLength; on the host mpEvents widens 4->8 and it moves. Read
//     through GetLength().
//
// ---- CONSTANTS, ALL VERIFIED IN THIS LANE ----------------------------------------------
//   flt_82001C98 == 1.0f -- re-dumped from the IDB in this lane (`3f800000`), the
//   move/smash-threshold floor ProcessPotentialContactWithProp compares against.
//
// ---- ⚠ DEFECT FOUND IN A COMMITTED HEADER -- NOW FIXED THERE ---------------------------
// Round 1 found that BrnPropEntityModule.h:493 defined
//     bool CanChangeState( EPropState leOldState, EPropState leNewState ) const
//     { return ( leOldState < leNewState ) || ( leOldState == E_MOVED ); }
// which is ChangePropState ASSERT text ("leOldState < leNewState || (leOldState ==
// E_MOVED)", fold at 0x822EF5AC-0x822EF5B8) and NOT the predicate. The two places the
// shipped build folds the PREDICATE ITSELF --
//     ProcessPotentialContacts        @0x822FA760..0x822FA7BC
//     ProcessPotentialContactWithProp @0x822DB2E0..0x822DB33C
// -- are byte-for-byte the same shape:
//     r11 = (new > old) ; r9 = r11
//     if (old == 5) { r11 = (new == 4) } else { r11 = 0 }
//     take the branch when (r9 | r11)
// i.e.  ( leOldState < leNewState ) || ( leOldState == E_MOVED && leNewState == E_PHYSICAL ),
// which is STRICTER than the old inline (that one also allowed E_MOVED -> E_NON_PHYSICAL /
// E_STATIC / E_LEANING / E_PHYSICAL_WITH_EXTRA_COM_OFFSET).
// ROUND-2 UPDATE (2026-08-18): the header owner landed the correction at
// BrnPropEntityModule.h:531, in the DWARF three-term spelling
// (lbHigherState / lbMovedToPhysical / lbMovedToSmashed, DWARF header lines 397/400/403;
// the third term is invisible in both ARTIST folds only because E_MOVED==5 < E_SMASHED==6
// makes it subsumed by lbHigherState), non-const, second parameter leNextState. Both bodies
// below now CALL the member; the round-1 file-local copy is gone.
// ============================================================================

// ---- THE WAVE-Q IMPLEMENTER INCLUDE SET (spec §5, verbatim) ----------------------
#include "GameSource/World/EntityModules/PropEntityModule/BrnPropEntityModule.h"
#include "GameSource/World/EntityModules/PropEntityModule/BrnPropEntityModuleIO.h"
#include "GameSource/World/EntityModules/PropEntityModule/BrnPropZoneManager.h"
#include "GameSource/World/EntityModules/PropEntityModule/BrnPropCellManager.h"
#include "GameSource/World/EntityModules/PropEntityModule/BrnPropEntityInstance.h"
#include "GameSource/World/EntityModules/PropEntityModule/SharedIO/BrnPropToTrafficInterface.h"

#include "SharedClasses/Physics/Props/BrnPropEntityID.h"
#include "SharedClasses/Physics/Props/BrnPhysicsPropTypeData.h"
#include "SharedClasses/Physics/Props/BrnPropPhysicsDataHeader.h"

#include "GameSource/Physics/PropManager/SharedIO/BrnPropInputInterface.h"
#include "GameSource/Physics/PropManager/SharedIO/BrnPropOutputInterface.h"
#include "GameSource/Physics/PropManager/SharedIO/BrnPropEvents.h"
#include "GameSource/Physics/ContactSpies/BrnContactSpyInterface.h"

#include "GameSource/Replays/Serialisers/BrnReplayPropEntitySerialiser.h"
#include "GameSource/Gui/BrnGuiEventTypeDefs.h"
#include "GameSource/Graphics/BrnCoronaManager.h"

#include "GameShared/GameClasses/Containers/CgsArray.h"
#include "GameShared/GameClasses/Containers/CgsSet.h"
#include "GameShared/GameClasses/Containers/CgsBitArray.h"
#include "GameShared/GameClasses/Module/CgsEventQueue.h"
#include "GameShared/GameClasses/Module/CgsIOBufferStack.h"
#include "GameShared/GameClasses/SceneManager/CgsSceneManagerModuleIO.h"
#include "GameShared/GameClasses/Development/PerfMon/Cpu/CgsPerfMonCpu.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
// ---------------------------------------------------------------------------------
// Named explicitly on top of the spec block (both already reachable transitively; spelled
// out because this TU names the symbols directly).
#include "GameShared/GameClasses/SceneManager/CgsEntityId.h"            // CgsSceneManager::EntityId
#include "GameShared/GameClasses/SceneManager/SharedIO/CgsPotentialContact.h" // PotentialContact
#include "GameSource/World/BrnEntityTypes.h"                            // E_ENTITYTYPE_*

namespace BrnWorld
{
    // ROUND-2 UPDATE (2026-08-18): this file no longer has an anonymous namespace at all.
    //   * `KI_MAX_CONTACTED_PROPS` was a file constant; the DWARF makes it FUNCTION-LOCAL to
    //     ProcessPotentialContacts (BrnPropEntityModule.cpp:1179), which is where it now
    //     lives -- with the same grounding it carried here: the tripwire the console bakes
    //     at 0x822FA5D0 / 0x822FA65C (`cmpwi r27, 0x1F4`), whose message names the constant
    //     ("liNumContactedProps < KI_MAX_CONTACTED_PROPS", BrnPropEntityModule.cpp:1224
    //     and :1242).
    //   * The two file-local HELPERS are gone, because both now have their real DWARF homes:
    //       - `IsOverheadSignGraphicsId(u32)` -> `PropTypeData::IsOverheadSign()`
    //         (BrnPhysicsPropTypeData.h:179, DWARF :114). It is grounded on the very fold
    //         this file measured, 0x822DB198..0x822DB1E0, and the DWARF records it as an
    //         INLINED SUBROUTINE three times inside ProcessPotentialContactWithProp
    //         (_compile/BrnEntityModuleUnity.cpp:32038-32039, :32057). Keeping a fourth
    //         hand-spelled copy of the 0x7A4D6/0x7A4C2/0x7B8C2 triple was exactly the
    //         divergence risk gotcha 7 exists to stop.
    //       - `CanChangePropStateMeasured(...)` -> the class own `CanChangeState(...)`
    //         (BrnPropEntityModule.h:531). Round 1 forked it because the committed inline
    //         was then a WEAKER two-term predicate; the round-2 owner corrected it to the
    //         DWARF three-term form, which is the same predicate this file measured at
    //         0x822FA760..0x822FA7BC / 0x822DB2E0..0x822DB33C plus a third conjunct
    //         (E_MOVED -> E_SMASHED) that 5 < 6 already subsumes -- so the fork is now a
    //         pure duplicate and is retired.
    //         NOTE it is NOT the same predicate as ChangePropState's assert: that guard
    //         really is two terms and is spelled inline at PropEntityModule_wQ_04.cpp.
    //         Do not re-merge them.

    // ============================================================================
    // PropEntityModule::ProcessPotentialContacts   @0x822FA538  (213 insns)
    // DWARF BrnPropEntityModule.cpp:1171.
    //
    // Drain the scene's per-frame potential-contact queue. Each record names TWO collision
    // volume instances; for each of them independently, when the volume belongs to a PROP the
    // contact is routed to the part handler (part index non-zero) or to the whole-prop handler
    // (part index zero). Whole props are additionally remembered in a frame-local list, and a
    // second pass then commits each remembered prop's pending state change.
    //
    // ---- ASM -> C++ DECODE (offsets are CONSOLE; every one is a named member here) --------
    //   0x822FA554  bl 0x822B92A0   InputBuffer_PrePhysics::GetPotentialContactQueue() const
    //                               (reads this+0x10 behind the read-lock tripwire)
    //   0x822FA560  lwz r11, 8(r24)         queue->GetLength()   (console offset of miLength;
    //                                       re-read every iteration -- the loop condition)
    //   0x822FA58C  bl 0x822AC2D8   BaseEventQueue<PotentialContact>::GetEvent(int) const
    //   0x822FA594  ld  0x30(r28) ; srdi 32 ; clrlwi 0
    //                               -> PotentialContact::muVolumeInstanceIdA (+48), HIGH dword
    //   0x822FA598  ld  0x38(r28) ; srdi 32 ; clrlwi 0
    //                               -> PotentialContact::muVolumeInstanceIdB (+56), HIGH dword
    //          The 64-bit VolumeInstanceId carries the packed 32-bit scene EntityId word in
    //          its HIGH dword (CgsVolumeInstanceId.h: KU_ENTITY_ID_START_INDEX == 32), which
    //          is exactly what `>> 32` yields, endian-independently.
    //   0x822FA5AC  srwi r11, r31, 24       EntityId::GetOwner()      (KU_OWNER_BASE == 24)
    //   0x822FA5B8  clrlwi r11, r31, 22     EntityId::GetPartIndex()  (low 10 bits)
    //   0x822FA608  lvx128 v1, r28, r26 (r26 == 0x20)
    //                               -> PotentialContact::mNormal (+32), the vector argument
    //   0x822FA6E0  addi r23, r21, 0x280    &mZoneManager
    //   0x822FA70C  bl sub_822CDA28         PropZoneManager::GetProp(PropEntityID)
    //   0x822FA714  lbz 0x4E(r30)           PropEntityInstance::mu8NextState (GetNextState)
    //   0x822FA738  lbz 0x4D(r30)           PropEntityInstance::mu8State     (GetState)
    //   0x822FA874  stb r31, 0x4E(r30)      SetNextState( GetState() )
    // ============================================================================
    void PropEntityModule::ProcessPotentialContacts( const PropEntityIO::InputBuffer_PrePhysics* lpInput,
                                                     PropEntityIO::OutputBuffer_PrePhysics* lpOutput )
    {
        // The frame-local list of whole props that took a contact this frame. Lives on the
        // stack exactly as it does on the console (`r29 = sp + var_840`, 4-byte elements).
        // DWARF names (_compile/BrnEntityModuleUnity.cpp:32124-32130): the bound is a
        // FUNCTION-LOCAL `const int32_t KI_MAX_CONTACTED_PROPS` (BrnPropEntityModule.cpp:1179)
        // and the array is `PropEntityID[500] laContactedProps` (:1180) -- `la`, a LOCAL
        // array, not the `ma` MEMBER prefix round 1 used. It must not persist across frames,
        // which matters here because the 500-entry overrun below is non-gating.
        const s32    KI_MAX_CONTACTED_PROPS = 500;
        PropEntityID laContactedProps[ KI_MAX_CONTACTED_PROPS ];
        s32          liNumContactedProps = 0;

        const PropEntityIO::InputBuffer_PrePhysics::OutPotentialContactQueue* lpContactQueue =
            lpInput->GetPotentialContactQueue();

        // The console re-loads the queue length at the BOTTOM of every iteration
        // (0x822FA6C4), so the bound is re-read, not cached.
        for ( s32 liContact = 0; liContact < lpContactQueue->GetLength(); ++liContact )
        {
            const CgsSceneManager::SceneManagerIO::PotentialContact& lrContact =
                lpContactQueue->GetEvent( liContact );

            const CgsSceneManager::EntityId lEntityIdA(
                static_cast<u32>( lrContact.muVolumeInstanceIdA.muId >> 32 ) );
            const CgsSceneManager::EntityId lEntityIdB(
                static_cast<u32>( lrContact.muVolumeInstanceIdB.muId >> 32 ) );

            // ---- side A --------------------------------------------------------------
            if ( lEntityIdA.GetOwner() == E_ENTITYTYPE_PROP )
            {
                if ( lEntityIdA.GetPartIndex() != 0 )
                {
                    ProcessPotentialContactWithPart( PropEntityID( static_cast<u32>( lEntityIdA ) ),
                                                     lpOutput->GetPropInputInterface() );
                }
                else
                {
                    const PropEntityID lPropEntityId( static_cast<u32>( lEntityIdA ) );

                    // NON-GATING tripwire, exactly as shipped: the console fires the assert
                    // and then stores anyway (0x822FA5D4 `blt` branches only around the assert;
                    // 0x822FA5F4 `stw r31,0(r29)` + 0x822FA5FC `addi r29,r29,4` run on BOTH
                    // paths). THE OVERRUN IS DELIBERATE AND FAITHFUL, AND ITS HOST FAILURE MODE
                    // DIFFERS FROM THE CONSOLE'S: past element 500 this writes off the end of
                    // `laContactedProps` into the caller's frame. The X360 walked its own frame
                    // quietly; on the host MSVC /GS will trip the frame cookie and ASAN/the CRT
                    // will abort here. A crash at this store is THE SHIPPED DEFECT SURFACING,
                    // not a transcription error -- do not "fix" it by gating the store.
                    CGS_ASSERT( liNumContactedProps < KI_MAX_CONTACTED_PROPS,
                                "liNumContactedProps < KI_MAX_CONTACTED_PROPS" );
                    laContactedProps[ liNumContactedProps ] = lPropEntityId;
                    ++liNumContactedProps;

                    // The header spells the third parameter `lContactImpulse`; MEASURED, the
                    // vector the console loads into v1 is the record's +0x20 row, i.e.
                    // PotentialContact::mNormal.
                    ProcessPotentialContactWithProp( lPropEntityId, lEntityIdB, lrContact.mNormal,
                                                     lpOutput->GetPropInputInterface() );
                }
            }

            // ---- side B (the same block with A/B swapped; asserts at :1242) -----------
            if ( lEntityIdB.GetOwner() == E_ENTITYTYPE_PROP )
            {
                if ( lEntityIdB.GetPartIndex() != 0 )
                {
                    ProcessPotentialContactWithPart( PropEntityID( static_cast<u32>( lEntityIdB ) ),
                                                     lpOutput->GetPropInputInterface() );
                }
                else
                {
                    const PropEntityID lPropEntityId( static_cast<u32>( lEntityIdB ) );

                    // Same non-gating overrun as side A, same host caveat: 0x822FA660 `blt`
                    // skips only the assert; 0x822FA680 `stw r30,0(r29)` always runs. See the
                    // note at the side-A store.
                    CGS_ASSERT( liNumContactedProps < KI_MAX_CONTACTED_PROPS,
                                "liNumContactedProps < KI_MAX_CONTACTED_PROPS" );
                    laContactedProps[ liNumContactedProps ] = lPropEntityId;
                    ++liNumContactedProps;

                    ProcessPotentialContactWithProp( lPropEntityId, lEntityIdA, lrContact.mNormal,
                                                     lpOutput->GetPropInputInterface() );
                }
            }
        }

        // ---- second pass: commit each contacted prop's pending state change --------------
        // The console counts liNumContactedProps back down to zero while walking the list
        // (0x822FA870 `addi r27, r27, -1`); a forward walk over the recorded entries is the
        // same traversal.
        for ( s32 liIndex = 0; liIndex < liNumContactedProps; ++liIndex )
        {
            const PropEntityID  lPropEntityId = laContactedProps[ liIndex ];
            PropEntityInstance* lpProp        = mZoneManager.GetProp( lPropEntityId );

            const EPropState leState     = lpProp->GetState();
            const EPropState leNextState = lpProp->GetNextState();

            if ( CanChangeState( leState, leNextState ) )
            {
                ChangePropState( lpProp, lPropEntityId, leState, leNextState, lpOutput );
            }

            // Whatever happened, the pending state is retired to the state the prop is
            // actually in now (`stb GetState(), 0x4E` at 0x822FA874).
            lpProp->SetNextState( lpProp->GetState() );
        }
    }

    // ============================================================================
    // PropEntityModule::ProcessPotentialContactWithProp   @0x822DB038  (206 insns)
    // DWARF BrnPropEntityModule.cpp:1265.
    //
    // Classify what state ONE whole prop should be pushed towards by ONE potential contact,
    // and latch it into the prop's pending (next) state. Nothing is published to physics
    // here -- ProcessPotentialContacts' second pass calls ChangePropState.
    //
    // ---- SIGNATURE (asm-confirmed at the call site, 0x822FA604..0x822FA618) --------------
    //   r4 = the prop's PropEntityID, r5 = the OTHER volume's CgsSceneManager::EntityId,
    //   v1 = the contact vector, r6 = the prop input interface.
    //   The vector argument consumes NO GPR slot here (r6 is the 4th register and the 4th
    //   parameter), which is what makes the committed 4-parameter declaration right.
    // ⚠ MEASURED: the shipped body reads NEITHER v1 NOR r6 -- r6 is copied nowhere and no
    //   vector register is read. Both parameters exist because the caller passes them; the
    //   (void) casts below record that unused-ness explicitly instead of leaving it implicit.
    //
    // ---- ASM -> C++ DECODE ---------------------------------------------------------------
    //   0x822DB04C  addi r28, r31, 0x280   &mZoneManager
    //   0x822DB058  bl sub_822CDA28        PropZoneManager::GetProp(PropEntityID)
    //   0x822DB05C  addis/addi -> +0xCDD88 mpPropPhysicsDataHeader
    //   0x822DB06C  lhz 0x44(r21)          PropEntityInstance::muTypeId
    //   0x822DB070  bl 0x822868E0          ResourcePtr<PropPhysicsDataHeader>::GetMemoryResource
    //   0x822DB078  bl 0x82277C50          PropPhysicsDataHeader::GetType(u32)
    //   0x822DB0B4  lbz 0x4D(r21) == 5     GetState() == E_MOVED     -> E_PHYSICAL, done
    //   0x822DB0C4  srwi r24, r25, 24      lContactEntityId.GetOwner()
    //   0x822DB0D8  lbzx this+0xD3341      mbEasySmashProps          -> E_PHYSICAL, done
    //   0x822DB0EC  extrwi r11, r25, 14,8  lContactEntityId.GetEntityIndex()
    //   0x822DB100  slwi 4 ; add -> +0xD3230 + 16*idx, vspltw lane 3
    //                                      maRaceCarVelocity[idx].w  (the w lane is the
    //                                      speed the setter parks there -- see the
    //                                      GetRaceCarVelocity/GetRaceCarSpeed note at
    //                                      BrnPropEntityModule.h:533)
    //   0x822DB11C  bl 0x822A93A8          GetDesiredState(lpType, speed)
    //   0x822DB128  r24 == 2               E_ENTITYTYPE_TRAFFIC_VEHICLE
    //   0x822DB13C  bl 0x822BC4D0          PropZoneManager::GetRespawnType(PropEntityID)
    //   0x822DB148  lfs 0x50(r30)          PropTypeData::mfMoveThreshold  (GetMoveThreshold)
    //   0x822DB168  lbz 0x5D(r30)          PropTypeData::muNumberOfParts  (IsSmashable)
    //   0x822DB180  lfs 0x54(r30)          PropTypeData::mfSmashThreshold (GetSmashThreshold)
    //   0x822DB198  lwz 0x58(r30)          PropTypeData::muSceneUriId     (GetGraphicsId)
    //   0x822DB238  bl 0x822B7888          PropEntityID::PropEntityID(u32)
    //   0x822DB2B4  addi r31, r11, 1       (predicate ? E_PHYSICAL(4) : E_STATIC(1))
    //   0x822DB364  stb r31, 0x4E(r21)     SetNextState(...)
    //
    // ---- ⚠ NaN POLARITY (gotcha 4) --------------------------------------------------------
    // Both float compares are `fcmpu` + `blt`, i.e. the ORDERED less-than, which is exactly
    // C++ `<` (false when unordered). The first is used positively
    // (`GetMoveThreshold() < 1.0f`); the second is used NEGATED
    // (`!( GetSmashThreshold() < 1.0f )`) because the console falls through to the
    // "true" store only when the blt is NOT taken. Neither is rewritten as `>=`.
    // ============================================================================
    void PropEntityModule::ProcessPotentialContactWithProp( PropEntityID lPropEntityId,
                                                            CgsSceneManager::EntityId lContactEntityId,
                                                            Vector3 lContactImpulse,
                                                            BrnPhysics::Props::PropInputInterface* lpPropInput )
    {
        // MEASURED: the shipped body reads neither of these. Kept in the signature because
        // the call site fills them (0x822FA604 r6 / 0x822FA608 v1).
        (void)lContactImpulse;
        (void)lpPropInput;

        PropEntityInstance* lpProp = mZoneManager.GetProp( lPropEntityId );

        const BrnPhysics::Props::PropTypeData* lpType =
            mpPropPhysicsDataHeader.GetMemoryResource()->GetType( lpProp->GetTypeId() );

        EPropState leDesiredState;

        if ( lpProp->GetState() == E_MOVED )
        {
            // Already moving: a further contact can only keep it physical.
            leDesiredState = E_PHYSICAL;
        }
        else if ( lContactEntityId.GetOwner() == E_ENTITYTYPE_RACECAR )
        {
            if ( mbEasySmashProps )
            {
                // The debug "everything gives way" switch (+0xD3341).
                leDesiredState = E_PHYSICAL;
            }
            else
            {
                // GetRaceCarSpeed is the DWARF own accessor for this read
                // (BrnPropEntityModule.h:605, DWARF BrnPropEntityModule.h:345); it returns
                // the .w lane of the same 16-byte slot, i.e. the `vspltw v0,v0,3` at
                // 0x822DB110. Reading `.w` directly was structurally fragile as well as
                // unfaithful -- the SDK reconstruction documents Vector3 w as unused.
                leDesiredState = GetDesiredState(
                    lpType,
                    GetRaceCarSpeed( lContactEntityId.GetEntityIndex() ) );
            }
        }
        else
        {
            // Anything that is not the player's car: no impulse classification at all, just a
            // small set of "does this prop give way to this kind of contact" rules.
            const bool lbHitByTrafficVehicle =
                ( lContactEntityId.GetOwner() == E_ENTITYTYPE_TRAFFIC_VEHICLE );

            const bool lbPropRespawns =
                ( mZoneManager.GetRespawnType( lPropEntityId ) == BrnPhysics::Props::E_RESPAWN );

            // "This prop has no threshold worth beating": its move threshold is below the
            // 1.0f floor (flt_82001C98 -- re-dumped in this lane as 3f800000), and it is
            // either not smashable at all or its smash threshold is NOT below that floor
            // (i.e. it would be moved, not smashed). That whole expression is the DWARF
            // PropTypeData::ShouldActivateOnNonRaceCarContact(), an INLINED SUBROUTINE of
            // this function (_compile/BrnEntityModuleUnity.cpp:32037) reading only that
            // class own fields; de-inlined at BrnPhysicsPropTypeData.h:161, including the
            // deliberately negated ordered second compare (gotcha 4).
            const bool lbMovesOnAnyContact = lpType->ShouldActivateOnNonRaceCarContact();

            const bool lbOverheadSign = lpType->IsOverheadSign();

            bool lbMakePhysical = ( !lbOverheadSign && lbPropRespawns && lbMovesOnAnyContact );

            // ...except that an overhead sign IS knocked physical when the thing that hit it
            // is another WHOLE prop that is itself an overhead sign (sign-on-sign collapse).
            if ( lbOverheadSign
                 && ( lContactEntityId.GetOwner() == E_ENTITYTYPE_PROP )
                 && ( lContactEntityId.GetPartIndex() == 0 ) )
            {
                const PropEntityID  lContactPropId( static_cast<u32>( lContactEntityId ) );
                PropEntityInstance* lpContactProp = mZoneManager.GetProp( lContactPropId );

                const BrnPhysics::Props::PropTypeData* lpContactType =
                    mpPropPhysicsDataHeader.GetMemoryResource()->GetType( lpContactProp->GetTypeId() );

                if ( lpContactType->IsOverheadSign() )
                {
                    lbMakePhysical = true;
                }
            }

            leDesiredState = ( lbMakePhysical || lbHitByTrafficVehicle ) ? E_PHYSICAL : E_STATIC;
        }

        // ---- latch it into the pending state ---------------------------------------------
        // The committed CanChangeState() (BrnPropEntityModule.h:531) IS this fold now -- see
        // the round-2 note where the file-local copy used to be.
        const EPropState leNextState = lpProp->GetNextState();

        if ( CanChangeState( leNextState, leDesiredState ) )
        {
            lpProp->SetNextState( leDesiredState );
        }
    }
}

// ============================================================================
// b5-decomp/src/GameSource/World/EntityModules/PropEntityModule/PropEntityModule_wQ2_03.cpp
//
// BrnWorld::PropEntityModule -- WAVE Q ROUND 2, LANDER 03. Three bodies that round 1
// reconstructed COMPLETELY but had to park out-of-tree for want of declarations. Those
// declarations landed in round 2 (shared-header owner, scratchpad/waveQ2/worldio.owner.md),
// so the bodies come in-tree here:
//
//   PropEntityModule::ProcessContacts                 @0x822FA890  (476 insns)  <- THE GATE
//   PropEntityModule::ProcessPotentialContactWithPart @0x822EEDA8  (126 insns)
//   PropEntityModule::PostSceneUpdate                 @0x822C4718  ( 61 insns)
//
// Instruction counts above are COUNTED, in this pass, off the non-empty lines of each
// address's `assembly` array in .ida-exports/BURNOUT_X360_ARTIST.XEX/<addr>.json.
//
// SOURCE OF TRUTH. Every line below is read off the X360 ARTIST **assembly** arrays for the
// three addresses. The Hex-Rays pseudocode is a map only -- it renders all three functions
// with `int` parameters and inlines the vector math as opaque blocks.
//
// ---- LAYOUT DISCIPLINE (AGENTS.md gotcha 1) ---------------------------------------------
// Not one console byte offset appears in the code. Every member is reached by name or
// accessor. The console values that DO appear in the asm and are deliberately NOT
// reproduced:
//   * the prop-contact queue at contactSpyData + 0x167E0 -> GetPropContacts()
//   * maRaceCarVelocity at module + 0xD3230 stride 16     -> GetRaceCarVelocity(idx)
//   * mauStartIndexOfZone at `2*(zone + 0x662A1) + module`-> GetStartIndexOfZone(zone)
//   * the queue length at queue + 8                       -> GetLength()  (mpEvents widens
//     4 -> 8 on the host, so that offset moves)
//   * maRecentlyRecycledParts at module + 0xCDEA8         -> the named member
//   * the physical-part budget bound 30                   -> KU_MAX_PHYSICAL_PROP_PARTS
//   * the crash-complete queue at lpInput + 8             -> GetCrashEventQueue()
//   * PropVFXLocatorEvent's +0x00/+0x10/+0x20/+0x30/+0x40/+0x44 stores -> Construct()
//
// ---- ROUND-1 VERIFY ISSUES CLOSED HERE ---------------------------------------------------
//   [MUST_FIX, G8] "the three queue re-types write through a NEVER-CONSTRUCTED EventQueue".
//     Both halves are gone: OutputBuffer_PostPhysics now HAS a Construct()
//     (BrnPropEntityModuleIO_OutputBuffer_PostPhysics.cpp:136, X360 @0x822EFE08) that points
//     every embedded queue's mpEvents at its inline maEvents[], and the four queues are real
//     typed CgsModule::EventQueue<T,N> members, so all three reinterpret_cast re-types are
//     DELETED below in favour of the typed accessors.
//   [NIT, G8] the stale BrnPropZoneManager.h addresses for HasPropBeenHit(PropEntityID) /
//     RecordHitProp(PropEntityID): re-checked this pass -- the header now reads 0x822CDD48 /
//     0x822CDCD0 (:353 / :359), which are the two addresses ProcessContacts actually calls at
//     0x822FAD40 / 0x822FAD5C. Closed, nothing to do here.
//   [G5 park] the miNumPartsInSim reader: PropZoneManager::IsInPhysicalBudgetForParts(s32)
//     landed at BrnPropZoneManager.h:488 with exactly the requested spelling.
//   [G2 park] the InputBuffer_PostScene 16-byte placeholder: the buffer now carries the real
//     RaceCarCrashCompleteEventQueue member plus Construct/Destruct/GetCrashEventQueue.
//
// ---- STALE PARK-BANNER CLAIMS RE-DERIVED AND CORRECTED (AGENTS.md gotchas 9 + 10) --------
//   1. The parked ProcessContacts banner's "SECOND FINDING" (three OutputBuffer_PostPhysics
//      accessors mislabelled) and its ⚠️ note at the Clear() call site are now HISTORY: the
//      header was retyped in round 2 and every accessor binds the member its X360 body
//      returns. The Clear() below goes through GetPropInputInterface(), which is both the
//      DWARF name and the +0xC86B0 member -- no deliberate misnaming remains.
//   2. That banner's "THIRD FINDING" opaque-storage re-type idiom no longer applies; the
//      queues are typed.
//   3. The parked ProcessPotentialContactWithPart banner said KVF_PROP_FLOOR "has NO header
//      home anywhere in the tree" and spelled it file-local. FALSE as of round 2: it is
//      homed at SharedClasses/Physics/Props/BrnPropConstants.h:56 as
//      BrnPhysics::Props::KVF_PROP_FLOOR = -1000.0f. The file-local copy is DELETED and the
//      header constant used, so there is one spelling of the value.
//   4. That same banner reported BrnPropZoneManager.cpp's -15000.0f / "not in .ida-exports"
//      comment defect. Re-checked: the round-2 owner already fixed it (that file now uses
//      KVF_PROP_FLOOR). The note is dropped rather than repeated.
//   5. The parked PostSceneUpdate banner's whole "WHY IT IS PARKED / THE EXACT LINES THAT
//      UNBLOCK IT" section described a placeholder that no longer exists. Dropped.
//
// ---- ⚠️ REPORTED, NOT FIXED (foreign headers this lane does not own) ---------------------
//   a. ⭐ CLOSED 2026-08-18 (round-3 fix pass) -- kept as the record, since this file's
//      consumer is what surfaced it. The old BrnPropConstants.h comment claimed all three
//      KVF_PROP_FLOOR consumers compared with `vcmpgtfp.` and that "the predicate is
//      (KVF_PROP_FLOOR > pos.y)". MEASURED at the consumer in THIS file: 0x822EEE50 is
//      `vcmpgefp. v0, v0, v13`, an ORDERED >=, not >. The header now carries a PER-CONSUMER
//      list instead (BrnPropConstants.h:31-61, re-read there this pass), and it was wrong for
//      TWO of the three, not one:
//        ProcessPotentialContactWithPart @0x822EEE50 `vcmpgefp.`  -> KVF_PROP_FLOOR >= pos.y
//        ChangePropState                 @0x822EF5F4 `vcmpgtfp.`  -> pos.y > KVF_PROP_FLOOR
//                                        (operands REVERSED; corroborated by the console's own
//                                         baked assert string, PropEntityModule_wQ_04.cpp:182)
//        PropZoneManager::UpdateInstance @0x822F0B9C `vcmpgtfp.`  -> KVF_PROP_FLOOR > pos.y
//                                        (the only one the old blanket claim described)
//      The >= site and the > sites differ at exactly pos.y == -1000.0f; all three are ORDERED,
//      so a NaN Y is false everywhere. DO NOT normalise them to one spelling.
//   b. BrnPropEntityModuleIO.h:141-146 says BrokenPropEvent's single byte is "the broken
//      prop's index/id". MEASURED: ProcessContacts stores the STRIKING CAR's entity index
//      there (0x822FAF5C `lwz r11, 4(r25)` == mEntityIdB, the car; `srwi r11,r11,10` then
//      `stb`). Written faithfully below and flagged at the site.
//
// ---- LINK-LEVEL (AGENTS.md gotcha 7 + 12; gate NOT retired by this lane) ------------------
//   b5-decomp/src/GameSource/World/WorldLinkStubs.cpp:1163 defines an inert boot gate for
//   BrnWorld::PropEntityModule::PostSceneUpdate with the identical 5-parameter signature
//   (BrnUpdateSet is `typedef u16`, so the two mangle the same). WorldLinkStubs.cpp IS
//   mounted, so the moment this file is mounted that is a DUPLICATE SYMBOL at link -- and it
//   is invisible to `cl /c` and to coverage_check, which globs only this directory and so
//   reports DUPLICATE(ODR) = 0.
//   EXACT RANGE, MEASURED 2026-08-18: comment block 1158-1162, definition 1163, body
//   1164-1172 (1172 is the closing brace). Delete 1158-1172 and nothing else -- 1156 is the
//   tail of the RETIRED GenerateDispatchLists note above and 1157 is blank -- then put the
//   house marker `// GATE RETIRED <date>: BrnWorld::PropEntityModule::PostSceneUpdate
//   @0x822C4718 is now REAL (PropEntityModule_wQ2_03.cpp).` in its place, in the form used at
//   WorldLinkStubs.cpp:1102/:1110/:1118/:1126.
//   MOUNT AND RETIRE IN ONE CHANGE, THEN RE-LINK; a recompile proves nothing here. Note that
//   mounting this file ALONE will not link: `grep -c PropEntityModule_wQ
//   tools/build/build_game_exe.bat` == 0, so none of the wQ_*/wQ2_* partfiles (nor
//   BrnPropEntityModuleIO_InputBuffer_PostScene.cpp) is mounted yet.
//   Neither ProcessContacts nor ProcessPotentialContactWithPart has a gate in either
//   WorldLinkStubs.cpp or BrnPhysicsConductorGates.cpp (re-grepped this pass).
// ============================================================================

#include "GameSource/World/EntityModules/PropEntityModule/BrnPropEntityModule.h"
#include "GameSource/World/EntityModules/PropEntityModule/BrnPropEntityModuleIO.h"
#include "GameSource/World/EntityModules/PropEntityModule/BrnPropZoneManager.h"
#include "GameSource/World/EntityModules/PropEntityModule/BrnPropCellManager.h"
#include "GameSource/World/EntityModules/PropEntityModule/BrnPropEntityInstance.h"

#include "GameSource/World/BrnEntityTypes.h"                                     // E_ENTITYTYPE_RACECAR
#include "GameSource/World/CrashModule/SharedIO/BrnCrashModuleRaceCarIOInterfaces.h" // CrashIO::RaceCarCrashCompleteEvent

#include "SharedClasses/Physics/Props/BrnPropEntityID.h"
#include "SharedClasses/Physics/Props/BrnPhysicsPropTypeData.h"
#include "SharedClasses/Physics/Props/BrnPropPhysicsDataHeader.h"
#include "SharedClasses/Physics/Props/BrnPropConstants.h"                        // KVF_PROP_FLOOR
#include "SharedClasses/Physics/Props/PropRespawnTypesEnum.h"

#include "GameSource/Physics/PropManager/SharedIO/BrnPropInputInterface.h"
#include "GameSource/Physics/ContactSpies/BrnContactSpyInterface.h"              // pulls BrnContactSpyData.h
#include "GameSource/Physics/ContactSpies/BrnContactSpyData.h"
#include "GameSource/GameState/BrnGameEvents.h"                                  // RecordPropHitEvent

#include "GameShared/GameClasses/Containers/CgsArray.h"
#include "GameShared/GameClasses/Containers/CgsSet.h"
#include "GameShared/GameClasses/Module/CgsEventQueue.h"
#include "GameShared/GameClasses/Module/CgsIOBufferStack.h"
#include "GameShared/GameClasses/SceneManager/CgsEntityId.h"
#include "GameShared/GameClasses/SceneManager/CgsVolumeInstanceId.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"                       // gpDebugPrint / gxMessageFilterFlags
#include "GameShared/GameClasses/Core/CgsAssert.h"

#include "rw/math/vpu/vector3_operation.h"                                       // rw::math::vpu::Dot
#include <math.h>                                                                // fabsf (the vandc sign-mask)

namespace BrnWorld
{
    namespace
    {
        // flt_82014648, the literal-pool float ProcessContacts' smash test multiplies the dot
        // product by. 1 / 0.44704 == metres-per-second -> miles-per-hour. The project already
        // homes the same value under this spelling in VehiclePhysics.cpp:2582 and
        // BrnVehicleManager_ValidateRaceCarWorldContact.cpp:57 (there from the NAMED X360
        // symbol KF_MPS_TO_MPH @0x8208F820, which is what fixes the name); this TU's copy is
        // an unnamed literal-pool entry, so it is spelled locally in the same style rather
        // than a new shared header being minted for it.
        const f32 KF_MPS_TO_MPH = 2.2369363f;

        // flt_820147E0, the epsilon the "this prop smashes as soon as it moves" test adds to
        // the move threshold before comparing it with the smash threshold. Loaded once into
        // f31 at 0x822FA9DC and used at 0x822FAE98 (`fadds f0, f0, f31`).
        const f32 KF_EASY_SMASH_THRESHOLD_EPSILON = 0.1f;
    }

    // ========================================================================================
    // PropEntityModule::ProcessContacts   @0x822FA890   (476 insns)
    //
    // THE SMASH RECORDER -- the gate function of wave Q. Runs inside PostPhysicsUpdate's
    // miProcessContactsPM monitor, once per frame, over the physics side's resolved
    // prop-contact list. For every contact between a WHOLE prop (part index 0) and a race car
    // it does two independent things:
    //
    //   1. PROGRESSION (player car only): if the prop respawns in a non-default way, publish a
    //      RecordPropHitEvent (position + zone + index-in-zone + "had it been hit before") for
    //      GameState's progression/StuntManager store, and mark the prop hit in the persistent
    //      bit array so it never respawns intact again.
    //   2. SMASH (any car): if the prop is smashable, not already smashed, and the impact
    //      along the contact normal beat its smash threshold (or the type is an "easy smash"),
    //      record it in maRecentlyBrokenProps and emit the outbound BrokenPropEvent +
    //      PropVFXLocatorEvent that the pre-scene pass turns into flying parts and VFX.
    //
    // Leg 2 runs for EVERY qualifying contact, not just player ones -- it is reached by the
    // fall-through at loc_822FAD60, outside leg 1's player/progression gate.
    //
    // CONSOLE OFFSET -> MEMBER DECODE (provenance only; nothing below indexes by offset):
    //   module +0xD3210 mu8PlayerIndex          module +0xD3342 mbAllowPropProgression
    //   module +0xD3343 mbPlayerCrashing        module +0xCD960 mbUseOverrides
    //   module +0xCD96C mfOverrideSmashThreshold module +0xD3230 maRaceCarVelocity
    //   module +0xCDDE4 maRecentlyBrokenProps    module +0x280   mZoneManager
    //   instance +0x4A  mu8Flags   +0x4D mu8State   +0x44 muTypeId   +0x40 muInstanceID
    //   instance +0x46  mu16ZoneIndex            +0x30 mWorldTransform.wAxis
    //   type     +0x50  mfMoveThreshold  +0x54 mfSmashThreshold  +0x5D muNumberOfParts
    // ========================================================================================
    void PropEntityModule::ProcessContacts( const PropEntityIO::InputBuffer_PostPhysics* lpInput,
                                            PropEntityIO::OutputBuffer_PostPhysics* lpOutput )
    {
        CGS_ASSERT( lpInput  != 0, "lpInput != NULL" );    // BrnPropEntityModule.cpp:1923 (li r5,0x783)
        CGS_ASSERT( lpOutput != 0, "lpOutput != NULL" );   // BrnPropEntityModule.cpp:1924 (li r5,0x784)

        // The scene-entity id of the player's own car. Built through EntityId::Set so the host
        // does the packing; the console folded two of Set()'s three tripwires away because
        // their arguments are compile-time constants there, leaving only the entity-index one
        // (CgsEntityId.h:116, `cmplwi r31, 0x4000` at 0x822FA90C) -- which is exactly what
        // Set() still emits. (asm 0x822FA934: `slwi r11, mu8PlayerIndex, 10 ; oris r11,0x100`.)
        CgsSceneManager::EntityId lPlayerCarEntityId;
        lPlayerCarEntityId.Set( E_ENTITYTYPE_RACECAR, mu8PlayerIndex, 0 );

        // Reset this frame's outbound physical-prop request bundle (the four add/remove queue
        // lengths + the remove-all flag). asm 0x822FA944..0x822FA964 -- a call to the buffer's
        // +0xC86B0 accessor followed by PropInputInterface::Clear() inlined (`stw 0` at +8 /
        // +0xFB8 / +0x1F68 / +0x28D4 and `stb 0` at +0x2C00, which is Clear()'s exact store
        // set, BrnPropInputInterface.h).
        lpOutput->GetPropInputInterface()->Clear();

        // The physics side's resolved prop-contact list for this frame. The console folds
        // ContactSpyInterface::GetPropContacts + ContactSpyData::GetPropContacts inline at
        // 0x822FA970..0x822FA9A8: the "mpData != NULL" tripwire (BrnContactSpyInterface.h:211)
        // then mpData + 0x167E0 == &mPropContactQueue. Both are header inlines in the tree now.
        const BrnPhysics::ContactSpy::ContactSpyData::PropContactQueue* lpPropContacts =
            lpInput->GetContactSpyInterface()->GetPropContacts();

        // The bound is RE-READ every iteration (asm 0x822FAFE4 `lwz r10, 8(r3)`), so a callee
        // that appended would be seen -- kept as a live-bound loop, not a hoisted length.
        for ( s32 liContact = 0; liContact < lpPropContacts->GetLength(); ++liContact )
        {
            const BrnPhysics::ContactSpy::PropContact& lrContact =
                lpPropContacts->GetEvent( liContact );

            // The OTHER entity must be a race car. asm 0x822FAA90: `lbz r11, 4(r25)` reads the
            // BIG-ENDIAN top byte of mEntityIdB, i.e. its owner field, and compares == 1.
            const CgsSceneManager::EntityId lOtherEntityId( lrContact.mEntityIdB.muValue );
            if ( lOtherEntityId.GetOwner() != E_ENTITYTYPE_RACECAR )
            {
                continue;
            }

            // The prop side. The constructor carries the owner tripwire the console emits
            // inline here ("mEntityId.GetOwner() == E_ENTITYTYPE_PROP", BrnPropEntityID.h:278,
            // asm 0x822FAAA0 `srwi r11, r31, 24 ; cmplwi r11, 3`).
            const PropEntityID lPropEntityId( lrContact.mEntityIdA.muValue );

            // Whole props only -- a contact against a broken-off PART is handled by the
            // pre-physics path. asm 0x822FAAC4 `clrlwi r11, r31, 22` then the subfic/subfe
            // "is non-zero" fold.
            if ( lPropEntityId.GetPartIndex() != 0 )
            {
                continue;
            }

            PropEntityInstance* lpInstance = mZoneManager.GetProp( lPropEntityId );

            // asm 0x822FAAF8 `lbz r11, 0x4A(r30) ; rlwinm r11, r11, 0,29,29` -- the accessor is
            // FOLDED to the bare mask (bit 29 from the MSB == 4 == KU_ADDED_TO_CONTACT_GEN_BIT),
            // there is no call. Spelled as the mask for two reasons, both of which the tree
            // already settled: it is what the console executes, and it is the idiom the four
            // committed sibling sites use (BrnPropCellManager.cpp:534/580/607/685, same assert
            // text, same open-coded mask). PropEntityInstance::IsAddedToContactGen() is declared
            // at BrnPropEntityInstance.h:125 and DEFINED NOWHERE in the tree, so calling it here
            // would add an unresolved external to the wave's gate function for no fidelity gain.
            CGS_ASSERT( ( lpInstance->mu8Flags & KU_ADDED_TO_CONTACT_GEN_BIT ) != 0,
                        "lpInstance->IsAddedToContactGen()" );                  // :1963 (0x7AB)
            // GetState() carries its own "mu8State < E_STATE_COUNT" tripwire
            // (BrnPropEntityInstance.h:653, asm 0x822FAB20..0x822FAB40, li r5,0x28D) -- that is
            // the second assert the console emits here, and it is NOT written out again.
            CGS_ASSERT( lpInstance->GetState() > E_NON_PHYSICAL,
                        "lpInstance->GetState() > E_NON_PHYSICAL" );            // :1964 (0x7AC)

            // -------------------------------------------------------------------------------
            // LEG 1 -- PROGRESSION. Player car only, prop not already moved, progression
            // enabled and the player not mid-crash. Four separate `bne/beq -> loc_822FAD60`
            // at 0x822FAB68 / 0x822FAB78 / 0x822FAB88 / 0x822FAB9C, in this order.
            // -------------------------------------------------------------------------------
            if ( static_cast<u32>( lPlayerCarEntityId ) == lrContact.mEntityIdB.muValue
                 && ( lpInstance->mu8Flags & KU_MOVED_BIT ) == 0
                 && mbAllowPropProgression
                 && !mbPlayerCrashing )
            {
                const BrnPhysics::Props::eRespawnType leRespawnType =
                    mZoneManager.GetRespawnType( PropEntityID( lrContact.mEntityIdA.muValue ) );

                // E_RESPAWN (0, the ordinary "it comes back" class) records nothing --
                // asm 0x822FABCC `cmplwi r3,1 ; blt -> loc_822FAD60`.
                if ( leRespawnType != BrnPhysics::Props::E_RESPAWN )
                {
                    bool lbRecord = true;
                    PropEntityID lHitPropEntityId( lrContact.mEntityIdA.muValue );

                    if ( leRespawnType == BrnPhysics::Props::E_DONT_RESPAWN )
                    {
                        if ( ( CgsDev::Message::gxMessageFilterFlags & 1 ) != 0 )
                        {
                            // ⚠️ MEASURED ASYMMETRY, reproduced verbatim: only THIS branch sets
                            // the hex-once print mode (asm 0x822FACB0/B8 `li r11,2 ;
                            // stw r11, 4(r31)` == StrStreamBase::mePrintMode). The
                            // E_RESPAWN_CHANGED branch below has no such store, so its id
                            // prints in decimal. Do not "fix" the inconsistency.
                            *CgsDev::Log::gpDebugPrint
                                << "Hit dont respawn prop: "
                                << CgsDev::E_PRINTMODE_HEXONCE
                                << lHitPropEntityId.GetValue()
                                << " Instance id: "
                                << lpInstance->muInstanceID
                                << "\n";
                        }
                    }
                    else if ( leRespawnType == BrnPhysics::Props::E_RESPAWN_CHANGED )
                    {
                        if ( ( CgsDev::Message::gxMessageFilterFlags & 1 ) != 0 )
                        {
                            *CgsDev::Log::gpDebugPrint
                                << "Hit respawn changed prop: "
                                << lHitPropEntityId.GetValue()
                                << " Instance id: "
                                << lpInstance->muInstanceID
                                << "\n";
                        }
                    }
                    else
                    {
                        // leRespawnType >= E_RESPAWN_TYPE_COUNT. asm 0x822FABE0: the assert
                        // then branches STRAIGHT to loc_822FAD60, skipping the record.
                        CGS_ASSERT( false, "Shouldn't get here" );              // :2021 (0x7E5)
                        lbRecord = false;
                    }

                    if ( lbRecord )
                    {
                        // ⭐ ROUND 2: this is the leg the wave-Q spec called "genuinely
                        // unrecovered" and round 1 landed through a reinterpret_cast. Both are
                        // gone -- mRecordHitPropQueue is a real
                        // EventQueue<RecordPropHitEvent,50> and the accessor is typed. The
                        // no-arg AddEvent() reserves the tail slot and returns a reference to
                        // it, which is exactly the console shape (`bl GetRecordHitPropQueue ;
                        // bl AddEvent` at 0x822FAC58/0x822FAC5C, result in r3 -> r31, then six
                        // stores through r31).
                        BrnGameState::GameStateModuleIO::RecordPropHitEvent& lrHitEvent =
                            lpOutput->GetRecordHitPropQueue()->AddEvent();

                        const s16 li16Zone = mZoneManager.GetZone( lHitPropEntityId );

                        // asm 0x822FAD10/14: `lvx128 v0, r30, 0x30 ; stvx128 v0, r0, r31` --
                        // the instance transform's translation row straight into the event's
                        // leading Vector3. The stores are in this order in the console too
                        // (position, zone, index-in-zone, hit-before).
                        lrHitEvent.mPosition = lpInstance->mWorldTransform.wAxis;
                        lrHitEvent.muZoneId  = static_cast<u16>( li16Zone );
                        // The prop's index WITHIN its zone. asm 0x822FAD18..0x822FAD3C does the
                        // `lhzx` from mauStartIndexOfZone by hand (`2*(zone + 0x662A1)`);
                        // routed through the named accessor instead (gotcha 1).
                        lrHitEvent.muPropId  = static_cast<u16>(
                            lHitPropEntityId.GetEntityIndex()
                            - mZoneManager.GetStartIndexOfZone( static_cast<u16>( li16Zone ) ) );
                        lrHitEvent.mbHitBefore = mZoneManager.HasPropBeenHit( lHitPropEntityId );

                        if ( !lrHitEvent.mbHitBefore )
                        {
                            mZoneManager.RecordHitProp( lHitPropEntityId );
                        }
                    }
                }
            }

            // -------------------------------------------------------------------------------
            // LEG 2 -- SMASH. Reached for every qualifying contact (loc_822FAD60), whoever hit
            // the prop.
            // -------------------------------------------------------------------------------
            // asm 0x822FAD60 `rlwinm r11, r11, 0,28,28` == mask 8 == KU_SMASHABLE_BIT.
            if ( ( lpInstance->mu8Flags & KU_SMASHABLE_BIT ) == 0 )
            {
                continue;
            }
            if ( lpInstance->IsSmashed() )
            {
                continue;
            }

            CGS_ASSERT( lpInstance == mZoneManager.GetProp( lPropEntityId ),
                        "lpInstance == mZoneManager.GetProp( lPropEntityId )" );  // :2035 (0x7F3)

            const BrnPhysics::Props::PropTypeData* lpType =
                GetPropPhysicsDataHeader()->GetType( lpInstance->muTypeId );

            // The debug menu can override the whole-prop smash threshold globally
            // (asm 0x822FADD8 `lbzx r11, r26, 0xCD960` -> 0x822FADF4 `lfsx f0, r26, 0xCD96C`).
            f32 lfSmashThreshold = lpType->GetSmashThreshold();
            if ( mbUseOverrides )
            {
                lfSmashThreshold = mfOverrideSmashThreshold;
            }

            // Impact speed along the contact normal, in MPH. asm 0x822FAE18..0x822FAE6C:
            // `vmsum3fp128` (3-lane dot of the striking car's cached velocity with the contact
            // normal), `vandc` against a splat 0x80000000 (absolute value), `vmulfp128` by
            // 2.2369363f, `vcmpgtfp.` against the splat threshold. The car slot is the OTHER
            // entity's index (0x822FAE14 `lwz r10, 4(r25) ; extrwi r10,r10,14,8`), not the
            // player's -- so a traffic-car or rival smash is scored off that car's velocity.
            // ⚠️ NaN (gotcha 4): `vcmpgtfp.` is an ORDERED greater-than -- unordered lanes
            // compare false -- and the branch tests CR6 bit 0 ("all lanes true"). That is
            // exactly C++ `>`. Do NOT rewrite as `!(<=)`.
            const f32 lfImpactSpeedMph =
                fabsf( rw::math::vpu::Dot( GetRaceCarVelocity( lOtherEntityId.GetEntityIndex() ),
                                           lrContact.mNormal ) ) * KF_MPS_TO_MPH;
            const bool lbHitHardEnough = lfImpactSpeedMph > lfSmashThreshold;

            // "Easy smash" types: a prop that has parts AND whose smash threshold is no more
            // than a hair above its move threshold breaks on any contact that would have moved
            // it. ⚠️ MEASURED: this test reads the TYPE's smash threshold (0x822FAE9C
            // `lfs f13, 0(r9)` with r9 == &lpType->mfSmashThreshold, loaded at 0x822FADCC
            // BEFORE the override branch), NOT lfSmashThreshold -- mbUseOverrides does not
            // reach it.
            // ⚠️ NaN (gotcha 4): the console's `fcmpu ; blt` at 0x822FAEA0/A4 takes the branch
            // ONLY when the compare is ORDERED-less-than; unordered falls through to
            // `mr r29, r18` == 0. `<` is therefore exact -- do NOT rewrite as `!(>=)`.
            const bool lbEasySmashType =
                ( lpType->GetNumberOfParts() != 0 )
                && ( lpType->GetSmashThreshold()
                     < lpType->GetMoveThreshold() + KF_EASY_SMASH_THRESHOLD_EPSILON );

            // Evaluation ORDER is the console's: IsFull first (0x822FAEB8), then the OR of the
            // two smash predicates (0x822FAECC), then Contains (0x822FAEE4).
            if ( !maRecentlyBrokenProps.IsFull()
                 && ( lbHitHardEnough || lbEasySmashType )
                 && !maRecentlyBrokenProps.Contains( lPropEntityId ) )
            {
                if ( ( CgsDev::Message::gxMessageFilterFlags & 1 ) != 0 )
                {
                    *CgsDev::Log::gpDebugPrint
                        << "PROPS Adding prop to Recently Broken Props: "
                        << CgsDev::E_PRINTMODE_HEXONCE
                        << lPropEntityId.GetValue()
                        << " Zone: "
                        << static_cast<s32>( lpInstance->mu16ZoneIndex )
                        << "\n";
                }

                maRecentlyBrokenProps.Insert( lPropEntityId );

                // ⚠️ MEASURED, and it contradicts the field's committed NAME AND its header
                // comment (BrnPropEntityModuleIO.h:141-146, "the broken prop's index/id"): the
                // byte the console stores is the STRIKING CAR's entity index, not the prop's
                // (asm 0x822FAF5C `lwz r11, 4(r25) ; srwi r11, r11, 10 ; stb r11, var_170` --
                // r25+4 is mEntityIdB, the car; the `stb` keeps the low 8 bits, which is the
                // low 8 bits of the 14-bit entity index, i.e. `(u8)GetEntityIndex()`).
                // Written faithfully here and REPORTED rather than renamed -- the field is
                // outside this lane's ownership.
                PropEntityIO::BrokenPropEvent lBrokenPropEvent;
                lBrokenPropEvent.muPropIndex =
                    static_cast<u8>( lOtherEntityId.GetEntityIndex() );

                lpOutput->GetBrokenPropQueue()->AddEvent( lBrokenPropEvent );

                // The VFX locator the particle system hangs the smash effect off. The console
                // open-codes the constructor (four `stvx128` transform rows at
                // 0x822FAF80..0x822FAFC8, the type id at +0x40 and the event type 1 at +0x44);
                // de-inlined here into the event's own Construct, which is now a header inline
                // in the tree (BrnPropEntityModuleIO.h:113).
                PropEntityIO::PropVFXLocatorEvent lVfxLocatorEvent;
                lVfxLocatorEvent.Construct( lpInstance->mWorldTransform,
                                            lpInstance->muTypeId,
                                            PropEntityIO::PropVFXLocatorEvent::E_EVENTTYPE_PROPSMASH );

                // AddEventSafe, not AddEvent: this queue holds only 10, and the console's
                // bounds-gated append returns false instead of appending when it is full
                // (`bge -> li r3,0` at 0x822C99C4/0x822C9A28). MEASURED, not a choice.
                // Note both appends take ONE argument (r3 = queue, r4 = &event); there is no
                // size argument in this family, so nothing here can carry a console sizeof.
                lpOutput->GetPropVFXLocatorQueue()->AddEventSafe( lVfxLocatorEvent );
            }
        }
    }

    // ========================================================================================
    // PropEntityModule::ProcessPotentialContactWithPart   @0x822EEDA8   (126 insns)
    //                   (DWARF BrnPropEntityModule.cpp:1351; declared BrnPropEntityModule.h:517)
    //
    // Make ONE prop PART physical because something touched it: claim a physical part slot
    // (evicting the oldest occupant if the pool is full) and hand the part to the physics side.
    // Early-outs, in the console's order: below the world floor, already physical, or the
    // physical-part budget is exhausted.
    //
    // GROUND TRUTH (X360 ARTIST assembly @0x822EEDA8, read instruction by instruction):
    //   0x822EEDBC  addi r28, r26, 0x280      &mZoneManager
    //   0x822EEDC8  bl sub_822CDB90           PropZoneManager::GetPart(PropEntityID)
    //   0x822EEDE8  assert "lpPartInstance != NULL"              BrnPropEntityModule.cpp:1382
    //   0x822EEDFC  clrlwi r11, r25, 22       lEntityId.GetPartIndex() != 0
    //   0x822EEE1C  assert "lEntityId.IsPart()"                  BrnPropEntityModule.cpp:1383
    //   0x822EEE30  addi r11, r29, 0x30 ; lvx128 ; vspltw v13, v0, 1
    //                                         PropPartEntityInstance::mWorldTransform.Pos().y
    //                                         (Matrix44Affine's Pos row is +0x30; lane 1 == Y)
    //   0x822EEE3C  unk_82FAD840              BrnPhysics::Props::KVF_PROP_FLOOR
    //   0x822EEE50  vcmpgefp. v0, v0, v13     KVF_PROP_FLOOR >= y  -> RETURN
    //   0x822EEE64  lbz 0x47(r29)             PropPartEntityInstance::mbPhysical -> RETURN if set
    //   0x822EEE70  lwz 0xB88(module) ; addi 1 ; cmpwi 0x1E ; ble
    //                                         the physical-PART budget gate. module+0xB88 ==
    //                                         (module+0x280)+0x908 ==
    //                                         mZoneManager.mCellManager.miNumPartsInSim -- the
    //                                         SAME member this function later read-modify-writes
    //                                         as `0x908(r28)` at 0x822EEF6C with r28 ==
    //                                         module+0x280, which is what proves the identity.
    //   0x822EEEA8  bl 0x822E0DB0             PropCellManager::GetPhysicalPartSlot, reached with
    //                                         r3 == module+0x280, i.e. through PropZoneManager's
    //                                         committed forwarder
    //   0x822EEEC4  stb 1, 0x47(r29)          mbPhysical = true
    //   0x822EEED8  assert "liPhysicalPartIndex >= 0"            BrnPropEntityModule.cpp:1409
    //   0x822EEEFC  assert "liPhysicalPartIndex < static_cast< int32_t > (
    //                       BrnPhysics::Props::KU_MAX_PHYSICAL_PROP_PARTS )"  ...cpp:1410
    //   0x822EEF24  bl sub_822CDB90           GetPart(evicted id)
    //   0x822EEF38  stb 0, 0x47(r11)          evicted->mbPhysical = false
    //   0x822EEF3C  bl 0x822CCE20             PropInputInterface::RemovePartInstance
    //   0x822EEF40  addis/addi -> +0xCDEA8    maRecentlyRecycledParts (Array<PropEntityID,30>)
    //   0x822EEF4C  bl 0x822CA3F8             Array<PropEntityID,30>::IsFull
    //   0x822EEF64  bl 0x822ADDC0             Array<PropEntityID,30>::Append
    //   0x822EEF6C  lwz/addi/stw 0x908(r28)   ++mCellManager.miNumPartsInSim
    //                                         == mZoneManager.IncrementNumberOfPartsInSim(1)
    //   0x822EEF7C  lhz 0x42(r29)             PropPartEntityInstance::muPartId
    //   0x822EEF84  lhz 0x40(r29)             PropPartEntityInstance::muTypeId
    //   0x822EEF8C  stb r30, 0x46(r29)        PropPartEntityInstance::mu8PhysicsIndex
    //   0x822EEF94  bl 0x822CCCA0             PropInputInterface::AddPartInstance (r7 ==
    //                                         lpPartInstance, and mWorldTransform is at offset 0
    //                                         of that object, so the 4th by-value
    //                                         Matrix44Affine argument IS its world transform)
    //
    // ⚠️ NaN POLARITY (gotcha 4), RE-MEASURED THIS PASS: 0x822EEE50 is `vcmpgefp.` -- the
    // ORDERED >= (false for unordered lanes) -- and 0x822EEE54..0x822EEE60
    // (`mfocrf r11,2 ; extrwi r11,r11,1,24 ; bne`) tests CR6 bit 0, "all lanes true". So
    // `KVF_PROP_FLOOR >= lfPositionY` maps to C++ `>=` EXACTLY: both are false when the Y lane
    // is NaN, and the console then falls through into the body. It is NOT rewritten as
    // `!(a < b)`; that would invert the NaN case. (BrnPropConstants.h used to claim this
    // consumer spelled `vcmpgtfp.`/`>`; that comment defect is now FIXED -- :31-61 there
    // carries the per-consumer list, with this site listed as the `vcmpgefp.` >= one.)
    // ========================================================================================
    void PropEntityModule::ProcessPotentialContactWithPart( PropEntityID lPropEntityId,
                                                            BrnPhysics::Props::PropInputInterface* lpPropInput )
    {
        PropPartEntityInstance* lpPartInstance = mZoneManager.GetPart( lPropEntityId );

        // Both tripwires are NON-GATING, exactly as shipped (the console branches only around
        // the assert bodies and carries on either way).
        CGS_ASSERT( lpPartInstance != nullptr, "lpPartInstance != NULL" );          // ...cpp:1382
        CGS_ASSERT( lPropEntityId.GetPartIndex() != 0, "lEntityId.IsPart()" );      // ...cpp:1383

        // ⭐ ROUND 2: KVF_PROP_FLOOR is now homed at BrnPropConstants.h:56; the parked body's
        // file-local copy (and its "no header home exists" claim) are gone.
        const f32 lfPositionY = lpPartInstance->mWorldTransform.Pos().y;
        if ( BrnPhysics::Props::KVF_PROP_FLOOR >= lfPositionY )
        {
            return;
        }

        if ( lpPartInstance->mbPhysical )
        {
            return;
        }

        // ⭐ ROUND 2: the miNumPartsInSim reader this body was parked on.
        if ( !mZoneManager.IsInPhysicalBudgetForParts( 1 ) )
        {
            return;
        }

        s32          liPhysicalPartIndex = 0;
        PropEntityID lEvictedPartId;
        bool         lbEvicted = false;

        if ( !mZoneManager.GetPhysicalPartSlot( lPropEntityId, liPhysicalPartIndex,
                                                lEvictedPartId, lbEvicted ) )
        {
            return;
        }

        lpPartInstance->mbPhysical = true;

        CGS_ASSERT( liPhysicalPartIndex >= 0, "liPhysicalPartIndex >= 0" );          // ...cpp:1409
        CGS_ASSERT( liPhysicalPartIndex < static_cast<s32>( BrnPhysics::Props::KU_MAX_PHYSICAL_PROP_PARTS ),
                    "liPhysicalPartIndex < static_cast< int32_t > ( BrnPhysics::Props::KU_MAX_PHYSICAL_PROP_PARTS )" );
                                                                                     // ...cpp:1410
        if ( lbEvicted )
        {
            // The slot was taken from another part: retire that one first. Its slot index is
            // the one we are about to occupy, which is why RemovePartInstance is passed
            // liPhysicalPartIndex and not a second lookup.
            mZoneManager.GetPart( lEvictedPartId )->mbPhysical = false;
            lpPropInput->RemovePartInstance( lEvictedPartId, liPhysicalPartIndex );

            // Remember it so the streaming side can put it back. IsFull is a gate here, not a
            // tripwire (0x822EEF58 `bne` skips the Append).
            if ( !maRecentlyRecycledParts.IsFull() )
            {
                maRecentlyRecycledParts.Append( lEvictedPartId );
            }
        }
        else
        {
            // A genuinely new occupant, so the sim population grows.
            mZoneManager.IncrementNumberOfPartsInSim( 1 );
        }

        lpPartInstance->mu8PhysicsIndex = static_cast<u8>( liPhysicalPartIndex );

        lpPropInput->AddPartInstance( lPropEntityId,
                                      lpPartInstance->muTypeId,
                                      lpPartInstance->muPartId,
                                      lpPartInstance->mWorldTransform,
                                      liPhysicalPartIndex );
    }

    // ========================================================================================
    // PropEntityModule::PostSceneUpdate   @0x822C4718   (61 insns)
    //
    // A lock/route shell over the crash module's "this race car has finished crashing" queue:
    // while the player is wrecked in an OFFLINE session, a crash-complete event for the
    // PLAYER's own car pushes the prop streaming machine into E_RESET_UNLOADING (the whole
    // prop world is dropped and re-streamed around the reset point).
    //
    // PARAMETER REGISTERS (from the prologue, not from Hex-Rays, which renders this as five
    // `int`s): r3 this, r4 lpInputBufferStack, r5 lpOutputBufferStack, r6 lpInput (-> r24),
    // r7 lpOutput (-> r23), r8 lUpdateSet. r4, r5 and r8 are NEVER READ by the body -- they
    // are declared because the caller (WorldModule::EntityModulePostSceneUpdate @0x827C3C58,
    // mirrored at BrnWorldModule.cpp:1881) passes them and the positions after them depend on
    // them.
    //
    // CONSOLE OFFSET -> MEMBER DECODE (provenance only; nothing below indexes by offset):
    //   module +0xD3340 mbCurrentlyOnline     module +0xD3344 mbPlayerWrecked
    //   module +0xD3210 mu8PlayerIndex        module +0xD3200 meStreamingMode
    //   lpInput +0x08   mRaceCarCrashCompleteEventQueue  (-> GetCrashEventQueue())
    //
    // ⚠️ LINK: WorldLinkStubs.cpp:1163 still carries the inert boot gate for this exact
    // signature (re-confirmed 2026-08-18; block 1158-1172). Reported as a link duplicate; NOT
    // retired by this lane (project rule: the conductor mounts this file and retires the gate
    // together, then re-links). See the LINK-LEVEL block in this file's banner for the exact
    // line range to delete and the marker to leave behind.
    // ========================================================================================
    void
    PropEntityModule::PostSceneUpdate( CgsModule::IOBufferStack* /*lpInputBufferStack*/,
                                       CgsModule::IOBufferStack* /*lpOutputBufferStack*/,
                                       PropEntityIO::InputBuffer_PostScene* lpInput,
                                       PropEntityIO::OutputBuffer_PostScene* lpOutput,
                                       BrnUpdateSet /*lUpdateSet*/ )
    {
        lpInput->LockForRead();      // 0x822C4734
        lpOutput->LockForWrite();    // 0x822C473C

        // 0x822C4740..0x822C4764. Two independent early-outs to the unlock tail:
        //   `bne loc_822C47F4` on mbCurrentlyOnline, then `beq loc_822C47F4` on
        //   mbPlayerWrecked. Online sessions never reset the prop world this way.
        if ( !mbCurrentlyOnline && mbPlayerWrecked )
        {
            const PropEntityIO::InputBuffer_PostScene::RaceCarCrashCompleteEventQueue*
                lpCrashCompleteQueue = lpInput->GetCrashEventQueue();

            // 0x822C4774 `lwz r25, 8(r28)` -- the length is read ONCE, before the loop (unlike
            // ProcessContacts, which re-reads it every iteration).
            const s32 liNumEvents = lpCrashCompleteQueue->GetLength();

            // 0x822C4778..0x822C47AC. The player's own race-car scene entity word, built ONCE
            // outside the loop: owner E_ENTITYTYPE_RACECAR (1), entity index mu8PlayerIndex,
            // part index 0 -- `slwi r11,r31,10 ; oris r30,r11,0x100`. EntityId::Set is inlined
            // here with two CONSTANT arguments, which is why only its entity-index tripwire
            // survives in the shipped code ("luEntityIndex < (1U << KU_NUM_BITS_FOR_ENTITY_NUM)"
            // @0x822C477C, `cmplwi r31, 0x4000`). Calling Set reproduces it exactly.
            CgsSceneManager::EntityId lPlayerEntityId;
            lPlayerEntityId.Set( E_ENTITYTYPE_RACECAR, mu8PlayerIndex, 0 );

            // 0x822C47B0 `cmpwi r25,0 ; ble` -- a zero-length queue skips the loop.
            for ( s32 liEvent = 0; liEvent < liNumEvents; ++liEvent )   // 0x822C47C4..0x822C47F0
            {
                const CrashIO::RaceCarCrashCompleteEvent& lrEvent =
                    lpCrashCompleteQueue->GetEvent( liEvent );          // 0x822C47CC

                // 0x822C47D0..D8 `ld r11,0(r3) ; srdi r11,r11,32 ; clrlwi r11,r11,0`.
                // CgsSceneManager::VolumeInstanceId is a PACKED 64-bit word whose embedded
                // 32-bit scene EntityId lives in the HIGH dword (bits 32..63) -- see
                // CgsVolumeInstanceId.h. This is the PACKING, not a console pointer width, so
                // the shift is correct on the host too (gotcha 1 does not bite here).
                const u32 luEventEntityWord =
                    static_cast<u32>( lrEvent.mRaceCarVolumeInstanceId.muId >> 32 );

                // 0x822C47DC..E4. NOTE: the comparison is over the WHOLE entity word, part
                // index included -- the console does not mask, and the built id has part index
                // 0, so only the car body itself matches.
                if ( luEventEntityWord == static_cast<u32>( lPlayerEntityId ) )
                {
                    meStreamingMode = E_RESET_UNLOADING;   // 0x822C47E4  `stwx r26(==2), ...`
                }
            }
        }

        lpInput->UnlockForRead();      // 0x822C47F8
        lpOutput->UnlockForWrite();    // 0x822C4800
    }
}

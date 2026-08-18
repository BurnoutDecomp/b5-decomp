// GameSource/Physics/PropManager/PropManager_wQ4_01.cpp
//
// BrnPhysics::Props::PropManager -- breakable-props / smash-gates wave Q4 (SEAM WAVE),
// partfile 01. Reconstructed from BURNOUT_X360_ARTIST.XEX:
//
//   SetupAndValidatePropContact @0x82628190   (573 instructions, 0x82628190..0x82628A80 --
//                                              COUNTED: (0x82628A80-0x82628190)/4 + 1 == 573;
//                                              the ledger's "572" is off by one)
//
// A slice of the TU's own BrnPropManager.cpp, split out per this wave's partfile convention
// (same spirit as BrnPropManager_PropInstanceQueries.cpp / PropManager_wQ2_08.cpp).
//
// ⚠️⚠️ MOUNT + RETIRE IS ONE COMMIT. The body below REPLACES the TRAP STUB that still stands in
// the MOUNTED BrnPropManager.cpp (that file's `SetupAndValidatePropContact` block). The trap
// stub was deliberately left in place by this wave so the shared checkout keeps linking -- its
// only caller, PhysicsModule::BridgeContactsToSimulation, lives in the MOUNTED
// BrnPhysicsModuleBridgeFunctions.cpp (build_game_exe.bat:781), so deleting the stub without
// mounting this file in the same commit is an immediate LNK2019. When this partfile is added to
// tools/build/build_game_exe.bat, DELETE BrnPropManager.cpp's trap-stub definition in the SAME
// commit or the link is an LNK2005 duplicate. Exact line range is in
// scratchpad/waveQ4/physcontact.owner.md.
//
// ==========================================================================================
// WHAT THIS FUNCTION IS FOR (and why wave Q4 needed it)
//
// This is the physics-side gate every car-versus-prop contact passes through. The scene
// manager hands PhysicsModule::BridgeContactsToSimulation a stream of PotentialContacts; for
// any pair where either side's owner byte is E_ENTITYTYPE_PROP (3) the bridge calls this
// function, and `false` DROPS the contact. Everything the prop pipeline does on impact
// starts here: the anti-herding shove, the impulse the car takes back, the jointed
// (lean/tilt) resolvers -- and, for a leaning prop pushed into the world, the SetBit into
// mBreakPropJoints that is what actually breaks a smash gate's joint.
//
// ==========================================================================================
// REGISTER MAP, measured from the prologue (0x826281A4..0x826281C8):
//   r3  -> r30 = this
//   r4  -> r23 = lpOutContact                 (InAddPotentialContact*)
//   r5  -> r22 = lpInPotentialContact         (const PotentialContact*)
//   r6  -> r24 = lpVehicleManager
//   r7  -> r16 = lpSimInputBuffer
//   r8  =  lpPropRaceCarContactBuffer         ⚠️ NEVER SAVED, NEVER READ  (see below)
//   r9  =  lWorldRigidBodyId                  ⚠️ NEVER SAVED, NEVER READ  (see below)
//   r10 -> r25 = lbOtherEntityIsFrozen        (its single use is 0x82628580)
//   f1  -> f31 = lfTimeStep                   (gotcha 3: the float rides f1 and SKIPS its GPR)
//
// ⚠️ TWO PARAMETERS ARE GENUINELY DEAD IN THE ARTIST EMISSION and are spelled unnamed below:
// `lpPropRaceCarContactBuffer` (r8) and `lWorldRigidBodyId` (r9). Neither register is copied in
// the prologue and neither is read anywhere in the 573 instructions (both are later reused as
// scratch -- r8 at 0x826289AC as a stack address, r9 at 0x826289A0 as an index literal -- which
// is the compiler proving to itself they were dead). They are KEPT in the signature because the
// DecFIGS DWARF declares them (dwarfdump BrnPropManager.h:191 / BrnPropManager.cpp:2653) and the
// asm cannot disprove an unused parameter; the caller in BrnPhysicsModuleBridgeFunctions.cpp
// already passes both. Flagged so nobody "discovers" the drop later and deletes them.
// ⚠️ NOTE what that means for wave Q4's seam question: this function does NOT write the
// PropRaceCarContactBuffer. Whoever is looking for the producer of that buffer must look
// elsewhere (PhysicsModule::Update pushes it; nothing here fills it).
//
// PARAMETER NAMES are the DWARF's (dwarfdump BrnPropManager.cpp:2653). The committed header
// spelled four of them differently (lpAddContactEvent / lpPotentialContact /
// lpSimModuleInputBuffer / lbFrozen); those are corrected to the DWARF's in BrnPropManager.h in
// this same wave. NAME-ONLY -- no type, order or count changed, so no call site moved.
//
// ==========================================================================================
// THE 573 INSTRUCTIONS, in emission order. Every console offset quoted below is reached BY
// NAME in the C++ (gotcha 1 -- they are meaningless on the LLP64 host).
//
//  (1) 0x826281C0..0x8262826C -- THE TWO TRAFFIC REMAPS, one per side, identical shape.
//      `ld 0x30(potential)` / `ld 0x38(potential)` == muVolumeInstanceIdA/B; `srdi 32 ; srwi 24`
//      == the embedded entity word's owner byte; == 2 (E_ENTITYTYPE_TRAFFIC_VEHICLE) selects
//      the remap. VehicleManager::GetTrafficPhysicsEntityIDFromGlobalEntityID_Safe resolves the
//      GLOBAL id to a PHYSICS one; a false return RETURNS FALSE (drops the contact). The
//      resolved id then gets `EntityId::SetOwner(0xC)` == E_ENTITYTYPE_PROP_COLLISION_TRAFFIC
//      and is spliced into the OUT event's own id (`stw 0,0xNN ; ld ; extldi 32 ; or ; std` ==
//      RigidBodyId::SetEntityId). DWARF blocks at BrnPropManager.cpp:1313/:1314 and :1333/:1334
//      name `bool lbTrafficExists` + `EntityId lDummyTrafficEntityId` twice -- once per side.
//      This is the exact twin of RoutePropVsRaceCarContactToDummyCar's owner-11 stamp.
//
//  (2) 0x82628270..0x82628298 -- the friction/restitution seed:
//      mStaticFriction <- mfStaticFriction (+0x74), mDynamicFriction <- mfDynamicFriction
//      (+0x78), mRestitution <- flt_82F2A398 == KF_PROP_RESTITUTION. (That address is the fifth
//      and last of this file's contiguous f32 tuning run 0x82F2A388..0x82F2A398, which is the
//      cross-check that the committed name for it is right.)
//
//  (3) 0x8262829C..0x826282AC -- `lbPropIsEntityA = (lpOutContact->mIDA owner == 3)`, computed
//      with the branchless `addi -3 ; cntlzw ; extrwi 1,26` idiom.
//
//  (4) 0x826282B0..0x8262855C -- THE MIRRORED PROP-SIDE RESOLUTION (branch A at 0x826282B0,
//      branch B at 0x8262840C; the two are instruction-for-instruction the same with 0x30/0x38
//      and mPointOnA/mPointOnB swapped). Per side:
//        * lPropRigidBodyId  = the OUT event's id on the prop side, lOtherRigidBodyId the other;
//        * lPropEntityId     = PropEntityID(potential->muVolumeInstanceIdX entity word);
//        * lu8OtherOwner     = lOtherRigidBodyId.GetEntityIDOwner();
//        * PART vs WHOLE PROP on `clrlwi r11,r31,22` == PropEntityID::GetPartIndex() != 0;
//        * liPhysicsIndex    = (lu8OtherOwner == WORLD) ? potential->muPolyTagA
//                                                       : Find{Prop,Part}Index(lPropEntityId)
//          -- see THE POLY-TAG SEAM below, this is measured end to end, not guessed;
//        * the bounds assert (baked lines 0x568/0x580/0x5A7/0x5BF of BrnPropManager.cpp);
//        * `== -1` (KI_PROP_INDEX_NOT_FOUND) RETURNS FALSE;
//        * Has{Prop,Part}JustBeenRemoved RETURNS FALSE;
//        * whole-prop only: lpPropInstance = &mpaPropInstances[liPhysicsIndex];
//        * then the join: lPointOnProp / lPointOnOtherRigidBody are lifted out of the potential
//          contact PROP-SIDE FIRST (v124 is always the prop's point, v126 always the other's),
//          and the OUT event's prop-side id gets its user-id-B set to liPhysicsIndex
//          (`sth 0,0x36|0x3E ; ld ; or ; std` == RigidBodyId::SetUserIDB).
//
//  (5) 0x82628560..0x82628728 -- THE JOINTED/STATIC PROP GATE, i.e. THE SMASH-GATE BREAK.
//      Only entered for a WHOLE prop (lpPropInstance != NULL) that IsJointed() or IsStatic().
//        * lbOtherEntityIsFrozen RETURNS FALSE (its only use in the whole body);
//        * other side is the WORLD -> the penetration test. lFlatNormal = potential->mNormal
//          with Y zeroed (`vrlimi128 v11,v12,4,0` -- mask 4 == lane 1); lvfPenetration =
//          Dot(lFlatNormal, mPointOnB - mPointOnA); if it exceeds
//          KVF_MAX_LEAN_PROP_WORLD_PENETRATION the prop's joint index is SET IN
//          mBreakPropJoints -- and the contact is dropped either way (the SetBit falls straight
//          into the `li r3,0` return). ⭐ THAT SetBit IS THE BREAK: UpdateJointedProps drains
//          mBreakPropJoints and calls BreakJoint.
//        * other side is another WHOLE PROP -> if that prop is itself jointed or static the
//          contact is dropped (a leaning prop may not lean on another leaning/static prop).
//
//  (6) 0x82628730..0x82628A00 -- THE RACE-CAR LEG (lu8OtherOwner == 1). Frictions are zeroed
//      (`flt_82001CC0` == 0.0f into both +0x40 and +0x44 -- a car/prop contact carries no
//      friction), then VehicleManager::GetRaceCarPhysics resolves the car (the console inlines
//      that accessor verbatim: eight `lwz`/`cmplw` over maRaceCarEntityIDs at +43584, then
//      `mulli 0x1460` + `addi 0x740` into maRaceCarVehicles). Then:
//        WHOLE PROP: lpProp / lCarTransform / lpPropType; RoutePropVsRaceCarContactToDummyCar
//          when the prop is non-static or jointed; lPropRigidBodyId.SetUserIDB(liPhysicsIndex);
//          lNormal flipped to point at the car; ApplyAntiHerdingForce unless the type is a
//          lamppost; then either the lean/tilt joint resolver (keyed on
//          PropTypeData::GetJointType()) or ApplyPropRaceCarCollisionImpulse.
//        PART:       FindPartIndex on the prop rigid-body id's own entity word, lPartRigidBodyId
//          = lPropRigidBodyId with user-id-B = liPartIndex, the part's mass out of the prop
//          type's part table, ApplyAntiHerdingForce, then unconditionally
//          RoutePropVsRaceCarContactToDummyCar.
//
//  (7) 0x82628A04..0x82628A80 -- the shared tail. When the OTHER side is a prop too, resolve
//      ITS index (prop or part, on its own part-index field) and stamp it into the OUT event's
//      mIDB; a miss RETURNS FALSE. Then return true.
//      ⚠️ The tail writes mIDB unconditionally, which is only correct because it is
//      UNREACHABLE when the prop is entity B: `lu8OtherOwner == 3` plus `lbPropIsEntityA ==
//      false` would mean A is a prop AND lbPropIsEntityA was false -- a contradiction, since
//      lbPropIsEntityA is exactly "A's owner == 3". Checked rather than assumed, because the
//      asymmetry reads like a bug on first pass.
//
// ==========================================================================================
// ⭐ THE POLY-TAG SEAM -- measured PRODUCER-to-CONSUMER, because it looks wrong until it does
// not. When the other side of the contact is the WORLD, this function does NOT search for the
// prop; it reads `lwz r27, 0x40(r22)` == PotentialContact::muPolyTagA and uses that word AS
// liPhysicsIndex. The producer chain confirms it, and all three links are in the tree today:
//   * DoPropInstanceWorldContactGeneration / DoPartWorldContactGeneration pass the prop's own
//     slot index as the PER-PRIMITIVE tag --
//     `lPrimPairList.AddPrimitive(volume, matrix, padding, static_cast<u16>(liPropIndex))`
//     (console `clrlwi r26,r26,16`), parked at scratchpad/waveQ2/parked/
//     PropManager_03_Do{PropInstance,Part}WorldContactGeneration.cpp;
//   * AddContactResultsToQueue (PropManager_wQ2_03.cpp:463) copies that tag straight through:
//     `lContact.muPolyTagA = lrResult.muPrimitive0Tag;`
//   * and primitive 0 is always the PROP side on that path (muVolumeInstanceIdA is built from
//     the prop entity word, muVolumeInstanceIdB from the world's).
// The same "a locally generated contact carries its own pool index in muPolyTagA" convention is
// already committed for the deformation pools (BrnDeformationManager_ContactFixups.cpp:71/:122).
// ⚠️ CONSEQUENCE FOR THE MOUNT: the two Do*WorldContactGeneration bodies are the only producers
// of that tag, and they are NOT in the tree (parked on two collision-generator declarations).
// Until they land, no prop-vs-WORLD contact reaches this function at all, so leg (5)'s break
// path is dead -- which is exactly the hole wave Q4 should report rather than paper over.
//
// ==========================================================================================
// ⚠️ NaN POLARITY (gotcha 4) -- two decisions here are vector compares, and both are written in
// the console's polarity rather than the "obvious" one:
//   * the penetration gate is `vcmpgtfp.` + a CR6[0] ("all lanes true") test, so an unordered
//     compare DROPS the contact. Spelled `if (!(penetration > K)) return false;`.
//   * the normal flip is `vcmpgefp` followed by `vnot128`, so an unordered compare FLIPS the
//     normal. Spelled `!(dot >= 0.0f)`, NOT `dot < 0.0f` -- those differ exactly on NaN.
// Both compares are between BROADCAST lanes (vmsum3fp128 splats its 3-lane dot, and both
// constants are splats), so the all-lanes CR6 test and the scalar compare below agree on every
// input, ordered or not.
//
// ⚠️ ONE UNGUARDED DEREFERENCE IS THE CONSOLE'S AND IS KEPT. `GetRaceCarPhysics` returns NULL on
// a miss (`li r29,0` at 0x82628770) and 0x826287A8 `lvx128 v125, r29, r24` then loads through it
// with no check. The console gets away with it because owner == E_ENTITYTYPE_RACECAR means the
// car table has the entity; it is still an unchecked deref and it is reproduced, not "fixed" --
// if this ever faults on the host, the bug is upstream in the owner byte, not here.
//
// ==========================================================================================
// TWO HEADER REQUESTS (both one declaration each; both stood in for with a file-local helper so
// this partfile links, and neither header is in this cluster's ownership):
//
//   REQUEST 1 -- GameShared/GameClasses/Physics/CgsRigidBody.h, public section:
//                    inline void SetUserIDB( u16 lu16UserIDB );
//                DWARF-attested: dwarfdump GameShared/GameClasses/Physics/CgsRigidBody.h:102
//                `void SetUserIDB(uint16_t);`, beside the already-committed GetIndex() (:94)
//                which reads the SAME field (`mId & 0xFFFF`). X360 shape, four sites in this
//                one function: `sth 0, 0x36(id) ; ld ; or <index & 0xFFFF> ; std`, i.e.
//                `mId = (mId & ~0xFFFFull) | lu16UserIDB;`. Stood in for by
//                SetRigidBodyIdUserIDB below -- fold the four call sites onto the method when
//                it lands.
//
//   REQUEST 2 -- GameShared/GameClasses/SceneManager/CgsVolumeInstanceId.h, public section:
//                    CgsSceneManager::EntityId GetEntityId() const;
//                DWARF-attested: this function's own DecFIGS scope lists
//                `CgsSceneManager::VolumeInstanceId::GetEntityId(...)` three times. The header
//                already owns GetEntityIDOwner() and GetEntityIDEntityIndex() over the same
//                embedded word; only the whole-word getter is missing. Stood in for by
//                GetVolumeInstanceEntityWord below. (⚠️ A CONCURRENT wave-Q4 owner is editing
//                that header right now -- its GetEntityIDOwner/SetVolumeIndex blocks are dated
//                today -- which is the second reason this partfile does not touch it.)
//
// ==========================================================================================
// LINK-LEVEL, INVISIBLE TO `cl /c` (gotcha 12). Every callee of this body is DECLARED, so the
// compile gate is green; these are the ones with NO BODY anywhere in the tree, i.e. the
// unresolved externals mounting this file adds:
//     PropManager::HandleContactWithLeanProp        @0x8260FB60  (parked, waveQ2/parked/)
//     PropManager::HandleContactWithTiltProp        @0x826108B8  (parked, waveQ2/parked/)
//     PropManager::ApplyPropRaceCarCollisionImpulse @0x825E3560  (parked, waveQ2/parked/)
// Everything else it calls is bodied: FindPropIndex / FindPartIndex / HasPropJustBeenRemoved /
// HasPartJustBeenRemoved / RoutePropVsRaceCarContactToDummyCar / ApplyAntiHerdingForce, plus
// PropPhysicsDataHeader::GetType, PropTypeData::IsLamppost, PropInstance::GetJointIndex and
// VehicleManager::GetTrafficPhysicsEntityIDFromGlobalEntityID_Safe -- but four of those TUs are
// NOT MOUNTED today. The full mount closure is in scratchpad/waveQ4/physcontact.owner.md.
// ==========================================================================================

#include "GameSource/Physics/PropManager/BrnPropManager.h"
#include "GameSource/Physics/PropManager/PropPhysics/BrnPropInstance.h"      // IsJointed / IsStatic / GetJointIndex
#include "GameSource/Physics/PropManager/PropPhysics/BrnPropPartInstance.h"  // GetType / GetPartId / GetPosition
#include "GameSource/Physics/PropManager/SharedIO/BrnPropInputInterface.h"   // KU_MAX_PHYSICAL_PROP{S,_PARTS} (the two assert bounds)
#include "GameSource/Physics/VehicleManager/BrnVehicleManager.h"             // GetRaceCarPhysics + the traffic remap
#include "GameSource/Physics/VehicleManager/VehiclePhysics/RaceCarPhysics.h" // ExternallySimulatedBody::GetTransform
#include "GameSource/World/BrnEntityTypes.h"                                 // BrnWorld::EEntityTypeID
#include "GameShared/GameClasses/Containers/CgsBitArray.h"                   // mBreakPropJoints.SetBit
#include "GameShared/GameClasses/Core/CgsAssert.h"                           // CGS_ASSERT
#include "GameShared/GameClasses/Physics/CgsPhysicsSimulationIO_Events.h"    // InAddPotentialContact
#include "GameShared/GameClasses/Physics/CgsRigidBody.h"                     // CgsPhysics::RigidBodyId
#include "GameShared/GameClasses/SceneManager/CgsEntityId.h"                 // EntityId::SetOwner
#include "GameShared/GameClasses/SceneManager/CgsVolumeInstanceId.h"         // VolumeInstanceId
#include "GameShared/GameClasses/SceneManager/SharedIO/CgsPotentialContact.h"// PotentialContact
#include "SharedClasses/Physics/Props/BrnPhysicsPropTypeData.h"              // PropTypeData / PropPartTypeData
#include "SharedClasses/Physics/Props/BrnPropEntityID.h"                     // BrnWorld::PropEntityID
#include "SharedClasses/Physics/Props/BrnPropPhysicsDataHeader.h"            // PropPhysicsDataHeader::GetType
#include "rw/math/vpu/vector3_operation.h"                                   // Dot / Subtract / Negate
#include "rw/math/vpu/vector4_operation.h"                                   // Splat (the VecFloat broadcast == vspltw)

namespace BrnPhysics
{
namespace Props
{
namespace
{
    // ⚠️ STANDS IN FOR HEADER REQUEST 1 -- CgsPhysics::RigidBodyId::SetUserIDB(uint16_t)
    // (DWARF CgsRigidBody.h:102). The field is the handle's low 16 bits, which is the SAME
    // field the committed RigidBodyId::GetIndex() (CgsRigidBody.h:70-74) already reads back;
    // the console spelling is `sth 0, +6(id) ; ld ; or index ; std`. Written once here rather
    // than four times inline so the geometry lives in ONE place until the method lands.
    inline void SetRigidBodyIdUserIDB( CgsPhysics::RigidBodyId& lrId, u16 lu16UserIDB )
    {
        lrId = CgsPhysics::RigidBodyId(
            ( static_cast<u64>( lrId ) & ~static_cast<u64>( 0xFFFFu ) )
            | static_cast<u64>( lu16UserIDB ) );
    }

    // ⚠️ STANDS IN FOR HEADER REQUEST 2 -- CgsSceneManager::VolumeInstanceId::GetEntityId().
    // The embedded entity word is the packed id's HIGH dword (the header's own
    // KU_ENTITY_ID_START_INDEX == 32); the console reads it as `ld ; srdi r,r,32`. The same
    // spelling is already committed in the mounted BrnPhysicsModuleBridgeFunctions.cpp, which
    // is this function's caller.
    inline u32 GetVolumeInstanceEntityWord( const CgsSceneManager::VolumeInstanceId& lrId )
    {
        return static_cast<u32>(
            lrId.muId >> CgsSceneManager::VolumeInstanceId::KU_ENTITY_ID_START_INDEX );
    }

    // PropTypeData's nested joint-type enum. The console compares mu8JointType against 1 then
    // 2, and the assert string baked at 0x826288E8 names the second value:
    // "lpType->mu8JointType == BrnPhysics::Props::PropTypeData::E_TILT".
    // ⚠️ The enum has NO home in this tree (BrnPhysicsPropTypeData.h declares only the u8 field
    // and its GetJointType()/GetLeanState() accessors) and this cluster does not own that
    // header, so the two values are named locally. E_TILT's NAME is attested by the assert
    // text; the lean value's name is NOT -- only its value (1) is measured. Do not promote the
    // authored name into a shared header without recovering the real enumerator.
    static const u8 KU8_JOINT_TYPE_LEAN = 1;   // ⚠️ AUTHORED NAME -- value measured, name is not
    static const u8 KU8_JOINT_TYPE_TILT = 2;   // == PropTypeData::E_TILT (assert text @0x826288E8)
}

// =============================================================================================
// SetupAndValidatePropContact -- X360 0x82628190, 573 instructions.
// DecFIGS: dwarfdump BrnPropManager.h:191 (declaration) / BrnPropManager.cpp:2653 (scope,
// source :1300 onwards). Parameter names and every local name below are the DWARF's.
// =============================================================================================
bool
PropManager::SetupAndValidatePropContact(
    CgsPhysics::PhysicsSimulationIO::InAddPotentialContact*   lpOutContact,
    const CgsSceneManager::SceneManagerIO::PotentialContact*  lpInPotentialContact,
    BrnPhysics::Vehicle::VehicleManager*                      lpVehicleManager,
    CgsPhysics::PhysicsSimulationIO::InputBuffer*             lpSimInputBuffer,
    PropRaceCarContactBuffer*                                 /*lpPropRaceCarContactBuffer*/,
    CgsPhysics::RigidBodyId                                   /*lWorldRigidBodyId*/,
    bool                                                      lbOtherEntityIsFrozen,
    f32                                                       lfTimeStep )
{
    namespace vpu = rw::math::vpu;

    // -----------------------------------------------------------------------------------------
    // (1) The two traffic remaps -- 0x826281C0..0x8262826C. DWARF blocks at :1313/:1314 (side A)
    //     and :1333/:1334 (side B) name `bool lbTrafficExists` + `EntityId lDummyTrafficEntityId`
    //     once per side, which is why this is written twice rather than folded into a loop.
    // -----------------------------------------------------------------------------------------
    if ( lpInPotentialContact->muVolumeInstanceIdA.GetEntityIDOwner()
             == static_cast<u8>( BrnWorld::E_ENTITYTYPE_TRAFFIC_VEHICLE ) )
    {
        // ⚠️ HEADER DEFECT (REPORTED, NOT FIXED HERE -- VehicleManager/** is outside this
        // cluster's ownership). BrnVehicleManager.h:1246 declares the out-parameter as the
        // BrnCommonTypes.h storage-word `::EntityId`, but the DWARF declares it as the real
        // handle class (dwarfdump BrnVehicleManager.h:1028 `bool
        // GetTrafficPhysicsEntityIDFromGlobalEntityID_Safe(EntityId, EntityId*)`, and this
        // function's own DecFIGS scope resolves that name by calling
        // `CgsSceneManager::EntityId::SetOwner` on the very slot it wrote). The console proves
        // they are one object: 0x826281E0 passes `r1+var_100` as the out-param and 0x826281FC
        // passes THE SAME slot as `this` to CgsSceneManager::EntityId::SetOwner. So the two
        // spellings are one 32-bit word and the hop below is exact, not a reinterpretation --
        // it is written by VALUE rather than by cast for exactly that reason.
        ::EntityId lRawTrafficEntityId;
        const bool lbTrafficExists =
            lpVehicleManager->GetTrafficPhysicsEntityIDFromGlobalEntityID_Safe(
                GetVolumeInstanceEntityWord( lpInPotentialContact->muVolumeInstanceIdA ),
                &lRawTrafficEntityId );
        if ( !lbTrafficExists )
        {
            return false;                                             // 0x826281F4 beq
        }

        CgsSceneManager::EntityId lDummyTrafficEntityId( lRawTrafficEntityId.muValue );
        lDummyTrafficEntityId.SetOwner(
            static_cast<u8>( BrnWorld::E_ENTITYTYPE_PROP_COLLISION_TRAFFIC ) );   // li r4,0xC

        CgsPhysics::RigidBodyId lIdA( lpOutContact->mIDA );
        lIdA.SetEntityId( lDummyTrafficEntityId );
        lpOutContact->mIDA = lIdA;
    }

    if ( lpInPotentialContact->muVolumeInstanceIdB.GetEntityIDOwner()
             == static_cast<u8>( BrnWorld::E_ENTITYTYPE_TRAFFIC_VEHICLE ) )
    {
        ::EntityId lRawTrafficEntityId;                               // see the block above
        const bool lbTrafficExists =
            lpVehicleManager->GetTrafficPhysicsEntityIDFromGlobalEntityID_Safe(
                GetVolumeInstanceEntityWord( lpInPotentialContact->muVolumeInstanceIdB ),
                &lRawTrafficEntityId );
        if ( !lbTrafficExists )
        {
            return false;                                             // 0x82628248 beq
        }

        CgsSceneManager::EntityId lDummyTrafficEntityId( lRawTrafficEntityId.muValue );
        lDummyTrafficEntityId.SetOwner(
            static_cast<u8>( BrnWorld::E_ENTITYTYPE_PROP_COLLISION_TRAFFIC ) );

        CgsPhysics::RigidBodyId lIdB( lpOutContact->mIDB );
        lIdB.SetEntityId( lDummyTrafficEntityId );
        lpOutContact->mIDB = lIdB;
    }

    // -----------------------------------------------------------------------------------------
    // (2) The prop friction/restitution seed -- 0x82628270..0x82628294.
    // -----------------------------------------------------------------------------------------
    lpOutContact->mStaticFriction  = mfStaticFriction;                // lfs 0x74 / stfs 0x40
    lpOutContact->mDynamicFriction = mfDynamicFriction;               // lfs 0x78 / stfs 0x44
    lpOutContact->mRestitution     = KF_PROP_RESTITUTION;             // flt_82F2A398 / stfs 0x48

    // -----------------------------------------------------------------------------------------
    // (3) Which side is the prop -- 0x8262829C..0x826282AC (DWARF `bool lbPropIsEntityA` :1355).
    // -----------------------------------------------------------------------------------------
    const bool lbPropIsEntityA =
        ( CgsPhysics::RigidBodyId( lpOutContact->mIDA ).GetEntityIDOwner()
              == static_cast<u8>( BrnWorld::E_ENTITYTYPE_PROP ) );

    // -----------------------------------------------------------------------------------------
    // (4) The mirrored prop-side resolution -- 0x826282B0..0x8262855C.
    //     DWARF locals :1300..:1306.
    // -----------------------------------------------------------------------------------------
    CgsPhysics::RigidBodyId lPropRigidBodyId(
        lbPropIsEntityA ? lpOutContact->mIDA : lpOutContact->mIDB );          // DWARF :1302
    const CgsPhysics::RigidBodyId lOtherRigidBodyId(
        lbPropIsEntityA ? lpOutContact->mIDB : lpOutContact->mIDA );          // DWARF :1303

    const PropEntityID lPropEntityId(                                          // DWARF :1301
        GetVolumeInstanceEntityWord( lbPropIsEntityA
                                         ? lpInPotentialContact->muVolumeInstanceIdA
                                         : lpInPotentialContact->muVolumeInstanceIdB ) );

    const u8 lu8OtherOwner = lOtherRigidBodyId.GetEntityIDOwner();

    // The prop's own contact-generation legs stamp their slot index into the per-primitive tag,
    // which AddContactResultsToQueue copies into muPolyTagA -- see THE POLY-TAG SEAM above. Only
    // a world contact carries it; anything else has to be searched for.
    const bool lbOtherIsWorld = ( lu8OtherOwner == static_cast<u8>( BrnWorld::E_ENTITYTYPE_WORLD ) );

    int32_t       liPhysicsIndex = KI_PROP_INDEX_NOT_FOUND;                    // DWARF :1300
    PropInstance* lpPropInstance = NULL;                                       // DWARF :1304

    if ( lPropEntityId.GetPartIndex() == 0u )
    {
        liPhysicsIndex = lbOtherIsWorld
                             ? static_cast<int32_t>( lpInPotentialContact->muPolyTagA )
                             : FindPropIndex( lPropEntityId );

        CGS_ASSERT( liPhysicsIndex < static_cast<int32_t>( KU_MAX_PHYSICAL_PROPS ),
                    "liPhysicsIndex < static_cast< int32_t > ( KU_MAX_PHYSICAL_PROPS )" );  // :1384

        if ( liPhysicsIndex == KI_PROP_INDEX_NOT_FOUND )
        {
            return false;                                             // 0x82628344 beq
        }
        if ( HasPropJustBeenRemoved( lPropEntityId, liPhysicsIndex ) )
        {
            return false;                                             // 0x82628360 bne
        }

        lpPropInstance = &mpaPropInstances[ liPhysicsIndex ];          // mulli 0x70 == the stride
    }
    else
    {
        liPhysicsIndex = lbOtherIsWorld
                             ? static_cast<int32_t>( lpInPotentialContact->muPolyTagA )
                             : FindPartIndex( lPropEntityId );

        CGS_ASSERT( liPhysicsIndex < static_cast<int32_t>( KU_MAX_PHYSICAL_PROP_PARTS ),
                    "liPhysicsIndex < static_cast< int32_t > ( KU_MAX_PHYSICAL_PROP_PARTS )" );  // :1408

        if ( liPhysicsIndex == KI_PROP_INDEX_NOT_FOUND )
        {
            return false;                                             // 0x826283C4 beq
        }
        if ( HasPartJustBeenRemoved( lPropEntityId, liPhysicsIndex ) )
        {
            return false;                                             // 0x826283E0 bne
        }
    }

    // The two contact points, PROP SIDE FIRST (v124 is always the prop's point, v126 the
    // other body's -- measured in both mirrored branches), and the out event's prop-side
    // user-id-B. DWARF :1305 / :1306.
    const Vector3 lPointOnProp           = lbPropIsEntityA ? lpInPotentialContact->mPointOnA
                                                           : lpInPotentialContact->mPointOnB;
    const Vector3 lPointOnOtherRigidBody = lbPropIsEntityA ? lpInPotentialContact->mPointOnB
                                                           : lpInPotentialContact->mPointOnA;
    {
        CgsPhysics::RigidBodyId lOutPropSideId(
            lbPropIsEntityA ? lpOutContact->mIDA : lpOutContact->mIDB );
        SetRigidBodyIdUserIDB( lOutPropSideId, static_cast<u16>( liPhysicsIndex ) );
        if ( lbPropIsEntityA ) { lpOutContact->mIDA = lOutPropSideId; }
        else                   { lpOutContact->mIDB = lOutPropSideId; }
    }

    // -----------------------------------------------------------------------------------------
    // (5) The jointed/static prop gate -- 0x82628560..0x82628728.
    // -----------------------------------------------------------------------------------------
    if ( lpPropInstance != NULL
         && ( lpPropInstance->IsJointed() || lpPropInstance->IsStatic() ) )   // lbz 0x6C / lbz 0x6D
    {
        if ( lbOtherEntityIsFrozen )
        {
            return false;                                             // 0x82628588 bne
        }

        if ( lbOtherIsWorld )
        {
            // ⭐ THE SMASH-GATE BREAK. DWARF block :1497/:1499.
            Vector3 lFlatNormal = lpInPotentialContact->mNormal;
            lFlatNormal.y = 0.0f;   // the DWARF spells this SetY(GetVecFloat_Zero()); this
                                    // tree's Vector3 has named lanes and no SetY -- same store.

            // `vmsum3fp128` is the 3-lane dot and it BROADCASTS, so the console's all-lanes
            // CR6[0] test below reduces exactly to this scalar compare.
            const f32 lvfPenetration =
                vpu::Dot( lFlatNormal, vpu::Subtract( lpInPotentialContact->mPointOnB,
                                                      lpInPotentialContact->mPointOnA ) );

            // gotcha 4: `vcmpgtfp.` + "all lanes true" -- an unordered compare DROPS the contact.
            if ( !( lvfPenetration > KVF_MAX_LEAN_PROP_WORLD_PENETRATION.x ) )
            {
                return false;                                         // 0x826285D8 beq
            }

            // GetJointIndex carries its own IsJointed() tripwire; the console additionally
            // inlines BitArray<15>::SetBit's index assert (baked CgsBitArray.h:222), which the
            // committed assert-free CgsBitArray owns rather than this body.
            mBreakPropJoints.SetBit(
                static_cast<u32>( lpPropInstance->GetJointIndex() ) );

            // ⚠️ The contact is dropped EITHER WAY: the SetBit falls straight into the console's
            // `li r3,0` return at 0x826286BC. A jointed prop resolves through the joint, never
            // through the simulation's contact solver.
            return false;
        }

        if ( lu8OtherOwner == static_cast<u8>( BrnWorld::E_ENTITYTYPE_PROP )
             && ( static_cast<u32>( lOtherRigidBodyId.GetEntityId() ) & 0x3FFu ) == 0u )
        {
            // A leaning/static prop may not lean on another leaning/static prop.
            // DWARF locals :1513 (liOtherPropIndex) / :1516 (lpOtherProp).
            // ⚠️ the part-index test above is the console's raw `clrlwi r11,r14,22` on the entity
            // word, taken BEFORE the PropEntityID is constructed; spelled with the same mask
            // rather than through PropEntityID::GetPartIndex() so the ORDER matches (that getter
            // runs an out-of-line AssertIsProp the console does not emit here).
            const PropEntityID lOtherEntityId(
                static_cast<u32>( lOtherRigidBodyId.GetEntityId() ) );
            const int32_t liOtherPropIndex = FindPropIndex( lOtherEntityId );
            if ( liOtherPropIndex == KI_PROP_INDEX_NOT_FOUND )
            {
                return false;                                         // 0x82628708 beq
            }

            const PropInstance* lpOtherProp = &mpaPropInstances[ liOtherPropIndex ];
            if ( lpOtherProp->IsJointed() )
            {
                return false;                                         // 0x82628720 bne
            }
            if ( lpOtherProp->IsStatic() )
            {
                return false;                                         // 0x8262872C bne
            }
        }
    }

    // -----------------------------------------------------------------------------------------
    // (6) The race-car leg -- 0x82628730..0x82628A00.
    // -----------------------------------------------------------------------------------------
    if ( lu8OtherOwner == static_cast<u8>( BrnWorld::E_ENTITYTYPE_RACECAR ) )
    {
        // flt_82001CC0 == 0.0f into both -- a prop/car contact carries no surface friction.
        lpOutContact->mStaticFriction  = 0.0f;                        // stfs 0x40
        lpOutContact->mDynamicFriction = 0.0f;                        // stfs 0x44

        // DWARF :1538. The console INLINES this accessor (the eight-slot maRaceCarEntityIDs
        // scan at +43584 then `mulli 0x1460 ; addi 0x740`); it is re-rolled into the named call.
        // ⚠️ NOT null-checked -- see the banner. The console's own deref is unguarded.
        BrnPhysics::Vehicle::RaceCarPhysics* lpRaceCar =
            lpVehicleManager->GetRaceCarPhysics( lOtherRigidBodyId );

        if ( lPropEntityId.GetPartIndex() == 0u )
        {
            // ---- whole prop ------------------------------------------------------------------
            // DWARF :1543..:1557. lpProp aliases lpPropInstance above (the console recomputes
            // &mpaPropInstances[liPhysicsIndex]); both locals are the DWARF's.
            PropInstance*         lpProp        = &mpaPropInstances[ liPhysicsIndex ];   // :1544
            const Matrix44Affine  lCarTransform = lpRaceCar->GetTransform();             // :1543
            const PropTypeData*   lpPropType    = mpPhysicsData->GetType( lpProp->GetTypeId() ); // :1545

            if ( !lpProp->IsStatic() || lpProp->IsJointed() )
            {
                RoutePropVsRaceCarContactToDummyCar( lbPropIsEntityA, lpOutContact );
            }

            SetRigidBodyIdUserIDB( lPropRigidBodyId, static_cast<u16>( liPhysicsIndex ) );

            // :1556 / :1557 -- flip the normal so it points at the car.
            // gotcha 4: the console is `vcmpgefp` then `vnot128`, so an unordered compare FLIPS.
            const f32 lfNormalDotToCar =
                vpu::Dot( lpInPotentialContact->mNormal,
                          vpu::Subtract( lCarTransform.wAxis, lPointOnOtherRigidBody ) );
            const bool    lNormalTowardsCar = !( lfNormalDotToCar >= 0.0f );
            const Vector3 lNormal           = lNormalTowardsCar
                                                  ? vpu::Negate( lpInPotentialContact->mNormal )
                                                  : lpInPotentialContact->mNormal;

            if ( !lpPropType->IsLamppost() )
            {
                ApplyAntiHerdingForce( lpSimInputBuffer,
                                       lpRaceCar,
                                       lPropRigidBodyId,
                                       lpProp->GetTransform().wAxis,          // lvx +0x30
                                       vpu::Splat( lpPropType->GetMass() ),   // lvlx +0x38 / vspltw
                                       lpProp->GetLinearVelocity(),           // lvx +0x40
                                       lpInPotentialContact->mNormal );       // the UNREAD 7th
            }

            if ( lpPropInstance->IsJointed() )
            {
                // DWARF :1575 -- the joint resolvers re-look-up the type through the instance.
                const PropTypeData* lpType =
                    mpPhysicsData->GetType( lpPropInstance->GetTypeId() );

                if ( lpType->GetJointType() == KU8_JOINT_TYPE_LEAN )
                {
                    HandleContactWithLeanProp( lpPropInstance, liPhysicsIndex, lpType, lpRaceCar,
                                               lNormal, lPointOnProp, lPointOnOtherRigidBody,
                                               lpOutContact, lbPropIsEntityA, lfTimeStep );
                }
                else
                {
                    CGS_ASSERT( lpType->GetJointType() == KU8_JOINT_TYPE_TILT,
                                "lpType->mu8JointType == BrnPhysics::Props::PropTypeData::E_TILT" ); // :1592
                    HandleContactWithTiltProp( lpPropInstance, liPhysicsIndex, lpType, lpRaceCar,
                                               lNormal, lPointOnProp, lPointOnOtherRigidBody,
                                               lpOutContact, lbPropIsEntityA, lfTimeStep );
                }
            }
            else
            {
                ApplyPropRaceCarCollisionImpulse( lpRaceCar, lpProp, lpPropType,
                                                  lNormal, lPointOnOtherRigidBody );
            }
        }
        else
        {
            // ---- a broken-off part ------------------------------------------------------------
            // DWARF :1613..:1618. The part index is searched on the PROP RIGID BODY's own entity
            // word (not on lPropEntityId) -- `srdi r11, r20, 32` at 0x8262894C.
            const PropEntityID lPartEntityId(
                static_cast<u32>( lPropRigidBodyId.GetEntityId() ) );
            const int32_t     liPartIndex = FindPartIndex( lPartEntityId );          // :1614
            PropPartInstance* lpPart      = &mpaPartInstances[ liPartIndex ];        // :1617

            CgsPhysics::RigidBodyId lPartRigidBodyId = lPropRigidBodyId;             // :1613
            SetRigidBodyIdUserIDB( lPartRigidBodyId, static_cast<u16>( liPartIndex ) );

            // The part's own type record: the PROP type's part table indexed by the part id.
            // ⚠️ The console walks it at its own 48-byte stride (`(id + id*2) << 4`) off the
            // console +0x40 slot; on the host both the slot and the stride are the compiler's
            // (PropPartTypeData is 64 bytes here) -- gotcha 1, reached by name.
            const PropTypeData* lpPropType = mpPhysicsData->GetType( lpPart->GetType() );
            const f32 lfMass = lpPropType->GetParts()[ lpPart->GetPartId() ].GetMass();  // :1618

            ApplyAntiHerdingForce( lpSimInputBuffer,
                                   lpRaceCar,
                                   lPartRigidBodyId,
                                   lpPart->GetPosition(),              // lvx +0x00
                                   vpu::Splat( lfMass ),               // lfs +0x20 / vspltw
                                   lpPart->GetLinearVelocity(),        // lvx +0x10
                                   lpInPotentialContact->mNormal );    // the UNREAD 7th

            RoutePropVsRaceCarContactToDummyCar( lbPropIsEntityA, lpOutContact );
        }
    }

    // -----------------------------------------------------------------------------------------
    // (7) The shared tail -- 0x82628A04..0x82628A80. Reachable with the prop as entity B only
    //     when lu8OtherOwner != PROP (see the banner), so writing mIDB is always the OTHER side.
    // -----------------------------------------------------------------------------------------
    if ( lu8OtherOwner == static_cast<u8>( BrnWorld::E_ENTITYTYPE_PROP ) )
    {
        const u32          luOtherEntityWord = static_cast<u32>( lOtherRigidBodyId.GetEntityId() );
        const PropEntityID lOtherEntityId( luOtherEntityWord );

        const int32_t liOtherIndex = ( ( luOtherEntityWord & 0x3FFu ) == 0u )
                                         ? FindPropIndex( lOtherEntityId )   // DWARF :1639
                                         : FindPartIndex( lOtherEntityId );  // DWARF :1651
        if ( liOtherIndex == KI_PROP_INDEX_NOT_FOUND )
        {
            return false;                                             // 0x82628A50 beq
        }

        CgsPhysics::RigidBodyId lIdB( lpOutContact->mIDB );
        SetRigidBodyIdUserIDB( lIdB, static_cast<u16>( liOtherIndex ) );
        lpOutContact->mIDB = lIdB;
    }

    return true;                                                      // 0x82628A6C li r3,1
}

}
}

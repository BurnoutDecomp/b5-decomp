// =================================================================================================
// GameSource/Physics/VehicleManager/BrnVehicleManager_CreateRemoveEvents.cpp
//
//     VehicleManager::SetAllNetworkRaceCarsHidden  @0x825E9380  (175)   -- BODIED here.
//
// Slice TU (the home BrnVehicleManager.cpp is still unmounted) -- the same shape as the sibling
// _Prepare.cpp / _ReadUpdatedBodies.cpp / _TractionLineTests.cpp / _MaintenanceEvents.cpp slices.
//
// =================================================================================================
// ⛔⛔ THE HANDLING-BODY HANDLE IS EIGHT BYTES -- and that is why this file is one function long.
//
// This TU was written on 2026-08-11 to carry THREE bodies: ProcessRemoveEvents @0x826160C8 (426),
// AddRaceCarDeformationModel @0x825E9118 (153) and the one below. The first two were fully
// recovered from the asm, written out, compiled -- and then DELETED rather than shipped. The reason
// is a fork this tree already knew about and had explicitly deferred, and the asm read for this
// wave is EXACTLY the evidence that deferral asked a future wave to bring.
//
// BrnDeformationManager.h's committed banner says, verbatim:
//     "ONLY THESE THREE ARE FIXED. THE NAME IS STILL USED FOR A DIFFERENT, 32-BIT HANDLE
//      ELSEWHERE IN THIS SUBSYSTEM ... So the handling-body handle may legitimately be a different
//      32-bit id, or those bodies may be wrong -- the DWARF spells both `RigidBodyId` and cannot
//      separate them. Deciding it needs the X360 asm for AddDeformationModel @0x825A95E0's
//      caller and the Process*Events drains."
//
// ⭐ BOTH OF THOSE ARE THE FUNCTIONS THIS WAVE READ, AND THEY DECIDE IT. Every access is EIGHT
// bytes wide, in both directions:
//     ProcessRemoveEvents         0x82616444  ldx  r11, r9, r25        <- maRaceCarHandlingBodyIDs[i]
//                                 0x82616448  std  r11, var_D0(r1)     -> the Deactivate event's
//                                                                         mHandlingBodyID
//                                 0x826164B0  ldx  r11, r11, r25 ; std r11, var_E8(r1)
//                                                                      -> the Remove event's field
//     AddRaceCarDeformationModel  0x825E9324  ldx  r8,  r23, r29 ; std r8, var_C8(r1)
//                                 0x825E9358  ld   r5,  0(r22)         -> AddDeformationModel's
//                                                                         RigidBodyId argument
//                                 0x825E932C  ldx  r4,  r20, r29       -> its ResourceHandle
//                                                                         argument, ALSO 8 bytes
// `ldx`/`std`/`ld` are the 64-bit forms; the 32-bit forms (`lwzx`/`stw`/`lwz`) appear all over both
// functions for the genuinely 32-bit fields, so this is a contrast within one body, not a reading
// of one isolated instruction.
//
// ⭐ AND IT EXPLAINS THE CONTRADICTION THE BANNER COULD NOT RESOLVE. It observed that the committed
// BrnDeformationManager.cpp bodies do `mHandlingBodyID.muValue >> 24` for the owner and
// `(... >> 10) & 0x3FFF` for the index, which "CONTRADICTS CgsRigidBody.h's documented packing
// (high dword = EntityId, low dword = index)". AddRaceCarDeformationModel does the SAME two
// extractions -- but on the HIGH DWORD of the 64-bit value:
//     0x825E9218  srdi   r11, r11, 32        <- take the high dword FIRST
//     0x825E9220  clrlwi r28, r11, 0
//     0x825E9224  srwi   r11, r28, 24        <- ... then the owner byte
//     0x825E9240  extrwi r11, r28, 14, 8     <- ... then the 14-bit index
// i.e. the ARITHMETIC is right and the WIDTH is wrong: the tree's 4-byte stand-in makes those
// bodies read the low dword. And for a race car the low dword is IDENTICALLY ZERO -- the create
// arm builds the id as `sldi r26, <entityWord>, 32` (@0x8261707C), so the whole value lives in the
// high half.
//
// => PASSING maRaceCarHandlingBodyIDs[i] THROUGH THE 4-BYTE `RigidBodyId` DELIVERS ZERO, the
// deformation manager finds no model, and NOTHING ASSERTS. That is [[silent-drop-stubs]] exactly,
// and it is the failure mode this campaign has paid for most often. The honest move is to leave
// ProcessRemoveEvents a LOUD gate (BrnVehicleManager_MaintenanceEvents.cpp) and hand the next wave
// the measurement instead of a body that compiles and lies.
//
// ⚠️ A SECOND, INDEPENDENT FORK ON THE SAME CALL: `BrnPhysics::Deformation::ResourceHandle`
// (BrnDeformationEvents.h:22, `struct ResourceHandle { u32 muValue; }`) against
// CgsResource::ResourceHandle -- which is what maRaceCarModelHandles is really made of as of this
// wave. The console loads that handle with a single `ldx` too.
//
// ⛔ WHAT THE NEXT WAVE MUST NOT DO: widen `::RigidBodyId` in BrnCommonTypes.h and call it fixed.
// Five `mHandlingBodyID` fields and the BrnDeformationManager.cpp bodies that shift them all move
// together, in a MOUNTED subsystem, and the event records' field offsets move with them. It is its
// own wave with its own boot test.
// =================================================================================================
//
// -------------------------------------------------------------------------------------------------
// ⛔ AND ProcessCreateEvents @0x82616770 (1067) IS STILL THE GATE IN _MaintenanceEvents.cpp.
// Its ONLY observable -- the mUsedRaceCars bit -- switches on four already-mounted per-frame loops
// against a car whose mTransform is seated by exactly one thing: the vtable slot +0x30 vcall to
// RaceCarPhysics::Prepare @0x82639CB8 (BODIED THIS WAVE, RaceCarPhysics.cpp), which forwards into
// VehiclePhysics::Prepare @0x82637C80 -- 306 insns, ~40 member seeds at +0x1114/+0x1118/+0x111C/
// +0x1120/+0x1128/+0x1158/+0x1220/+0x10D4/+0x10F7/+0x135A..+0x1362/+0x13DC plus a dozen VMX stores,
// and DECLARE-ONLY in this tree. Setting the bit without it would start the already-mounted
// gravity+integrate loop on a car whose transform is still the identity -- the compensating-pair
// failure the campaign brief flags. ⭐ THE CREATE DRAIN'S REAL BLOCKER IS VehiclePhysics::Prepare;
// no brief before this one costed it as the blocker rather than as a line item.
// -------------------------------------------------------------------------------------------------

#include "GameSource/Physics/VehicleManager/BrnVehicleManager.h"

namespace BrnPhysics
{
namespace Vehicle
{
    // =============================================================================================
    // VehicleManager::SetAllNetworkRaceCarsHidden @0x825E9380 (175 insns).
    //
    // Almost all 175 instructions are the INLINED CgsBitArray<8> walk: GetFirstNonZeroBit's
    // `addi r10,r11,-1 ; and ; subf ; cntlzd ; subf ; addi 0x3F` idiom (@0x825E93D4..0x825E93EC),
    // the per-field rescan at the tail (@0x825E95C8..0x825E9628), and the CgsBitArray.h:203 bounds
    // assert with its two StrStreamBase `<<` chains. The body is the two lines below.
    //
    // r19 == `addis r19,this,1 ; addi r19,r19,-0x5340` == this + 44224 == mUsedRaceCars, and
    // `addi r11,r31,0x2B28 ; slwi 2 ; lwzx r11,r11,r15` == this + 44192 + 4*i == maeRaceCarTypes[i].
    // Both are reached BY NAME here, so neither console offset appears in code.
    // The forward is `mr r5,r14 ; mr r4,r31 ; mr r3,r15 ; bl SetNetworkRaceCarHidden` -- this
    // function's own second parameter, unchanged; its one caller (ProcessCreateEvents' tail
    // @0x826177CC) passes `li r4, 1`.
    //
    // ⭐ NO LIVE CALLER YET (that caller is the still-gated create arm). Mounted anyway: zero bytes
    // enter the exe under /OPT:REF, and the create arm's link closure is enforced now rather than
    // discovered later -- the [[shadowing-redeclarations]] mitigation this project already relies on.
    // =============================================================================================
    void VehicleManager::SetAllNetworkRaceCarsHidden(s32 liFrames)
    {
        for (s32 liCar = mUsedRaceCars.GetFirstNonZeroBit();
             liCar >= 0 && liCar < E_ACTIVE_RACE_CAR_INDEX_COUNT;
             liCar = mUsedRaceCars.GetNextNonZeroBit(liCar))
        {
            if (maeRaceCarTypes[liCar] == BrnWorld::E_RACE_CAR_TYPE_NETWORK)
            {
                SetNetworkRaceCarHidden(static_cast<EActiveRaceCarIndex>(liCar), liFrames);
            }
        }
    }
}
}

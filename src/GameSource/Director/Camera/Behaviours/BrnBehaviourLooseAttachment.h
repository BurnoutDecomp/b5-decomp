#ifndef GAMESOURCE_DIRECTOR_CAMERA_BEHAVIOURS_BRN_BEHAVIOUR_LOOSE_ATTACHMENT_H
#define GAMESOURCE_DIRECTOR_CAMERA_BEHAVIOURS_BRN_BEHAVIOUR_LOOSE_ATTACHMENT_H

#include "types.hpp"
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT (SetParameters type assert + race-car index asserts)

// ============================================================================
// GameSource/Director/Camera/Behaviours/BrnBehaviourLooseAttachment.h
//
// BrnDirector::Camera::BehaviourLooseAttachment -- the "loose attachment" camera behaviour (a
// camera softly tethered to a race car / target, installed by the new-car-joined and shutdown-
// takedown moments and the testbed arbitrator state). HOME for the four BehaviourLooseAttachment
// class slices this TU bodies:
//   - AttachTo      @0x821F4458  (bind the attachment to a race car; VehicleRef block @+0x314)
//   - Get           @0x821FAA58  (return &the embedded sub-object @+0x20, or null if a flag is set)
//   - SetParameters @0x821F43E8  (adopt a loose-attachment param block; type tag == 11)
//   - SetTarget     @0x821F44B8  (bind the target to a race car; VehicleRef block @+0x304)
// The full behaviour (Construct/Prepare/Update/the rig) and the Behaviour base land with their own
// TUs; this header models only the members these four functions touch, BY NAME, at their asm-
// attested offsets. Reserved byte spans place them exactly.
// ----------------------------------------------------------------------------

namespace BrnDirector
{
namespace Camera
{

// FLAG: minimal slice of the camera-behaviour type tag. Each behaviour carries a type id in the
//   leading word of its Parameters block; SetParameters asserts the block's id is the
//   loose-attachment one. The console value for eBehaviourLooseAttachment is 11 (the asm at
//   0x821F4408 compares the block's first word against 0xB). Replace with the real EBehaviourType
//   enum when the Behaviour base TU lands; the enumerator's VALUE (11) is asm.
enum EBehaviourTypeLooseAttachment
{
    eBehaviourLooseAttachment = 11
};

// FLAG: the upper bound the race-car index asserts enforce. The console value for
//   BrnPhysics::Vehicle::ku8MaxNumRaceCars is 8 (the asm at 0x821F4470 / 0x821F44D0 compares the
//   race-car index against 8). Replace with the real BrnPhysics::Vehicle constant when that TU
//   lands; the VALUE (8) is asm.
enum { KU_MAX_NUM_RACE_CARS = 8 };

class BehaviourLooseAttachment
{
public:

    // The loose-attachment parameter block: a type tag in its leading word plus behaviour-specific
    // data. GetType returns the tag SetParameters asserts on.
    class Parameters
    {
    public:
        EBehaviourTypeLooseAttachment GetType() const
        {
            return static_cast<EBehaviourTypeLooseAttachment>(meType);
        }

        s32 meType;        // +0x00  the behaviour type tag (eBehaviour*)
        s32 miParamWord1;  // +0x04  first behaviour-specific word (cached by SetParameters)
    };

    // FLAG: the +0x20 sub-object Get exposes is an embedded behaviour sub-object (the attachment
    //   transform / source). Its concrete type lands with the full behaviour TU; modelled here as
    //   an opaque embedded sub-object so the accessor returns its address at the asm-attested
    //   offset. Get returns null instead when the +0x32C "no result" flag is set.
    class SubObject;

    // Bind the attachment to a race car: record the race-car index, mark valid / set, assert the
    // index is in range. @0x821F4458 (VehicleRef block @+0x314).
    void AttachTo(s32 meRaceCarIndex);

    // Return the address of the embedded sub-object at +0x20, or null if the +0x32C flag is set.
    // @0x821FAA58.
    SubObject* Get();

    // Adopt a loose-attachment parameter block: assert it carries the loose-attachment type tag,
    // then cache its first word at +0x10 and store the pointer at +0x324. @0x821F43E8.
    void SetParameters(const Parameters* lpParameters);

    // Bind the target to a race car: record the race-car index, mark valid / set, assert the index
    // is in range. @0x821F44B8 (VehicleRef block @+0x304).
    void SetTarget(s32 meRaceCarIndex);

    // FLAG: only the members these four functions touch are modelled at their asm-attested offsets.
    //   The layout uses SIZE-STABLE fields only in the pinned region (the X360 is a 4-byte-pointer
    //   build; this PC reconstruction is 64-bit, so a real pointer here would be 8 bytes and shift
    //   every later offset). The vtable + the adopted-parameter pointer are therefore the size-
    //   stable raw slots the X360 stores via `stw` (32-bit); the typed pointer is reached through
    //   the by-name accessors below, so by-name access stays type-correct. All fields are public
    //   so the file-scope offsetof pins in the .cpp can verify the (now exact) layout. The rest of
    //   the loose-attachment rig lands with the full behaviour TU; reserved spans place each field.
    u8    maHead000[0x10];                     // +0x000 .. +0x00F  vtable + rig head (X360 4B ptr slot)
    s32   mParamWord1;                         // +0x010  cached lpParameters->miParamWord1
    u8    maReserved014[0x20 - 0x14];          // +0x014 .. +0x01F (rig members not modelled here)
    u8    maSubObject[0x304 - 0x20];           // +0x020  embedded sub-object (&-of by Get)

    // --- mTarget (Behaviour::VehicleRef) sub-block SetTarget writes, +0x304 .. +0x313 ---
    s32   miTargetSet;                         // +0x304  target-set flag (= 1)
    s32   meTargetRaceCarIndex;                // +0x308  the target race car index
    s32   miTargetField30C;                    // +0x30C  cleared to 0 by SetTarget
    u8    mbTargetField310;                    // +0x310  flag set (= 1) by SetTarget
    u8    maReserved311[0x314 - 0x311];        // +0x311 .. +0x313 (VehicleRef tail not modelled)

    // --- mAttachment (Behaviour::VehicleRef) sub-block AttachTo writes, +0x314 .. +0x323 ---
    s32   miAttachSet;                         // +0x314  attach-set flag (= 1)
    s32   meAttachRaceCarIndex;                // +0x318  the attachment race car index
    s32   miAttachField31C;                    // +0x31C  cleared to 0 by AttachTo
    u8    mbAttachField320;                    // +0x320  flag set (= 1) by AttachTo
    u8    maReserved321[0x324 - 0x321];        // +0x321 .. +0x323 (VehicleRef tail not modelled)

    // +0x324 (X360): the adopted parameter block pointer (the console stores it via `stw`, a
    // 4-byte slot). On this 64-bit reconstruction a real 8-byte pointer cannot live mid-struct
    // without breaking the pinned offsets, so the typed pointer (mpParameters) is appended at the
    // tail and reached by name; this reserved slot holds the console's 4-byte pointer position.
    u32   muParametersSlot;                    // +0x324  adopted parameter block (X360 4B ptr slot)
    u8    maReserved328[0x32C - 0x328];        // +0x328 .. +0x32B (rig members not modelled here)
    u8    mbNoResult;                          // +0x32C  when set, Get returns null

    // x64 typed view of the adopted parameter pointer (the by-name, type-correct store target).
    // Appended at the tail so it never disturbs the pinned offsets above; the X360 packs the same
    // pointer into the 4-byte slot at +0x324.
    const Parameters* mpParameters;
};

// ----------------------------------------------------------------------------
// BrnDirector::Camera::BehaviourLooseAttachment::AttachTo @0x821F4458
//   stw  r4,  0x318(r3)      ; meAttachRaceCarIndex = meRaceCarIndex
//   stb  1,   0x320(r3)      ; mbAttachField320     = 1
//   stw  1,   0x314(r3)      ; miAttachSet          = 1
//   stw  0,   0x31C(r3)      ; miAttachField31C     = 0
//   cmpwi r4, 8 ; blt skip   ; assert meRaceCarIndex < ku8MaxNumRaceCars (BrnVehicleRef.h:222)
// (all four stores precede the assert).
// ----------------------------------------------------------------------------
inline void
BehaviourLooseAttachment::AttachTo(s32 meRaceCarIndex)
{
    meAttachRaceCarIndex = meRaceCarIndex;     // stw r4,  0x318(this)
    mbAttachField320     = 1;                  // stb r11(=1), 0x320(this)
    miAttachSet          = 1;                  // stw r11(=1), 0x314(this)
    miAttachField31C     = 0;                  // stw r10(=0), 0x31C(this)
    CGS_ASSERT(meRaceCarIndex < KU_MAX_NUM_RACE_CARS,
               "meRaceCarIndex < BrnPhysics::Vehicle::ku8MaxNumRaceCars");
}

// ----------------------------------------------------------------------------
// BrnDirector::Camera::BehaviourLooseAttachment::Get @0x821FAA58
//   lbz  r11, 0x32C(r3)      ; mbNoResult
//   addi r3, r3, 0x20        ; &mSubObject
//   cmplwi r11, 0 ; beqlr    ; if (!mbNoResult) return &mSubObject
//   li   r3, 0               ; else return null
// ----------------------------------------------------------------------------
inline BehaviourLooseAttachment::SubObject*
BehaviourLooseAttachment::Get()
{
    if (mbNoResult)
    {
        return 0;                              // li r3, 0
    }
    return reinterpret_cast<SubObject*>(maSubObject);   // addi r3, r3, 0x20
}

// ----------------------------------------------------------------------------
// BrnDirector::Camera::BehaviourLooseAttachment::SetParameters @0x821F43E8
//   lwz  r11, 0(r4)          ; lpParameters->meType
//   cmplwi r11, 0xB          ; == eBehaviourLooseAttachment
//   ... assert on mismatch (BrnBehaviourLooseAttachment.h:164) ...
//   lwz  r11, 4(r4)          ; lpParameters->miParamWord1
//   stw  r4,  0x324(r3)      ; mpParameters = lpParameters
//   stw  r11, 0x10(r3)       ; mParamWord1  = lpParameters->miParamWord1
// ----------------------------------------------------------------------------
inline void
BehaviourLooseAttachment::SetParameters(const Parameters* lpParameters)
{
    CGS_ASSERT(lpParameters->GetType() == eBehaviourLooseAttachment,
               "lpParameters->GetType() == eBehaviourLooseAttachment");
    mpParameters = lpParameters;               // stw r4,  0x324(this)
    mParamWord1  = lpParameters->miParamWord1; // lwz r11,4(lp); stw r11, 0x10(this)
}

// ----------------------------------------------------------------------------
// BrnDirector::Camera::BehaviourLooseAttachment::SetTarget @0x821F44B8
//   stw  r4,  0x308(r3)      ; meTargetRaceCarIndex = meRaceCarIndex
//   stb  1,   0x310(r3)      ; mbTargetField310     = 1
//   stw  1,   0x304(r3)      ; miTargetSet          = 1
//   stw  0,   0x30C(r3)      ; miTargetField30C     = 0
//   cmpwi r4, 8 ; blt skip   ; assert meRaceCarIndex < ku8MaxNumRaceCars (BrnVehicleRef.h:222)
// (all four stores precede the assert).
// ----------------------------------------------------------------------------
inline void
BehaviourLooseAttachment::SetTarget(s32 meRaceCarIndex)
{
    meTargetRaceCarIndex = meRaceCarIndex;     // stw r4,  0x308(this)
    mbTargetField310     = 1;                  // stb r11(=1), 0x310(this)
    miTargetSet          = 1;                  // stw r11(=1), 0x304(this)
    miTargetField30C     = 0;                  // stw r10(=0), 0x30C(this)
    CGS_ASSERT(meRaceCarIndex < KU_MAX_NUM_RACE_CARS,
               "meRaceCarIndex < BrnPhysics::Vehicle::ku8MaxNumRaceCars");
}

} // namespace Camera
} // namespace BrnDirector

#endif // GAMESOURCE_DIRECTOR_CAMERA_BEHAVIOURS_BRN_BEHAVIOUR_LOOSE_ATTACHMENT_H

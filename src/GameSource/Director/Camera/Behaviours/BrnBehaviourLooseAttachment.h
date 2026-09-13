#ifndef GAMESOURCE_DIRECTOR_CAMERA_BEHAVIOURS_BRN_BEHAVIOUR_LOOSE_ATTACHMENT_H
#define GAMESOURCE_DIRECTOR_CAMERA_BEHAVIOURS_BRN_BEHAVIOUR_LOOSE_ATTACHMENT_H

#include "types.hpp"
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT (SetParameters type assert + race-car index asserts)
#include "GameSource/Director/Camera/Utils/BrnCameraImpactEffect.h"   // Utils::CameraImpactEffect::Parameters (embedded "Impact" sub-block @+0x2C of Parameters)
#include "GameSource/Director/Camera/Utils/BrnPositionLag.h"          // Utils::PositionLag::Parameters (embedded lag sub-block @+0x08 of Parameters)
#include "GameSource/Director/Utils/BrnDirectorTimestep.h"     // Timestep::EType (the base behaviour word at +0x04)

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
// Guarded: see the identical guard note in BrnBehaviourGyroCam.h / BrnBehaviourBystanderCam.h --
// this same unnamed enum is independently (re)declared in each; the guard makes a second
// inclusion in one TU (e.g. BrnArbStateTakedown.cpp, which needs both GyroCam and
// LooseAttachment) a no-op instead of a redefinition error.
#ifndef BRNDIRECTOR_CAMERA_KU_MAX_NUM_RACE_CARS_DEFINED
#define BRNDIRECTOR_CAMERA_KU_MAX_NUM_RACE_CARS_DEFINED
enum { KU_MAX_NUM_RACE_CARS = 8 };
#endif

class BehaviourLooseAttachment
{
public:

    // The loose-attachment parameter block: a type tag in its leading word plus behaviour-specific
    // data. GetType returns the tag SetParameters asserts on.
    //
    // The field-walk region (the embedded "Impact" sub-block + the loose-attachment tunables) is
    // pinned store-for-store from the three Serialise<S> visitor bodies (write @0x82254BC8, read
    // @0x8224D2F0, debug-menu @0x82254248): a by-value CameraImpactEffect::Parameters sub-block at
    // +0x2C (walked as the nested "Impact" section) followed by the loose-attachment f32/bool
    // tunables at the a1+0x48..a1+0x60 displacements the write/read/menu asm loads/stores. No
    // pointers in the walked region => the offsets are host-pointer-width invariant (pinned in the
    // .cpp). Every field is modelled by name: meType (+0x00) / miParamWord1 (+0x04), the two
    // sub-blocks at +0x08 / +0x1C the behaviour's Update passes by address to PositionLag::Update
    // and CameraShake::Update, and the walked fields from +0x2C on.
    class Parameters
    {
    public:
        // X360 visitor: `void Serialise<S>(S&)` -- walks this block's fields into the camera-tunings
        // serialiser S (DebugMenuSerialiser / TextFile{Read,Write}Serialiser), recursing into the
        // embedded impact block for the "Impact" section. The per-instance body is a separate TU
        // (bodied in BrnBehaviourLooseAttachment.cpp with one explicit instantiation per serialiser).
        // Declared so a serialiser's Serialise<Parameters> can drive it by name.
        template<class TSerialiser> void Serialise(TSerialiser& lrSerialiser);

        // Seed the block to its defaults. A leaf with no calls: it writes the type tag, clears
        // miParamWord1, seeds both sub-blocks at +0x08 / +0x1C and the impact block at +0x2C,
        // then the +0x48..+0x60 tunables. Defined below, beside the other inline members.
        // Called by MomentNewCarJoined::Construct on its own by-value parameter block.
        void Construct();

        EBehaviourTypeLooseAttachment GetType() const
        {
            return static_cast<EBehaviourTypeLooseAttachment>(meType);
        }

        s32 meType;        // +0x00  the behaviour type tag (eBehaviour*)
        s32 miParamWord1;  // +0x04  first behaviour-specific word (cached by SetParameters)

        // +0x08..+0x2B was a reserved span ("rig data not walked here") until the behaviour's own
        // Update was read: it hands &(params +0x08) to PositionLag::Update and &(params +0x1C) to
        // CameraShake::Update, so the span is two by-value sub-blocks, not opaque bytes. Neither is
        // reached by a Serialise<S> visitor -- the field-walk starts at the +0x2C "Impact" block --
        // which is why the tunings file carries no section for either.
        Utils::PositionLag::Parameters        mPositionLagParams;   // +0x08  camera position smoother (20B)
        Utils::CameraShake::Parameters        mShakeParams;         // +0x1C  the rig's own shake block (16B)

        Utils::CameraImpactEffect::Parameters mImpact;   // +0x2C  embedded impact-shake block ("Impact")
        f32 mfPitch;                                     // +0x48  "Pitch"
        f32 mfHeight;                                    // +0x4C  "Height"
        f32 mfDistance;                                  // +0x50  "Distance"
        f32 mfField54;                                   // +0x54  <unk_820051C0 label> tunable (label rodata unrecovered)
                                                         //  ⓘ The declaration reference names this slot mfFOV
                                                         //  (BrnBehaviourLooseAttachment.h), which the
                                                         //        seeds corroborate: 90.0f by default, 40.0f for the
                                                         //        new-car-joined moment, 100.0f for the shutdown
                                                         //        takedown's zoom beats -- all field-of-view degrees.
                                                         //        NOT renamed here: two TUs outside this header's
                                                         //        ownership spell it mfField54 (this class's own .cpp
                                                         //        serialiser and BrnMomentNewCarJoined_wO_01.cpp), so
                                                         //        the rename must land with them in one change.
        f32 mfDutch;                                     // +0x58  "Dutch"
        f32 mfDetachLerpAmount;                          // +0x5C  "Detach Lerp Amount"
        bool mbLookFromTarget;                           // +0x60  "Look from target"
    };

    // FLAG: the +0x20 sub-object Get exposes is an embedded behaviour sub-object (the attachment
    //   transform / source). Its concrete type lands with the full behaviour TU; modelled here as
    //   an opaque embedded sub-object so the accessor returns its address at the asm-attested
    //   offset. Get returns null instead when the +0x32C "no result" flag is set.
    class SubObject;

    // The embedded impact-effect sub-object at +0x2F0. The shutdown-takedown moment's three
    // zoom beats each allocate a loose-attachment behaviour, bind it, and then register a UNIT
    // impact on it -- the console reaches the effect as `behaviour + 752` and hands it straight
    // to Utils::CameraImpactEffect::RegisterImpact with a magnitude of 1.0. 752 == +0x2F0, and
    // CameraImpactEffect is twenty bytes, so the sub-object closes exactly where the target
    // VehicleRef block begins at +0x304 -- the placement has no slack in it.
    // Exposed by name so an arbitrator state never forms that displacement itself.
    Utils::CameraImpactEffect&       GetImpactEffect()       { return mImpactEffect; }
    const Utils::CameraImpactEffect& GetImpactEffect() const { return mImpactEffect; }

    // The base Camera::Behaviour timestep-flavour word at +0x04. Declared with the base's exact
    // signature (see Behaviour.h) so a call site reads identically whichever behaviour it holds.
    // The same three zoom beats store 1 == E_WORLD_NO_SLOMO into this word right after
    // registering the impact, which is what keeps the beat running at world rate through the
    // takedown's slow-motion.
    // FLAG: this class is not re-based onto Camera::Behaviour yet (its head is still modelled as
    // a reserved span), so the word is written through this class's own member rather than
    // inherited. DELETE-WHEN: BehaviourLooseAttachment derives from Camera::Behaviour, at which
    // point this setter and meTimestepType both come from the base.
    void SetTimestepType(BrnDirector::Timestep::EType leType) { meTimestepType = leType; }

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
    u8    maHead000[0x04];                     // +0x000 .. +0x003  vtable (console 4B ptr slot)
    BrnDirector::Timestep::EType meTimestepType;  // +0x004  the base behaviour's timestep flavour
    u8    maReserved008[0x10 - 0x08];          // +0x008 .. +0x00F (base flags/name not modelled here)
    s32   mParamWord1;                         // +0x010  cached lpParameters->miParamWord1
    u8    maReserved014[0x20 - 0x14];          // +0x014 .. +0x01F (rig members not modelled here)
    u8    maSubObject[0x2F0 - 0x20];           // +0x020  embedded sub-object (&-of by Get)

    // --- the embedded impact effect the shutdown-takedown zoom beats register on ----------
    // Twenty bytes (one f32 accumulator + the sixteen-byte runtime shake), so it runs
    // +0x2F0 .. +0x303 and the target VehicleRef block below picks up with no padding.
    Utils::CameraImpactEffect mImpactEffect;   // +0x2F0

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
    u8    maReserved32D;                       // +0x32D  (rig byte not modelled)
    u8    mbDetachRequested;                   // +0x32E  set (=1) by MomentNewCarJoined::Update
                                               //         @0x82266DB0 (`stb r26(=1), 0x32E(r11)`)
                                               //         when the return blend (loose->gameplay)
                                               //         starts. FLAG: the NAME is role-inferred
                                               //         (the params carry mfDetachLerpAmount);
                                               //         the STORE and the OFFSET are asm.

    // x64 typed view of the adopted parameter pointer (the by-name, type-correct store target).
    // Appended at the tail so it never disturbs the pinned offsets above; the X360 packs the same
    // pointer into the 4-byte slot at +0x324.
    const Parameters* mpParameters;
};

// ----------------------------------------------------------------------------
// BrnDirector::Camera::BehaviourLooseAttachment::Parameters::Construct
//   Seed the whole block. Twenty-three stores, every one to a distinct slot (nothing is written
//   twice, so the console's scheduling order carries no meaning and the seeds are grouped by
//   sub-block here). Two of the three sub-blocks are seeded with exactly the values their own
//   Construct writes -- PositionLag::Parameters (1/1/1 responses, 0.5 smoothing, muVersion left
//   alone) and CameraShake::Parameters (0.06 / 0.0 / 1.15 / 0.11) -- written out field by field
//   because the console inlines both rather than calling them.
// ----------------------------------------------------------------------------
inline void
BehaviourLooseAttachment::Parameters::Construct()
{
    meType       = eBehaviourLooseAttachment;   // +0x00  the type tag SetParameters asserts on
    miParamWord1 = 0;                           // +0x04

    // +0x08 mPositionLagParams -- the PositionLag::Parameters seed. muVersion (+0x08) is NOT
    // written, exactly as PositionLag::Parameters::Construct leaves it (the serialiser stamps it).
    mPositionLagParams.mfXResponse = 1.0f;      // +0x0C
    mPositionLagParams.mfYResponse = 1.0f;      // +0x10
    mPositionLagParams.mfZResponse = 1.0f;      // +0x14
    mPositionLagParams.mfSmoothing = 0.5f;      // +0x18

    // +0x1C mShakeParams -- the CameraShake::Parameters seed.
    mShakeParams.mfXYShakeMagnitudeDegs  = 0.06f;   // +0x1C
    mShakeParams.mfZShakeMagnitudeDegs   = 0.0f;    // +0x20
    mShakeParams.mfXYWobbleMagnitudeDegs = 1.15f;   // +0x24
    mShakeParams.mfWobbleCenteringFactor = 0.11f;   // +0x28

    // +0x2C mImpact -- the same shake seed again, then the three impact tunables.
    mImpact.mShakeParams.mfXYShakeMagnitudeDegs  = 0.06f;   // +0x2C
    mImpact.mShakeParams.mfZShakeMagnitudeDegs   = 0.0f;    // +0x30
    mImpact.mShakeParams.mfXYWobbleMagnitudeDegs = 1.15f;   // +0x34
    mImpact.mShakeParams.mfWobbleCenteringFactor = 0.11f;   // +0x38
    mImpact.mfShakeDecayFactor    = 0.05f;      // +0x3C
    mImpact.mfShakeMagnitude      = 15.0f;      // +0x40
    mImpact.mfShakeFrequencyScale = 5.0f;       // +0x44

    mfPitch            = 5.0f;                  // +0x48
    mfHeight           = 1.0f;                  // +0x4C
    mfDistance         = 4.0f;                  // +0x50
    mfField54          = 90.0f;                 // +0x54
    mfDutch            = 0.0f;                  // +0x58
    mfDetachLerpAmount = 0.1f;                  // +0x5C
    mbLookFromTarget   = false;                 // +0x60  (a byte store, the only non-f32 seed)
}

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

// BrnPhysics::Vehicle::VehicleDriver -- the per-car driver.
//
// This TU holds Construct @0x825B83C8 (96 instructions, ZERO callees) and -- since the
// 2026-08-11 driving-path wave -- UpdateVehicle @0x825D7290 (219 instructions). Everything
// else on the class is declare-only in the header.

#include "GameSource/Physics/VehicleManager/VehiclePhysics/BrnVehicleDriver.h"
#include "GameSource/Physics/VehicleManager/VehiclePhysics/VehiclePhysics.h"   // VehiclePhysics (Get/SetTransform, IsFrozen)
#include "GameShared/GameClasses/Core/CgsAssert.h"                             // CGS_ASSERT (the two slerp-transform tripwires)
#include "rw/math/vpu/vector3_operation.h"                                     // rw::math::vpu::{Dot, MagnitudeSquared}
#include "rw/math/vpu/matrix44affine_operation.h"                              // rw::math::vpu::operator* (the affine concat)

#include <cmath>   // std::fabs (the vandc sign-mask lowering)

namespace BrnPhysics
{
namespace Vehicle
{
    namespace vpu = rw::math::vpu;

    // @0x825B83C8  BrnPhysics::Vehicle::VehicleDriver::Construct
    //
    // Callers: VehicleManager::Construct @0x8263B7C8 (the eight-car maRaceCarDrivers array at
    // this+64, stride 224, and again for the spare mPlayerAiDriver at this+171968) and
    // PhysicalTrafficManager::Prepare @0x8262CA48.
    //
    // ⚠️ IT IS A PARTIAL INITIALISER, DELIBERATELY. The console clears only the subset of
    // mControls listed below -- it is NOT `mControls.Clear()` (compare
    // BrnNetworkDriverControls::Clear @0x82581200, which writes every member of the base plus the
    // network tail, seeds miVehicleID to -1 rather than 0, and seeds +0x34 to 1.0f and
    // miVehicleIDToMerge to -1). Reproduced store-for-store rather than "tidied" into a full
    // clear, because the members it SKIPS keep whatever the previous owner of the memory left
    // there, and that is observable:
    //     untouched here -- mfAftertouchLevel, the four sensors, mfBoostMaxSpeedScale,
    //                       miVehicleIDToMerge, mbToggle, mbIsOnStartLine, mbHorn,
    //                       and the whole BrnAIDriverControls tail (+0x48..+0x4E).
    // VehicleManager::Construct runs on a freshly-placed manager, so in practice the skipped
    // bytes are zero there; PhysicalTrafficManager::Prepare re-runs it on live memory, where they
    // are not.
    void VehicleDriver::Construct()
    {
        // int 0 at +0x00, then 0.0f at +0x04 .. +0x1C (seven floats).
        mControls.miVehicleID       = 0;
        mControls.mfGas             = 0.0f;   // +0x04
        mControls.mfBrake           = 0.0f;   // +0x08
        mControls.mfHandBrake       = 0.0f;   // +0x0C
        mControls.mfSteering        = 0.0f;   // +0x10
        mControls.mfForwardSteering = 0.0f;   // +0x14
        mControls.mfSpin            = 0.0f;   // +0x18
        mControls.mfRequestedGas    = 0.0f;   // +0x1C

        // The seven cleared flag bytes: 0x39, 0x3B, 0x3C, 0x3D, 0x3E, 0x3F, 0x41.
        mControls.mbReset                    = false;   // +0x39
        mControls.mbBoost                    = false;   // +0x3B
        mControls.mbIsInvulnerableToVehicles = false;   // +0x3C
        mControls.mbIsInvulnerableToWorld    = false;   // +0x3D
        mControls.mbForceDrift               = false;   // +0x3E
        mControls.mbBoostBounce              = false;   // +0x3F
        mControls.mbIsSteeringWheel          = false;   // +0x41

        // Both interpolation transforms are the identity affine. The X360 builds the four rows on
        // the stack out of flt_82001C98 (1.0f) and flt_82001CC0 (0.0f) and stores them with eight
        // stvx128s -- {1,0,0,0} {0,1,0,0} {0,0,1,0} {0,0,0,0} for each, which is exactly what
        // rw::math::vpu::Matrix44Affine::SetIdentity() writes (note the ZERO w row: it is an
        // affine, so the translation lane is 0, not 1).
        mCatchupTargetTransform.SetIdentity();          // +0x50
        mSlerpTransform.SetIdentity();                  // +0x90

        // `stw r28, 0xD0(r3)` with r28 == 4. Four is E_NUM_E_DRIVER_TYPE_EVENTS -- the count, one
        // past E_DRIVER_TYPE_TRAFFIC -- i.e. the driver starts with NO valid type. Transcribed as
        // the literal enumerator rather than as a fabricated "E_DRIVER_TYPE_NONE".
        meDriverType        = E_NUM_E_DRIVER_TYPE_EVENTS;   // +0xD0
        mi8NumOfInterpSteps = 0;                            // +0xD4
        mbSnappedThisFrame  = false;                        // +0xD5
    }

    // @0x825B8680  VehicleDriver::Prepare (98 instructions). Store-for-store the same body as
    // Construct (the controls word + seven floats + seven flag bytes zeroed, both transforms the
    // identity affine built from flt_82001C98 / flt_82001CC0, meDriverType = 4, +0xD4/+0xD5 = 0),
    // returning true. Callers: VehicleManager::PrepareData @0x82633568 (x8 + the spare) and
    // PhysicalTrafficManager::Prepare @0x8262CA48 (x20).
    bool VehicleDriver::Prepare()
    {
        mControls.miVehicleID       = 0;
        mControls.mfGas             = 0.0f;
        mControls.mfBrake           = 0.0f;
        mControls.mfHandBrake       = 0.0f;
        mControls.mfSteering        = 0.0f;
        mControls.mfForwardSteering = 0.0f;
        mControls.mfSpin            = 0.0f;
        mControls.mfRequestedGas    = 0.0f;

        mControls.mbReset                    = false;
        mControls.mbBoost                    = false;
        mControls.mbIsInvulnerableToVehicles = false;
        mControls.mbIsInvulnerableToWorld    = false;
        mControls.mbForceDrift               = false;
        mControls.mbBoostBounce              = false;
        mControls.mbIsSteeringWheel          = false;

        mCatchupTargetTransform.SetIdentity();
        mSlerpTransform.SetIdentity();

        meDriverType        = E_NUM_E_DRIVER_TYPE_EVENTS;
        mi8NumOfInterpSteps = 0;
        mbSnappedThisFrame  = false;
        return true;
    }

    // =============================================================================================
    // @0x825D7290  BrnPhysics::Vehicle::VehicleDriver::UpdateVehicle  (219 instructions)
    //
    // ⭐⭐ THE TRAP THIS REPLACES CARRIED A WRONG COMMENT, AND THE WRONG COMMENT IS THE LESSON.
    // BrnVehicleManagerLinkStubs.cpp:196 described this function as "the driver-type dispatch into
    // the four Update(controls) overloads". It is nothing of the sort -- there is no `meDriverType`
    // read, no switch, and no call to any Update overload anywhere in the 219 instructions. It is
    // the NETWORK CATCH-UP SLERP APPLIER: one step of the interpolation VehicleDriver::
    // StartCatchupInterpolation arms, applied to the vehicle it is handed.
    //
    // Callers (both per-live-car, every frame): VehicleManager::UpdateVehiclePhysics @0x82644FA8
    // and PhysicalTrafficManager::UpdateTrafficPhysics @0x82644418.
    //
    // ⭐ IT IS A NO-OP ON A NORMAL DRIVING FRAME. `lbz r11,0xD4(r3) ; extsb ; cmpwi ; ble` gates the
    // WHOLE body on mi8NumOfInterpSteps > 0, and Construct seeds that byte to 0. Only a network snap
    // (StartCatchupInterpolation, which seats mSlerpTransform and loads the counter with
    // ki8NumNetworkSlerpSteps == 10) ever makes it do work. That is why the trap did not fire in the
    // boot-drive harness and why it WOULD have fired the moment a networked car existed.
    //
    // BODY, register-traced:
    //   0x825D72A8  if ((s8)this->mi8NumOfInterpSteps <= 0) return;                 // the gate
    //   0x825D72B8  if (!lpVehicle->mbFrozen)                                       // base +0x60 == VP +0x70
    //   0x825D72CC     vehicle mTransform (VP +0x10..+0x40, i.e. base +0x00..+0x30)
    //                  = mSlerpTransform (this +0x90..+0xC0) * itself.
    //                  The four `stvx128` write BACK over the vehicle's rows, and the vmaddfp
    //                  cascade is the canonical row-major affine concat: out.row_i =
    //                  A[i].x*B.row0 + A[i].y*B.row1 + A[i].z*B.row2 (+ B.row3 for the w row,
    //                  which the asm seeds with `vmaddfp v0, v2, v0, v7` == A[3].x*B.row0 + B.row3).
    //                  That is exactly rw::math::vpu::Mult(mSlerpTransform, vehicleTransform).
    //   0x825D7368  DEV TRIPWIRE 1 (BrnVehicleDriver.cpp:219) -- "not normalised".
    //                  d_i = |row_i|^2 - 1 for i in {0,1,2}; err = d0^2+d1^2+d2^2 (the second
    //                  vmsum3fp128 over the gathered triple); assert |err| <= 0.01f
    //                  (flt_82002138, image-read). `vandc v0,v0,vslw(-1,-1)` is fabs.
    //   0x825D7488  DEV TRIPWIRE 2 (BrnVehicleDriver.cpp:220) -- "not orthogonal".
    //                  P = M3 * M3^T (the vmrghw/vmrglw block IS the transpose: it gathers
    //                  col0/col1/col2 out of rows 0/1/2); E = P - I, with the identity rows read
    //                  from w::math::vpu::detail::gIVector {1,0,0,0} @0x82181500, {0,1,0,0}
    //                  @0x82181510 and {0,0,1,0} @0x82181520 (image-read); err = |E.row0|^2 +
    //                  |E.row1|^2 + |E.row2|^2; assert |err| <= 0.1f (flt_82004014, image-read).
    //   0x825D75E0  --this->mi8NumOfInterpSteps;                                    // lbz/addi -1/stb
    //
    // ⚠️ BOTH tripwires are UNCONDITIONAL side-effect-free asserts on the console -- they run even
    // when the concat was skipped because the vehicle was frozen (the `beq` at 0x825D7434 /
    // 0x825D7590 skips only the Fire, never the maths). Reproduced in that position.
    //
    // ⚠️ The console builds its assert text through CgsDev::StrStream over
    // CgsDev::Assert::gpcMessageBuffer (the `BasePriorityQueue::Clear` in the pseudocode is the
    // ICF-folded StrStream constructor). Per house style that is lowered to CGS_ASSERT with the
    // console's own literal message; no values are streamed in either of these two.
    // =============================================================================================
    void VehicleDriver::UpdateVehicle(VehiclePhysics* lpVehicle)
    {
        // flt_82002138 / flt_82004014 -- both read out of the X360 image, not guessed.
        static const f32 KF_NORMALISED_TOLERANCE = 0.0099999998f;   // flt_82002138 (0x3C23D70A)
        static const f32 KF_ORTHOGONAL_TOLERANCE = 0.1f;            // flt_82004014 (0x3DCCCCCD)

        if (mi8NumOfInterpSteps <= 0)
            return;

        if (!lpVehicle->IsFrozen())
            lpVehicle->SetTransform(vpu::Mult(mSlerpTransform, lpVehicle->GetTransform()));

        const Matrix44Affine lTransform = lpVehicle->GetTransform();
        const Vector3& lvRow0 = lTransform.xAxis;
        const Vector3& lvRow1 = lTransform.yAxis;
        const Vector3& lvRow2 = lTransform.zAxis;

        // ----- tripwire 1: the three rotation rows are unit length (BrnVehicleDriver.cpp:219) -----
        {
            const f32 lfD0 = vpu::MagnitudeSquared(lvRow0) - 1.0f;
            const f32 lfD1 = vpu::MagnitudeSquared(lvRow1) - 1.0f;
            const f32 lfD2 = vpu::MagnitudeSquared(lvRow2) - 1.0f;
            const f32 lfError = lfD0 * lfD0 + lfD1 * lfD1 + lfD2 * lfD2;
            CGS_ASSERT(!(std::fabs(lfError) > KF_NORMALISED_TOLERANCE),
                       "Slerped race car transform is not normalised");
        }

        // ----- tripwire 2: the 3x3 is orthogonal, i.e. M*M^T == I (BrnVehicleDriver.cpp:220) -----
        {
            // P = M3 * M3^T, row by row. The console reaches the transpose columns with
            // vmrghw/vmrglw; the dot form below is the same three numbers per row.
            const f32 lfP00 = vpu::Dot(lvRow0, lvRow0) - 1.0f;   // - gIVector      {1,0,0,0}
            const f32 lfP01 = vpu::Dot(lvRow0, lvRow1);
            const f32 lfP02 = vpu::Dot(lvRow0, lvRow2);

            const f32 lfP10 = vpu::Dot(lvRow1, lvRow0);
            const f32 lfP11 = vpu::Dot(lvRow1, lvRow1) - 1.0f;   // - unk_82181510  {0,1,0,0}
            const f32 lfP12 = vpu::Dot(lvRow1, lvRow2);

            const f32 lfP20 = vpu::Dot(lvRow2, lvRow0);
            const f32 lfP21 = vpu::Dot(lvRow2, lvRow1);
            const f32 lfP22 = vpu::Dot(lvRow2, lvRow2) - 1.0f;   // - unk_82181520  {0,0,1,0}

            const f32 lfError = (lfP00 * lfP00 + lfP01 * lfP01 + lfP02 * lfP02)
                              + (lfP10 * lfP10 + lfP11 * lfP11 + lfP12 * lfP12)
                              + (lfP20 * lfP20 + lfP21 * lfP21 + lfP22 * lfP22);
            CGS_ASSERT(!(std::fabs(lfError) > KF_ORTHOGONAL_TOLERANCE),
                       "Slerped race car transform is not orthogonal");
        }

        --mi8NumOfInterpSteps;
    }
}
}

namespace BrnPhysics
{
namespace Vehicle
{
    // =============================================================================================
    // VehicleDriver::ClearControls -- NO STANDALONE X360 SYMBOL. Recovered 2026-08-11 from the TWO
    // places the console INLINES it, which is the strongest form of evidence this project accepts
    // for an inlined leaf: the same 27-store block appears verbatim in
    //     VehicleManager::ProcessCreateEvents @0x82617404..0x82617490   and
    //     VehicleManager::ProcessRemoveEvents @0x826163B0..0x82616444
    // both addressed as `mulli r11, idx, 0xE0 ; add r11, r11, this` and then offsets 0x40..0x8E --
    // i.e. `maRaceCarDrivers[idx]` (+64) plus in-record 0x00..0x4E, which is exactly the span of
    // `BrnAIDriverControls mControls`. The two copies are store-for-store identical (only the FPR
    // holding 0.0f/1.0f differs: create has f30=0.0f/f31=1.0f, remove has f31=0.0f/f30=1.0f -- both
    // read from flt_82001CC0 / flt_82001C98, values read from the image, not guessed).
    //
    // ⭐ THE SIGNATURE OF AN INLINED `ClearControls` RATHER THAN A MEMSET: the block writes 28 of
    // BrnAIDriverControls' 30 scalar fields and SKIPS EXACTLY TWO -- `mbToggle` (+0x3A) and
    // `meDriverType` (+0x44). Both gaps are visible as holes in the store run (create: 0x79 then
    // 0x7B, no 0x7A) and both make sense only for a *semantic* clear: the driver's TYPE must
    // survive a control reset, and mbToggle is a latch the input layer owns. A memset or a
    // defaulted ctor would have written them. That is what makes this ClearControls (declared in
    // BrnVehicleDriver.h, DWARF-attested) and not something invented to hold the block.
    //
    // ⭐⭐ VERIFIED AGAINST A SECOND, INDEPENDENT TRANSCRIPTION (2026-08-11 merge). The other
    // create-drain wave wrote the same block out INLINE at both console call sites rather than
    // recovering it as a function, and its two copies are FIELD-FOR-FIELD identical to this body:
    // same 28 writes, same two omissions (mbToggle, meDriverType), same lone non-zero (1.0f into
    // mfBoostMaxSpeedScale). The only discrepancy was arithmetic in the prose -- "27 of 29" here
    // vs "28 stores" there. Counted against the committed struct: BrnPlayerDriverControls has 26
    // scalars (miVehicleID + 13 floats + miVehicleIDToMerge + 10 bools + meDriverType) and
    // BrnAIDriverControls adds 4, so it is **28 of 30**, corrected above. Nothing about the
    // recovered store SET was in dispute. This body is now the single home; both inline copies
    // are retired to a call, per their own DELETE-WHEN markers.
    //
    // ⚠️ mfBoostMaxSpeedScale (+0x34) is seeded to 1.0f, not 0.0f -- the ONE non-zero float, and the
    // reason this cannot be spelled as a zero-fill.
    // =============================================================================================
    void VehicleDriver::ClearControls()
    {
        mControls.miVehicleID         = -1;     // +0x00  (`li r10,-1 ; stw r10,0x40(r11)`)
        mControls.mfGas               = 0.0f;   // +0x04
        mControls.mfBrake             = 0.0f;   // +0x08
        mControls.mfHandBrake         = 0.0f;   // +0x0C
        mControls.mfSteering          = 0.0f;   // +0x10
        mControls.mfForwardSteering   = 0.0f;   // +0x14
        mControls.mfSpin              = 0.0f;   // +0x18
        mControls.mfRequestedGas      = 0.0f;   // +0x1C
        mControls.mfAftertouchLevel   = 0.0f;   // +0x20
        mControls.mfXSensor           = 0.0f;   // +0x24
        mControls.mfYSensor           = 0.0f;   // +0x28
        mControls.mfZSensor           = 0.0f;   // +0x2C
        mControls.mfGSensor           = 0.0f;   // +0x30
        mControls.mfBoostMaxSpeedScale = 1.0f;  // +0x34  (`stfs f31, 0x74(r11)` -- flt_82001C98)

        mControls.miVehicleIDToMerge  = -1;     // +0x38  (`stb r10, 0x78(r11)`)
        mControls.mbReset             = false;  // +0x39
        // +0x3A mbToggle: NOT WRITTEN by either console copy -- see the banner.
        mControls.mbBoost                    = false;  // +0x3B
        mControls.mbIsInvulnerableToVehicles = false;  // +0x3C
        mControls.mbIsInvulnerableToWorld    = false;  // +0x3D
        mControls.mbForceDrift               = false;  // +0x3E
        mControls.mbBoostBounce              = false;  // +0x3F
        mControls.mbIsOnStartLine            = false;  // +0x40
        mControls.mbIsSteeringWheel          = false;  // +0x41
        mControls.mbHorn                     = false;  // +0x42
        // +0x44 meDriverType: NOT WRITTEN by either console copy -- see the banner.

        mControls.mfSpeedMatchSpeed      = 0.0f;   // +0x48
        mControls.mbDoSpeedMatch         = false;  // +0x4C
        mControls.mbForceComeOutOfDrift  = false;  // +0x4D
        mControls.mbSlamPlayer           = false;  // +0x4E
    }
}
}

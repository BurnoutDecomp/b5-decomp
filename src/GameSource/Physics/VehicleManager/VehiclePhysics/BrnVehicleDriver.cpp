// BrnPhysics::Vehicle::VehicleDriver -- the per-car driver.
//
// This TU holds the one function with a recovered body: Construct @0x825B83C8 (96 instructions,
// ZERO callees). Everything else on the class is declare-only in the header.

#include "GameSource/Physics/VehicleManager/VehiclePhysics/BrnVehicleDriver.h"

namespace BrnPhysics
{
namespace Vehicle
{
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
    // ⭐ THE SIGNATURE OF AN INLINED `ClearControls` RATHER THAN A MEMSET: the block writes 27 of
    // the 29 scalar fields and SKIPS EXACTLY TWO -- `mbToggle` (+0x3A) and `meDriverType` (+0x44).
    // Both gaps are visible as holes in the store run (create: 0x79 then 0x7B, no 0x7A) and both
    // make sense only for a *semantic* clear: the driver's TYPE must survive a control reset, and
    // mbToggle is a latch the input layer owns. A memset or a defaulted ctor would have written
    // them. That is what makes this ClearControls (declared in BrnVehicleDriver.h, DWARF-attested)
    // and not something invented to hold the block.
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

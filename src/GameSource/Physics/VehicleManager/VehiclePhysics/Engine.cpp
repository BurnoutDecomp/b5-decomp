#include "GameSource/Physics/VehicleManager/VehiclePhysics/Engine.h"

#include <cstring>   // std::memcpy

// BrnPhysics::Vehicle::Engine -- the two ledger functions owned by the Vehicle-physics group.
// The X360 build is VMX128 inline asm; these are the de-SIMD'd named-member equivalents
// recovered store-for-store from the asm at 0x825F3EE8 (Construct) and 0x825F3F38 (Prepare).

namespace BrnPhysics
{
namespace Vehicle
{
    // ---------------------------------------------------------------------------------------
    // Construct  @0x825F3EE8
    //   asm: bl EngineAttribs::Construct(this)   -- mAttribs is the leading member (&mAttribs ==
    //        this), so this constructs the default attribs block in place; then it splats a 0.0
    //        constant into a register (the wheel-angular-velocity argument) and tail-calls
    //        Reset(0.0). No running-state writes happen here -- Reset does them.
    // ---------------------------------------------------------------------------------------
    void Engine::Construct()
    {
        mAttribs.Construct();
        Reset(VecFloat{ 0.0f, 0.0f, 0.0f, 0.0f });
    }

    // ---------------------------------------------------------------------------------------
    // Prepare  @0x825F3F38
    //   asm: memcpy(this, lpAttribs, 160)        -- copy the supplied attribs block into mAttribs
    //        (sizeof EngineAttribs == 0xA0 == 160).
    //        addi r11,this,0xB0 ; vrlimi128 v13,<0.0 splat>,8,0 ; stvx r11
    //                                            -- write 0.0 into lane 0 (.x = clutch factor) of
    //                                               mvClutchFactor_RPM_CurrentGearChangeTime,
    //                                               preserving the other lanes.
    //        stw <1>,0xC0(this)                  -- mu8CurrentGear = KU8_FIRST_GEAR (1).
    //        bl Reset(0.0)                       -- seed the running state from a 0 wheel velocity.
    //        li r3,1                             -- return true.
    // ---------------------------------------------------------------------------------------
    bool Engine::Prepare(const EngineAttribs* lpAttribs)
    {
        std::memcpy(&mAttribs, lpAttribs, sizeof(EngineAttribs));   // 160-byte attribs copy

        mvClutchFactor_RPM_CurrentGearChangeTime.x = 0.0f;          // clutch factor lane -> 0
        mu8CurrentGear = KU8_FIRST_GEAR;

        Reset(VecFloat{ 0.0f, 0.0f, 0.0f, 0.0f });
        return true;
    }
}
}

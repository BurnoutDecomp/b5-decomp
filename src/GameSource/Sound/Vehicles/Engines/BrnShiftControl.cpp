#include "GameSource/Sound/Vehicles/Engines/BrnShiftControl.h"

#include <cstring>   // std::memset

// =============================================================================
// BrnSound::Vehicles::Engines::ShiftControl -- out-of-line bodies.
// Reconstructed from BURNOUT_X360_ARTIST.XEX (semantic parity; store-for-store
// against the ctor asm @ 0x826AF010). Recon'd function set:
//   ShiftControl()                 @ 0x826AF010  (field-init ctor)
//   `scalar deleting destructor'   @ 0x826AF200  (-> ~ShiftControl anchor)
// =============================================================================

namespace BrnSound
{
namespace Vehicles
{
namespace Engines
{

// ---------------------------------------------------------------------------
// ShiftControl::ShiftControl  @ 0x826AF010
// The leading dual-vptr install and the base-region clears are produced by the chained
// BrnEffectControl() base ctor; this body sets the LEAF members the X360 explicitly
// initialises. Each InterpolateLine default-ctors to a complete (mbComplete=true)
// zeroed line -- matching the five stb-1 stores. The Attrib::Gen::shiftpattern
// sub-object (constructed with (0,0) on the X360) is an opaque span here -- zeroed.
// ---------------------------------------------------------------------------
ShiftControl::ShiftControl()
    : mpPhysicsControl(nullptr)          // stw 0, 0x38
    , mpEngineControl(nullptr)           // (not stored by asm; default)
    , mpHybridControl(nullptr)           // (not stored by asm; default)
    , mbNeed_ShiftGearSnd(false)         // stb 0, 0x44
    , mbNeed_DisengageSnd(false)         // stb 0, 0x45
    , mbNeed_EngageSnd(false)            // stb 0, 0x46
    , meShiftState(E_SHFT_NONE)          // stw 0, 0x58
    , meShiftStageChanged(E_SHFT_NONE)   // stw 0, 0x5C
    , miRaceCarIndex(0)                  // (not stored by asm; default 0)
    , meShift_LFO(E_SHIFT_LFO_NONE)      // (not stored by asm; default 0)
    , mfVOL_LFO_AMP(0.0f)                // stfs 0.0, 0x68
    , mfVOL_LFO_FRQ(0.0f)                // stfs 0.0, 0x6C
    , mfRPM_LFO_AMP(0.0f)                // stfs 0.0, 0x70
    , mfRPM_LFO_FRQ(0.0f)                // stfs 0.0, 0x74
    // mInterpRPM_LFODecay @0x78 / mInterpVol_LFODecay @0x94: default-ctored (mbComplete=true)
    , mfRPMAtShift(0.0f)                 // stfs 0.0, 0xB0
    , mfLastUpShift(0.0f)                // (not stored by asm; default 0)
    , mpShiftingActivator(nullptr)       // stw 0, 0xB8
    // mInterpShiftThrottle @0xBC / mInterpShiftRPM @0xD8 / mInterpShiftVol @0xF4: default-ctored
{
    // Attrib::Gen::shiftpattern mShiftingPatternData(this+0x48, 0, 0): un-homed opaque
    // span, constructed empty/unbound -> zeroed (see header FLAG).
    std::memset(mau8ShiftingPatternData, 0, sizeof(mau8ShiftingPatternData));
}

// ---------------------------------------------------------------------------
// ~ShiftControl  @ 0x826AF200  (anchor for the X360 `scalar deleting destructor').
// The observable teardown lives in the inherited BrnEffectControl chain plus the
// embedded ~shiftpattern; the (flags&1) free through the global sound allocator
// (off_82FFB954) is left to the host toolchain's delete. Empty leaf body.
// ---------------------------------------------------------------------------
ShiftControl::~ShiftControl()
{
}

} // namespace Engines
} // namespace Vehicles
} // namespace BrnSound

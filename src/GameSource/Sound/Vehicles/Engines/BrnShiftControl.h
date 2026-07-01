#ifndef BRN_SOUND_VEHICLES_ENGINES_SHIFT_CONTROL_H
#define BRN_SOUND_VEHICLES_ENGINES_SHIFT_CONTROL_H

#include "types.hpp"
#include "GameSource/Sound/Module/LogicModule/BrnEffectControl.h"   // committed BrnEffectControl dual base (BY NAME)
#include "GameShared/GameClasses/Sound/CgsSoundUtils.h"            // CgsSound::Utils::InterpolateLine (BY NAME)

// =============================================================================
// BrnSound::Vehicles::Engines::ShiftControl
//   GameSource/Sound/Vehicles/Engines/BrnShiftControl.{h,cpp}  (DWARF home)
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX (semantic parity; store-for-store against
// the ctor asm @ 0x826AF010). ShiftControl is the gear-shift sound CONTROL (DWARF
// BrnShiftControl.h:23: ShiftControl : public BrnEffectControl). It drives per-shift
// LFO decay + throttle/RPM/volume interpolation ramps.
//
// InterpolateLine stride verified = 0x1C (28 bytes): the 5 mbComplete stb-1 markers
// @0x90/0xAC/0xD4/0xF0/0x10C give block starts 0x78/0x94/0xBC/0xD8/0xF4.
//
// FLAG (opaque shiftpattern span): mShiftingPatternData is DWARF-typed Attrib::Gen::
// shiftpattern, which has NO homed type in src (not in AttribSys/Generated). Per the
// anti-fabrication rule it is modelled as an opaque byte span (documented X360 size)
// rather than a fabricated Attrib::Instance; the ctor zeroes it (the X360 constructs it
// with (0,0), i.e. an empty/unbound attribute instance). Named scalar members + the
// InterpolateLine ramps are pinned BY NAME. mpShiftingActivator points at the un-homed
// IShiftingActivator interface (opaque forward). Absolute offsets are NOT static_asserted.
//
// LAYOUT NOTE (X360 32-bit vs host 64-bit): members are pinned BY NAME + SEQUENCE.
// =============================================================================

namespace BrnSound
{
namespace Vehicles
{
namespace Engines
{

struct ShiftControl : public BrnSound::Logic::BrnEffectControl
{
    // DWARF BrnShiftControl.h:25. The shift stage the sound state machine tracks.
    enum EShiftStage
    {
        E_SHFT_NONE = 0,
    };

    // DWARF BrnShiftControl.h:37. The post-shift LFO variant.
    enum EPostShiftLFO
    {
        E_SHIFT_LFO_NONE = 0,
    };

    // DWARF BrnShiftControl.h:42. The shift-activator interface (un-homed; opaque).
    struct IShiftingActivator;

    ShiftControl();             // @ 0x826AF010
    virtual ~ShiftControl();    // anchor for the scalar deleting destructor @ 0x826AF200

    // ---- members in DWARF order (offsets are X360 facts, not asserted on host) ----
    void*         mpPhysicsControl;         // +0x38
    void*         mpEngineControl;
    void*         mpHybridControl;
    bool          mbNeed_ShiftGearSnd;      // +0x44
    bool          mbNeed_DisengageSnd;      // +0x45
    bool          mbNeed_EngageSnd;         // +0x46
    u8            mau8ShiftingPatternData[16]; // +0x48: Attrib::Gen::shiftpattern (opaque span; see FLAG)
    EShiftStage   meShiftState;             // +0x58
    EShiftStage   meShiftStageChanged;      // +0x5C
    s32           miRaceCarIndex;           // +0x60
    EPostShiftLFO meShift_LFO;              // +0x64
    f32           mfVOL_LFO_AMP;            // +0x68
    f32           mfVOL_LFO_FRQ;            // +0x6C
    f32           mfRPM_LFO_AMP;            // +0x70
    f32           mfRPM_LFO_FRQ;            // +0x74
    CgsSound::Utils::InterpolateLine mInterpRPM_LFODecay;  // +0x78
    CgsSound::Utils::InterpolateLine mInterpVol_LFODecay;  // +0x94
    f32           mfRPMAtShift;             // +0xB0
    f32           mfLastUpShift;            // +0xB4
    IShiftingActivator* mpShiftingActivator; // +0xB8
    CgsSound::Utils::InterpolateLine mInterpShiftThrottle; // +0xBC
    CgsSound::Utils::InterpolateLine mInterpShiftRPM;      // +0xD8
    CgsSound::Utils::InterpolateLine mInterpShiftVol;      // +0xF4
};

} // namespace Engines
} // namespace Vehicles
} // namespace BrnSound

#endif // BRN_SOUND_VEHICLES_ENGINES_SHIFT_CONTROL_H

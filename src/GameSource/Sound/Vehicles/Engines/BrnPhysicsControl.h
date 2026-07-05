#ifndef BRN_SOUND_VEHICLES_ENGINES_PHYSICS_CONTROL_H
#define BRN_SOUND_VEHICLES_ENGINES_PHYSICS_CONTROL_H

#include "types.hpp"
#include "GameSource/Sound/Module/LogicModule/BrnEffectControl.h"   // committed BrnEffectControl dual base (BY NAME)
#include "GameSource/Sound/Vehicles/BrnVehicleState.h"              // VehicleState::EEngineComponentType / GetEngineComponentName / VehicleData (BY NAME)
#include "SDKs/Packages/AttribSys/1.2.1.2/AttribSys/runtime/common/AttributeKey.h" // Attribute::Key (BY NAME)

// =============================================================================
// BrnSound::Vehicles::Engines::PhysicsControl
//   GameSource/Sound/Vehicles/Engines/BrnPhysicsControl.{h,cpp}  (DWARF home)
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX (semantic parity, not byte match).
// PhysicsControl is the per-car physics->engine-audio bridge (DWARF
// BrnPhysicsControl.h:41: PhysicsControl : public BrnEffectControl). It caches the
// raw physics blob + a processed PhysicsData snapshot, plus the engine-rev intro
// block, and exposes engine-component key/name lookups back into VehicleState.
//
// FLAG (opaque-span layout): the two large embedded sub-objects the ctor constructs --
// mProcessedPhysicsData (PhysicsControl::PhysicsData, a big DataPoint/Average-of-
// Vector3/Matrix44 aggregate @ +0x40) and mVehicleEngineAttributes (Attrib::Gen::
// vehicleengine @ +0x228) -- have NO homed type in src. Per the anti-fabrication rule
// they are modelled as opaque byte spans (documented X360 sizes) rather than fabricated
// members; the intro-reving block is likewise a byte span with the one attested trailing
// flag byte. Named scalar members (mpVehicleState/mfOscillator/mpVehiclePhysicsData/...)
// are pinned BY NAME. Absolute offsets are NOT static_asserted across the 32/64 boundary.
// =============================================================================

namespace BrnSound
{
namespace Vehicles
{
namespace Engines
{

// BrnPhysicsControl.h:41 (DWARF). Reuses the committed BrnEffectControl dual base BY NAME.
struct PhysicsControl : public BrnSound::Logic::BrnEffectControl
{
    // BrnPhysicsControl.h:278 (DWARF). Intro-reving (start-line rev) sub-state.
    enum eIntroRevingState
    {
        E_NIS_REVING_STATE_OFF       = 0,
        E_NIS_REVING_STATE_STARTLINE = 1,
        E_NIS_REVING_STATE_RESUMING  = 2,
    };

    PhysicsControl();               // @ 0x826C8890
    virtual ~PhysicsControl();      // anchor for the vector deleting destructor @ 0x826AF8B0

    // @ 0x82682CA8 (DWARF h:266). Forward to VehicleState::GetEngineComponentName.
    const char* GetEngineComponentName( BrnSound::Vehicles::VehicleState::EEngineComponentType aeComponentType );
    // @ 0x82682D10 (DWARF h:269). Read VehicleState's mEngineComponentKey[type].
    Attribute::Key GetEngineComponentKey( BrnSound::Vehicles::VehicleState::EEngineComponentType aeComponentType );
    // @ 0x82682DA0 (DWARF h:260). Return the cached raw physics blob.
    const BrnSound::Vehicles::VehicleData* GetRawPhysicsData() const;

    // @ 0x82684368 (DWARF h:218). Per-class static RTTI descriptor ("PhysicsControl").
    // 2-instruction leaf returning &sTypeInfo (function-local static ClassTypeInfo<
    // EffectControl>), the committed per-class GetStaticTypeInfo() accessor form
    // (ExplosionState::GetStaticTypeInfo precedent).
    static CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::EffectControl>* GetStaticTypeInfo();

    // ---- named scalar members (BY NAME) + opaque spans (X360 sizes) ----
    // Base BrnEffectControl occupies +0x00..~+0x34.
    u8    mau8BaseGap[4];                 // +0x08..+0x0B: base-region scratch (inlined base zero-init)
    void* mpVehicleState;                 // +0x0C: the owning VehicleState (opaque; GetEngineComponentKey byte-views it)
    u8    mau8Gap0x10[12];                // +0x10..+0x1B: base-region scratch
    f32   mfOscillator;                   // +0x1C
    f32   mfAngularVelocityAccumulator;   // +0x20
    u8    mau8Gap0x24[20];                // +0x24..+0x37: base-region scratch
    const BrnSound::Vehicles::VehicleData* mpVehiclePhysicsData; // +0x38 (DWARF first own member)

    // +0x40: PhysicsControl::PhysicsData mProcessedPhysicsData (un-homed aggregate; span).
    u8    mau8ProcessedPhysicsData[0x1E8]; // +0x40..+0x227 (0x228 - 0x40)
    // +0x228: Attrib::Gen::vehicleengine mVehicleEngineAttributes (un-homed; span).
    u8    mau8VehicleEngineAttributes[0x20]; // +0x228..+0x247 (0x248 - 0x228)

    // +0x248..+0x287: intro-reving / start-line block. Cleared, trailing flag @ +0x284 = 1.
    static const u32 KU_INTRO_FLAG_REL = 0x284 - 0x248; // relative index of the flag byte (= 0x3C)
    u8    maIntroRevingBlock[0x40];       // +0x248..+0x287
};

} // namespace Engines
} // namespace Vehicles
} // namespace BrnSound

#endif // BRN_SOUND_VEHICLES_ENGINES_PHYSICS_CONTROL_H

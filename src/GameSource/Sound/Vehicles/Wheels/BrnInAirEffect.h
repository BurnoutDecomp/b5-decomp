#ifndef BRN_SOUND_VEHICLES_WHEELS_IN_AIR_EFFECT_H
#define BRN_SOUND_VEHICLES_WHEELS_IN_AIR_EFFECT_H

#include "types.hpp"
#include "GameSource/Sound/Module/LogicModule/BrnEffectObject.h"   // committed BrnEffectObject dual base (BY NAME)
#include "GameShared/GameClasses/Sound/CgsSoundUtils.h"            // CgsSound::Utils::DataPoint<T> (BY NAME)
#include "GameSource/Sound/Global/BrnMusicEffect.h"                // BrnSound::Logic::MusicEffect::EJunkyardAmbience (BY NAME)

// =============================================================================
// BrnSound::Vehicles::Wheels::InAirEffect  (+ TrafficInAir, JunkyardInAirEffect)
//   GameSource/Sound/Vehicles/Wheels/BrnInAirEffect.{h,cpp}  (DWARF home)
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX (semantic parity, not byte match).
// InAirEffect is the wheels-in-air landing/suspension sound EFFECT OBJECT. DWARF
// (BrnInAirEffect.h:163): InAirEffect : public BrnEffectObject. It is subclassed by
// TrafficInAir (traffic-car landings) and JunkyardInAirEffect (junkyard landings).
//
// FLAG (MINIMAL base home): InAirEffect's own body slice (GetSampleLandingId @
// 0x8269AA68) is BLOCKED in this wave -- it needs the un-homed
// BrnSound::Logic::Collision::ECollisionSpliceTags enum and the AttribSys
// CrashBinUtils<Attrib::Gen::crashbin>::GetSampleIds helper + crashbin Attribute::Key
// constants, none of which are homed anywhere in src (see .cpp BLOCK note). The base's
// full member surface (SuspensionStatus / InAirPhysicsData / mBin crashbin /
// muRoundRobin) is therefore DECLARATION-DEFERRED; only the polymorphic surface the
// two subclasses need (ProcessUpdate override slot + the virtual dtor) is modelled.
//
// LAYOUT NOTE (X360 32-bit vs host 64-bit): members are pinned BY NAME + SEQUENCE;
// absolute offsets are NOT static_asserted across pointer members on the 64-bit host.
// =============================================================================

// (2026-08-25, audio-faithfulness wave 5): the former `namespace BrnReplays {
// namespace SoundSerialiser { GetTrafficEnt(void*, void*) } }` shim is RETIRED --
// it declared a NAMESPACE with the same qualified name as the real `class
// BrnReplays::SoundSerialiser` (BrnReplaySoundSerialiser.h, fully homed since
// wave 4), an outright ill-formed collision if ever co-included. The .cpp now
// calls the real member GetTrafficEnt(u32) on the serialiser object embedded in
// the sound logic module.

namespace BrnSound
{
namespace Vehicles
{
namespace Wheels
{

// BrnInAirEffect.h:163 (DWARF). Reuses the committed BrnEffectObject dual base BY NAME.
// Minimal: only the two virtuals the subclasses drive are declared here.
struct InAirEffect : public BrnSound::Logic::BrnEffectObject
{
    InAirEffect() {}
    virtual ~InAirEffect();

    // BrnInAirEffect.h:269 (DWARF). The per-frame update the subclasses override.
    // Declared only (base body is a separate un-homed slice); TrafficInAir::ProcessUpdate
    // tail-calls this BY NAME -- the declaration satisfies the per-TU compile gate.
    virtual void ProcessUpdate();
};

// BrnInAirEffect.h:273 (DWARF): TrafficInAir : public InAirEffect. Overrides
// ProcessUpdate to stamp the per-traffic-car replay record. mpTrafficEntity (DWARF
// h:289) uses the un-homed nested type TrafficStateManager::Slot::TrafficSoundEntity
// and is DECLARATION-DEFERRED (not touched by this slice's functions).
struct TrafficInAir : public InAirEffect
{
    TrafficInAir() {}
    virtual ~TrafficInAir();                 // anchor for the scalar deleting destructor @ 0x826FDD18

    virtual void ProcessUpdate();            // @ 0x826B7340 (real override)
};

// BrnInAirEffect.h:104 (DWARF): JunkyardInAirEffect : public InAirEffect. Adds the
// two DWARF leaf members mbIsVehicleValid (DataPoint<bool>) + meJunkyardAmbience.
struct JunkyardInAirEffect : public InAirEffect
{
    JunkyardInAirEffect();                   // @ 0x826F4200
    virtual ~JunkyardInAirEffect();          // anchor for the vector deleting destructor @ 0x826FDBD8

    // @ 0x826FDB78 -- RTTI factory hook.
    static CgsSound::Logic::EffectObject* CreateObject( u32 luType );

    // DWARF BrnInAirEffect.h:111/115. The only two leaf members the ctor writes.
    CgsSound::Utils::DataPoint<bool>            mbIsVehicleValid;   // @ +0x3F0 (both bytes 0)
    BrnSound::Logic::MusicEffect::EJunkyardAmbience meJunkyardAmbience; // @ +0x3F4
};

} // namespace Wheels
} // namespace Vehicles
} // namespace BrnSound

#endif // BRN_SOUND_VEHICLES_WHEELS_IN_AIR_EFFECT_H

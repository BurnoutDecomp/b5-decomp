#ifndef BRN_SOUND_VEHICLES_ENGINES_AI_PHYSICS_CONTROL_H
#define BRN_SOUND_VEHICLES_ENGINES_AI_PHYSICS_CONTROL_H

#include "types.hpp"
#include "GameSource/Sound/Vehicles/Engines/BrnPhysicsControl.h"          // PhysicsControl base (BY NAME)
#include "GameSource/Physics/VehicleManager/VehiclePhysics/Engine.h"       // BrnPhysics::Vehicle::Engine (the AI engine sim)
#include "GameShared/GameClasses/Sound/CgsSoundUtils.h"                    // CgsSound::Utils::Average
#include "BrnCommonTypes.h"                                                // Vector3

// =============================================================================
// BrnSound::Vehicles::Engines::AIPhysicsControl
//   GameSource/Sound/Vehicles/Engines/BrnAIPhysicsControl.{h,cpp}  (DWARF home)
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX (semantic parity, not byte match).
// DWARF (BrnAIPhysicsControl.h:55): AIPhysicsControl : public PhysicsControl.
// The AI car's physics -> engine-audio bridge. The race-car output record the
// entity module publishes for an AI car carries no usable engine state, so this
// control runs its OWN BrnPhysics::Vehicle::Engine off the car's rear-wheel
// angular velocity and forward speed, writes the simulated RPM / gear (and a
// 9-frame averaged velocity) INTO the state's RaceCarState copy, and only then
// lets the shared PhysicsControl::UpdateParams derive the AEMS parameter feed. It
// also ramps its own throttle, drives the occlusion DMix input (slot 9) and posts
// the AI passby / near-miss events.
//
// RTTI: descriptor 0x82F2F638 {0x20000, "AIPhysicsControl", base PhysicsControl
// 0x82F2F578, &CreateObject @0x826E4558}, registered from the CRT init bank
// @0x82C62560. Vtables: primary off_820AF4B4 (+8 UpdateCollisionPassbys @0x826B4DF8,
// +12/+20 folded-empty `blr` = the DWARF's UpdateFx / UpdateCrashStreams overrides,
// +16 UpdateStartLineReving @0x826CEC90), EffectBase sub-object off_820AF480 (+20
// Attach @0x826CE860, +24 UpdateParams @0x826CE8D8, +36 Notify @0x8269A610,
// +48 GetTypeName @0x82685300).
//
// X360 layout over the PhysicsControl base (32-bit; BY NAME on the host; the
// sub-object virtuals see this+4): +0x28C mAverageRPM, +0x2B8 mAverageWheelVel,
// +0x330 mAverageVelocity, +0x3DC mAverageSpeedMps, +0x410 mPhysicsEngine (0xD0),
// +0x4E0 mfPassbyTriggered, +0x4E4 mfNearMissPassby, +0x4E8 mu8NotOccludedCount,
// +0x4E9 mbIsCurrentlyOccluded, +0x4EA mbEnableOcclusion, +0x4EB mbInsideRadius;
// sizeof 0x4F0 (CreateObject allocates 1264).
// =============================================================================

namespace BrnSound
{
namespace Vehicles
{
namespace Engines
{

struct AIPhysicsControl : public PhysicsControl
{
    // DWARF BrnAIPhysicsControl.h:42.
    typedef BrnPhysics::Vehicle::Engine PhysicsEngine;

    // The payload of sound message 39 (E_SOUNDMESSAGE_ENABLE_OCCLUSION) as its one
    // consumer reads it: Notify @0x8269A63C `lbz r11, 0x10(msg)` -> `stb 0x4EA(this)`.
    struct EnableOcclusionData
    {
        bool mbEnable;
    };

    AIPhysicsControl();                       // @ 0x826CE758
    virtual ~AIPhysicsControl();              // anchor for the vector deleting destructor @ 0x826B49F8

    // -- RTTI (DWARF cpp:59).
    virtual CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::EffectControl>* GetTypeInfo() const;
    virtual const char* GetTypeName() const;                                             // @ 0x82685300
    static CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::EffectControl>* GetStaticTypeInfo();
    static CgsSound::Logic::EffectControl* CreateObject( u32 luType );                   // @ 0x826E4558

    virtual bool Attach();                                                               // @ 0x826CE860 (DWARF cpp:118)
    virtual void UpdateParams( f32 afTimeStep );                                         // @ 0x826CE8D8 (DWARF cpp:149)
    virtual void Notify( const CgsSound::Io::MessageHeader* apkMessage );                // @ 0x8269A610 (DWARF cpp:459)

    virtual void UpdateCollisionPassbys( f32 afTimeStep );                               // @ 0x826B4DF8 (DWARF cpp:412)
    virtual void UpdateStartLineReving( f32 afTimeStep );                                // @ 0x826CEC90 (DWARF cpp:511)

private:
    void PlayPassBy( f32 afRelativeVelocityMagnitude, bool abNearMiss );                 // @ 0x8269A4C0 (DWARF cpp:362)
    void UpdateAIPassbys( f32 afTimeStep );                                              // @ 0x826B4A98 (DWARF cpp:268)

    // [DIAG] NOT IN THE X360 BINARY -- BRN_AI_SOUND_DIAG (see BrnAISoundDiag.h).
    void AIEngineWitness( f32 afTimeStep );

    CgsSound::Utils::Average<9u,  f32>     mAverageRPM;         // DWARF h:88   +0x28C
    CgsSound::Utils::Average<25u, f32>     mAverageWheelVel;    // DWARF h:89   +0x2B8
    CgsSound::Utils::Average<9u,  Vector3> mAverageVelocity;    // DWARF h:90   +0x330
    CgsSound::Utils::Average<9u,  f32>     mAverageSpeedMps;    // DWARF h:91   +0x3DC
    PhysicsEngine                          mPhysicsEngine;      // DWARF h:93   +0x410
    f32                                    mfPassbyTriggered;   // DWARF h:96   +0x4E0
    f32                                    mfNearMissPassby;    // DWARF h:97   +0x4E4
    u8                                     mu8NotOccludedCount; // DWARF h:99   +0x4E8
    bool                                   mbIsCurrentlyOccluded; // DWARF h:100 +0x4E9
    bool                                   mbEnableOcclusion;   // DWARF h:101  +0x4EA
    bool                                   mbInsideRadius;      // DWARF h:102  +0x4EB

    // [DIAG] NOT IN THE X360 BINARY -- witness accumulator.
    f32                                    mfDiagAIWitnessTimer;
};

} // namespace Engines
} // namespace Vehicles
} // namespace BrnSound

#endif // BRN_SOUND_VEHICLES_ENGINES_AI_PHYSICS_CONTROL_H

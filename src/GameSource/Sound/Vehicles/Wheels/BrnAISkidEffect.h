#ifndef BRN_SOUND_VEHICLES_WHEELS_AI_SKID_EFFECT_H
#define BRN_SOUND_VEHICLES_WHEELS_AI_SKID_EFFECT_H

#include "types.hpp"
#include "GameSource/Sound/Module/LogicModule/BrnEffectObject.h"   // committed BrnEffectObject dual base (BY NAME)
#include "GameShared/GameClasses/Sound/Logic/CgsVoiceWrapper.h"    // CgsSound::Logic::VoiceWrapper member (BY NAME)
#include "GameShared/GameClasses/Sound/CgsSoundUtils.h"            // CgsSound::Utils::PathLine<2>
#include "GameSource/Physics/VehicleManager/BrnVehicleConstants.h" // BrnPhysics::Vehicle::EImpactType

// =============================================================================
// BrnSound::Vehicles::Wheels::AISkidEffect
//   GameSource/Sound/Vehicles/Wheels/BrnAISkidEffect.{h,cpp}  (DWARF home)
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX (semantic parity, not byte match).
// The AI car's skid voice: one "AEMS_Skids_Traffic" AEMS voice on the PLAYER
// manager's Skids.abi bank, driven by the car's drift scale and by a
// crash/impact envelope (PathLine<2>). It is effect 4 of the AI vehicle state
// mask (0x4218); until it existed the alias mechanism handed the AI state the
// PLAYER SkidEffect, whose Attach asserted `mCreateParams.mpContent` every frame
// (the AI manager has no Skids content of its own).
//
// RTTI: descriptor 0x82F2F774 {0x20040, "AISkidEffect", base BrnEffectObject
// 0x82F2E7FC, &CreateObject @0x826D0840}. Vtables (ctor @0x826D0780): primary
// 0x820B3EE0 (+4 GetController @0x82685D38, +8 AttachController @0x82685D48,
// +20 Attach @0x826F4D20, +24 UpdateParams @0x826B8FD0, +28 ProcessUpdate
// @0x826E5F10, +32 Detach @0x826F4E98, +36 Notify @0x826D0920, +44 GetTypeInfo
// @0x82685D18, +48 GetTypeName @0x82685D28), IResourceRequester 0x820B3F14.
//
// X360 layout (32-bit; BY NAME on the host): +0x34 mpPhysicsControl, +0x3C
// mDriftInterp (PathLine<2>, 0x38), +0x74 mfDriftFactor, +0x78 mSkidsVoice;
// sizeof 200 (CreateObject).
// =============================================================================

namespace BrnSound
{
namespace Vehicles
{
namespace Engines { struct PhysicsControl; }
namespace Wheels
{

struct AISkidEffect : public BrnSound::Logic::BrnEffectObject
{
    // The payload of sound message 19 (E_SOUNDMESSAGE_RACE_CAR_IMPACT) as its
    // consumer reads it: Notify @0x826D0920 `lwz 0x10/0x14/0x18(msg)` -> the
    // impact type and the two race-car indices.
    struct RaceCarImpactData
    {
        s32 miImpactType;   // BrnPhysics::Vehicle::EImpactType
        s32 miRaceCarA;
        s32 miRaceCarB;
    };

    AISkidEffect();                                                   // @ 0x826D0780
    virtual ~AISkidEffect();                                          // @ 0x826D08A0

    virtual CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::EffectObject>* GetTypeInfo() const; // @ 0x82685D18
    virtual const char* GetTypeName() const;                          // @ 0x82685D28
    static CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::EffectObject>* GetStaticTypeInfo();
    static CgsSound::Logic::EffectObject* CreateObject( u32 luType ); // @ 0x826D0840

    virtual s32  GetController( s32 aiSlot );                         // @ 0x82685D38
    virtual void AttachController( CgsSound::Logic::EffectBase* apController ); // @ 0x82685D48
    virtual bool Attach();                                            // @ 0x826F4D20
    virtual void UpdateParams( f32 afTimeStep );                      // @ 0x826B8FD0
    virtual void ProcessUpdate();                                     // @ 0x826E5F10
    virtual bool Detach();                                            // @ 0x826F4E98
    virtual void Notify( const CgsSound::Io::MessageHeader* apkMessage ); // @ 0x826D0920

    void ReceiveImpact( BrnPhysics::Vehicle::EImpactType aeType, bool abHitByPlayer ); // @ 0x826B93A0

private:
    BrnSound::Vehicles::Engines::PhysicsControl* mpPhysicsControl;    // DWARF h  +0x34
    CgsSound::Utils::PathLine<2u>                mDriftInterp;        // DWARF h  +0x3C
    f32                                          mfDriftFactor;       // DWARF h  +0x74
    CgsSound::Logic::VoiceWrapper                mSkidsVoice;         // DWARF h  +0x78
};

} // namespace Wheels
} // namespace Vehicles
} // namespace BrnSound

#endif // BRN_SOUND_VEHICLES_WHEELS_AI_SKID_EFFECT_H

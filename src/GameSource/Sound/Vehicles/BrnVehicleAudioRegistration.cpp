#include "GameShared/GameClasses/Sound/Logic/CgsEffectBase.h"

#include "GameSource/Sound/Vehicles/Brn3dCarPosition.h"
#include "GameSource/Sound/Vehicles/Engines/BrnPhysicsControl.h"
#include "GameSource/Sound/Vehicles/Engines/BrnShiftControl.h"
#include "GameSource/Sound/Vehicles/Engines/BrnClutchControl.h"
#include "GameSource/Sound/Vehicles/Engines/BrnEngineControl.h"
#include "GameSource/Sound/Vehicles/Engines/BrnHybridExhaustControl.h"
#include "GameSource/Sound/Vehicles/Engines/BrnHybridEngineControl.h"
#include "GameSource/Sound/Vehicles/Engines/BrnDualGinsuEffect.h"
#include "GameSource/Sound/Vehicles/Engines/BrnDualGinsuExhaustEffect.h"
#include "GameSource/Sound/Vehicles/Engines/BrnBoostEffect.h"
#include "GameSource/Sound/Vehicles/Engines/BrnTurboEffect.h"
#include "GameSource/Sound/Vehicles/Engines/BrnWhineEffect.h"
#include "GameSource/Sound/Vehicles/Engines/BrnSweetenersEffect.h"
#include "GameSource/Sound/Vehicles/Wheels/BrnWheelControl.h"
#include "GameSource/Sound/Vehicles/Wheels/BrnSkidEffect.h"
#include "GameSource/Sound/Vehicles/Wheels/BrnInAirEffect.h"
#include "GameSource/Sound/Vehicles/Wheels/BrnRoadNoise.h"
#include "GameSource/Sound/Vehicles/Environment/BrnEnclosureControl.h"
#include "GameSource/Sound/Vehicles/Environment/BrnStaticPassbyControl.h"
#include "GameSource/Sound/Vehicles/Environment/BrnSpeedStreamControl.h"
#include "GameSource/Sound/Vehicles/Environment/BrnAmbienceControl.h"
#include "GameSource/Sound/Vehicles/Environment/BrnCrashStreamEffect.h"
#include "GameSource/Sound/Vehicles/Environment/BrnSpeedStreamEffect.h"
#include "GameSource/Sound/Vehicles/Environment/BrnAmbienceEffect.h"
#include "GameSource/Sound/Vehicles/Environment/BrnReverbEffect.h"
#include "GameSource/Sound/Vehicles/Deformation/BrnDeformationEffect.h"

// The original Unity translation unit's static initializer registers the complete
// player-car control/effect family before the sound module constructs its state
// managers.  DecFIGS' __static_initialization_and_destruction_0 @ 0x85FA1C pins
// these ObjectIDs and class names; the ARTIST state masks select them by the middle
// seven bits of ObjectID.  Keeping the table together here mirrors that original
// initializer and avoids creating PC-only stand-in effect classes.
namespace
{
template <class T>
CgsSound::Logic::EffectControl* CreateVehicleControl(u32)
{
    return new T();
}

template <class T>
CgsSound::Logic::EffectObject* CreateVehicleEffect(u32)
{
    return new T();
}

template <class T>
CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::EffectControl>* RegisterControl(
    s32 aiObjectId, const char* apTypeName)
{
    static CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::EffectControl> sTypeInfo(
        aiObjectId, apTypeName,
        CgsSound::Logic::EffectControl::GetStaticTypeInfo(),
        &CreateVehicleControl<T>);
    return CgsSound::Logic::EffectControl::AddToClassTypeInfoArray(&sTypeInfo);
}

template <class T>
CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::EffectObject>* RegisterEffect(
    s32 aiObjectId, const char* apTypeName)
{
    static CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::EffectObject> sTypeInfo(
        aiObjectId, apTypeName,
        CgsSound::Logic::EffectObject::GetStaticTypeInfo(),
        &CreateVehicleEffect<T>);
    return CgsSound::Logic::EffectObject::AddToClassTypeInfoArray(&sTypeInfo);
}

using namespace BrnSound::Vehicles;

// Effect controls: ObjectID = 0x10000 + (effect-id << 4).
static CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::EffectControl>* const gpPhysicsControl =
    RegisterControl<Engines::PhysicsControl>(0x10000, "PhysicsControl");
static CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::EffectControl>* const gpWheelControl =
    RegisterControl<Wheels::WheelControl>(0x10010, "WheelControl");
static CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::EffectControl>* const gpShiftControl =
    RegisterControl<Engines::ShiftControl>(0x10020, "ShiftControl");
static CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::EffectControl>* const gpClutchControl =
    RegisterControl<Engines::ClutchControl>(0x10030, "ClutchControl");
static CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::EffectControl>* const gpEngineControl =
    RegisterControl<Engines::EngineControl>(0x10040, "EngineControl");
static CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::EffectControl>* const gpHybridExhaustControl =
    RegisterControl<Engines::HybridExhaustControl>(0x10050, "HybridExhaustControl");
static CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::EffectControl>* const gpHybridEngineControl =
    RegisterControl<Engines::HybridEngineControl>(0x10060, "HybridEngineControl");
static CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::EffectControl>* const gpCar3DControl =
    RegisterControl<Car3DControl>(0x10070, "Car3DControl");
static CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::EffectControl>* const gpLeftSide3dControl =
    RegisterControl<LeftSide3dControl>(0x10080, "LeftSide3dControl");
static CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::EffectControl>* const gpRightSide3dControl =
    RegisterControl<RightSide3dControl>(0x10090, "RightSide3dControl");
static CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::EffectControl>* const gpEnclosureControl =
    RegisterControl<Environment::EnclosureControl>(0x100A0, "EnclosureControl");
static CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::EffectControl>* const gpExhaust3dControl =
    RegisterControl<Exhaust3dControl>(0x100C0, "Exhaust3dControl");
static CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::EffectControl>* const gpEngine3dControl =
    RegisterControl<Engine3dControl>(0x100D0, "Engine3dControl");
static CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::EffectControl>* const gpStaticPassbyControl =
    RegisterControl<Environment::StaticPassbyControl>(0x100E0, "StaticPassbyControl");
static CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::EffectControl>* const gpSpeedStreamControl =
    RegisterControl<Environment::SpeedStreamControl>(0x100F0, "SpeedStreamControl");
static CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::EffectControl>* const gpAmbienceControl =
    RegisterControl<Environment::AmbienceControl>(0x10100, "AmbienceControl");

// Effect objects selected by PlayerVehicleStateManager::Prepare's 0x1BFF9 mask.
static CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::EffectObject>* const gpDualGinsuEffect =
    RegisterEffect<Engines::DualGinsuEffect>(0x10000, "DualGinsuEffect");
static CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::EffectObject>* const gpDualGinsuExhaustEffect =
    RegisterEffect<Engines::DualGinsuExhaustEffect>(0x10030, "DualGinsuExhaustEffect");
static CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::EffectObject>* const gpSkidEffect =
    RegisterEffect<Wheels::SkidEffect>(0x10040, "SkidEffect");
static CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::EffectObject>* const gpBoostEffect =
    RegisterEffect<Engines::BoostEffect>(0x10050, "BoostEffect");
static CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::EffectObject>* const gpCrashStreamEffect =
    RegisterEffect<Environment::CrashStreamEffect>(0x10060, "CrashStreamEffect");
static CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::EffectObject>* const gpSpeedStreamEffect =
    RegisterEffect<Environment::SpeedStreamEffect>(0x10070, "SpeedStreamEffect");
static CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::EffectObject>* const gpAmbienceEffect =
    RegisterEffect<Environment::AmbienceEffect>(0x10080, "AmbienceEffect");
static CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::EffectObject>* const gpInAirEffect =
    RegisterEffect<Wheels::InAirEffect>(0x10090, "InAirEffect");
static CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::EffectObject>* const gpRoadnoiseEffect =
    RegisterEffect<Wheels::RoadnoiseEffect>(0x100A0, "RoadnoiseEffect");
static CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::EffectObject>* const gpTurboEffect =
    RegisterEffect<Engines::TurboEffect>(0x100B0, "TurboEffect");
static CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::EffectObject>* const gpWhineEffect =
    RegisterEffect<Engines::WhineEffect>(0x100C0, "WhineEffect");
static CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::EffectObject>* const gpReverbEffect =
    RegisterEffect<Environment::ReverbEffect>(0x100D0, "ReverbEffect");
static CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::EffectObject>* const gpDeformationEffect =
    RegisterEffect<Deformation::DeformationEffect>(0x100F0, "DeformationEffect");
static CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::EffectObject>* const gpSweetenersEffect =
    RegisterEffect<Engines::SweetenersEffect>(0x10100, "SweetenersEffect");
}

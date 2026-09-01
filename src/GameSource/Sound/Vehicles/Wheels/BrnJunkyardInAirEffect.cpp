#include "GameSource/Sound/Vehicles/Wheels/BrnInAirEffect.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Sound/IO/CgsMessage.h"
#include "GameSource/Physics/VehicleManager/SharedIO/BrnVehicleEvents.h"
#include "GameSource/Sound/Module/BrnRootSoundModuleIo.h"
#include "GameSource/Sound/Module/LogicModule/BrnMessageData.h"
#include "GameSource/Sound/Module/LogicModule/BrnSoundLogicModule.h"
#include "GameSource/World/EntityModules/RaceCarEntityModule/SharedIO/BrnRaceCarEntityModuleOutputInterface.h"

#include <cmath>

// The global state table creates JunkyardInAirEffect even before vehicle sound
// states are active. This TU keeps that exact constructor/factory/RTTI surface
// independently mountable from BrnInAirEffect.cpp's unrelated Traffic replay path.

namespace BrnSound
{
namespace Vehicles
{
namespace Wheels
{

CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::EffectObject>*
JunkyardInAirEffect::GetStaticTypeInfo()
{
    static CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::EffectObject> sTypeInfo(
        0x60, "JunkyardInAirEffect", CgsSound::Logic::EffectObject::GetStaticTypeInfo(),
        &JunkyardInAirEffect::CreateObject);
    return &sTypeInfo;
}

CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::EffectObject>*
JunkyardInAirEffect::GetTypeInfo() const
{
    return GetStaticTypeInfo();
}

const char* JunkyardInAirEffect::GetTypeName() const
{
    return "JunkyardInAirEffect";
}

static CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::EffectObject>* const
    gpJunkyardInAirEffectReg = CgsSound::Logic::EffectObject::AddToClassTypeInfoArray(
        JunkyardInAirEffect::GetStaticTypeInfo());

CgsSound::Logic::EffectObject* JunkyardInAirEffect::CreateObject(u32)
{
    return new JunkyardInAirEffect();
}

JunkyardInAirEffect::JunkyardInAirEffect()
    : InAirEffect()
    , mbIsVehicleValid(false)
    , meJunkyardAmbience(BrnSound::Logic::MusicEffect::E_JUNKYARD_AMBIENCE_NONE)
{
}

JunkyardInAirEffect::~JunkyardInAirEffect()
{
}

bool JunkyardInAirEffect::Attach()
{
    meJunkyardAmbience = BrnSound::Logic::MusicEffect::E_JUNKYARD_AMBIENCE_NONE;
    return InAirEffect::Attach();
}

void JunkyardInAirEffect::Notify(const CgsSound::Io::MessageHeader* apkMessage)
{
    CGS_ASSERT(apkMessage != nullptr, "lpMessageHeader");
    if (!apkMessage)
        return;

    if (apkMessage->GetEventId() == BrnSound::E_SOUNDMESSAGE_IN_JUNKYARD)
    {
        const CgsSound::Io::Message<BrnSound::Logic::MusicEffect::EJunkyardAmbience>*
            lpMessage = static_cast<const CgsSound::Io::Message<
                BrnSound::Logic::MusicEffect::EJunkyardAmbience>*>(apkMessage);
        meJunkyardAmbience = lpMessage->mData;
        CGS_ASSERT(
            meJunkyardAmbience < BrnSound::Logic::MusicEffect::E_JUNKYARD_AMBIENCE_COUNT,
            "meJunkyardAmbience < BrnSound::Logic::MusicEffect::E_JUNKYARD_AMBIENCE_COUNT");
    }
    else if (apkMessage->GetEventId() ==
             BrnSound::E_SOUNDMESSAGE_RESET_CAR_IN_JUNKYARD)
    {
        const CgsSound::Io::Message<bool>* lpMessage =
            static_cast<const CgsSound::Io::Message<bool>*>(apkMessage);
        for (u32 luWheel = 0; luWheel < 4; ++luWheel)
            mSuspensionStatus[luWheel].Clear();

        mPhysicsData.mfTimeInAir.mPreviousValue =
            lpMessage->mData ? 0.0f : -10000000.0f;
        mPhysicsData.mfTimeInAir.mCurrentValue = 0.0f;
        mbIsVehicleValid.Update(false);
    }
}

void JunkyardInAirEffect::UpdatePhysicsData(f32 afTimeStep)
{
    if (meJunkyardAmbience == BrnSound::Logic::MusicEffect::E_JUNKYARD_AMBIENCE_NONE)
        return;

    BrnSound::Module::SoundLogicModule* lpModule =
        static_cast<BrnSound::Module::SoundLogicModule*>(GetLogicModule());
    BrnSound::Module::Io::LogicInputBuffer* lpInput =
        lpModule ? lpModule->GetBrnInputStructure() : nullptr;
    const BrnWorld::RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface*
        lpVehicles = lpInput ? lpInput->GetVehicleInterface() : nullptr;
    if (!lpVehicles || !lpVehicles->IsPlayerCarActive())
        return;

    const EActiveRaceCarIndex lePlayer =
        lpVehicles->GetPlayerActiveRaceCarIndex();
    const BrnPhysics::Vehicle::RaceCarState* lpVehicleData =
        lpVehicles->GetRaceCarState(lePlayer);
    CGS_ASSERT(lpVehicleData != nullptr, "lpVehiclePhysicsData");
    if (!lpVehicleData)
        return;

    s32 liGroundedWheels = 0;
    for (u32 luWheel = 0; luWheel < 4; ++luWheel)
    {
        const BrnPhysics::Vehicle::WheelLite& lrWheel =
            lpVehicleData->maWheels[luWheel];
        const bool lbOnGround = lrWheel.mbAttached &&
            std::fabs(lrWheel.mfSuspensionHeight + 1.0f) >= 0.05f;
        mPhysicsData.maWheelOnGround[luWheel].Update(lbOnGround);
        mPhysicsData.mafSuspensionHeights[luWheel] = lrWheel.mfSuspensionHeight;
        if (lbOnGround)
            ++liGroundedWheels;
    }

    const bool lbIsOnGround = liGroundedWheels != 0;
    mPhysicsData.mIsOnGround.Update(lbIsOnGround);
    const f32 lfPreviousTime = mPhysicsData.mfTimeInAir.GetCurrent();
    mPhysicsData.mfTimeInAir.Update(
        lbIsOnGround ? 0.0f : lfPreviousTime + afTimeStep);
    mPhysicsData.mbIsCrashing = false;

    mbIsVehicleValid.Update(lpVehicleData->mbIsDriveable);
    if (mbIsVehicleValid.GetCurrent() && !mbIsVehicleValid.GetPrevious())
        FlushData(lpVehicleData);
    mPhysicsData.mfTimeSinceReset += afTimeStep;
}

void JunkyardInAirEffect::FlushData(
    const BrnPhysics::Vehicle::RaceCarState*)
{
    for (u32 luWheel = 0; luWheel < 4; ++luWheel)
    {
        mSuspensionStatus[luWheel].Clear();
        mSuspensionStatus[luWheel].mfSuspensionHeight =
            mPhysicsData.mafSuspensionHeights[luWheel];
    }
}

} // namespace Wheels
} // namespace Vehicles
} // namespace BrnSound

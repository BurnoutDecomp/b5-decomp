#include "GameSource/Sound/Vehicles/Wheels/BrnInAirEffect.h"

// The global state table creates JunkyardInAirEffect even before vehicle sound
// states are active. This TU keeps that exact constructor/factory/RTTI surface
// independently mountable from BrnInAirEffect.cpp's unrelated Traffic replay path.

namespace BrnSound
{
namespace Vehicles
{
namespace Wheels
{

InAirEffect::~InAirEffect()
{
}

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
    , meJunkyardAmbience(BrnSound::Logic::MusicEffect::E_JUNKYARD_AMBIENCE_NONE)
{
}

JunkyardInAirEffect::~JunkyardInAirEffect()
{
}

} // namespace Wheels
} // namespace Vehicles
} // namespace BrnSound

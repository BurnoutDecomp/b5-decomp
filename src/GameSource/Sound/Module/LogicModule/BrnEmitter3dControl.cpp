#include "GameSource/Sound/Module/LogicModule/BrnEmitter3dControl.h"
#include "GameSource/Sound/Module/LogicModule/Brn3DEffectControl.h"   // Brn3DEffectControl::sTypeInfo (the descriptor base, ARTIST 0x82F2E81C)

namespace BrnSound
{
namespace Logic
{
namespace World
{

Emitter3dControl::Emitter3dControl()
    : CgsSound::Logic::Cgs3dEffectControl()
{
}

Emitter3dControl::~Emitter3dControl()
{
}

// ARTIST 0x82F2F82C: {0x70000, "Emitter3dControl", Brn3DEffectControl, CreateObject}, registered by
// its startup thunk through ClassTypeInfo<EffectControl>::AddToClassTypeInfoArray.
CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::EffectControl> Emitter3dControl::sTypeInfo(
    0x70000, "Emitter3dControl", &BrnSound::Logic::Brn3DEffectControl::sTypeInfo, &Emitter3dControl::CreateObject);

const char* Emitter3dControl::GetTypeName() const
{
    return sTypeInfo.typeName;
}

CgsSound::Logic::EffectControl* Emitter3dControl::CreateObject(u32 luType)
{
    if (luType == 0)
        return new (sTypeInfo.typeName, false) Emitter3dControl();
    else
        return new (sTypeInfo.typeName, true) Emitter3dControl();
}

CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::EffectControl>*
Emitter3dControl::GetStaticTypeInfo()
{
    return &sTypeInfo;
}

CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::EffectControl>*
Emitter3dControl::GetTypeInfo() const
{
    return &sTypeInfo;
}


} // namespace World
} // namespace Logic
} // namespace BrnSound

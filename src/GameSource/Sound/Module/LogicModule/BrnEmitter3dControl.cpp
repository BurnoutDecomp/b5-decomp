#include "GameSource/Sound/Module/LogicModule/BrnEmitter3dControl.h"

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

const char* Emitter3dControl::GetTypeName() const
{
    return "Emitter3dControl";
}

CgsSound::Logic::EffectControl* Emitter3dControl::CreateObject(u32)
{
    return new Emitter3dControl();
}

CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::EffectControl>*
Emitter3dControl::GetStaticTypeInfo()
{
    static CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::EffectControl>
        sTypeInfo(0x70000, "Emitter3dControl", 0, &Emitter3dControl::CreateObject);
    return &sTypeInfo;
}

CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::EffectControl>*
Emitter3dControl::GetTypeInfo() const
{
    return GetStaticTypeInfo();
}

static CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::EffectControl>* const
    gpEmitter3dControlReg =
        CgsSound::Logic::EffectControl::AddToClassTypeInfoArray(
            Emitter3dControl::GetStaticTypeInfo());

} // namespace World
} // namespace Logic
} // namespace BrnSound

#include "GameSource/Sound/Global/BrnGlobalState.h"

// =============================================================================
// BrnSound::Logic::GlobalState — out-of-line bodies.
// Reconstructed from BURNOUT_X360_ARTIST.XEX. See BrnGlobalState.h for the
// inheritance rationale.
// =============================================================================

namespace BrnSound
{
namespace Logic
{

// ARTIST 0x82F2F86C.
CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::State> GlobalState::sTypeInfo(0, "GlobalState", &BrnState::sTypeInfo, &GlobalState::CreateObject);

const char* GlobalState::GetTypeName() const
{
    return sTypeInfo.typeName; // ARTIST 0x82686808
}

// ARTIST 0x826D2250 installs the class vtable before tearing down the effects.
GlobalState::~GlobalState()
{
    DestroyEffects();
}

CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::State>* GlobalState::GetStaticTypeInfo()
{
    return &sTypeInfo;
}

CgsSound::Logic::State* GlobalState::CreateObject(u32 luType)
{
    return new (sTypeInfo.typeName, luType != 0) GlobalState();
}

CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::State>* GlobalState::GetTypeInfo() const
{
    return &sTypeInfo;
}

static CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::State>* const gpGlobalStateReg =
    CgsSound::Logic::State::AddToClassTypeInfoArray(GlobalState::GetStaticTypeInfo());

} // namespace Logic
} // namespace BrnSound

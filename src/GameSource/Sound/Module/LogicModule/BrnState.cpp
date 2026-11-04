#include "GameSource/Sound/Module/LogicModule/BrnState.h"

// =============================================================================
// BrnSound::Logic::BrnState — out-of-line bodies.
// Reconstructed from BURNOUT_X360_ARTIST.XEX. See BrnState.h for the inheritance
// rationale.
// =============================================================================

namespace BrnSound
{
namespace Logic
{

// ARTIST 0x826C84D8 uses the base State teardown.
BrnState::~BrnState()
{
}

CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::State>* BrnState::GetStaticTypeInfo()
{
    static CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::State> sTypeInfo(
        -1, "BrnState", CgsSound::Logic::State::GetStaticTypeInfo(), &BrnState::CreateObject);
    return &sTypeInfo;
}

CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::State>* BrnState::GetTypeInfo() const
{
    return GetStaticTypeInfo(); // ARTIST 0x82682A88
}

const char* BrnState::GetTypeName() const
{
    return GetStaticTypeInfo()->typeName; // ARTIST 0x82682A98
}

CgsSound::Logic::State* BrnState::CreateObject(u32 luFlavour)
{
    return new (GetStaticTypeInfo()->typeName, luFlavour != 0) BrnState(); // ARTIST 0x826C8390
}

// ARTIST 0x82C61B48 registers descriptor 0x82F2E7DC.
static CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::State>* const gpBrnStateReg =
    CgsSound::Logic::State::AddToClassTypeInfoArray(BrnState::GetStaticTypeInfo());

} // namespace Logic
} // namespace BrnSound

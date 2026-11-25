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

// ARTIST 0x82F2E7DC.
CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::State> BrnState::sTypeInfo(-1, "BrnState", &State::sTypeInfo, &BrnState::CreateObject);

CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::State>* BrnState::GetStaticTypeInfo()
{
    return &sTypeInfo;
}

CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::State>* BrnState::GetTypeInfo() const
{
    return &sTypeInfo; // ARTIST 0x82682A88
}

const char* BrnState::GetTypeName() const
{
    return sTypeInfo.typeName; // ARTIST 0x82682A98
}

CgsSound::Logic::State* BrnState::CreateObject(u32 luFlavour)
{
    return new (sTypeInfo.typeName, luFlavour != 0) BrnState(); // ARTIST 0x826C8390
}

} // namespace Logic
} // namespace BrnSound

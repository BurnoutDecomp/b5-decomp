#include "GameSource/Sound/Explosions/BrnExplosionState.h"

#include "GameShared/GameClasses/Core/CgsAssertMessageAt.h"   // CGS_ASSERT

// =============================================================================
// BrnSound::Logic::Explosion::ExplosionState::Attach  @ 0x826891B8
//
// The X360 body is a single argument null-check:
//   cmplwi cr6, r4, 0          ; if (lpvAttachment == 0)
//   bne    cr6, <return>
//   bl     CgsDev::Assert::BeginAssert
//   ... FireAssert("lpvAttachment", "...BrnExplosionState.cpp", 69)
//   bl     CgsDev::Assert::EndAssert
//   <return>                   ; void
//
// No member is read or written; the function only validates its attachment
// pointer (the per-frame attachment the state binds to). CGS_ASSERT folds the
// Begin/Fire/End sequence; the baked d:\p4 file/line is replaced by __FILE__/
// __LINE__.
// =============================================================================

namespace BrnSound
{
namespace Logic
{
namespace Explosion
{

void ExplosionState::Attach(void* lpvAttachment)
{
    CGS_ASSERT(lpvAttachment != 0,
        CGS_ASSERT_MESSAGE_AT(
            "lpvAttachment",
            "d:\\p4\\b5_main\\burnout\\main\\code\\gamesource\\unity\\"
            "../Sound/Explosions/BrnExplosionState.cpp", 69 /* ARTIST 0x826891D4 */));
}

// ARTIST 0x826D5740 uses the base State teardown.
ExplosionState::~ExplosionState()
{
}

#if defined(_XBOX)
CGS_STATIC_ASSERT((sizeof(ExplosionState) == 0x54), "ExplosionState allocation size");
#endif

// ARTIST descriptor 0x82F2F9AC.
// ARTIST descriptor 0x82F2F9AC.
CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::State> ExplosionState::sTypeInfo(
    0x80000, "ExplosionState", &BrnState::sTypeInfo, &ExplosionState::CreateObject);

CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::State>* ExplosionState::GetStaticTypeInfo()
{
    return &sTypeInfo;
}

CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::State>* ExplosionState::GetTypeInfo() const
{
    return &sTypeInfo; // ARTIST 0x82689198
}

const char* ExplosionState::GetTypeName() const
{
    return sTypeInfo.typeName; // ARTIST 0x826891A8
}

CgsSound::Logic::State* ExplosionState::CreateObject(u32 luFlavour)
{
    if (luFlavour == 0)
        return new (sTypeInfo.typeName, false) ExplosionState(); // ARTIST 0x826D55F8
    else
        return new (sTypeInfo.typeName, true) ExplosionState();
}

} // namespace Explosion
} // namespace Logic
} // namespace BrnSound

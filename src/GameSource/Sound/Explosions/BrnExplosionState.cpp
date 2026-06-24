#include "GameSource/Sound/Explosions/BrnExplosionState.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

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
    CGS_ASSERT(lpvAttachment != 0, "lpvAttachment");
}

} // namespace Explosion
} // namespace Logic
} // namespace BrnSound

#include "GameSource/Sound/Passby/BrnPassbyState.h"

// =============================================================================
// BrnSound::Logic::Passby::PassbyState — out-of-line bodies.
// Reconstructed from BURNOUT_X360_ARTIST.XEX. See BrnPassbyState.h for the
// inheritance rationale and the un-recoverable-descriptor-fields FLAG.
//
// This TU's recon'd function set is exactly ONE entry:
//   GetStaticTypeInfo()  @ 0x82688FC8
// =============================================================================

namespace BrnSound
{
namespace Logic
{
namespace Passby
{

// ---------------------------------------------------------------------------
// GetStaticTypeInfo  @ 0x82688FC8
//
//   lis   r11, unk_82F2F95C@ha
//   addi  r3,  r11, unk_82F2F95C@l   ; r3 = &sTypeInfo
//   blr
//
// Returns the address of PassbyState's per-class static RTTI descriptor. The
// X360 builds this descriptor in rodata (at 0x82F2F95C). Here it is a
// function-local static, seeded with the canonical type name "PassbyState" and
// HONEST placeholder fields for the parts not recoverable from this leaf:
//   ObjectID        = 0        (FLAG: real registration id is DEFERRED)
//   mpcTypeName     = "PassbyState"
//   mpBaseTypeInfo  = nullptr  (FLAG: BrnState's descriptor chain is DEFERRED)
//   mpfnCreateObject= nullptr  (CreateObject @ 0x826D4978 is its own DEFERRED slice)
// This mirrors the committed CgsEffectBase.cpp GetStaticTypeInfo precedent: the
// observable return — a stable &sTypeInfo whose typeName is the recovered class
// name — matches, without fabricating the descriptor's id/base wiring.
//
// NOTE: BrnState.h's minimal CgsSound::Logic::ClassTypeInfo<T> has no constructor
// (aggregate members ObjectID / mpcTypeName / mpBaseTypeInfo / mpfnCreateObject),
// so the descriptor is aggregate-initialised here.
// ---------------------------------------------------------------------------
CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::State>* PassbyState::GetStaticTypeInfo()
{
    static CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::State> sTypeInfo =
    {
        0,             // ObjectID        (DEFERRED — real registration id)
        "PassbyState", // mpcTypeName
        nullptr,       // mpBaseTypeInfo  (DEFERRED — BrnState descriptor chain)
        nullptr,       // mpfnCreateObject(DEFERRED — CreateObject @ 0x826D4978)
    };
    return &sTypeInfo;
}

} // namespace Passby
} // namespace Logic
} // namespace BrnSound

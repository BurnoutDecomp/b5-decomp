#include "GameSource/Sound/Collision/BrnScrapeEffect.h"

// =============================================================================
// BrnSound::Logic::Collision::ScrapeEffect -- out-of-line bodies.
// Reconstructed from BURNOUT_X360_ARTIST.XEX. See BrnScrapeEffect.h for the class
// shape (DWARF: BrnScrapeEffect.h/.cpp) and the X360-32-bit-vs-host-64-bit offset note.
//
// This TU SHIPS the scalar deleting destructor @ 0x826E8A28 (its ~ScrapeEffect anchor).
//
// MapScrapeToMaterial @ 0x8269EC18 is verified store-for-store (both prior blockers
// resolved: CgsEntityId.h is 8/14/10; RootInputBuffer::GetPlayerActiveRaceCarIndex is
// committed) and its DWARF signature (private `u8 ... const`) is declared in the header.
// The BODY is left PARTIAL/deferred here for one reason only: a faithful body must call
// SoundLogicModule::GetBrnInputStructure(), which drags in the StateManager RTTI subtree
// (CgsEnvironment.h -> CgsStateManager.h), while ScrapeEffect's own base pulls the effect
// RTTI subtree (BrnEffectObject.h -> CgsEffectBase.h). Those two subtrees each define an
// INCOMPATIBLE CgsSound::Logic::ClassTypeInfo<T> (Variant A `typeName/baseTypeInfo` with
// ctor vs Variant B `mpcTypeName/mpfnCreateObject` aggregate), so co-including them in one
// TU is a pre-existing C2953 ODR fork. Unblocking cleanly needs a tree-wide ClassTypeInfo
// unification (out of this wave's scope), NOT an offset-hack/fabrication -- so per HARD
// RULE 6 the body stays deferred (declared-only) until that reconciliation lands.
// =============================================================================

namespace BrnSound
{
namespace Logic
{
namespace Collision
{

// ---------------------------------------------------------------------------
// ~ScrapeEffect  (the out-of-line anchor the scalar deleting destructor @ 0x826E8A28
// forwards to).
//
//   0x826E8A44  bl   ScrapeEffect::~ScrapeEffect()   ; real virtual dtor body
//   0x826E8A48  if (a2 & 1) { free via off_82FFB954 (slot +0x14) }
//   0x826E8A90  return this
//
// Same shape as the committed Brn3DEffectControl / Passby3DControl scalar deleting
// destructors: the compiler-emitted wrapper calls the real virtual destructor and then
// conditionally frees the object through the global sound allocator (off_82FFB954) when
// bit0 of the second arg is set. The real member teardown (mScrapeVoice + base settle)
// is produced by the inherited BrnEffectObject base chain + the embedded VoiceWrapper
// member dtor (BY NAME), so this anchor body is empty; the host toolchain re-synthesises
// the deleting-destructor thunk from this virtual destructor + operator delete. The raw
// allocator vtable call is NOT reproduced and no allocator is fabricated.
// ---------------------------------------------------------------------------
ScrapeEffect::~ScrapeEffect()
{
}

} // namespace Collision
} // namespace Logic
} // namespace BrnSound

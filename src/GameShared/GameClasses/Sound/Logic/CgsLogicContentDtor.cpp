// ============================================================================
// CgsLogicContentDtor.cpp -- CgsSound::Logic::Content out-of-line destructor.
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX @ 0x826DC378
//   (CgsSound::Logic::Content::`vector deleting destructor')
//     *this = &off_820B3250;                 // install Content's own vtable
//     if (mpContent)                          // a1[1] == mpContent @ +0x04
//         CgsSound::Playback::Object::Release(mpContent);
//     if (a2 & 1) operator delete(this);      // (vector-)deleting tail (host delete)
//     return this;
//
// The vtable install and the (flags & 1) allocator/host free are the compiler-
// synthesised parts of MSVC's vector-deleting-destructor thunk, re-emitted from this
// virtual destructor + the class's operator delete. The single observable source-
// level side effect is the held-reference drop `if (mpContent) mpContent->Release();`.
// Moved out-of-line here (was inline in CgsContent.h) so this TU emits the single
// CgsSound::Logic::Content::~Content symbol the deleting thunk is synthesised from.
//
// DISTINCT from CgsSound::Playback::Content: this is the logic-side 12-byte handle
// {vptr, mpContent, muTrailing}, not the refcounted spec. Mirrors the committed
// sibling destructor-home pattern (CgsEffectObjectDtor.cpp).
// ============================================================================

#include "GameShared/GameClasses/Sound/Logic/CgsContent.h"

namespace CgsSound
{
namespace Logic
{

Content::~Content()
{
    if (mpContent != 0)
        mpContent->Release();
}

} // namespace Logic
} // namespace CgsSound

// ============================================================================
// CgsContentDtor.cpp -- CgsSound::Playback::Content out-of-line destructor.
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX @ 0x82692A70
//   (CgsSound::Playback::Content::`vector deleting destructor')
//     bl  Content::~Content(this);        // base source dtor (out-of-line symbol)
//     if (a2 & 1) operator delete(this);  // (vector-)deleting tail (host delete)
//
// The compiler synthesises the deleting thunk from the out-of-line class dtor, so
// only the source-level ~Content() body is hand-written here. That body is the two
// load-count / ref-count asserts (moved out of the header so this TU emits the
// single Content::~Content symbol the deleting dtor and subclass dtors call).
// ============================================================================

#include "GameShared/GameClasses/Sound/Playback/CgsContent.h"

namespace CgsSound
{
namespace Playback
{
    Content::~Content()
    {
        // CgsContent.h. Content must be fully unloaded and unreferenced at teardown.
        CGS_ASSERT(mu16LoadCount == 1, "Destroying content while it's still loaded. This is a Bad thing.");

        CGS_ASSERT(mu32RefCount == 0, "0 == mu32RefCount");
    }
}
}

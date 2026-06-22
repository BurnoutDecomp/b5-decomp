// ============================================================================
// CgsAemsContentDtor.cpp -- CgsSound::Playback::AemsContent destructor.
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX @ 0x826DA150
//   (CgsSound::Playback::AemsContent::`vector deleting destructor')
//
// Same-shape as the two generic-RWAC content destructors (identical Content +
// ContentLoader<> layout): the compiler synthesis runs
//   *this = vtable;                                  // off_820B54DC
//   v4 = *(this+44); if (v4) *(v4+16) = *(this+48);  // unlink mLoader.mpResource
//   v5 = *(this+48); if (v5) *(v5+12) = *(this+44);  //   BaseResourcePtr alias ring
//   *(this+44) = this+32; *(this+48) = this+32;      // re-self-link the ring
//   CgsSound::Playback::Content::~Content(this);     // base dtor
//   if (a2 & 1) operator delete(this);               // (vector-)deleting tail
//
// this+32 (0x20) is mLoader.mpResource (a ResourcePtr<BinaryFileResource>); the
// alias links at +0x0C/+0x10 of its BaseResourcePtr == object +44/+48. The unlink +
// re-self-link IS CgsResource::BaseResourcePtr::~BaseResourcePtr() (@0x821F1E18),
// which the compiler runs when it destroys mLoader.mpResource. Defining the class
// destructor out-of-line emits that exact sequence; its body is empty because the
// member (mLoader -> ~ResourcePtr -> ~BaseResourcePtr unlink) and Content base
// destructors do all the work the asm performs. The trailing members
// (miAemsBankHandle/mbRemoveBegun/mpAemsData) are trivially destructible and add no
// asm. The X360 names it the *vector*-deleting destructor; for a single embedded
// loader with no array-of-subobjects member the emitted shape matches the scalar
// (member/base dtor + deleting tail) -- there is no per-element count loop in the
// asm because the destroyed members are not arrays.
// ============================================================================

#include "GameShared/GameClasses/Sound/Playback/AEMS/CgsAemsContent.h"

namespace CgsSound
{
namespace Playback
{
    AemsContent::~AemsContent()
    {
        // mLoader (-> ResourcePtr<BinaryFileResource> -> ~BaseResourcePtr alias-list
        // unlink) and the Content base dtor are run implicitly here.
    }
}
}

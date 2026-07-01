// ============================================================================
// CgsSplicerContentDtor.cpp -- CgsSound::Playback::SplicerContent destructor.
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX @ 0x826D5A60
//   (CgsSound::Playback::SplicerContent::`scalar deleting destructor')
//
// Same shape as the RWAC/AEMS content destructors (Content + ContentLoader<> layout).
// The compiler synthesis runs:
//   SplicerContent::~SplicerContent(this);  // base/member dtor
//   if (a2 & 1) operator delete(this);       // scalar-deleting tail
//
// The member teardown (mStatistics, then mLoader -> ~ContentLoader ->
// ~BaseResourcePtr alias-ring unlink, then the Content base dtor) is what the called
// ~SplicerContent does; defining the class destructor out-of-line emits that exact
// sequence, so the body here is empty. mpSplicerData and meType are trivially
// destructible and add no asm.
// ============================================================================

#include "GameShared/GameClasses/Sound/Playback/Splicer/CgsSplicerContent.h"

namespace CgsSound
{
namespace Playback
{
    SplicerContent::~SplicerContent()
    {
        // mStatistics, mLoader (-> ResourcePtr<BinaryFileResource> -> ~BaseResourcePtr
        // alias-list unlink) and the Content base dtor are run implicitly here.
    }
}
}

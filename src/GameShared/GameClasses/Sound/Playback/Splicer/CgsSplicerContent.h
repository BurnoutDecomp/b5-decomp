#ifndef CGS_SOUND_PLAYBACK_SPLICER_CGSSPLICERCONTENT_H
#define CGS_SOUND_PLAYBACK_SPLICER_CGSSPLICERCONTENT_H

#include "types.hpp"

#include "GameShared/GameClasses/Sound/Playback/CgsContent.h"                 // Content / Factory / ContentSpec
#include "GameShared/GameClasses/Sound/Playback/RWAC/CgsGenericRwacContent.h" // ContentLoader<T> + CgsResource::BinaryFileResource
#include "GameShared/GameClasses/Sound/Playback/Splicer/CgsSpliceBankStatistics.h" // SpliceBankStatistics

// ============================================================================
// CgsSound::Playback::SplicerContent  (DWARF home CgsSplicerContent.h:154).
//
//   SplicerContent::`scalar deleting destructor'  @ 0x826D5A60
//
// SplicerContent : public Content, carrying the splice bank statistics, a
// ContentLoader<BinaryFileResource> (the SAME Content+ContentLoader layout as the
// AEMS/RWAC contents), a raw splicer-data pointer, and a splice-type enum. The class
// destructor is empty: the member teardown (mStatistics, then mLoader ->
// ~ContentLoader -> ~BaseResourcePtr alias-ring unlink, then the Content base dtor)
// is what the compiler emits from the out-of-line dtor. mpSplicerData / meType are
// trivially destructible. DWARF-verified members (:158/:238/:239/:240; ~ @:173).
// ============================================================================

namespace CgsSound
{
namespace Playback
{

// CgsSplicerContent.h (DWARF). The splice family enum meType selects.
enum SPLICE_TYPE
{
    E_SPLICE_TYPE_INVALID = 0
};

struct SplicerContent : public Content
{
    SplicerContent(Factory& aFactory, const ContentSpec& aSpec, u32 au32DataSize); // own TU

    // @ 0x826D5A60. Empty out-of-line dtor (member + Content base dtors run implicitly).
    virtual ~SplicerContent();

    SpliceBankStatistics mStatistics;   // :158 public

private:
    ContentLoader<CgsResource::BinaryFileResource> mLoader; // :238
    void*      mpSplicerData;           // :239
    SPLICE_TYPE meType;                 // :240
};

} // namespace Playback
} // namespace CgsSound

#endif // CGS_SOUND_PLAYBACK_SPLICER_CGSSPLICERCONTENT_H

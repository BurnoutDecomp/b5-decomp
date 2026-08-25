#ifndef CGS_SOUND_PLAYBACK_SPLICER_CGSSPLICERCONTENT_H
#define CGS_SOUND_PLAYBACK_SPLICER_CGSSPLICERCONTENT_H

#include "types.hpp"

#include "GameShared/GameClasses/Sound/Playback/CgsContent.h"                 // Content / Factory / ContentSpec
#include "GameShared/GameClasses/Sound/Playback/RWAC/CgsGenericRwacContent.h" // ContentLoader<T> + CgsResource::BinaryFileResource
#include "GameShared/GameClasses/Sound/Playback/Splicer/CgsSpliceBankStatistics.h" // SpliceBankStatistics
#include "GameShared/GameClasses/Sound/Playback/CgsVoice.h"                   // Slot / Voice / PlayerVoice / ISlotImplementation / System (real homes)
#include "GameShared/GameClasses/Sound/Playback/Splicer/internal/SpliceObjects.h" // Splice (real home, :101)
#include "GameShared/GameClasses/Sound/Playback/Splicer/CgsSplicerPlayerVoice.h"  // SplicerPlayerVoice (the +0x88 mpSplice owner)

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

// SPLICE_TYPE (the splice family enum meType selects) is homed in SpliceManager.h and
// reaches this header through the CgsSpliceBankStatistics.h include above.

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

// =============================================================================
// CgsSound::Playback::SplicerContentSlot  (DWARF home CgsSplicerContent.h:248)
//
// The splicer bank's ISlotImplementation: it owns the per-voice Splice object and,
// on stop / pre-detach, tears it down (`delete voice->mpSplice; voice->mpSplice=0`).
// The two boot-relevant teardown overrides live in CgsSplicerContent.cpp:
//   SplicerContentSlot::DoStop      @ 0x826FA7E8
//   SplicerContentSlot::DoPreDetach @ 0x826FA840
//
// (2026-08-25, audio-faithfulness wave 6: the local rival Slot / Voice /
// PlayerVoice / ISlotImplementation / Splice minimal models are FOLDED onto their
// real homes -- CgsVoice.h + Splicer/internal/SpliceObjects.h, included above.
// The old local rival ALSO collided with CgsContent.h's fwd-decls of the same
// names in the same namespace: a live compile tripwire, now gone. The +0x88
// splice pointer belongs to SplicerPlayerVoice, so the teardown reaches it via a
// static_cast down the real PlayerVoice base -- the console DoStop/DoPreDetach
// read voice+0x88 directly, which IS that member on the console layout.)
// =============================================================================

struct SplicerContentSlot : public ISlotImplementation
{
    // own TU @ 0x826FA... (CgsSplicerContent.cpp:101 DWARF) -- declared only.
    virtual bool DoPlay(const Slot& aSlot, PlayerVoice& aVoice, Content& aContent, u32 au32Param);
    // this TU @ 0x826FA7E8.
    virtual bool DoStop(const Slot& aSlot, PlayerVoice& aVoice, Content& aContent);
    // own TU (CgsSplicerContent.cpp:66 DWARF) -- declared only.
    virtual bool DoUpdatePlaying(System* apSystem, const Slot& aSlot, PlayerVoice& aVoice,
                                 Content& aContent, f32 af32Dt);
    // this TU @ 0x826FA840.
    virtual void DoPreDetach(const Slot& aSlot, Voice& aVoice, Content& aContent);
};

} // namespace Playback
} // namespace CgsSound

#endif // CGS_SOUND_PLAYBACK_SPLICER_CGSSPLICERCONTENT_H

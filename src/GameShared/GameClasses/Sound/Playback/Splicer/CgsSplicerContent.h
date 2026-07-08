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

// =============================================================================
// CgsSound::Playback::SplicerContentSlot  (DWARF home CgsSplicerContent.h:248)
//
// The splicer bank's ISlotImplementation: it owns the per-voice Splice object and,
// on stop / pre-detach, tears it down (`delete voice->mpSplice; voice->mpSplice=0`).
// The two boot-relevant teardown overrides live in CgsSplicerContent.cpp:
//   SplicerContentSlot::DoStop      @ 0x826FA7E8
//   SplicerContentSlot::DoPreDetach @ 0x826FA840
//
// FLAG (collaborators are MINIMAL, un-homed elsewhere -- same treatment as the AEMS
// slot in CgsAemsContent.h): Slot / Voice / PlayerVoice / System / ISlotImplementation
// are large engine types with their own DWARF homes (CgsVoice.h etc.). Here only the
// polymorphic-base shape and the one collaborator field the teardown touches -- the
// Splice pointer at voice +0x88 (the same SplicerPlayerVoice::mpSplice modelled in
// CgsSplicerPlayerVoice.h) -- are modelled BY NAME; the exact voice layout slot is
// DEFERRED to the Voice keystone TU and is intentionally NOT offset-asserted.
// =============================================================================

// Forward-declared engine collaborator (full home CgsSystem.h).
struct System;

// The per-voice splice object. Full home elsewhere; only its destructor + class
// operator delete are exercised here (the teardown does `delete splice`).
struct Splice
{
    ~Splice();                              // own TU -- declared only
    static void operator delete(void* apMem); // own TU -- declared only
};

// CgsVoice.h:195 (DWARF). One slot on a voice. MINIMAL: the overrides receive it by
// const-ref but read nothing from it, so only the type identity is load-bearing.
class Slot {};

// CgsVoice.h (DWARF). The base voice. MINIMAL: only the Splice pointer the teardown
// nulls is modelled. FLAG: exact layout slot (+0x88) DEFERRED to the Voice keystone.
class Voice
{
public:
    Splice* mpSplice;   // X360 voice +0x88 (== SplicerPlayerVoice::mpSplice)
};

// The player voice the slot drives; derives from Voice so both the PlayerVoice&
// (DoStop) and Voice& (DoPreDetach) references reach mpSplice by name.
class PlayerVoice : public Voice {};

// CgsVoice.h:676 (DWARF). The slot-implementation interface. The splicer slot
// overrides all four hooks; only stop/pre-detach are bodied in this TU.
struct ISlotImplementation
{
    virtual ~ISlotImplementation() {}

    virtual bool DoPlay(const Slot& aSlot, PlayerVoice& aVoice, Content& aContent, u32 au32Param) = 0;
    virtual bool DoStop(const Slot& aSlot, PlayerVoice& aVoice, Content& aContent) = 0;
    virtual bool DoUpdatePlaying(System* apSystem, const Slot& aSlot, PlayerVoice& aVoice,
                                 Content& aContent, f32 af32Dt) = 0;
    virtual void DoPreDetach(const Slot& aSlot, Voice& aVoice, Content& aContent) = 0;
};

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

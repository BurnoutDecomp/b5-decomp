#ifndef CGS_SOUND_PLAYBACK_AEMS_CGSAEMSCONTENT_H
#define CGS_SOUND_PLAYBACK_AEMS_CGSAEMSCONTENT_H

#include "types.hpp"

#include "GameShared/GameClasses/Sound/Playback/CgsContent.h"             // CgsSound::Playback::Content / Factory / ContentSpec
#include "GameShared/GameClasses/Sound/Playback/RWAC/CgsGenericRwacContent.h" // ContentLoader<T> + CgsResource::BinaryFileResource

// =============================================================================
// CgsSound::Playback::AemsContentSlot
//   GameShared/GameClasses/Sound/Playback/aems/CgsAemsContent.h (DWARF home) +
//   GameShared/GameClasses/Sound/Playback/aems/CgsAemsContent.cpp
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX. AemsContentSlot is the AEMS bank's
// ISlotImplementation: it routes a Slot's play/stop/update through the bank's
// AemsPlayerVoice. This home models the slot's polymorphic surface (its three
// virtual overrides) and the un-homed collaborator types BY NAME so the three
// boot-trace functions in this TU body cleanly:
//   AemsContentSlot::DoPlay          @ 0x826DAFF0
//   AemsContentSlot::DoStop          @ 0x826DB008
//   AemsContentSlot::DoUpdatePlaying @ 0x826DAFE8
//
// FLAG (collaborators are MINIMAL, un-homed elsewhere): Slot, PlayerVoice,
// Content, System and ISlotImplementation are large engine types with their own
// DWARF homes (CgsVoice.h, CgsPlayerVoice.h, CgsContent.h, CgsSystem.h). Here only
// the polymorphic-base shape and the collaborator references the overrides touch
// are modelled; the full surfaces are reconstructed in their own TUs.
//
// FLAG (AemsPlayerVoice forwarders are CROSS-TU): the overrides downcast the
// PlayerVoice& to AemsPlayerVoice& and tail-call AemsPlayerVoice::Play(u32) /
// Stop() / Update(f32). Those member bodies live in their own (not-yet-done)
// AemsPlayerVoice TUs; here they are DECLARED (extern) and resolved at link/
// consolidation time -- NOT defined here.
//
// FLAG (the +0x80 playback flag, layout DEFERRED): DoPlay sets bit-1 of a byte at
// AemsPlayerVoice+0x80 (X360: `lbz r11,0x80(r5); ori r11,r11,2; stb`). Bit value 2
// is E_PLAYBACK_STATE_PLAYING (CgsVoice.h EPlaybackState). The exact named member
// at +0x80 sits inside the deep, un-homed Voice -> GenericRwacVoice ->
// AemsPlayerVoice layout. To keep member access BY NAME (no raw-offset cast), this
// home models that byte as a named playback-flags member on a MINIMAL
// AemsPlayerVoice and exposes a named mutator; its true layout slot is DEFERRED to
// the AemsPlayerVoice keystone TU and is intentionally NOT offset-asserted.
// =============================================================================

namespace CgsSound
{
namespace Playback
{

// Forward-declared engine collaborators (full homes elsewhere).
struct System;
struct Content;

// CgsVoice.h:43 (DWARF). Playback state bits/values; bit-1 == PLAYING is the bit
// DoPlay raises on the player voice's playback-flags byte.
enum EPlaybackStateBits
{
    E_PLAYBACK_STATE_PLAYING_BIT = 2,
};

// CgsVoice.h:195 (DWARF). One slot on a voice. MINIMAL: the overrides receive it
// by const-ref but read nothing from it (the X360 ignores the Slot arg), so only
// the type identity is load-bearing here.
struct Slot
{
};

// CgsPlayerVoice.h:42 / CgsAemsPlayerVoice.h:53 (DWARF). The AEMS player voice the
// slot drives. MINIMAL home -- see file banner FLAGs. Only the playback-flags byte
// DoPlay raises and the three forwarded member functions are modelled.
struct AemsPlayerVoice
{
    // The playback-flags byte at AemsPlayerVoice+0x80 (X360). DoPlay raises the
    // PLAYING bit here before starting playback. FLAG: exact layout slot DEFERRED.
    u8 mu8PlaybackFlags;

    // Raises the PLAYING bit on the playback-flags byte (X360: `|= 2`).
    void SetPlaying() { mu8PlaybackFlags = static_cast<u8>(mu8PlaybackFlags | E_PLAYBACK_STATE_PLAYING_BIT); }

    // Cross-TU member bodies (defined in the AemsPlayerVoice TUs; declared here).
    bool Play(u32 au32Param); // that TU
    bool Stop();              // that TU
    bool Update(f32 af32Dt);  // that TU
};

// The Slot dispatches play/stop/update through a PlayerVoice& base; for the AEMS
// slot that reference always denotes an AemsPlayerVoice. Modelled as an alias of
// the minimal AemsPlayerVoice for this TU's purposes (the real PlayerVoice is the
// AemsPlayerVoice's base; the overrides only ever see AemsPlayerVoice instances).
typedef AemsPlayerVoice PlayerVoice;

// CgsVoice.h:676 (DWARF). The slot-implementation interface. Pure-virtual play/
// stop/update hooks the concrete slot implementations override.
struct ISlotImplementation
{
    virtual ~ISlotImplementation() {}

    virtual bool DoPlay(const Slot& aSlot, PlayerVoice& aVoice, Content& aContent, u32 au32Param) = 0;
    virtual bool DoStop(const Slot& aSlot, PlayerVoice& aVoice, Content& aContent) = 0;
    virtual bool DoUpdatePlaying(System* apSystem, const Slot& aSlot, PlayerVoice& aVoice,
                                 Content& aContent, f32 af32Dt) = 0;
};

// =============================================================================
// CgsSound::Playback::AemsContent  (CgsAemsContent.h:115 DWARF home)
//
// The AEMS bank's concrete Content. `: public Content` (CgsContent.h) carrying a
// single ContentLoader<CgsResource::BinaryFileResource> mLoader at object +0x20
// (DWARF CgsAemsContent.h:189) -- the SAME Content+ContentLoader layout as the two
// generic-RWAC contents in CgsGenericRwacContent.h. The remaining members
// (miAemsBankHandle, mbRemoveBegun, mpAemsData -- CgsAemsContent.h:190..192) are
// trivially destructible and do not appear in the dtor asm.
//
// The TU here is the compiler-synthesized vector-deleting destructor @ 0x826DA150:
//   *this = vtable;                                  // off_820B54DC
//   v4 = *(this+44); if (v4) *(v4+16) = *(this+48);  // unlink mLoader.mpResource
//   v5 = *(this+48); if (v5) *(v5+12) = *(this+44);  //   BaseResourcePtr alias ring
//   *(this+44) = this+32; *(this+48) = this+32;      // re-self-link the ring
//   CgsSound::Playback::Content::~Content(this);     // base dtor
//   if (a2 & 1) operator delete(this);               // (vector-)deleting tail
// this+32(0x20) == mLoader.mpResource; the alias links at +0x0C/+0x10 of its
// BaseResourcePtr == object +44/+48. The unlink + re-self-link IS
// CgsResource::BaseResourcePtr::~BaseResourcePtr() (@0x821F1E18), run when the
// compiler destroys mLoader.mpResource. Defining the class destructor out-of-line
// (CgsAemsContentDtor.cpp) emits that exact sequence; the body is empty because the
// member/base destructors do all the work.
//
// FLAG (method family is CROSS-TU): AemsContent's ctor, DoLoad/DoUnload/DoUpdate/
// DoGetData/DoOnPostLoad/DoOnPreUnload and AddAemsBankCallback (CgsAemsContent.h:
// 123..183) live in their own (not-yet-done) AemsContent TUs; they are DECLARED here
// (where they affect the vtable shape) and resolved at consolidation -- NOT defined.
// =============================================================================
struct AemsContent : public Content
{
    AemsContent(Factory& aFactory, const ContentSpec& aSpec, u32 au32DataSize); // :123 (own TU)
    virtual ~AemsContent();                                                     // :129

protected:
    virtual bool  DoLoad();            // :133 (own TU)
    virtual bool  DoUnload();          // :134 (own TU)
    virtual void  DoUpdate(f32 af32Dt); // :135 (own TU)
    virtual void* DoGetData();         // :136 (own TU)
    virtual bool  DoOnPostLoad();      // :141 (own TU)
    virtual bool  DoOnPreUnload();     // :155 (own TU)

    void* AddAemsBankCallback(void* apData, int aiArg1, int aiArg2); // :183 (own TU)

private:
    ContentLoader<CgsResource::BinaryFileResource> mLoader; // :189  (object +0x20)
    int   miAemsBankHandle;                                 // :190
    bool  mbRemoveBegun;                                    // :191
    void* mpAemsData;                                       // :192
};

// CgsAemsContent.h:200 (DWARF): AemsContentSlot : public ISlotImplementation.
struct AemsContentSlot : public ISlotImplementation
{
    // that TU @ 0x826DAFF0.
    virtual bool DoPlay(const Slot& aSlot, PlayerVoice& aVoice, Content& aContent, u32 au32Param);
    // that TU @ 0x826DB008.
    virtual bool DoStop(const Slot& aSlot, PlayerVoice& aVoice, Content& aContent);
    // that TU @ 0x826DAFE8.
    virtual bool DoUpdatePlaying(System* apSystem, const Slot& aSlot, PlayerVoice& aVoice,
                                 Content& aContent, f32 af32Dt);
};

} // namespace Playback
} // namespace CgsSound

#endif // CGS_SOUND_PLAYBACK_AEMS_CGSAEMSCONTENT_H

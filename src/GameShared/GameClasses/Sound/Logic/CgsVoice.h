#ifndef CGS_SOUND_LOGIC_CGSVOICE_H
#define CGS_SOUND_LOGIC_CGSVOICE_H

#include "types.hpp"

#include "GameShared/GameClasses/Sound/Playback/CgsHandle.h"   // CgsSound::Playback::Handle<T>
#include "GameShared/GameClasses/Sound/Playback/CgsCommon.h"   // CgsSound::Playback::Name (MakeHash / hash idents)

// =============================================================================
// CgsSound::Logic::Voice -- the sound-LOGIC voice object.
//   GameShared/GameClasses/Sound/Logic/CgsVoice.h  (DWARF home) +
//   GameShared/GameClasses/Sound/Logic/CgsVoice.cpp
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX. This is the THIN LOGIC-LAYER wrapper
// around a reference-counted CgsSound::Playback::Voice: it owns a Handle to that
// playback voice and forwards Connect/Attach/Play/Stop/Detach/gain/state queries
// into the Playback layer. It is the leaf that fills SoundLogicModule::Prepare
// stage 2 (which Constructs + Connects three of these: Submix / Master / GlobalReverb).
//
// X360 SOURCE PATHS (from the FireAssert 2nd args in the Voice bodies):
//   ..\..\..\GameShared\GameClasses\Sound/Logic/CgsVoice.cpp   (Connect/Detach/Attach/
//                                                               Play/Stop/Destruct bodies)
//   ..\..\..\GameShared\GameClasses\Sound/Logic/CgsVoice.h     (GetGain/GetIdent/SetGain
//                                                               inline asserts)
//   ..\..\..\GameShared\GameClasses\Sound/Playback/CgsHandle.h (the "mpObject" guard)
//
// -----------------------------------------------------------------------------
// X360 LAYOUT (proven by this TU's functions, 32-bit guest):
//   sizeof(Voice) == 12 bytes (3 words). PROOF: SoundLogicModule::Prepare
//   (0x82703C18) places the three voices at _DWORD* offsets a1+5244 / a1+5247 /
//   a1+5250 -- 3 words == 12 bytes apart -- so a Voice is exactly 3 words.
//
//     +0x00  vtable                       (this is a virtual class -- the scalar
//                                          deleting dtor @0x826C7E18 installs
//                                          off_820B0E20 into *this)
//     +0x04  mVoiceHandle                 Handle<Playback::Voice> (1 ptr; its
//                                          mpObject is read as *(a1+4) by every
//                                          method, asserted "Voice not yet created!")
//     +0x08  mpOwnerModule                the owning sound module (read as *(a1+8)
//                                          by Connect: ConnectVoice(*(a1+8)+568,...);
//                                          set by Construct from its parent arg)
//
// LAYOUT NOTE (X360 32-bit vs host 64-bit): mVoiceHandle holds one pointer and
// mpOwnerModule is a pointer, so on the 64-bit host the absolute byte offsets DIFFER
// (8-byte ptrs). Per project rule we pin members BY NAME + SEQUENCE and record the
// X360 offsets in comments only -- NOT static_asserted on the 64-bit host.
//
// PLAYBACK::VOICE field map the logic methods read THROUGH mVoiceHandle.mpObject
// (these belong to the Playback layer, NOT to this object):
//     pbVoice+0x04  ref count        (Acquire == ++*(obj+4))
//     pbVoice+0x0C  ident            (GetIdent returns *(obj+12))
//     pbVoice+0x10  playback-state   (low 7 bits; ==2 -> PLAYING)
//     pbVoice+0x11  a state byte     (Destruct writes 2; Play/Stop test ==1 before
//                                     driving the slot)
// =============================================================================

namespace CgsSound
{
namespace Playback
{
// Forward-declared: the reference-counted PLAYBACK voice this logic Voice wraps via
// mVoiceHandle. Its full definition lives in the (not-yet-reconstructed) Playback
// layer (Sound/Playback/CgsVoice.h). We only ever hold a Handle to it here.
class Voice;
}

namespace Logic
{

// CgsVoice.h. The sound-logic voice. Virtual (the X360 scalar-deleting dtor installs
// a vtable at +0), so it carries an implicit vptr at +0.
class Voice
{
public:
    // The interned name-hash type the logic layer passes around (Connect's send name,
    // Construct's spec name). Playback::Name::MakeHash returns this widened value.
    typedef uintptr_t Ident;

    // -------------------------------------------------------------------------
    // Construct(parentModule, idx, globalSpecTable, nameHash)
    //   FLAG: RECONSTRUCTED FROM CALL SITES -- the X360 INLINES Construct (no
    //   standalone dump). Recovered from the three call sites in
    //   BrnSound::Module::SoundLogicModule::Prepare @0x82703C18:
    //     Construct(a1+5250, a1, -16, dword_83008650, MakeHash("SubmixVoiceSpec"))
    //     Construct(a1+5244, a1,   1, dword_83008650, MakeHash("MasterVoiceSpec"))
    //     Construct(a1+5247, a1,   2, dword_83008650, MakeHash("GlobalReverbVoiceSpec"))
    //   where a1 is the owning module, dword_83008650 is the shared voice-spec
    //   table/factory global, idx is the submix index (note -16 == 0xFFFFFFF0 ==
    //   the reserved K_INIT_SND9_SUBMIX_IDENT), and nameHash is the spec's name hash.
    //   The body sets mpOwnerModule and creates the Playback::Voice into mVoiceHandle
    //   -- see the .cpp for the proven-vs-FLAGGED split.
    void Construct(void* lpOwnerModule, s32 liIndex, void* lpGlobalSpecTable, Ident luNameHash);

    // Destruct  @ 0x826C4E38. Assert the wrapped voice exists and is NOT playing,
    // mark its state byte (+0x11 = 2), then Release the handle and null mpObject.
    void Destruct();

    // Connect(sendNameHash, send)  @ 0x826C4F18. Acquire the handle and forward to
    // Playback::Module::Module::ConnectVoice(ownerModule+568, &voice, ...).
    s32 Connect(Ident luSendNameHash, s32 liSend);

    // GetIdent  @ 0x826AD988. Return the wrapped Playback::Voice's ident (obj+0x0C).
    s32 GetIdent() const;

    // GetGain(send)  @ 0x826AD808. Find the named send on the playback voice and
    // return its gain (send+0x04); flt_82F93D88 (1.0f default) when not found.
    f32 GetGain(const s32* lpSendName) const;

    // SetGain(sendNameHash, gain, ..., sendNameCheck)  @ 0x826942C0. Look up the send
    // by hash, assert its name matches, store the new gain and a "changed" flag.
    void SetGain(u32 luSendNameHash, f32 lfGain, s32 liReserved, const u32* lpSendName);

    // IsPlaying  @ 0x826943F8. true iff (playbackState & 0x7F) == 2.
    bool IsPlaying() const;

    // IsReady  @ 0x82694378. true iff (playbackState & 0x7F) != 0.
    bool IsReady() const;

    // ---- Playback-layer-dependent legs (see .cpp for the FLAGGED stubs) ----------
    // Attach  @ 0x826DC4C8 -> Playback::Module::Module::AttachVoice
    s32 Attach(s32 liSlotName, s32* lppOther);
    // Detach  @ 0x826C4F98 -> Playback::Voice::FindNamedSlot + Playback::Slot::Detach
    void* Detach(s32 liSlotName);
    // Play    @ 0x826C5068 -> Playback::Voice::GetSlot + Playback::Slot::Play
    s32 Play(s32 liParam);
    // Stop    @ 0x826C5148 -> Playback::Voice::GetSlot + Playback::Slot::Stop
    s32 Stop();

private:
    // +0x04. Owning handle to the reference-counted playback voice. Read as *(a1+4)
    // by every method; "Voice not yet created!" asserts it is non-null.
    Playback::Handle<Playback::Voice> mVoiceHandle;

    // +0x08. The sound module that owns this voice. Connect reads it as *(a1+8) and
    // routes ConnectVoice through (*(a1+8) + 568). Stored opaquely (void*) -- the
    // owning-module type (BrnSound::Module::SoundLogicModule) is another group's
    // surface and only its +568 Playback::Module sub-object is touched here.
    void* mpOwnerModule;
};

} // namespace Logic
} // namespace CgsSound

#endif // CGS_SOUND_LOGIC_CGSVOICE_H

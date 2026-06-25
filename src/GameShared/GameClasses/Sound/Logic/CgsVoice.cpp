// ============================================================================
// CgsVoice.cpp -- CgsSound::Logic::Voice runtime bodies.
//
// Bodied from BURNOUT_X360_ARTIST.XEX. The sound-LOGIC voice: a thin wrapper that
// owns a Handle to a reference-counted CgsSound::Playback::Voice and forwards the
// connect / attach / play / stop / gain / state API into the Playback layer.
//
// Decompiled (self-contained, real bodies):
//   Voice::Construct (RECONSTRUCTED from call sites -- X360 inlines it)
//   Voice::Destruct   @ 0x826C4E38
//   Voice::Connect    @ 0x826C4F18   (its only external call, ConnectVoice, is FLAGGED)
//   Voice::GetIdent   @ 0x826AD988
//   Voice::IsPlaying  @ 0x826943F8
//   Voice::IsReady    @ 0x82694378
//
// Playback-layer dependent (FLAGGED stubs -- the bodies call into the NOT-yet-
// reconstructed CgsSound::Playback layer; each stub names its X360 address + the
// Playback symbol it blocks on):
//   Voice::Attach  @ 0x826DC4C8   Voice::Detach @ 0x826C4F98
//   Voice::Play    @ 0x826C5068   Voice::Stop   @ 0x826C5148
//   Voice::GetGain @ 0x826AD808   Voice::SetGain @ 0x826942C0
//
// LAYOUT / OFFSET NOTE: the methods read THROUGH mVoiceHandle.GetObject() into the
// Playback::Voice at FIXED BYTE OFFSETS (ref @+4, ident @+0xC, playback-state @+0x10,
// state byte @+0x11). Playback::Voice is incomplete here (its real layout lives in
// the unreconstructed Sound/Playback/CgsVoice.h), so those reads are done as raw
// byte arithmetic exactly as the X360 does (`*(obj+12)` etc.) and are FLAGGED. They
// are PLAYBACK-LAYER offsets; when Playback::Voice is reconstructed these should be
// replaced by named field access on a complete type.
// ============================================================================

#include "GameShared/GameClasses/Sound/Logic/CgsVoice.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Sound/Playback/CgsCommon.h"  // Playback::Name::MakeHash (reused)

namespace CgsSound
{
namespace Logic
{

// ----------------------------------------------------------------------------
// Raw helpers for reaching into the (incomplete) Playback::Voice at the byte
// offsets the X360 bodies use. FLAG: PLAYBACK-LAYER LAYOUT REACH -- these encode
// Playback::Voice field offsets that belong to the unreconstructed Playback layer.
// Replace with named field access once Sound/Playback/CgsVoice.h is reconstructed.
// ----------------------------------------------------------------------------
namespace
{
    // Playback::Voice+0x10: the playback-state word. Low 7 bits are the state enum
    // (E_PLAYBACK_STATE_PLAYING == 2). Read by IsPlaying / IsReady / Destruct.
    inline u8 GetPlaybackStateByte(const Playback::Voice* lpVoice)
    {
        return *(reinterpret_cast<const u8*>(lpVoice) + 0x10);
    }
}

// ----------------------------------------------------------------------------
// Voice::Construct(lpOwnerModule, liIndex, lpGlobalSpecTable, luNameHash)
//   FLAG: RECONSTRUCTED FROM CALL SITES -- the X360 INLINES Construct (no standalone
//   dump). Call sites (BrnSound::Module::SoundLogicModule::Prepare @0x82703C18):
//     Construct(a1+5250, a1, -16, dword_83008650, MakeHash("SubmixVoiceSpec"))
//     Construct(a1+5244, a1,   1, dword_83008650, MakeHash("MasterVoiceSpec"))
//     Construct(a1+5247, a1,   2, dword_83008650, MakeHash("GlobalReverbVoiceSpec"))
//
//   What is PROVEN by the call sites + the sibling method offsets:
//     * arg1 (a1, the owning module) is stored as mpOwnerModule (+0x08): Connect
//       reads it as *(a1+8) and routes ConnectVoice through (*(a1+8)+568).
//     * the object starts with mVoiceHandle == null on entry to creation; every
//       method asserts "Voice not yet created!" on a null handle, and Destruct/the
//       scalar-deleting dtor leave it null -- so Construct's job is to FILL it.
//
//   What is FLAGGED (the actual playback-voice creation is a Playback-layer call):
//     * liIndex (the submix index; -16 == 0xFFFFFFF0 == K_INIT_SND9_SUBMIX_IDENT),
//       lpGlobalSpecTable (dword_83008650, the shared voice-spec factory/table) and
//       luNameHash (the spec name hash) feed a Playback factory call that produces
//       the Playback::Voice and seeds mVoiceHandle. That factory entry point is NOT
//       reconstructed, so we cannot create a real Playback::Voice here.
// ----------------------------------------------------------------------------
void Voice::Construct(void* lpOwnerModule, s32 liIndex, void* lpGlobalSpecTable, Ident luNameHash)
{
    // PROVEN: stash the owning module (read by Connect at +0x08).
    mpOwnerModule = lpOwnerModule;

    // FLAG: STUB (reconstructed-from-callsite + Playback dep). The real X360 creates
    // the Playback::Voice from (lpGlobalSpecTable, liIndex, luNameHash) via the
    // Playback voice factory and stores the resulting handle into mVoiceHandle.
    //   BLOCKS ON: CgsSound::Playback voice-creation factory fed by dword_83008650
    //              (the spec table @ guest 0x83008650) -- not reconstructed.
    // Until that lands, leave mVoiceHandle null (mpObject == 0). NOTE: every other
    // method then trips its "Voice not yet created!" assert -- that is the honest
    // state for an unwired leaf; the parent wires the real factory.
    (void)liIndex;
    (void)lpGlobalSpecTable;
    (void)luNameHash;
    // mVoiceHandle is left as default-constructed/null by the caller's placement;
    // we deliberately do NOT fabricate a fake object.
}

// ----------------------------------------------------------------------------
// Voice::Destruct  @ 0x826C4E38
//   Assert the voice exists (CgsVoice.cpp:83) and the dereference guard
//   (CgsHandle.h:287); assert it is NOT playing (CgsVoice.h:1548), write the state
//   byte at +0x11 = 2, then Release the playback object and null the handle.
//     asm: v2=*(a1+4); if((*(v2+16)&0x7F)==2) <assert>; *(v2+17)=2;
//          if(*(a1+4)) Playback::Object::Release(); *(a1+4)=0;
// ----------------------------------------------------------------------------
void Voice::Destruct()
{
    CGS_ASSERT(mVoiceHandle.GetObject(), "Voice not yet created!");
    CGS_ASSERT(mVoiceHandle.GetObject(), "mpObject");

    Playback::Voice* lpVoice = mVoiceHandle.GetObject();

    // E_PLAYBACK_STATE_PLAYING (==2) must not be the current state.
    CGS_ASSERT((GetPlaybackStateByte(lpVoice) & 0x7F) != 2,
               "E_PLAYBACK_STATE_PLAYING != GetPlaybackState()");

    // FLAG: PLAYBACK-LAYER LAYOUT REACH -- write the Playback::Voice state byte at
    // +0x11 (the X360 `*(v2+17) = 2`). Belongs to the unreconstructed Playback::Voice.
    *(reinterpret_cast<u8*>(lpVoice) + 0x11) = 2;

    // FLAG: STUB -- the X360 calls CgsSound::Playback::Object::Release() to drop the
    // ref count and (at zero) dispose the playback voice.
    //   BLOCKS ON: CgsSound::Playback::Object::Release (the playback ref-count drop;
    //              the Playback::Voice/Object teardown path is not reconstructed).
    // We model the intent (null the handle) without driving the playback teardown.
    // Playback::Handle's dtor would do the Release once that layer exists; here we
    // only clear the owned pointer to match `*(a1+4) = 0`.
    // (mVoiceHandle clear: no public setter on Handle -- the real Release is the
    //  Playback dep above. Left as-is; the handle is freed by its dtor.)
}

// ----------------------------------------------------------------------------
// Voice::Connect(luSendNameHash, liSend)  @ 0x826C4F18
//   Assert the voice exists (CgsVoice.cpp:104), take a temp ref on the playback
//   voice (++*(obj+4)), then forward to ConnectVoice on the owner module's playback
//   sub-module (*(owner+8) ... + 568).
//     asm: if(!*(a1+4)) <assert>; v7=*(a1+4); if(v7) ++*(v7+4);
//          result = Playback::Module::Module::ConnectVoice(*(a1+8)+568,&v7,a2,a3);
// ----------------------------------------------------------------------------
s32 Voice::Connect(Ident luSendNameHash, s32 liSend)
{
    CGS_ASSERT(mVoiceHandle.GetObject(), "Voice not yet created!");

    Playback::Voice* lpVoice = mVoiceHandle.GetObject();

    // Take a temporary reference for the duration of the connect (X360 ++*(v7+4)).
    // FLAG: PLAYBACK-LAYER LAYOUT REACH -- the Playback::Voice/Object ref count lives
    // at +0x04. Bumped here exactly as the X360 does (`++*(v7+4)`).
    if (lpVoice)
        ++(*(reinterpret_cast<u32*>(reinterpret_cast<u8*>(lpVoice) + 0x04)));

    // FLAG: STUB -- the actual connect is a Playback-layer call.
    //   BLOCKS ON: CgsSound::Playback::Module::Module::ConnectVoice
    //              (routed through *(mpOwnerModule + 568), the owner's playback
    //              sub-module). Playback::Module::Module is not reconstructed.
    // Returns 0 (failure/no-op) until the Playback module exists.
    (void)luSendNameHash;
    (void)liSend;
    return 0;
}

// ----------------------------------------------------------------------------
// Voice::GetIdent  @ 0x826AD988
//   Assert "Content not yet created!" (CgsVoice.h:629) + the dereference guard
//   (CgsHandle.h:296), then return the playback voice's ident at +0x0C.
//     asm: if(!*(a1+4)) <assert>; return *(*(a1+4)+12);
// ----------------------------------------------------------------------------
s32 Voice::GetIdent() const
{
    CGS_ASSERT(mVoiceHandle.GetObject(), "Content not yet created!");
    CGS_ASSERT(mVoiceHandle.GetObject(), "mpObject");

    // FLAG: PLAYBACK-LAYER LAYOUT REACH -- ident at Playback::Voice+0x0C (X360
    // `*(obj+12)`). Belongs to the unreconstructed Playback::Voice.
    const Playback::Voice* lpVoice = mVoiceHandle.GetObject();
    return *(reinterpret_cast<const s32*>(reinterpret_cast<const u8*>(lpVoice) + 0x0C));
}

// ----------------------------------------------------------------------------
// Voice::IsPlaying  @ 0x826943F8
//   false if no playback voice; else (playbackState & 0x7F) == 2.
//     asm: v3=*(a1+4); if(!v3) return 0; return (*(v3+16)&0x7F)==2;
// ----------------------------------------------------------------------------
bool Voice::IsPlaying() const
{
    const Playback::Voice* lpVoice = mVoiceHandle.GetObject();
    if (!lpVoice)
        return false;

    return (GetPlaybackStateByte(lpVoice) & 0x7F) == 2;
}

// ----------------------------------------------------------------------------
// Voice::IsReady  @ 0x82694378
//   false if no playback voice; else (playbackState & 0x7F) != 0.
//   The X360 computes `(_cntlzw(x & 0x7F) & 0x20) == 0`: count-leading-zeros of a
//   value in [0,0x7F] is 32 iff the value is 0, so bit 0x20 of the cntlzw result is
//   set ONLY when x==0. `(... & 0x20) == 0` is therefore exactly "x != 0".
//     asm: v3=*(a1+4); if(!v3) return 0; return (cntlzw(*(v3+16)&0x7F)&0x20)==0;
// ----------------------------------------------------------------------------
bool Voice::IsReady() const
{
    const Playback::Voice* lpVoice = mVoiceHandle.GetObject();
    if (!lpVoice)
        return false;

    return (GetPlaybackStateByte(lpVoice) & 0x7F) != 0;
}

// ----------------------------------------------------------------------------
// Voice::GetGain(lpSendName)  @ 0x826AD808
//   Assert the voice exists (CgsVoice.h:593) + dereference guard, find the named
//   send on the playback voice, return its gain (send+0x04) or the 1.0 default.
//     asm: v5=*(a1+4); v8=*a2; NamedSend=Playback::Voice::FindNamedSend(v5,&v8);
//          return NamedSend ? *(NamedSend+4) : flt_82F93D88;
// FLAG: STUB -- depends on the Playback layer.
//   BLOCKS ON: CgsSound::Playback::Voice::FindNamedSend (not reconstructed).
//   flt_82F93D88 is the unity-gain default (1.0f) returned when no send is found;
//   we return that as the safe default for the whole stub.
// ----------------------------------------------------------------------------
f32 Voice::GetGain(const s32* lpSendName) const
{
    CGS_ASSERT(mVoiceHandle.GetObject(), "Voice not yet created!");
    CGS_ASSERT(mVoiceHandle.GetObject(), "mpObject");
    (void)lpSendName;
    // flt_82F93D88 default gain (1.0f). FLAG: Playback::Voice::FindNamedSend missing.
    return 1.0f;
}

// ----------------------------------------------------------------------------
// Voice::SetGain(luSendNameHash, lfGain, liReserved, lpSendName)  @ 0x826942C0
//   Dereference guard, GetSend(obj, hash), assert the send's name matches, store the
//   gain at send+0x04 and a "changed" flag at send+0x08.
//     asm: send=Playback::Voice::GetSend(*(a1+4),a2); if(*a5!=*send)<assert>;
//          changed = (send[1]!=a3); send[1]=a3; *(send+8)=changed;
// FLAG: STUB -- depends on the Playback layer.
//   BLOCKS ON: CgsSound::Playback::Voice::GetSend (returns a Send descriptor whose
//   name(+0x00)/gain(+0x04)/changed-flag(+0x08) this method writes). Not reconstructed.
// ----------------------------------------------------------------------------
void Voice::SetGain(u32 luSendNameHash, f32 lfGain, s32 liReserved, const u32* lpSendName)
{
    CGS_ASSERT(mVoiceHandle.GetObject(), "mpObject");
    (void)luSendNameHash;
    (void)lfGain;
    (void)liReserved;
    (void)lpSendName;
    // No-op until Playback::Voice::GetSend exists.
}

// ----------------------------------------------------------------------------
// Voice::Attach(liSlotName, lppOther)  @ 0x826DC4C8
//   Assert the voice exists (CgsVoice.cpp:162), ref both this voice and *lppOther,
//   call Playback::Module::Module::AttachVoice(&this, &other, slotName), then drop
//   the *lppOther reference.
// FLAG: STUB -- depends on the Playback layer.
//   BLOCKS ON: CgsSound::Playback::Module::Module::AttachVoice +
//              CgsSound::Playback::Object::Release. Not reconstructed.
// ----------------------------------------------------------------------------
s32 Voice::Attach(s32 liSlotName, s32* lppOther)
{
    CGS_ASSERT(mVoiceHandle.GetObject(), "Voice not yet created!");
    (void)liSlotName;
    (void)lppOther;
    return 0;
}

// ----------------------------------------------------------------------------
// Voice::Detach(liSlotName)  @ 0x826C4F98
//   Assert the voice exists (CgsVoice.cpp:181) + dereference guard, then
//   FindNamedSlot(obj, &slotName) and Slot::Detach(slot, obj); assert the detach
//   happened ("mVoiceHandle->Detach( lSlotName )", CgsVoice.cpp:182).
// FLAG: STUB -- depends on the Playback layer.
//   BLOCKS ON: CgsSound::Playback::Voice::FindNamedSlot +
//              CgsSound::Playback::Slot::Detach. Not reconstructed.
// ----------------------------------------------------------------------------
void* Voice::Detach(s32 liSlotName)
{
    CGS_ASSERT(mVoiceHandle.GetObject(), "Voice not yet created!");
    CGS_ASSERT(mVoiceHandle.GetObject(), "mpObject");
    (void)liSlotName;
    return 0;
}

// ----------------------------------------------------------------------------
// Voice::Play(liParam)  @ 0x826C5068
//   Assert exists (CgsVoice.cpp:200) + dereference guard, ref the voice; if the
//   state byte (+0x11) == 1, GetSlot(obj,0) then Slot::Play(slot,obj,param) and on
//   success SetPlaybackState(obj, 2); finally Release.
// FLAG: STUB -- depends on the Playback layer.
//   BLOCKS ON: CgsSound::Playback::Voice::GetSlot / SetPlaybackState +
//              CgsSound::Playback::Slot::Play + Playback::Object::Release.
// ----------------------------------------------------------------------------
s32 Voice::Play(s32 liParam)
{
    CGS_ASSERT(mVoiceHandle.GetObject(), "Voice not yet created!");
    CGS_ASSERT(mVoiceHandle.GetObject(), "mpObject");
    (void)liParam;
    return 0;
}

// ----------------------------------------------------------------------------
// Voice::Stop  @ 0x826C5148
//   Assert exists (CgsVoice.cpp:220) + dereference guard, ref the voice; if the
//   state byte (+0x11) == 1, GetSlot(obj,0) then Slot::Stop(slot,obj) and on success
//   SetPlaybackState(obj, 1); finally Release.
// FLAG: STUB -- depends on the Playback layer.
//   BLOCKS ON: CgsSound::Playback::Voice::GetSlot / SetPlaybackState +
//              CgsSound::Playback::Slot::Stop + Playback::Object::Release.
// ----------------------------------------------------------------------------
s32 Voice::Stop()
{
    CGS_ASSERT(mVoiceHandle.GetObject(), "Voice not yet created!");
    CGS_ASSERT(mVoiceHandle.GetObject(), "mpObject");
    return 0;
}

} // namespace Logic
} // namespace CgsSound

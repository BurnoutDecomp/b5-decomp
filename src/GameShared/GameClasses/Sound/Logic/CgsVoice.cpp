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
// LAYOUT NOTE (updated 2026-08-25, audio-faithfulness wave 1): the methods read
// THROUGH mVoiceHandle.GetObject() into the Playback::Voice. The X360 inlines the
// member access (`*(obj+4)` refcount, `*(obj+12)` ident, `*(obj+0x10)` playback
// state, `*(obj+0x11)` remove state); the real Playback::Voice layout is NOW
// reconstructed (Sound/Playback/CgsVoice.h: Object::mu32RefCount @+0x04, mIdent
// @+0x0C, mu8PlaybackState @+0x10, mu8RemoveState @+0x11), so those reaches are
// done BY NAME here (Acquire / GetIdent / GetPlaybackState / SetRemoveState) --
// the former raw-byte-offset helpers are retired.
// ============================================================================

#include "GameShared/GameClasses/Sound/Logic/CgsVoice.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Sound/Playback/CgsCommon.h"  // Playback::Name::MakeHash (reused)
#include "GameShared/GameClasses/Sound/Playback/CgsVoice.h"   // the REAL Playback::Voice layout
#include "GameShared/GameClasses/Sound/Logic/CgsSoundLogicModule.h"

namespace CgsSound
{
namespace Logic
{

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
void Voice::Construct(Module* lpOwnerModule, Ident luIdent,
                      Ident luFactoryName, Ident luVoiceSpecName)
{
    CGS_ASSERT(lpOwnerModule != 0, "lpOwnerModule");
    mpOwnerModule = lpOwnerModule;

    // DecFIGS pins the three scalar arguments as Command::QueueElement values. The
    // ARTIST call site supplies {ident, GenericRwacFactory::SK_NAME, VoiceSpec name};
    // Playback::Module::CreateVoice is the exact engine endpoint at owner+0x238.
    lpOwnerModule->GetPlaybackModule().CreateVoice(
        &mVoiceHandle,
        luIdent,
        Playback::Name(static_cast<uintptr_t>(luFactoryName)),
        luVoiceSpecName);
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

    // E_PLAYBACK_STATE_PLAYING must not be the current state.
    CGS_ASSERT(Playback::E_PLAYBACK_STATE_PLAYING != lpVoice->GetPlaybackState(),
               "E_PLAYBACK_STATE_PLAYING != GetPlaybackState()");

    // Mark the playback voice REMOVING (the X360 `*(v2+17) = 2` -- mu8RemoveState
    // by name).
    lpVoice->SetRemoveState(Playback::E_VOICE_REMOVE_REMOVING);

    lpVoice->Release();
    mVoiceHandle.SetObject(0);
}

// ----------------------------------------------------------------------------
// Voice::Connect(luSendNameHash, liSend)  @ 0x826C4F18
//   Assert the voice exists (CgsVoice.cpp:104), take a temp ref on the playback
//   voice (++*(obj+4)), then forward to ConnectVoice on the owner module's playback
//   sub-module (*(owner+8) ... + 568).
//     asm: if(!*(a1+4)) <assert>; v7=*(a1+4); if(v7) ++*(v7+4);
//          result = Playback::Module::Module::ConnectVoice(*(a1+8)+568,&v7,a2,a3);
// ----------------------------------------------------------------------------
void Voice::Connect(Ident luSendNameHash, Ident luSubmixIdent)
{
    CGS_ASSERT(mVoiceHandle.GetObject(), "Voice not yet created!");

    Playback::Voice* lpVoice = mVoiceHandle.GetObject();

    // Take a temporary reference for the duration of the connect -- the X360
    // `++*(v7+4)` is Playback::Object::Acquire inlined (mu32RefCount by name).
    if (lpVoice)
        lpVoice->Acquire();

    Playback::Handle<Playback::Voice> lhVoice(lpVoice);
    mpOwnerModule->GetPlaybackModule().ConnectVoice(
        &lhVoice, luSendNameHash, luSubmixIdent);
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

    // The X360 `*(obj+12)` == Playback::Voice::mIdent, read by name.
    const Playback::Voice* lpVoice = mVoiceHandle.GetObject();
    return static_cast<s32>(lpVoice->GetIdent());
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

    return lpVoice->GetPlaybackState() == Playback::E_PLAYBACK_STATE_PLAYING;
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

    return lpVoice->GetPlaybackState() != Playback::E_PLAYBACK_STATE_INVALID;
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
    Playback::Send* lpSend = mVoiceHandle.GetObject()->FindNamedSend(
        Playback::Name(static_cast<uintptr_t>(*lpSendName)));
    return lpSend ? lpSend->Get() : 1.0f;
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
    (void)liReserved;
    Playback::Send& lrSend = mVoiceHandle.GetObject()->GetSend(luSendNameHash);
    CGS_ASSERT(lrSend.GetName() == Playback::Name(static_cast<uintptr_t>(*lpSendName)),
               "lSend.GetName() == lSendName");
    lrSend.Set(lfGain);
}

// ----------------------------------------------------------------------------
// Voice::SetParameter(luSendNameHash, lfValue, liReserved, lpSendName)
//   The broadcast target of VoicePoolBase::SetParameter (@ 0x826B6628): the pool
//   forwards a raw parameter value to every live pooled voice's logic Voice. The
//   underlying set is a Playback-layer call (parallel to SetGain).
// FLAG: STUB -- depends on the Playback layer.
//   BLOCKS ON: the CgsSound::Playback parameter-set path (parallel to
//   Playback::Voice::GetSend). Not reconstructed. Added additively so the pool
//   broadcast compiles + names the call BY NAME.
// ----------------------------------------------------------------------------
void Voice::SetParameter(u32 luSendNameHash, f32 lfValue, s32 liReserved, const u32* lpSendName)
{
    CGS_ASSERT(mVoiceHandle.GetObject(), "mpObject");
    (void)liReserved;
    mVoiceHandle.GetObject()->SetParameter(
        static_cast<s32>(luSendNameHash),
        lfValue,
        Playback::Name(static_cast<uintptr_t>(*lpSendName)));
}

// ----------------------------------------------------------------------------
// Voice::~Voice  (compiler-synthesized `scalar deleting destructor')  @ 0x826C7E18
//   X360 sequence:
//     *this = &off_820B0E20;              // install the Voice vtable @+0
//     v4 = *(this+4);                     // mVoiceHandle.mpObject
//     if (v4) Playback::Object::Release(v4);
//     if (a2 & 1) operator delete(this);  // scalar-deleting tail
//     return this;
//
//   The only real work is dropping the reference the handle owns on its
//   Playback::Voice -- i.e. Handle<Voice>::~Handle() -> ReleaseObject() -> Release.
//   Defining the class destructor out-of-line emits exactly that: the mVoiceHandle
//   member destructor runs the Release, the vtable store and the operator-delete tail
//   are the compiler's scalar-deleting-destructor synthesis (host delete stands in
//   for the custom-allocator tail). Mirrors committed CgsAemsPlayerVoiceDtor.cpp /
//   CgsSubmixVoiceDtor.cpp.
//
//   NOTE: this is DISTINCT from Voice::Destruct() (@0x826C4E38) -- Destruct asserts
//   the voice exists + is not playing and writes the Playback state byte (+0x11=2);
//   the destructor here does NOT assert and only Releases the handle's object.
// ----------------------------------------------------------------------------
Voice::~Voice()
{
    // mVoiceHandle's destructor (Handle<Voice>::~Handle -> ReleaseObject) drops the
    // reference on the wrapped Playback::Voice when mpObject is non-null -- exactly
    // the X360 `if (mVoiceHandle.mpObject) Playback::Object::Release(...)`. The
    // vtable install and operator-delete tail are the compiler's synthesis.
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
s32 Voice::Attach(s32 liSlotName, Playback::Handle<Playback::Content>* lphContent)
{
    CGS_ASSERT(mVoiceHandle.GetObject(), "Voice not yet created!");

    Playback::Content* lpContent = lphContent->GetObject();
    if (lpContent)
        lpContent->Acquire();
    Playback::Handle<Playback::Content> lhContent(lpContent);

    Playback::Voice* lpVoice = mVoiceHandle.GetObject();
    if (lpVoice)
        lpVoice->Acquire();
    Playback::Handle<Playback::Voice> lhVoice(lpVoice);

    mpOwnerModule->GetPlaybackModule().AttachVoice(
        &lhVoice, &lhContent, static_cast<u32>(liSlotName));

    if (lphContent->GetObject())
        lphContent->GetObject()->Release();
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
    Playback::Slot* lpSlot = mVoiceHandle.GetObject()->FindNamedSlot(
        Playback::Name(static_cast<uintptr_t>(liSlotName)));
    if (lpSlot)
        lpSlot->Detach(*mVoiceHandle.GetObject());
    CGS_ASSERT(lpSlot != 0, "mVoiceHandle->Detach( lSlotName )");
    return lpSlot;
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
    Playback::Voice* lpVoice = mVoiceHandle.GetObject();
    lpVoice->Acquire();
    if (lpVoice->GetRemoveState() == Playback::E_VOICE_REMOVE_ALIVE)
    {
        Playback::Slot& lrSlot = lpVoice->GetSlot(0);
        if (lrSlot.Play(static_cast<Playback::PlayerVoice&>(*lpVoice),
                        static_cast<u32>(liParam)))
            lpVoice->SetPlaybackState(Playback::E_PLAYBACK_STATE_PLAYING);
    }
    lpVoice->Release();
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
    Playback::Voice* lpVoice = mVoiceHandle.GetObject();
    lpVoice->Acquire();
    if (lpVoice->GetRemoveState() == Playback::E_VOICE_REMOVE_ALIVE)
    {
        Playback::Slot& lrSlot = lpVoice->GetSlot(0);
        if (lrSlot.Stop(static_cast<Playback::PlayerVoice&>(*lpVoice)))
            lpVoice->SetPlaybackState(Playback::E_PLAYBACK_STATE_STOPPED);
    }
    lpVoice->Release();
    return 0;
}

} // namespace Logic
} // namespace CgsSound

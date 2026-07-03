// ============================================================================
// CgsVoiceWrapper.cpp -- CgsSound::Logic::VoiceWrapper runtime bodies.
//
// Bodied from BURNOUT_X360_ARTIST.XEX:
//   VoiceWrapper::GetGain                              @ 0x826C5218
//   VoiceWrapper::~VoiceWrapper (scalar deleting dtor) @ 0x826E0B88
//
// VoiceWrapper is the per-slot voice wrapper the sound-logic pool embeds. See
// CgsVoiceWrapper.h for the layout (mVoice @+0x34, mpObject @+0x40).
//
// FLAG (Playback-layer reach): the embedded Voice's owned object and the wrapper's
// +0x40 handle are ref-counted CgsSound::Playback objects. The embedded-Voice object
// is fetched via Voice::GetVoiceObject() as a Playback::Voice* (incomplete type in
// this layer); the X360 drops its reference through the {vptr, mu32RefCount} base it
// shares with Playback::Object, so the drop is expressed through that shared base BY
// NAME (reinterpret to the ref-counted Object base -- the ODR-compatible layout).
// ============================================================================

#include "GameShared/GameClasses/Sound/Logic/CgsVoiceWrapper.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"

namespace CgsSound
{
namespace Logic
{

// ---------------------------------------------------------------------------
// VoiceWrapper::GetGain(lpSendName)  @ 0x826C5218
//   If the embedded logic Voice has no playback voice yet (its handle's mpObject,
//   read as *(this+0x38), is null) return 0.0f (flt_82001CC0). Otherwise spill the
//   caller's send-name word into a local and forward to Voice::GetGain(&mVoice,
//   &sendName), passing back whatever that leaves in fp1.
//     asm: if (!*(this+0x38)) return 0.0f;      // mVoice.mVoiceHandle.mpObject
//          v3 = *lpSendName;                     // spill the name word
//          return Voice::GetGain(&mVoice, &v3);
// ---------------------------------------------------------------------------
f32 VoiceWrapper::GetGain(const s32* lpSendName) const
{
    // *(this+0x38) == mVoice.mVoiceHandle.mpObject: the embedded Voice's owned
    // playback-voice pointer. Null -> no voice yet -> 0.0f.
    if (mVoice.GetVoiceObject() == 0)
        return 0.0f;                    // flt_82001CC0

    // Spill the send-name word into a local and forward by pointer, exactly as the
    // X360 does (stw r11,var_10; addi r4,r1,var_10; addi r3,r3,0x34).
    s32 liSendName = *lpSendName;
    return mVoice.GetGain(&liSendName);
}

// ---------------------------------------------------------------------------
// VoiceWrapper::~VoiceWrapper   (anchor for the X360 `scalar deleting destructor'
//   @ 0x826E0B88)
//
//   The X360 scalar-deleting-destructor is the canonical MSVC shape:
//     ~VoiceWrapper(this);
//     if (a2 & 1) <sound allocator>.Free(this);   // off_82FFB954, vtable slot +0x14
//   The conditional allocator-routed free is the compiler-synthesised deleting tail
//   (off_82FFB954 not homed in this group); the host `delete` stands in for it --
//   exactly as in CgsEffectObjectDtor.cpp.
//
//   The OBSERVABLE class-destructor body is the teardown the pool dtor (0x826E5370)
//   inlines per element: Release() the wrapper, drop the +0x40 ref-counted object
//   handle, then tear down the embedded Voice's +0x38 handle, each with the
//   CgsObject.h:117 "mu32RefCount > 0" assert. Mirrors
//   StateManager::RegisteredContent::~RegisteredContent.
// ---------------------------------------------------------------------------
VoiceWrapper::~VoiceWrapper()
{
    // The wrapper's own detach step (X360 `bl VoiceWrapper::Release' right after the
    // mid-teardown vtable install). Its body is a separate DEFERRED slice.
    Release();

    // Drop the +0x40 ref-counted object handle FIRST: the X360 inlines the CgsObject
    // Release (assert the count is still positive, --count, DoDispose at zero).
    if (mpObject != 0)
    {
        CGS_ASSERT(mpObject->GetRefCount() > 0, "mu32RefCount > 0");
        mpObject->Release();
        if (mpObject->GetRefCount() == 0)
            mpObject->DoDispose();      // (*(*v4+4))(v4): the virtual dispose slot
    }

    // The embedded logic Voice (+0x34) tears down last: its +0x38 handle drops its
    // playback object the same way. The X360 inlines the identical assert-decrement-
    // dispose sequence on *(this+0x38). The embedded Voice's owned object is a
    // Playback::Voice* (incomplete here); its reference is dropped through the shared
    // ref-counted Playback::Object base (the {vptr, mu32RefCount} layout both share).
    if (Playback::Voice* lpVoice = mVoice.GetVoiceObject())
    {
        Playback::Object* lpVoiceObj = reinterpret_cast<Playback::Object*>(lpVoice);
        CGS_ASSERT(lpVoiceObj->GetRefCount() > 0, "mu32RefCount > 0");
        lpVoiceObj->Release();
        if (lpVoiceObj->GetRefCount() == 0)
            lpVoiceObj->DoDispose();
    }
}

} // namespace Logic
} // namespace CgsSound

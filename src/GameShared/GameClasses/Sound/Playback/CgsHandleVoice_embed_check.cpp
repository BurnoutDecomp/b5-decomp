// ============================================================================
// CgsHandleVoice_embed_check.cpp
//   CgsSound::Playback::Handle<CgsSound::Playback::Voice>::operator* @ 0x8268E1D0
//
// This is the T = Voice instantiation of the committed CgsHandle.h template
// Handle<T>::operator* (already bodied inline there). Its X360 body is byte-identical
// to the committed Handle<Factory>::operator* @ 0x8268E178: null-check the owned
// pointer (fires the CgsHandle.h:287 assert "mpObject"), then return *mpObject.
// Nothing new to reconstruct -- this TU only forces the linker to keep the
// instantiation that Module::CreateVoice (@0x826D7B00) calls.
//
// The inline operator* body (`return *mpObject`) requires a COMPLETE T, so we
// instantiate against a local complete stub and force emission by taking the
// member-function pointer -- exactly the committed CgsBranchlessOperations_
// embed_check.cpp pattern (a bare `template ... Handle<Voice>::operator*();` over a
// forward-declared Voice would be ill-formed: you cannot deref an incomplete type).
// Only completeness is load-bearing here, not the real Voice layout (the body reads
// mpObject@+0 and returns it).
// ============================================================================

#include "GameShared/GameClasses/Sound/Playback/CgsHandle.h"

namespace
{

// Complete stand-in so Handle<VoiceStub>::operator* (return *mpObject) instantiates.
struct VoiceStub
{
    int miValue;
};

// Taking the member-function pointer forces emission of the inline dereference
// (both overloads) without needing the declared-only Handle ctor.
typedef CgsSound::Playback::Handle<VoiceStub> StubHandle;
VoiceStub&       (StubHandle::*gpfDeref)()            = &StubHandle::operator*;
const VoiceStub& (StubHandle::*gpfDerefConst)() const = &StubHandle::operator*;

void EmbedCheck()
{
    (void)gpfDeref;
    (void)gpfDerefConst;
}

} // namespace

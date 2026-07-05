// Handle_Object_operator_deref.cpp
//   CgsSound::Playback::Handle<CgsSound::Playback::Object>::operator* @ 0x8268E2D8
//
// T = Object instantiation of the committed CgsHandle.h template operator*
// (bodied inline there): assert mpObject non-null ("mpObject", CgsHandle.h line
// 287), then return *mpObject. Byte-identical to Handle<Factory>::operator*
// @0x8268E178. Dereferenced by GenericRwacFactory::HandlePluginEvent. Emit the
// linker-kept out-of-line copy so the address resolves; no distinct body.
//
// Object is a COMPLETE type in CgsContent.h (ctor + virtual dtor + mu32RefCount),
// so the inline body `return *mpObject;` is well-formed and the bare explicit
// instantiation compiles directly -- no complete-stub trick needed (that was only
// required for the forward-declared Voice in CgsHandleVoice_embed_check.cpp).

#include "GameShared/GameClasses/Sound/Playback/CgsHandle.h"
#include "GameShared/GameClasses/Sound/Playback/CgsContent.h"

template CgsSound::Playback::Object&
    CgsSound::Playback::Handle<CgsSound::Playback::Object>::operator*();

#include "GameShared/GameClasses/Sound/Logic/CgsContent.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"

// CgsSound::Logic::Content -- reconstructed from BURNOUT_X360_ARTIST.XEX.
//   Construct @ 0x826C4CA0
//   Destruct  @ 0x826C4D98

namespace CgsSound
{
namespace Logic
{

// ---------------------------------------------------------------------------
// CgsSound::Logic::Content::Construct  @ 0x826C4CA0
//
// Create (or replace) the held playback-content object. The X360 (a) asserts a
// module was supplied and this handle is not already created, (b) asks the module
// for the content ident (module vtable slot +0x48), (c) builds a temporary
// Handle<Playback::Content> via the content-create helper (X360 bl
// CgsSound::Playback::Module::Module_) against the module's embedded
// Playback::Module (X360 r4 == lpModule + 0x238), passing the two
// Command::QueueElement selectors, (d) Acquires the freshly-created content,
// releases any previously-held content, stores the new content into
// mContentHandle, releases the temporary handle's own reference, stores the
// owning module, and (e) asserts the handle ended up non-null.
//
// Store-for-store from 0x826C4CA0-0x826C4D94. The Acquire (++*(content+4)) and the
// two Releases are the inlined Handle-copy ref accounting. ModuleGetContentIdent /
// ModuleCreateContent are declared-only deferred BY-NAME shims (their real homes
// are the Logic::Module keystone and the Playback::Module TU); they are named here
// to match the vtable dispatch and the single `bl` exactly.
// ---------------------------------------------------------------------------
void Content::Construct(Module* lpModule,
                        Command::QueueElement aQueueElementA,
                        Command::QueueElement aQueueElementB)
{
    CGS_ASSERT(lpModule && !IsCreated(), "( lpModule ) && ( !IsCreated() )");

    // Module supplies the content ident (X360 virtual dispatch, module vtable +0x48).
    Command::QueueElement lIdent = ModuleGetContentIdent(lpModule);

    // Build the new content into a temporary handle via the content-create helper
    // (X360 bl Module_ with r4 == lpModule + 0x238 == &mPlaybackModule).
    ContentHandle lhNewContent;
    ModuleCreateContent(&lhNewContent, lpModule, lIdent, aQueueElementA, aQueueElementB);

    // Acquire the freshly-created content (X360 ++*(content+4)).
    CgsSound::Playback::Content* lpNewContent = lhNewContent.GetObject();
    if (lpNewContent != 0)
        reinterpret_cast<CgsSound::Playback::Object*>(lpNewContent)->Acquire();

    // Release any content this handle previously held (X360 releases OLD before store).
    if (mContentHandle.GetObject() != 0)
        reinterpret_cast<CgsSound::Playback::Object*>(mContentHandle.GetObject())->Release();

    // Adopt the new content; drop the temporary handle's own reference.
    mContentHandle.SetObject(lpNewContent);
    if (lhNewContent.GetObject() != 0)
        reinterpret_cast<CgsSound::Playback::Object*>(lhNewContent.GetObject())->Release();

    mpModule = lpModule;

    CGS_ASSERT(mContentHandle.GetObject() != 0, "mContentHandle");
}

// ---------------------------------------------------------------------------
// CgsSound::Logic::Content::Destruct  @ 0x826C4D98
//
// Tear down the held content: assert it was created, begin its removal on the
// underlying Playback::Content, drop this handle's reference, and null the handle.
// The X360 fires two asserts -- the source-level "Content not yet created!"
// (CgsContent.cpp:119) and the inlined Handle::operator-> null guard ("mpObject",
// CgsHandle.h:287) that expands where mContentHandle-> is reached to call
// BeginRemove. Both collapse to CGS_ASSERT; both test the same
// mContentHandle.mpObject != 0 predicate the asm loads (lwz r11,4(r31)).
//
// BeginRemove is routed through the deferred BY-NAME shim ContentBeginRemove
// (Playback::Content is incomplete in this TU) to match the `bl` exactly; no body
// is fabricated for it.
// ---------------------------------------------------------------------------
void Content::Destruct()
{
    CGS_ASSERT(mContentHandle.GetObject() != 0, "Content not yet created!");

    // The X360 reaches the content through mContentHandle-> (the operator-> null
    // guard is the CgsHandle.h:287 "mpObject" assert) to begin its removal.
    CGS_ASSERT(mContentHandle.GetObject() != 0, "mpObject");
    CgsSound::Playback::ContentBeginRemove(mContentHandle.GetObject());

    // Drop this handle's reference and null the slot.
    if (mContentHandle.GetObject() != 0)
        reinterpret_cast<CgsSound::Playback::Object*>(mContentHandle.GetObject())->Release();
    mContentHandle.SetObject(0);
}

} // namespace Logic
} // namespace CgsSound

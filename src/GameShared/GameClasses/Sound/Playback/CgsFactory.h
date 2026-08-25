#ifndef CGS_SOUND_PLAYBACK_CGSFACTORY_H
#define CGS_SOUND_PLAYBACK_CGSFACTORY_H

#include "types.hpp"

#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Sound/Playback/CgsObject.h"      // Object (Factory base)
#include "GameShared/GameClasses/Sound/Playback/CgsHandle.h"      // Handle<Content>
#include "GameShared/GameClasses/Sound/Playback/CgsCommon.h"      // Name
#include "GameShared/GameClasses/Sound/Playback/CgsEnvironment.h" // the REAL Environment (AddFactory/AddContent/GetAllocator)

// ============================================================================
// CgsFactory.h  (HOME for CgsSound::Playback::Factory).
//
// Factory is the reference-counted base that produces playback Content (and, in
// subclasses, Voices). It is registered with the owning Environment on construction
// and asserts a zero refcount on teardown. The reconstructed functions:
//   Factory::Factory(Name, Environment&)     @ 0x826AD340
//   Factory::CreateContent(spec, handle, id) @ 0x826AD4D0
//   Factory::~Factory()                      @ 0x826AD3C8
//   Factory::DoDispose()                     @ 0x826AD450
//
// LAYOUT (X360; host-width FLAG -- pointer/ref members widen, so members are pinned
// BY NAME + SEQUENCE, no absolute-offset static_assert):
//   Object base:  vptr @+0, mu32RefCount @+4
//   Factory adds: mName @+8 (one Name word), mEnvironment @+0xC (Environment ref)
//
// FLAG: MINIMAL FLAGGED HOME. The owning Environment and the ContentDisposer are
// their own keystone TUs; only the members + static helpers the four Factory methods
// touch are modelled here (Environment::AddFactory/AddContent and the disposer walk).
// ============================================================================

namespace CgsSound
{
namespace Playback
{

struct ContentSpec;   // CgsContent.h (Factory::CreateContent takes it by const ref).
struct Content;       // CgsContent.h.
class  VoiceSpec;     // DWARF CgsFactory.h:157 (DoCreateVoice) -- own TU.
class  Voice;         // Handle<Voice> in DoCreateVoice.

// (2026-08-25, audio-faithfulness wave 3): the former TU-local minimal `class
// Environment { two statics }` rival is RETIRED for the real DWARF home -- the
// AddFactory/AddContent statics live on the real struct (CgsEnvironment.h:120/122)
// with identical signatures. The former `ContentDisposer`/`FactoryDisposeRequest`/
// `GetContentDisposer()` trio is DELETED outright: the DWARF Factory surface has no
// such members -- they were an invented model of DoDispose's tail, which is really
// the environment ALLOCATOR handing the carve back (see CgsFactory.cpp).
// ---------------------------------------------------------------------------
// CgsSound::Playback::Factory. DWARF CgsFactory.h:54 -- surface below mirrors the
// DecFIGS declaration order; vtable = [0] dtor, [1] DoDispose (Object slot,
// overridden), [2] DoCreateVoice, [3] DoCreateContent, [4] DoUpdate.
// ---------------------------------------------------------------------------
class Factory : public Object
{
public:
    // @ 0x826AD340 (DWARF h:243). Install the vtable, zero the refcount, store
    // name + environment, and register with the environment (assert success).
    Factory(Name aName, Environment& arEnvironment);

    // @ 0x826AD3C8 (DWARF h:85). Assert a zero refcount at teardown.
    virtual ~Factory();

    // @ 0x826AD450 (DWARF h:317). Tear the factory down and hand its carve back
    // through the owning environment's allocator. Bodied in CgsFactory.cpp.
    virtual void DoDispose();

    // @ 0x826AD4D0 (DWARF h:290). Create content from a spec into the out-handle:
    // dispatch the subclass DoCreateContent (vtable slot 3), then register the
    // produced content with the environment; (u32)-1 on failure (clearing the
    // out-handle on registration rejection).
    u32 CreateContent(const ContentSpec& akrSpec, Handle<Content>& arHandleOut, u32 au32Ident);

    // DWARF CgsFactory.h:91 (template; u32 return). The voice mirror of
    // CreateContent: dispatch the subclass DoCreateVoice, register the produced
    // voice with the environment; (u32)-1 on failure. Caller: Module::CreateVoice
    // @0x826D7B00. FLAG (DEFER): declared-only -- bodied with the factory slices.
    template <typename T>
    u32 CreateVoice(const VoiceSpec& akrSpec, Handle<T>& arHandleOut, u32 au32Ident);

    // DWARF h:255 / h:310. FLAG (DEFER): declared-only -- their own ledger slices.
    // GetName is const (a pure accessor; Environment::GetR reads the name through a
    // const Factory&).
    Name GetName() const;
    void Update(f32 af32DeltaTime);

    // DWARF h:334. The owning environment, by name (the X360 reads Factory+0x0C;
    // ContentLoader's allocator walk and Content::DoDispose go through here).
    Environment& GetEnvironment() { return mEnvironment; }

protected:
    // DWARF h:139/147. FLAG (DEFER): declared-only.
    bool Install(Factory* apFactory);
    void Uninstall(Factory* apFactory);

    // DWARF h:157/168/172 -- the subclass factory hooks (vtable slots 2/3/4).
    // FLAG (DEFER): declared-only base implementations; each concrete factory
    // (AEMS / Splicer / GenericRwac) overrides them in its own TU. NOTE: any TU
    // that emits this vtable (defines a Factory virtual out-of-line) makes the
    // linker want these three symbols -- body them with the base slices when a
    // Factory-deriving TU is first mounted.
    virtual bool DoCreateVoice(const VoiceSpec& akrSpec, Handle<Voice>& arHandleOut, u32 au32Ident);
    virtual bool DoCreateContent(const ContentSpec& akrSpec, Handle<Content>& arHandleOut, u32 au32Ident);
    virtual void DoUpdate(f32 af32DeltaTime);

    Name              mName;         // X360 +0x8   (DWARF h:201)
    Environment&      mEnvironment;  // X360 +0xC   (DWARF h:202 spells `const
                                     // Environment&`; kept non-const here because the
                                     // attested AddFactory/AddContent/GetAllocator
                                     // call paths mutate through it -- revisit with
                                     // the Environment const-surface slice)
};

} // namespace Playback
} // namespace CgsSound

#endif // CGS_SOUND_PLAYBACK_CGSFACTORY_H

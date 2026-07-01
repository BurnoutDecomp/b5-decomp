#ifndef CGS_SOUND_PLAYBACK_CGSFACTORY_H
#define CGS_SOUND_PLAYBACK_CGSFACTORY_H

#include "types.hpp"

#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Sound/Playback/CgsObject.h"   // Object (Factory base)
#include "GameShared/GameClasses/Sound/Playback/CgsHandle.h"   // Handle<Content>
#include "GameShared/GameClasses/Sound/Playback/CgsCommon.h"   // Name

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
class  Environment;
class  Factory;       // fwd -- Environment::AddFactory takes a Factory*.

// The dispose request/disposer the factory hands a torn-down instance to. Modelled
// minimally (only the members DoDispose builds/reads). Bodied elsewhere.
struct FactoryDisposeRequest
{
    void* mpInstance;         // +0x00
    u32   mau32Reserved[4];   // +0x04..0x13 (zeroed)
};

class ContentDisposer
{
public:
    virtual ~ContentDisposer() {}
    virtual int DisposeContent(FactoryDisposeRequest* apRequest) = 0;
};

// ---------------------------------------------------------------------------
// FLAG (DEFER): minimal Playback::Environment slice. Factory registers itself and
// its produced content with the environment; only these statics are load-bearing.
// Full Environment is its own TU.
// ---------------------------------------------------------------------------
class Environment
{
public:
    static bool AddFactory(Environment& arEnvironment, Factory* apFactory);
    static u32  AddContent(Environment& arEnvironment, Content* apContent);
};

// ---------------------------------------------------------------------------
// CgsSound::Playback::Factory.
// ---------------------------------------------------------------------------
class Factory : public Object
{
public:
    // @ 0x826AD340. Install the vtable, zero the refcount, store name + environment,
    // and register with the environment (assert registration succeeds).
    Factory(Name aName, Environment& arEnvironment);

    // @ 0x826AD3C8. Assert a zero refcount at teardown (Object base parity).
    virtual ~Factory();

    // @ 0x826AD450. Dispose a factory-produced instance via the owning manager's
    // content disposer. FLAG: the manager/disposer walk is DEFERRED -- DoDispose is
    // bodied in CgsFactory.cpp through the raw dispatch it performs. Void per DWARF.
    virtual void DoDispose();

    // @ 0x826AD4D0. Create content from a spec into the out-handle. Gated on a leading
    // no-arg bool virtual (vtable slot 3); returns the environment content index or
    // (u32)-1 (clearing the out-handle on rejection).
    u32 CreateContent(const ContentSpec& akrSpec, Handle<Content>& arHandleOut, u32 au32Ident);

    // The owning content disposer (reached through the factory's manager). FLAG
    // (DEFER): declared-only; bodied in the Factory/Environment TU.
    ContentDisposer* GetContentDisposer();

protected:
    Name              mName;         // X360 +0x8   one interned Name word
    Environment&      mEnvironment;  // X360 +0xC   owning environment
};

} // namespace Playback
} // namespace CgsSound

#endif // CGS_SOUND_PLAYBACK_CGSFACTORY_H

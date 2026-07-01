// ============================================================================
// CgsFactory.cpp -- CgsSound::Playback::Factory runtime bodies.
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX:
//   Factory::Factory(Name, Environment&) @ 0x826AD340
//   Factory::~Factory()                  @ 0x826AD3C8
//   Factory::CreateContent(...)          @ 0x826AD4D0
//   Factory::DoDispose()                 @ 0x826AD450
// ============================================================================

#include "GameShared/GameClasses/Sound/Playback/CgsFactory.h"

#include <cstring>

namespace CgsSound
{
namespace Playback
{

// @ 0x826AD340. Installs the Factory vtable, zero-inits the Object refcount base,
// stores the name + owning environment, then registers with the environment.
Factory::Factory(Name aName, Environment& arEnvironment)
    : Object(),
      mName(aName),
      mEnvironment(arEnvironment)
{
    bool lbResult = Environment::AddFactory(mEnvironment, this);
    CGS_ASSERT(lbResult, "lbResult");
}

// @ 0x826AD3C8. The scalar-deleting thunk reinstalls the Object base vtable, asserts
// a zero refcount, then frees on (flag & 1). The source ~Factory() body is the one
// assert; MSVC re-synthesises the deleting thunk from this out-of-line virtual dtor.
Factory::~Factory()
{
    CGS_ASSERT(mu32RefCount == 0, "0 == mu32RefCount");
}

// @ 0x826AD4D0. Gate on a leading no-arg bool virtual (vtable slot 3 / byte 0xC); if
// false, return (u32)-1. Otherwise assert the out-handle owns a Content, register it
// with the environment, and on rejection clear the out-handle before returning -1.
u32 Factory::CreateContent(const ContentSpec& /*akrSpec*/, Handle<Content>& arHandleOut, u32 /*au32Ident*/)
{
    void** lpapVtbl = *reinterpret_cast<void***>(this);
    if (!reinterpret_cast<bool (*)(void*)>(lpapVtbl[3])(this))
    {
        return static_cast<u32>(-1);
    }

    // The out-handle must already own a Content (CgsHandle.h:305).
    CGS_ASSERT(arHandleOut.GetObject() != 0, "mpObject");

    u32 luResult = Environment::AddContent(mEnvironment, arHandleOut.GetObject());
    if (luResult == static_cast<u32>(-1))
    {
        // AddContent rejected the content: clear the out-handle (X360 assigns a
        // stack-built null Content* through Handle<Content>::operator=).
        arHandleOut = Handle<Content>(0);
        return static_cast<u32>(-1);
    }
    return luResult;
}

// @ 0x826AD450. Dispose a factory-produced instance via two virtual dispatches.
//   manager = *(this + 12);
//   (*this->vtable[0])(this, 0);              // self virtual call, slot 0
//   disposer = *(manager + 48);
//   params[0] = this; params[1..4] = 0;
//   (*disposer->vtable[5])(disposer, params); // disposer virtual, byte offset 20
// The two callees dispatch through raw vtables (their concrete types are DEFERRED),
// so the calls are issued through function-pointer casts, faithful to the pseudocode.
void Factory::DoDispose()
{
    char* lpThis = reinterpret_cast<char*>(this);

    void** lpapVtbl = *reinterpret_cast<void***>(lpThis);
    reinterpret_cast<void (*)(void*, int)>(lpapVtbl[0])(this, 0);

    // owning manager (X360 +12 == mEnvironment slot) and its disposer sub-object (+48)
    char* lpManager  = *reinterpret_cast<char**>(lpThis + 12);
    void* lpDisposer = *reinterpret_cast<void**>(lpManager + 48);

    void* lapParams[6];
    lapParams[0] = this;
    std::memset(&lapParams[1], 0, 16);

    void** lpapDisposerVtbl = *reinterpret_cast<void***>(lpDisposer);
    reinterpret_cast<int (*)(void*, void*)>(lpapDisposerVtbl[5])(lpDisposer, lapParams);
}

} // namespace Playback
} // namespace CgsSound

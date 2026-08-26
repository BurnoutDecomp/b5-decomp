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

#include "rw/rwcore_structs.h"   // rw::Resource / rw::IResourceAllocator (the DoDispose carve return)

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

// @ 0x826AD4D0. The Hex-Rays view shows an "arg-less bool virtual at vtable slot 3";
// the DWARF vtable order ([0] dtor, [1] DoDispose, [2] DoCreateVoice,
// [3] DoCreateContent, [4] DoUpdate) names slot 3: this is the subclass hook
// DoCreateContent(spec, handle, ident) with the incoming args passed straight
// through (Hex-Rays dropped the untouched pass-through registers). By name:
// dispatch the hook; on success assert the out-handle owns the produced Content,
// register it with the environment, and on rejection clear the out-handle.
u32 Factory::CreateContent(const ContentSpec& akrSpec, Handle<Content>& arHandleOut, u32 au32Ident)
{
    if (!DoCreateContent(akrSpec, arHandleOut, au32Ident))
    {
        return static_cast<u32>(-1);
    }

    // The out-handle must own the produced Content (CgsHandle.h:305).
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

// @ 0x826AD450. The Hex-Rays view:
//   manager = *(this + 12);                   // mEnvironment (by name)
//   (*this->vtable[0])(this, 0);              // scalar-deleting dtor, flag 0
//                                             //   == non-deleting virtual destroy
//   disposer = *(manager + 48);               // Environment + 0x30 == mpAllocator
//                                             //   (the SAME word Environment::
//                                             //    DoDispose @0x826BFDF8 snapshots)
//   params = { this, 0, 0, 0, 0 };            // 20 bytes == the CONSOLE rw::Resource
//                                             //   (BaseResources<5>; first base =
//                                             //    this factory's carve)
//   (*disposer->vtable[5])(disposer, params); // the allocator's resource-free entry
//
// i.e. DoDispose destroys the (most-derived) factory in place, then hands the
// carve back to the owning environment's rw allocator as a Resource -- the exact
// counterpart of `operator new(size_t, Environment&)` (DWARF h:224) carving it.
// FLAG (vtable-slot inference): slot 5 is read as the console IResourceAllocator's
// DoFree(const Resource&) (the retail console vtable has no AllocDebug pair; the
// x64 rwcore.pdb vtable puts DoFree at slot 7 behind two AllocDebug entries). The
// host body calls DoFree by name with the host 4-slot Resource; if the console
// slot is ever attested otherwise, revisit here.
void Factory::DoDispose()
{
    // Snapshot the owning environment BEFORE destroying ourselves (the X360 loads
    // *(this+12) first; the environment outlives its factories).
    Environment& lrEnvironment = mEnvironment;

    // Non-deleting virtual destroy (the vtbl[0](this, 0) dispatch): C++'s explicit
    // virtual destructor call runs the most-derived chain without freeing.
    this->~Factory();

    // Hand the carve back through the environment's allocator.
    rw::IResourceAllocator* lpAllocator = lrEnvironment.GetAllocator();

    rw::Resource lResource;
    lResource.m_baseResources[0] = this;
    lResource.m_baseResources[1] = 0;
    lResource.m_baseResources[2] = 0;
    lResource.m_baseResources[3] = 0;
    lpAllocator->DoFree(lResource);
}

// DWARF CgsFactory.h:91 (template; bodied phase B5). The voice mirror of
// CreateContent above -- the wave-6 asm reconciliation attests the shape (the
// same dispatch-register-clear-on-reject walk, u32 return compared against
// (u32)-1 at the Module::CreateVoice caller): dispatch the subclass
// DoCreateVoice (vtable slot 2), assert the out-handle owns the produced voice,
// register it with the environment (Environment::AddVoice), clearing the handle
// when registration rejects. The X360 emits one instantiation per T.
template <typename T>
u32 Factory::CreateVoice(const VoiceSpec& akrSpec, Handle<T>& arHandleOut, u32 au32Ident)
{
    if (!DoCreateVoice(akrSpec, reinterpret_cast<Handle<Voice>&>(arHandleOut), au32Ident))
    {
        return static_cast<u32>(-1);
    }

    CGS_ASSERT(arHandleOut.GetObject() != 0, "mpObject");

    u32 luResult = Environment::AddVoice(mEnvironment, arHandleOut.GetObject());
    if (luResult == static_cast<u32>(-1))
    {
        arHandleOut = Handle<T>(0);
        return static_cast<u32>(-1);
    }
    return luResult;
}

// The one in-build instantiation (Module::CreateVoice @0x826D7B00's T = Voice).
template u32 Factory::CreateVoice<Voice>(const VoiceSpec&, Handle<Voice>&, u32);

// The base subclass-hook defaults (phase B5 -- this TU now emits the Factory
// vtable, which demands the three symbols exactly as the header's mount note
// predicted). No standalone X360 dump exists for the base slots (every live
// vtable carries a concrete factory's override); the bases decline to create
// and tick nothing, so an un-overridden slot behaves as "this factory offers
// none" -- the only reachable behaviour until the concrete factory slices land.
bool Factory::DoCreateVoice(const VoiceSpec& /*akrSpec*/, Handle<Voice>& /*arHandleOut*/,
                            u32 /*au32Ident*/)
{
    return false;
}
bool Factory::DoCreateContent(const ContentSpec& /*akrSpec*/, Handle<Content>& /*arHandleOut*/,
                              u32 /*au32Ident*/)
{
    return false;
}
void Factory::DoUpdate(f32 /*af32DeltaTime*/)
{
}

} // namespace Playback
} // namespace CgsSound

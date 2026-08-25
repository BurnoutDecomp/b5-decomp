#include "GameShared/GameClasses/Sound/Playback/CgsObject.h"

// CgsSound::Playback::Object out-of-line members.
//
// Reconstructed from the X360 vector-deleting-destructor at 0x826916F0:
//
//   *a1 = off_820AA908;            // install the Object vtable (compiler thunk)
//   if (a1[1])                     // a1[1] == mu32RefCount
//       assert("0 == mu32RefCount",
//              "..\\..\\..\\GameShared\\GameClasses\\Sound/Playback/CgsObject.h",
//              104);
//   if (a2 & 1) operator delete(a1);   // deleting-flavour frees storage
//
// The vtable install and the (a2 & 1) `operator delete` belong to the
// compiler-generated deleting-destructor thunk, not to the source-level
// destructor body. The source ~Object() is exactly the one assert below
// (CgsObject.h:104). MSVC re-synthesises its own deleting thunk from this
// out-of-line dtor, so it is faithful without hand-writing the thunk.
namespace CgsSound
{
namespace Playback
{

Object::~Object()
{
    // CgsObject.h:104. The object must hold no outstanding references at teardown.
    CGS_ASSERT(mu32RefCount == 0, "0 == mu32RefCount");
}

}
}

// ============================================================================
// Content::DoDispose -- RELOCATED out of CgsContent.h (2026-08-25, audio-
// faithfulness wave 3; was header-inline against an invented
// Factory::GetContentDisposer()/ContentDisposer model).
//
// Mirrors Factory::DoDispose @0x826AD450: snapshot the owning factory, run the
// non-deleting virtual destructor (the console vtbl[0](this, 0) dispatch), then
// hand the 20-byte {this, 0,0,0,0} block -- the CONSOLE rw::Resource
// (BaseResources<5>, first base = this content's carve) -- to the factory's
// environment allocator via its console vtable slot 5 == DoFree(const Resource&).
// FLAG (shape inference): no standalone X360 dump is cited for Content::DoDispose
// itself; the body is the Factory::DoDispose dispose-walk applied to the content
// carve (same request block, same slot), which the retired disposer model also
// encoded. Revisit if a Content::DoDispose dump lands.
// ============================================================================

#include "GameShared/GameClasses/Sound/Playback/CgsContent.h"
#include "GameShared/GameClasses/Sound/Playback/CgsFactory.h"
#include "rw/rwcore_structs.h"

namespace CgsSound
{
namespace Playback
{

void Content::DoDispose()
{
    // Snapshot the owning factory BEFORE destroying ourselves.
    Factory& lrFactory = mFactory;

    // Non-deleting virtual destroy (the vtbl[0](this, 0) dispatch).
    this->~Content();

    // Hand the carve back through the factory's environment allocator.
    rw::IResourceAllocator* lpAllocator = lrFactory.GetEnvironment().GetAllocator();

    rw::Resource lResource;
    lResource.m_baseResources[0] = this;
    lResource.m_baseResources[1] = 0;
    lResource.m_baseResources[2] = 0;
    lResource.m_baseResources[3] = 0;
    lpAllocator->DoFree(lResource);
}

} // namespace Playback
} // namespace CgsSound

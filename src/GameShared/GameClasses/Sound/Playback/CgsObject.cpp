#include "GameShared/GameClasses/Sound/Playback/CgsObject.h"
#include "GameShared/GameClasses/Core/CgsAssertMessageAt.h"

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

// Object::Release (ARTIST COMDAT 0x82680938) is defined in CgsObject.h (DWARF CgsObject.h:115).

}
}

// ARTIST 0x826AC970 snapshots the factory before destruction, then returns
// the content allocation through the factory environment.
#include "GameShared/GameClasses/Sound/Playback/CgsContent.h"
#include "GameShared/GameClasses/Sound/Playback/CgsFactory.h"
#include "rw/rwcore_structs.h"

namespace CgsSound
{
namespace Playback
{

void* Content::GetData(EContentState aeExpectedState)
{
    CGS_ASSERT(static_cast<u8>(aeExpectedState) == (mu8ContentState & 0x7Fu),
               "aeExpectedState == GetState()");
    return DoGetData();
}

void Content::DoDispose()
{
    // Snapshot the owning factory BEFORE destroying ourselves.
    Factory& lrFactory = mFactory;

    // Non-deleting virtual destroy (the vtbl[0](this, 0) dispatch).
    this->~Content();

    lrFactory.GetEnvironment().Free(this);
}

// ---------------------------------------------------------------------------
// Content::Update  @ 0x82680BA8  (bodied 2026-08-25, faithful-audio-engine
// phase B4 -- the base per-frame tick Environment::UpdateContent drives)
//   if (!mu16LoadCount && DoLoad()) ++mu16LoadCount;    // vtbl slot [2]
//   DoUpdate(dt);                                        // vtbl slot [6]
//   if (REMOVING && state == UNLOADED) removeState = REMOVED;
// ---------------------------------------------------------------------------
void Content::Update(f32 af32DeltaTime)
{
    if (mu16LoadCount == 0 && DoLoad())
        ++mu16LoadCount;

    DoUpdate(af32DeltaTime);

    if (mu8RemoveState == E_CONTENT_REMOVE_REMOVING &&
        mu8ContentState == E_CONTENT_STATE_UNLOADED)
    {
        mu8RemoveState = E_CONTENT_REMOVE_REMOVED;
    }
}

} // namespace Playback
} // namespace CgsSound

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

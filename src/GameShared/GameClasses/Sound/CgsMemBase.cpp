#include "GameShared/GameClasses/Sound/CgsMemBase.h"

// CgsSound::MemBase out-of-line members.
//
// Reconstructed from the X360 `vector deleting destructor` at 0x826AB548:
//
//   *this = &off_820AA820;            // install the MemBase vtable (compiler thunk)
//   if (flags & 1)                    // deleting flavour
//   {
//       _DWORD desc[5];               // a 20-byte free descriptor (sp+0x50)
//       desc[0] = this;               // [0]  = pointer to free
//       desc[1..4] = 0;               // [4..0x10] zeroed
//       allocator = off_82FFB954;     // the sound allocator instance
//       (*(allocator->vtable + 0x14))(allocator, desc);   // Free(desc)
//   }
//   return this;
//
// The vtable install and the (flags & 1) allocator-routed free are the
// compiler-generated deleting-destructor thunk, not the source-level destructor
// body. The source ~MemBase() body is empty (the destructor writes only the
// vptr; MemBase has no data members it tears down). MSVC re-synthesises its own
// deleting thunk from this out-of-line virtual destructor, so it is faithful
// without hand-writing the thunk or the off_82FFB954 allocator vtable call.
namespace CgsSound
{

MemBase::~MemBase()
{
    // No member teardown: the X360 destructor only re-installs the vtable and
    // (in the deleting flavour) frees storage via the sound allocator -- both are
    // thunk/operator-delete concerns, not source-level body statements.
}

}

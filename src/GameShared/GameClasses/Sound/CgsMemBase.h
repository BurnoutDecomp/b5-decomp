#ifndef CGS_SOUND_CGSMEMBASE_H
#define CGS_SOUND_CGSMEMBASE_H

#include "types.hpp"

// CgsSound::MemBase - the polymorphic base for CgsSound objects that are
// allocated/freed through the sound subsystem's own allocator rather than the
// global new/delete. Reconstructed from the X360 vector-deleting-destructor at
// 0x826AB548 (the only function in this TU's ledger). No DWARF/leak home exists
// for this type; the layout and the single member function below are recovered
// from that destructor's asm.
//
// What the X360 deleting destructor proves about the type:
//   *this = &off_820AA820;          // installs this class's vtable at +0
//   if (flags & 1)                  // deleting flavour
//       <sound allocator>.Free(this);  // free via off_82FFB954, vtable slot +0x14
//
// So MemBase is polymorphic (vptr @ +0) and, in its deleting destructor, hands
// its own storage back to the sound allocator instance held at off_82FFB954.
// That vtable (off_820AA820) is the SAME one a BrnSound::Logic::BrnEffectObject's
// IResourceRequester sub-object installs (see BrnEffectObject.cpp:48) -- i.e.
// MemBase is the shared allocation base those sound objects inherit.
//
// FLAG (faithful, not byte-identical): the leading vtable store and the
// allocator-routed free are the compiler-generated deleting-destructor thunk, not
// the source-level destructor body. Following the established convention in this
// tree (CgsObject.cpp @ 0x826916F0, BrnEffectObject.cpp @ 0x826AF4C8), the
// source-level ~MemBase() is modelled as the observable member teardown -- which
// here is empty: the destructor only re-installs the vtable and frees storage,
// both of which MSVC re-synthesises from a virtual destructor + the class's own
// operator delete. The sound allocator (off_82FFB954) is not homed in this group,
// so the custom-allocator dispatch is left to the host toolchain's `delete`
// rather than fabricating the raw allocator vtable call. MemBase carries no data
// members the destructor touches (it only writes the vptr), so no fields are
// modelled beyond the implicit vptr.
namespace CgsSound
{

// Allocation base for sound objects routed through the sound allocator.
class MemBase
{
public:
    MemBase() {}

    // Virtual so the deleting destructor at 0x826AB548 is reached through the
    // vtable (off_820AA820) exactly as the X360 build dispatches it. Bodied
    // out-of-line in CgsMemBase.cpp.
    virtual ~MemBase();
};

}

#endif // CGS_SOUND_CGSMEMBASE_H

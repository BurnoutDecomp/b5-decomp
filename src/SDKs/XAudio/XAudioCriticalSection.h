#pragma once

// ===========================================================================
// XAUDIO::CCriticalSection -- the XAudio critical-section primitive in
// BURNOUT_X360_ARTIST.XEX. This header is the canonical OWNING home for:
//
//     XAUDIO::CCriticalSection::`scalar deleting destructor'  @ 0x8295F3B8
//
// There is no reference source and no DWARF for this TU, so the SHAPE is
// reconstructed from the X360 asm. `XAUDIO` is an external middleware boundary,
// so its identifiers are preserved verbatim per the naming convention.
//
// Layout / behaviour from the asm:
//   - It is a polymorphic leaf: the deleting destructor stores its own vtable
//     pointer (off_82108FF0) into *this (`stw r11, 0(r31)`), then, when the
//     low bit of the deleting flag is set, releases `this` through the XAudio
//     XMem manager (`XMemFree(&unk_83222C40, this, 0x61820000)`). That free-via-
//     manager is exactly a class-specific `operator delete`, modelled below.
//   - The dtor itself performs no member teardown in this TU (only the vtable
//     reset + the conditional free), so ~CCriticalSection has an empty body and
//     exists only to anchor the vtable; the host compiler synthesises the
//     scalar deleting destructor (run dtor, then `if (flag & 1) operator
//     delete(this)`) from it.
// ===========================================================================

#include "types.hpp"

namespace XAUDIO
{

class CCriticalSection
{
public:
    // Out-of-line virtual destructor: anchors the vtable (off_82108FF0). The
    // X360 scalar deleting destructor @ 0x8295F3B8 is the compiler-synthesised
    // thunk over this dtor.
    virtual ~CCriticalSection();

    // Class-specific deallocation: the deleting destructor's conditional free
    // routes `this` through the XAudio XMem manager rather than the global
    // heap. (@ 0x8295F3B8: XMemFree(&unk_83222C40, this, 0x61820000).)
    void operator delete(void* pMemory);
};

} // namespace XAUDIO

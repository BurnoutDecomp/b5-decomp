#pragma once

// ===========================================================================
// XAUDIO::CEffect -- the XAudio effect object in BURNOUT_X360_ARTIST.XEX. This
// header is the canonical OWNING home for:
//
//     XAUDIO::CEffect::GetContext               @ 0x8295F4A8
//     XAUDIO::CEffect::`scalar deleting destructor' @ 0x82961060
//
// There is no reference source and no DWARF for this TU, so the SHAPE is
// reconstructed from the X360 asm. `XAUDIO` is an external middleware boundary,
// so its identifiers are preserved verbatim per the naming convention.
//
// Layout / behaviour from the asm:
//   - Polymorphic: the deleting destructor @ 0x82961060 stores its vtable
//     pointer (off_82108FF4) into *this and, when the deleting flag's low bit
//     is set, releases `this` through the XAudio XMem manager
//     (XMemFree(&unk_83222C40, this, 0x61820000)) -- a class-specific
//     `operator delete`.
//   - GetContext @ 0x8295F4A8: `*a2 = *(this + 8); return 0;` -- reads the
//     context pointer stored at byte offset +8 and writes it through the
//     caller's out-parameter, returning 0 (S_OK). With the vptr at +0 this puts
//     a single 4-byte word at +4 ahead of the context; its role is not used by
//     either function in this TU, so it is modelled as a flagged reserved word
//     to land mpContext at the asm's +8.
// ===========================================================================

#include "types.hpp"

namespace XAUDIO
{

class CEffect
{
public:
    // Out-of-line virtual destructor: anchors the vtable (off_82108FF4); the
    // X360 scalar deleting destructor @ 0x82961060 is its compiler-synthesised
    // thunk.
    virtual ~CEffect();

    // @ 0x8295F4A8 -- write the stored context pointer (+8) through *ppContext
    // and return 0 (S_OK). Signature mirrors the asm out-param store width
    // (a full pointer word).
    s32 GetContext(void** ppContext);

    // Class-specific deallocation routing through the XAudio XMem manager.
    void operator delete(void* pMemory);

private:
    // +4: a reserved word that precedes mpContext in the X360 layout. Neither
    // function in this TU reads it; FLAGGED -- modelled only to place mpContext
    // at the asm's byte offset +8.
    u32   muReserved4; // +4

    // +8: the context pointer GetContext returns (lwz r11, 8(r3)).
    void* mpContext;   // +8
};

} // namespace XAUDIO

#include "vendor/renderware/collision/VolRef.hpp"

// ===========================================================================
// rw::collision::VolRef -- reconstructed from BURNOUT_X360_ARTIST.XEX.
//
//   VolRef::operator=  @ 0x82BB33E8
//
// A field-by-field copy. The asm copies, in this order: two words (+0,+4); four
// 16-byte VMX vectors (+0x10,+0x20,+0x30,+0x40 via lvx128/stvx128); four 8-byte
// quantities (+0x50,+0x58,+0x60,+0x68 via ld/std); a word (+0x70); a byte
// (+0x74). The gap +0x08..+0x0F is not touched. See VolRef.hpp for the layout.
//
// The two leading words are POINTERS and are host-width here (waveQ5 C1), so
// the console's untouched +0x08..+0x0F slot is where their upper halves live;
// copying them by name reproduces the console copy exactly on both widths.
// ===========================================================================

namespace rw
{
namespace collision
{

VolRef& VolRef::operator=(const VolRef& rOther)
{
    muVolumePtr = rOther.muVolumePtr;   // console +0x00 word (host: a pointer)
    muTransformPtr = rOther.muTransformPtr;   // console +0x04 word (host: a pointer)
    mRow0       = rOther.mRow0;         // +0x10
    mRow1       = rOther.mRow1;         // +0x20
    mRow2       = rOther.mRow2;         // +0x30
    mRow3       = rOther.mRow3;         // +0x40
    mu50        = rOther.mu50;          // +0x50
    mu58        = rOther.mu58;          // +0x58
    mu60        = rOther.mu60;          // +0x60
    mu68        = rOther.mu68;          // +0x68
    muTag       = rOther.muTag;         // +0x70
    muNumTagBits = rOther.muNumTagBits; // +0x74
    return *this;
}

} // namespace collision
} // namespace rw

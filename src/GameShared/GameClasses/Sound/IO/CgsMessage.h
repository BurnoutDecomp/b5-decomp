#ifndef CGS_SOUND_IO_CGSMESSAGE_H
#define CGS_SOUND_IO_CGSMESSAGE_H

#include "types.hpp"

// CgsSound::Io::MessageHeader - the leading header of a sound-subsystem message
// (this is the SOUND message system, distinct from the network CgsMessage). The
// only function homed here is Destruct() at 0x82689868, recovered from the X360
// pseudocode/asm:
//
//   0x82689868  li   r11, 0           ; r11 = 0
//   0x8268986C  li   r10, -1          ; r10 = -1 (0xFFFF as u16)
//   0x82689870  sth  r11, 8(r3)       ; *(this + 0x08) = 0
//   0x82689874  sth  r10, 0xA(r3)     ; *(this + 0x0A) = 0xFFFF
//   0x82689878  blr                   ; return this
//
// So Destruct() resets the two 16-bit fields at +0x08 and +0x0A: the first to 0
// and the second to 0xFFFF ("invalid id"). The Hex-Rays pseudocode collapses the
// pair into a single "*(result + 8) = 0xFFFF" store; the asm is authoritative and
// shows two separate halfword stores, so both fields are modelled by name.
//
// FLAG (faithful, not byte-identical; un-homed surface modelled honestly): the
// leading 8 bytes (+0x00..+0x07) are not touched by Destruct and are modelled as a
// single named opaque qword so the two reset fields land at their proven offsets
// (+0x08, +0x0A). Their meaning is unknown and is NOT fabricated. Destruct returns
// `this` (the X360 thunk returns r3 unchanged), matching the in-place destructor
// convention used elsewhere in this tree.
namespace CgsSound
{
namespace Io
{

// Leading header block of a sound message.
class MessageHeader
{
public:
    // Reset the two trailing 16-bit fields to their "destructed" values:
    // mu16Field08 = 0, mu16Id = 0xFFFF (invalid). Returns `this`.
    // Recovered from the X360 in-place destructor at 0x82689868.
    MessageHeader* Destruct()
    {
        mu16Field08 = 0;
        mu16Id      = 0xFFFF;
        return this;
    }

private:
    // +0x00: leading block untouched by Destruct (opaque, not fabricated).
    // Modelled as two 4-byte words (not a u64) to keep 4-byte alignment so the
    // reset fields below land at +0x08/+0x0A without alignment padding.
    u32 muLeadingOpaque0;
    u32 muLeadingOpaque1;
    // +0x08: 16-bit field reset to 0 on destruct.
    u16 mu16Field08;
    // +0x0A: 16-bit message id, reset to 0xFFFF ("invalid") on destruct.
    u16 mu16Id;
};

}
}

#endif // CGS_SOUND_IO_CGSMESSAGE_H

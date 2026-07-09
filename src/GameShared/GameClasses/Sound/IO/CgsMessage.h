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
// leading 8 bytes (+0x00..+0x07) are not touched by Destruct and are modelled as two
// named opaque 4-byte words so the two reset fields land at their proven offsets
// (+0x08, +0x0A). Their meaning is unknown and is NOT fabricated (the DecFIGS DWARF
// names them the X360 4-byte vtable pointer slot at +0x00 and meEffectType at +0x04,
// but their exact PC modelling is deferred). Destruct returns `this` (the X360 thunk
// returns r3 unchanged), matching the in-place destructor convention used elsewhere.
//
// SIZE (X360-attested = 16 bytes, corrected from the earlier 12-byte model): the
// header carries two more 16-bit destination fields at +0x0C/+0x0E (DWARF
// mu16InstanceID / mu16EffectId) that Destruct does not touch. Their existence is
// proven by the sound command queues: every CgsModule::VariableEventQueue<*,16>::
// AddEvent<CgsSound::Io::Message<T>> bakes sizeof(Message<T>) as its record-size arg,
// and those args (0x14==20 for Message<bool>/<float>/<u32>/<enum>, 0x18==24 for
// Message<unsigned __int64>) require sizeof(MessageHeader)==16 -- a 12-byte header
// would yield 16/16/20. BrnSound::Logic::SubmixesEffect::Notify independently reads a
// Message payload byte at +0x10, i.e. immediately past this 16-byte header.
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
    // reset fields below land at +0x08/+0x0A without alignment padding. DWARF:
    // X360 vtable pointer slot (+0x00) and meEffectType (+0x04).
    u32 muLeadingOpaque0;
    u32 muLeadingOpaque1;
    // +0x08: 16-bit field reset to 0 on destruct (DWARF mi16EventId).
    u16 mu16Field08;
    // +0x0A: 16-bit message id, reset to 0xFFFF ("invalid") on destruct
    // (DWARF mu16StateManagerId; 0xFFFF == KU16_NO_DESTINATION).
    u16 mu16Id;
    // +0x0C/+0x0E: the two trailing destination fields (DWARF mu16InstanceID /
    // mu16EffectId). NOT written by Destruct (its asm halfword stores touch only
    // +0x08/+0x0A); modelled for SIZE correctness -- see the SIZE note above. Their
    // meaning is DWARF-named but their reset/use is not attested here, so they carry
    // no fabricated value.
    u16 mu16InstanceID;
    u16 mu16EffectId;
};

// CgsSound::Io::Message<T> -- a MessageHeader followed by a T payload. DecFIGS DWARF
// (CgsMessage.h:272) declares "struct Message<T> : public MessageHeader { T mData; }".
// The sound command queues store these by byte image: CgsModule::VariableEventQueue
// <*,16>::AddEvent<Message<T>> forwards sizeof(Message<T>) as the record size. With
// the 16-byte MessageHeader above, sizeof(Message<T>) == 16 + sizeof(T) (rounded to
// alignof(T)), which reproduces the X360 size args exactly for the fixed-width
// payloads (Message<bool>/<float>/<u32>/<enum> == 20, Message<unsigned __int64> == 24,
// the GUI event payloads == 16 + their attested size, etc.). Payloads that embed a
// pointer/uintptr_t (e.g. CgsSound::Playback::Name) are wider on the x64 PC target
// than on the 32-bit X360, so their Message<T> sizeof carries the documented
// pointer-width delta rather than matching the X360 arg byte-for-byte.
template <typename T>
class Message : public MessageHeader
{
public:
    T mData;
};

}
}

#endif // CGS_SOUND_IO_CGSMESSAGE_H

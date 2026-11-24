#ifndef CGS_SOUND_IO_CGSMESSAGE_H
#define CGS_SOUND_IO_CGSMESSAGE_H

#include "types.hpp"

#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"   // CgsModule::Event (the queued-record base)

// CgsSound::Io::MessageHeader - the leading header of a sound-subsystem message
// (this is the SOUND message system, distinct from the network CgsMessage).
//
// DWARF (DecFIGS CgsMessage.h:82): `struct MessageHeader : public Event` with a
// `_vptr.MessageHeader` -- its one virtual is Destruct() (CgsMessage.cpp:119). The X360
// layout follows from that: the vptr at +0x00 (ARTIST vtable 0x820AEC04 = { Destruct
// 0x82689868 }, stored by every Message<T> built on the stack, e.g. 0x826EC1B0), the
// empty Event base and meEffectType at +0x04, then the four 16-bit fields at +0x08..+0x0E;
// 16 bytes. The PS3 bodies of Construct (0x82E878 / 0x82E898 / 0x82E8B8) and
// SetDestination (0x822EDC) store meEffectType at +4 and leave +0 alone. Because the
// Event base sits at +4, ARTIST's AddEvent<Message<T>> (0x826DDF28) converts the record
// to `const Event*` with `addi 4` and a null check.
//
// The sizes of the queued records follow: every CgsModule::VariableEventQueue<8192,16>::
// AddEvent<CgsSound::Io::Message<T>> bakes sizeof(Message<T>) as its record size
// (0x14 for Message<bool>/<float>/<u32>/<enum>, 0x18 for Message<unsigned __int64>).
namespace CgsSound
{
namespace Io
{

// DWARF (canonical home CgsCommandStream.h, alongside the Command/DataStream
// queues that carry it; parked here until that TU lands): the 4-byte element the
// sound-IO word queues move around -- e.g. the playback OutputBuffer's
// freed-stream-buffer array Array<QueueElement,3> carries the freed records'
// voice idents.
typedef u32 QueueElement;

// Leading header block of a sound message.
class MessageHeader : public CgsModule::Event
{
public:
    enum eEffectTypes
    {
        E_EFFECT_TYPE_NONE    = 0,
        E_EFFECT_TYPE_OBJECT  = 1,
        E_EFFECT_TYPE_CONTROL = 2,
    };

    static const u16 KU16_NO_DESTINATION  = 0xFFFF;
    static const u16 KU16_ALL_DESTINATIONS = 0xFFFE;

    // CgsMessage.cpp:52 (PS3 0x82E878): no destination.
    void Construct(s16 ai16EventId)
    {
        SetEventId(ai16EventId);
        SetStateManagerId(KU16_NO_DESTINATION);
        SetInstanceID(KU16_NO_DESTINATION);
        SetEffectId(KU16_NO_DESTINATION, E_EFFECT_TYPE_NONE);
    }

    // CgsMessage.cpp:74 (PS3 0x82E898).
    void Construct(s16 ai16EventId, u16 au16StateManagerId)
    {
        SetEventId(ai16EventId);
        SetStateManagerId(au16StateManagerId);
        SetInstanceID(KU16_NO_DESTINATION);
        SetEffectId(KU16_NO_DESTINATION, E_EFFECT_TYPE_NONE);
    }

    // CgsMessage.cpp:99 (PS3 0x82E8B8).
    void Construct(s16 ai16EventId, u16 au16StateManagerId,
                   u16 au16InstanceId, u16 au16EffectId,
                   eEffectTypes aeEffectType)
    {
        SetStateManagerId(au16StateManagerId);
        SetInstanceID(au16InstanceId);
        SetEffectId(au16EffectId, aeEffectType);
        SetEventId(ai16EventId);
    }

    // CgsMessage.cpp:119, ARTIST 0x82689868: clears the event id and the state manager.
    virtual void Destruct();

    // CgsMessage.cpp:142 (PS3 0x822EDC).
    void SetDestination(u16 au16StateManagerId, u16 au16InstanceId,
                        u16 au16EffectId, eEffectTypes aeEffectType)
    {
        SetStateManagerId(au16StateManagerId);
        SetInstanceID(au16InstanceId);
        SetEffectId(au16EffectId, aeEffectType);
    }

    // CgsMessage.h:298..366 (DWARF). Message consumers dispatch on the event id at +0x08,
    // e.g. SubmixesEffect::Notify @0x82687EE8 (`lhz r11, 8(r4)`).
    s16          GetEventId() const        { return mi16EventId; }
    u16          GetStateManagerId() const { return mu16StateManagerId; }
    u16          GetInstanceId() const     { return mu16InstanceID; }
    eEffectTypes GetEffectType() const     { return meEffectType; }
    u16          GetEffectId() const       { return mu16EffectId; }

private:
    // CgsMessage.h:435..490 (DWARF, private).
    void SetEventId(s16 ai16EventId)               { mi16EventId = ai16EventId; }
    void SetStateManagerId(u16 au16StateManagerId) { mu16StateManagerId = au16StateManagerId; }
    void SetInstanceID(u16 au16InstanceID)         { mu16InstanceID = au16InstanceID; }
    void SetEffectId(u16 au16EffectId, eEffectTypes aeEffectType)
    {
        mu16EffectId = au16EffectId;
        meEffectType = aeEffectType;
    }

    eEffectTypes meEffectType;       // +0x04  CgsMessage.h:250
    s16          mi16EventId;        // +0x08  CgsMessage.h:252
    u16          mu16StateManagerId; // +0x0A  CgsMessage.h:254
    u16          mu16InstanceID;     // +0x0C  CgsMessage.h:255
    u16          mu16EffectId;       // +0x0E  CgsMessage.h:256
};

// CgsSound::Io::Message<T> -- a MessageHeader followed by a T payload. DecFIGS DWARF
// (CgsMessage.h:272) declares "struct Message<T> : public MessageHeader { T mData; }" with
// only the implicit constructors (ARTIST stores just the vptr when one is built, e.g.
// 0x826EC1B0, the payload is assigned afterwards). sizeof(Message<T>) == 16 + sizeof(T)
// (rounded to alignof(T)), the record size every AddEvent<Message<T>> instance bakes.
template <typename T>
class Message : public MessageHeader
{
public:
    T mData;   // CgsMessage.h:274
};

}
}

#endif // CGS_SOUND_IO_CGSMESSAGE_H

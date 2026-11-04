#pragma once

// This file models only the payload views consumed by the ARTIST
// GameCallbackProcessor handlers. ARTIST does not provide native class names or
// vtables for these message types in the Realmc cluster. The `Payload` names
// below are inferred local dispatch tags, not recovered vendor spellings.
//
// These records are receiver views, not message constructors or vtable
// definitions. Every field is justified by a load/address passed by the native
// handler; unread trailing storage is intentionally not represented.

#include "types.hpp"
#include "SDKs/Realmc/RealmcCore.h"

namespace RealmcIface
{

// Slots whose ARTIST handlers are the shared no-op body read no payload words.
// Only their common Message header is modeled; their full native object sizes
// and spellings remain unknown.
struct MessageSlot08Payload
{
    RealmcCore::Message mHeader;
};

struct MessageSlot1CPayload
{
    RealmcCore::Message mHeader;
};

struct MessageSlot2CPayload
{
    RealmcCore::Message mHeader;
};

// ClearEntries calls the named IGameInterface callback without reading a
// payload field. The spelling is inferred from that callback and its +0x14
// processor slot; only the Message header is modeled.
struct MessageClearEntriesPayload
{
    RealmcCore::Message mHeader;
};

// Callback3C @0x82B546C0 loads the two words at +0x08/+0x0C and passes the
// address +0x10 as its third callback argument. The third pointer is the
// one-past address of the two observed words; no pointed-to bytes are read here.
struct MessageCallback3CPayload
{
    RealmcCore::Message mHeader;
    u32 maArguments[2]; // +0x08, +0x0C
};

// FoundEntry @0x82B54648 passes the address of +0x08 directly; it does not load
// a pointer from that address. Model one opaque byte to name the inline payload
// start without assigning meaning or claiming its full extent.
struct MessageFoundEntryPayload
{
    RealmcCore::Message mHeader;
    u8 maPayload08[1]; // +0x08; only its address is observed
};

// FindEntriesDone @0x82B545D0 forwards the two words at +0x08/+0x0C.
struct MessageFindEntriesDonePayload
{
    RealmcCore::Message mHeader;
    u32 muField08;
    u32 muField0C;
};

// SaveCheckDone @0x82B54AC0 reads the two leading words, a begin/end pair,
// opaque words at +0x18/+0x1C, and the words at +0x20/+0x24. Only the named
// begin/end pair's array role is inferred from the native difference / 4 and
// forwarded pointer; the class spelling and meanings of the other words remain
// unresolved.
struct MessageSaveCheckDonePayload
{
    RealmcCore::Message mHeader;
    u32 muField08;
    u32 muField0C;
    const s32* mpValuesBegin; // +0x10
    const s32* mpValuesEnd;   // +0x14
    u8 maOpaque18_1F[8];      // +0x18..+0x1F; not read by the handler
    u32 muField20;
    u32 muField24;
};

} // namespace RealmcIface

#if defined(CGS_PLATFORM_X360)
CGS_STATIC_ASSERT((sizeof(RealmcCore::Message) == 8), "ARTIST Message header size");
CGS_STATIC_ASSERT((offsetof(RealmcIface::MessageCallback3CPayload, maArguments) == 8),
                  "ARTIST Callback3C first payload word");
CGS_STATIC_ASSERT((offsetof(RealmcIface::MessageFoundEntryPayload, maPayload08) == 8),
                  "ARTIST FoundEntry inline payload start");
CGS_STATIC_ASSERT((offsetof(RealmcIface::MessageFindEntriesDonePayload, muField08) == 8),
                  "ARTIST FindEntriesDone first payload word");
CGS_STATIC_ASSERT((offsetof(RealmcIface::MessageFindEntriesDonePayload, muField0C) == 12),
                  "ARTIST FindEntriesDone second payload word");
CGS_STATIC_ASSERT((offsetof(RealmcIface::MessageSaveCheckDonePayload, muField08) == 8),
                  "ARTIST SaveCheckDone first payload word");
CGS_STATIC_ASSERT((offsetof(RealmcIface::MessageSaveCheckDonePayload, muField0C) == 12),
                  "ARTIST SaveCheckDone second payload word");
CGS_STATIC_ASSERT((offsetof(RealmcIface::MessageSaveCheckDonePayload, mpValuesBegin) == 16),
                  "ARTIST SaveCheckDone values begin");
CGS_STATIC_ASSERT((offsetof(RealmcIface::MessageSaveCheckDonePayload, mpValuesEnd) == 20),
                  "ARTIST SaveCheckDone values end");
CGS_STATIC_ASSERT((offsetof(RealmcIface::MessageSaveCheckDonePayload, maOpaque18_1F) == 24),
                  "ARTIST SaveCheckDone opaque middle bytes");
CGS_STATIC_ASSERT((offsetof(RealmcIface::MessageSaveCheckDonePayload, muField20) == 32),
                  "ARTIST SaveCheckDone fifth callback word");
CGS_STATIC_ASSERT((offsetof(RealmcIface::MessageSaveCheckDonePayload, muField24) == 36),
                  "ARTIST SaveCheckDone sixth callback word");
#endif

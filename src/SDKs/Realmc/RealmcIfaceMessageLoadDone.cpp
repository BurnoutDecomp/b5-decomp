#include "SDKs/Realmc/RealmcIfaceMessageLoadDone.h"

// ===========================================================================
// RealmcIface::MessageLoadDone -- reconstructed from BURNOUT_X360_ARTIST.XEX.
//
// No leak source / no DWARF: SHAPE and BODY both come from the X360 asm. See
// RealmcIfaceMessageLoadDone.h for the dispatch layout (target vtable slot
// +0x30) and the class layout.
//
// Bodied here:
//   MessageLoadDone::MessageLoadDone
//   MessageLoadDone::Apply @0x82B570B8
//   MessageLoadDone::`vector deleting destructor` @0x82B570D8 (compiler-generated
//     from the virtual dtor + the class operator delete in the header)
// ===========================================================================

namespace RealmcIface
{

// ---------------------------------------------------------------------------
// MessageLoadDone::MessageLoadDone
//
//   allocator("EASTL basic_string")                 (an empty body)
//   temp = basic_string<char>(pText, allocator)      (zero, strlen, RangeInitialize)
//   MessageString::MessageString(this, uId, temp)    (copies temp's range)
//   if (temp capacity > 1 && temp.begin) allocator::deallocate(temp.begin, capacity)
//   +0x1C = uField1C ; +0x20 = pField20 ; +0x24 = iField24
//   install the MessageLoadDone vtable
//
// The temporary string is the base initializer's argument, so it is destroyed
// (RealmcString::~RealmcString) right after the base is built, before the own
// words are stored -- the console order.
// ---------------------------------------------------------------------------
MessageLoadDone::MessageLoadDone(std::uint32_t uId, int iField24, const char* pText,
                                 u32 uField1C, void* pField20)
    : RealmcCore::MessageString(uId, RealmcCore::RealmcString(pText, RealmcCore::allocator()))
    , muField1C(uField1C)
    , mpField20(pField20)
    , miField24(iField24)
{
}

// ---------------------------------------------------------------------------
// MessageLoadDone::Apply (Message vtable +8)
//
// Swap the two arguments so the processor becomes `this`, load the processor's
// vtable slot +0x30 and tail-call it with the message.
// ---------------------------------------------------------------------------
void MessageLoadDone::Apply(RealmcCore::IMessageProcessor* pProcessor)
{
    pProcessor->ProcessMessage(this);
}

} // namespace RealmcIface

#include "GameShared/GameClasses/Containers/CgsArray.h"

// Explicit instantiation(s) of the generic Array<T,N> container methods (inline in CgsArray.h)
// for the Array<u16, 256> leaf instantiation -- the committed Array_/EventQueue_ explicit-
// instantiation pattern. The element type is the X360/DWARF-authoritative unsigned 16-bit word
// (CgsContainers::Array<uint16_t,256u>, CgsArray.h:5519 / IDA `unsigned __int16*`); a primitive
// element needs no element_home include.
//   X360 0x82318A40 = Array<u16,256u>::GetLength (const)
//   X360 0x8231AA68 = Array<u16,256u>::GetItem (non-const)
//   X360 0x8235D608 = Array<u16,256u>::Append
//   X360 0x8235D728 = Array<u16,256u>::FindFirstInstanceOf (const)
template u32  Array<u16, 256>::GetLength() const;
template u16& Array<u16, 256>::GetItem(u32);
template void Array<u16, 256>::Append(const u16&);
template s32  Array<u16, 256>::FindFirstInstanceOf(const u16&) const;

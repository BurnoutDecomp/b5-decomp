// Per-instantiation .cpp for Stack<u16,2>. The generic Stack<Type,N> body (Push / Pop /
// Peek / operator[] + siblings) is fully inline in CgsStack.h, so this TU just forces the
// out-of-line emission of the four members the X360 attests (under the IDA-truncated symbol
// "short,2>" == unsigned short == u16), all driven by BrnFlapt::FlaptRenderer's per-mask
// mesh-count stack (Stack<u16,2> mMaskMeshCounts @ +0x880, miLength @ +0x884):
//   Stack<u16,2>::Push       @ 0x8246E738  (FlaptRenderer::StartDrawingMask)
//   Stack<u16,2>::Pop        @ 0x8246E7F8  (FlaptRenderer::PopMask)
//   Stack<u16,2>::Peek       @ 0x8246E8A8  (FlaptRenderer::PopMask)
//   Stack<u16,2>::operator[] @ 0x8246D8F8  (FlaptRenderer::RenderMask)
//
// Element type u16 (unsigned short): the asm slot stride is exactly 2 bytes -- Push does
// lhz/sthx (single halfword load/store) with slwi idx,1, Peek returns this + miLength*2 - 2,
// operator[] returns idx*2 + this. maData[2] occupies +0..+3, the count word miLength sits at +4.
// The X360 Push !IsFull assert compares miLength against 2, so N=2. Bodies are header-inline in
// CgsStack.h; this TU mirrors CgsStackUnsignedChar199.cpp.
#include "GameShared/GameClasses/Containers/CgsStack.h"

template void              CgsContainers::Stack<u16, 2>::Push(const u16&);
template void              CgsContainers::Stack<u16, 2>::Pop();
template const u16&        CgsContainers::Stack<u16, 2>::Peek() const;
template u16&              CgsContainers::Stack<u16, 2>::operator[](s32);
